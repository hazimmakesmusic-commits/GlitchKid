#include "MainScreen.h"
#include "NeonKit.h"
#include "PlaygroundWidgets.h"
#include "../Macros/MacroEngine.h"

namespace glitchkid::ui
{
using namespace nk;

// ---- the screen ----
MainScreen::MainScreen (GlitchKidProcessor& p, Hooks h) : proc (p), hooks (std::move (h))
{
    setOpaque (true);
    scratch.resize (1600);
    setupDials();

    auto choices = [this] (juce::ComboBox& box, const char* id, std::unique_ptr<juce::ComboBoxParameterAttachment>& att, const juce::String& tip)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        {
            box.addItemList (c->choices, 1);
            att = std::make_unique<juce::ComboBoxParameterAttachment> (*c, box);
        }
        box.setTooltip (tip);
        box.setColour (juce::ComboBox::backgroundColourId, surface);
        box.setColour (juce::ComboBox::outlineColourId, hairline);
        box.setColour (juce::ComboBox::textColourId, white);
        addAndMakeVisible (box);
    };
    choices (keyBox, GlitchKidProcessor::tuneKeyId, keyAtt, "The key Pitch Tune pulls toward.");
    choices (scaleBox, GlitchKidProcessor::tuneScaleId, scaleAtt, "The scale Pitch Tune pulls toward (Chromatic = every note).");
    keyBox.setTitle ("Key");
    scaleBox.setTitle ("Scale");

    soundsButton.setTooltip ("The starting sounds, each described, and Build My Mix (fits a starting sound to your vocal).");
    soundsButton.onClick = [this] { if (hooks.openSounds) hooks.openSounds(); };
    kidButton.setTooltip ("Kid, your vocal producer: describe the sound you want and Kid sets these controls for you. Undo goes back.");
    kidButton.onClick = [this] { if (hooks.openKid) hooks.openKid(); };
    for (auto* b : { &soundsButton, &kidButton })
        addAndMakeVisible (b);
    sync();
}

MainScreen::~MainScreen()
{
    stopTimer();
    for (auto& d : dials)
        d.slider.setLookAndFeel (nullptr);
}

float MainScreen::value (const char* id) const
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        return r->convertFrom0to1 (r->getValue());
    return 0.0f;
}

void MainScreen::setParam (const char* id, float plain)
{
    auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
    if (r == nullptr || ! std::isfinite (plain))
        return;
    const float v = juce::jlimit (0.0f, 1.0f, r->convertTo0to1 (plain));
    if (std::abs (v - r->getValue()) < 1.0e-6f)
        return;
    r->beginChangeGesture();
    r->setValueNotifyingHost (v);
    r->endChangeGesture();
}

void MainScreen::setupDials()
{
    auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
    auto onOff = [this, pct] (Dial& d, const char* onId, const char* amountId)
    {
        d.get = [this, onId, amountId] { return value (onId) >= 0.5f ? value (amountId) : 0.0f; };
        d.set = [this, onId, amountId] (float v) { if (v >= 0.5f) { setParam (amountId, v); setParam (onId, 1.0f); } else setParam (onId, 0.0f); };
        d.text = [pct] (float v) { return v < 0.5f ? juce::String ("Off") : pct (v); };
    };
    auto macro = [this] (Dial& d, macros::Macro m)
    {
        d.get = [this, m] { return 100.0f * macros::read (m, proc.captureSoundState()); };
        d.set = [this, m] (float v) { auto s = proc.captureSoundState(); macros::apply (m, v / 100.0f, s); proc.applyParameterChanges (s); };
    };
    auto& tune = dials[0];
    tune.name = "TUNE";
    tune.what = "Pitch correction strength toward the key and scale below. 0 is off. Adds about 31 ms of latency while on.";
    onOff (tune, GlitchKidProcessor::tuneOnId, GlitchKidProcessor::tuneStrengthId);
    auto& clean = dials[1];
    clean.name = "CLEAN";
    clean.what = "CLEAN LAB: rumble, low-mid build-up, harsh resonances and sharp S sounds, measured on your voice. 0 is off.";
    onOff (clean, GlitchKidProcessor::cleanOnId, GlitchKidProcessor::cleanAmountId);
    auto& tone = dials[2];
    tone.name = "TONE";
    tone.what = "Darker to brighter: the air (8 kHz) and presence (3 kHz) EQ bands together. The middle is flat.";
    macro (tone, macros::Macro::tone);
    tone.text = [] (float v) { const int a = juce::roundToInt (std::abs (v - 50.0f) * 2.0f); return a < 3 ? juce::String ("Flat") : (v > 50.0f ? "Brighter " : "Darker ") + juce::String (a); };
    auto& punch = dials[3];
    punch.name = "PUNCH";
    punch.what = "How firmly the compressor holds your voice: steadier, more upfront.";
    macro (punch, macros::Macro::punch);
    punch.text = pct;
    auto& space = dials[4];
    space.name = "SPACE";
    space.what = "Echo, width and doubles together. GlitchKid has no reverb: space is delay and width.";
    macro (space, macros::Macro::space);
    space.text = [pct] (float v) { return v < 0.5f ? juce::String ("Dry") : pct (v); };
    auto& mix = dials[5];
    mix.name = "MIX";
    mix.what = "Processed against your untouched voice.";
    mix.large = false;
    mix.text = pct;
    auto& out = dials[6];
    out.name = "OUTPUT";
    out.what = "Output level after GlitchKid (before the limiter).";
    out.large = false;
    out.text = [] (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; };

    const char* attached[numDials] = { nullptr, nullptr, nullptr, nullptr, nullptr, GlitchKidProcessor::mixId, GlitchKidProcessor::gainId };
    const float defaults[numDials] = { 0.0f, 0.0f, 50.0f, 0.0f, 0.0f, 100.0f, 0.0f };
    for (int i = 0; i < numDials; ++i)
    {
        auto& d = dials[(size_t) i];
        d.slider.setLookAndFeel (&knob);
        d.slider.setTitle (d.name);
        d.slider.setTooltip (d.what);
        d.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        if (attached[i] != nullptr)
        {
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (attached[i])))
            {
                d.attachment = std::make_unique<juce::SliderParameterAttachment> (*prm, d.slider);
                d.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            }
        }
        else
        {
            d.slider.setRange (0.0, 100.0, 0.1);
            d.slider.setDoubleClickReturnValue (true, defaults[i]);
            d.slider.setValue (d.get(), juce::dontSendNotification);
            d.slider.onValueChange = [this, i] { auto& x = dials[(size_t) i]; x.set ((float) x.slider.getValue()); repaint (controls); };
        }
        d.slider.textFromValueFunction = [this, i] (double v) { return dials[(size_t) i].text ((float) v); }; // what a screen reader announces
        addAndMakeVisible (d.slider);
    }
}

void MainScreen::pressForTest (const juce::String& text)
{
    for (auto* c : getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == text && b->onClick)
        {
            b->onClick();
            return;
        }
}

// The dials driven by several parameters (on/off + amount, the macros) read back from the parameters, so they always agree with the detailed pages.
void MainScreen::sync()
{
    for (auto& d : dials)
        if (d.attachment == nullptr && ! d.slider.isMouseButtonDown())
        {
            const double v = d.get();
            if (std::abs (v - d.slider.getValue()) > 0.05)
                d.slider.setValue (v, juce::dontSendNotification);
        }
    keyBox.setEnabled (value (GlitchKidProcessor::tuneOnId) >= 0.5f);
    scaleBox.setEnabled (keyBox.isEnabled());
    repaint (controls);
}

void MainScreen::visibilityChanged()
{
    if (isShowing()) startTimerHz (30);
    else stopTimer();
}

void MainScreen::timerCallback()
{
    ++tick;
    auto level = [this] (const analysis::DisplayTap& tap, juce::uint32& last, float& db)
    {
        const auto wc = tap.writeCount();
        const bool moving = wc != last;
        last = wc;
        tap.copyLatest (scratch.data(), (int) scratch.size());
        float peak = 0.0f;
        for (auto v : scratch) peak = juce::jmax (peak, std::abs (v));
        db = moving ? juce::Decibels::gainToDecibels (peak, -100.0f) : -100.0f;
        return moving && db > -70.0f ? juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) : 0.0f;
    };
    std::rotate (envIn.rbegin(), envIn.rbegin() + 1, envIn.rend());
    std::rotate (envOut.rbegin(), envOut.rbegin() + 1, envOut.rend());
    envIn[0] = level (proc.getInputTap(), lastIn, inDb);
    envOut[0] = level (proc.getOutputTap(), lastOut, outDb);
    signalOut = envOut[0] > 0.0f || envIn[0] > 0.0f;
    if (tick % 6 == 0)
        sync();
    repaint (ribbon.expanded (12));
}

// ---- layout ----
void MainScreen::resized()
{
    const int W = getWidth();
    guide = { 16, 8, W - 32, 34 };
    soundsButton.setBounds (guide.getX(), guide.getY() + 2, 150, 30);
    kidButton.setBounds (guide.getRight() - 96, guide.getY() + 2, 96, 30);
    ribbon = { 24, 54, W - 48, 172 };
    controls = { 24, 244, W - 48, getHeight() - 252 };

    // three sections: VOICE (2 large), CHARACTER (3 large), OUTPUT (2 smaller)
    const int x0 = controls.getX(), w = controls.getWidth();
    sections[0] = { x0, controls.getY(), w * 31 / 100, controls.getHeight() };
    sections[1] = { sections[0].getRight(), controls.getY(), w * 45 / 100, controls.getHeight() };
    sections[2] = { sections[1].getRight(), controls.getY(), controls.getRight() - sections[1].getRight(), controls.getHeight() };
    auto place = [this] (int dial, juce::Rectangle<int> sec, int slot, int slots)
    {
        const int size = dials[(size_t) dial].large ? 116 : 88;
        const int cx = sec.getX() + sec.getWidth() * (2 * slot + 1) / (2 * slots);
        dials[(size_t) dial].slider.setBounds (cx - size / 2, sec.getY() + 50 + (116 - size) / 2, size, size);
    };
    place (0, sections[0], 0, 2);
    place (1, sections[0], 1, 2);
    place (2, sections[1], 0, 3);
    place (3, sections[1], 1, 3);
    place (4, sections[1], 2, 3);
    place (5, sections[2], 0, 2);
    place (6, sections[2], 1, 2);
    const auto t = dials[0].slider.getBounds();
    keyBox.setBounds (t.getCentreX() - 84, t.getBottom() + 28, 58, 26);
    scaleBox.setBounds (t.getCentreX() - 22, t.getBottom() + 28, 106, 26);
}

// ---- painting ----
void MainScreen::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();
    // a faint ultraviolet bloom behind the ribbon: the screen's one light source
    const auto rc = ribbon.toFloat().getCentre();
    {
        juce::Graphics::ScopedSaveState glowState (g); // an elliptical bloom: a round gradient squashed vertically, fading to nothing before its edge
        g.addTransform (juce::AffineTransform::scale (1.0f, 0.32f, rc.x, rc.y));
        const float R = (float) ribbon.getWidth() * 0.6f;
        g.setGradientFill (juce::ColourGradient (uv.withAlpha (0.11f), rc.x, rc.y, uv.withAlpha (0.0f), rc.x + R, rc.y, true));
        g.fillEllipse (rc.x - R, rc.y - R, 2.0f * R, 2.0f * R);
    }

    // the guide: the next step, in one line
    g.setFont (fontOf (12.5f));
    g.setColour (dim);
    g.drawFittedText ("then play your vocal, shape it with the dials, compare with ORIGINAL / PROCESSED (top right) and save it in the bar below.",
                      soundsButton.getRight() + 14, guide.getY(), kidButton.getX() - 14 - soundsButton.getRight() - 14, guide.getHeight(), juce::Justification::centredLeft, 2, 0.85f);

    paintRibbon (g);

    // the sections: a small name and a hairline between them, no boxes
    static const char* sectionName[3] = { "VOICE", "CHARACTER", "OUTPUT" };
    for (int i = 0; i < 3; ++i)
    {
        g.setFont (fontOf (11.5f, true));
        g.setColour (dim);
        g.drawText (sectionName[i], sections[(size_t) i].getX() + (i == 0 ? 0 : 24), sections[(size_t) i].getY(), 200, 16, juce::Justification::centredLeft, false);
        if (i > 0)
        {
            g.setColour (hairline);
            g.fillRect (sections[(size_t) i].getX(), sections[(size_t) i].getY() + 4, 1, 236);
        }
    }
    // each dial: its name above, its value below
    for (auto& d : dials)
    {
        const auto b = d.slider.getBounds();
        g.setFont (fontOf (d.large ? 13.5f : 12.5f, true));
        g.setColour (white);
        g.drawText (d.name, b.getX() - 30, b.getY() - (d.large ? 24 : 22), b.getWidth() + 60, 16, juce::Justification::centred, false);
        g.setFont (fontOf (d.large ? 14.0f : 13.0f));
        g.setColour (d.slider.getValue() > 0.4 || d.attachment != nullptr ? uvSoft : faint);
        g.drawText (d.text ((float) d.slider.getValue()), b.getX() - 30, b.getBottom() + 4, b.getWidth() + 60, 18, juce::Justification::centred, false);
    }
}

void MainScreen::paintRibbon (juce::Graphics& g)
{
    const auto r = ribbon.toFloat();
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.46f;
    const int n = (int) envOut.size();
    auto xAt = [&] (int i) { return r.getX() + (float) (n - 1 - i) / (float) (n - 1) * r.getWidth(); }; // newest on the right
    auto smooth = [] (const std::array<float, 240>& e, int i) { const int a = juce::jmax (0, i - 1), b = juce::jmin ((int) e.size() - 1, i + 1); return (e[(size_t) a] + 2.0f * e[(size_t) i] + e[(size_t) b]) * 0.25f; };

    g.setColour (hairline);
    g.fillRect (r.getX(), mid, r.getWidth(), 1.0f);

    // what GlitchKid sends out: a mirrored, glowing ribbon
    juce::Path out, edge;
    for (int i = n - 1; i >= 0; --i)
    {
        const float y = mid - juce::jmax (0.5f, smooth (envOut, i) * amp);
        if (i == n - 1) { out.startNewSubPath (xAt (i), y); edge.startNewSubPath (xAt (i), y); }
        else { out.lineTo (xAt (i), y); edge.lineTo (xAt (i), y); }
    }
    for (int i = 0; i < n; ++i)
        out.lineTo (xAt (i), mid + juce::jmax (0.5f, smooth (envOut, i) * amp));
    out.closeSubPath();
    juce::ColourGradient fill (uv.withAlpha (0.04f), r.getX(), 0.0f, uv.withAlpha (0.42f), r.getRight(), 0.0f, false);
    g.setGradientFill (fill);
    g.fillPath (out);
    juce::Path lower (edge);
    lower.applyTransform (juce::AffineTransform::verticalFlip (2.0f * mid));
    juce::ColourGradient line (uv.withAlpha (0.25f), r.getX(), 0.0f, uv, r.getRight() - 40.0f, 0.0f, false);
    line.addColour (1.0, pinkNeon);
    for (auto* p : { &edge, &lower })
    {
        g.setGradientFill (line);
        g.setOpacity (0.18f);
        g.strokePath (*p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setOpacity (1.0f);
        g.strokePath (*p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // what comes in: a faint line, so you see what GlitchKid did to it
    juce::Path in;
    for (int i = n - 1; i >= 0; --i)
    {
        const float y = mid - smooth (envIn, i) * amp;
        if (i == n - 1) in.startNewSubPath (xAt (i), y); else in.lineTo (xAt (i), y);
    }
    g.setColour (white.withAlpha (0.28f));
    g.strokePath (in, juce::PathStrokeType (1.1f));
    if (envOut[0] > 0.0f)
    {
        const float y = smooth (envOut, 0) * amp;
        g.setColour (pinkNeon.withAlpha (0.3f));
        g.fillEllipse (r.getRight() - 6.0f, mid - y - 6.0f, 12.0f, 12.0f);
        g.setColour (pinkNeon);
        g.fillEllipse (r.getRight() - 3.0f, mid - y - 3.0f, 6.0f, 6.0f);
    }

    // the legend and the real levels
    g.setFont (fontOf (11.5f));
    g.setColour (white.withAlpha (0.45f));
    g.fillRect (r.getX(), r.getY() + 7.0f, 14.0f, 1.2f);
    g.drawText ("In", (int) r.getX() + 20, (int) r.getY(), 40, 14, juce::Justification::centredLeft, false);
    g.setColour (uv);
    g.fillRect (r.getX() + 52.0f, r.getY() + 6.0f, 14.0f, 3.0f);
    g.setColour (dim);
    g.drawText ("Out", (int) r.getX() + 72, (int) r.getY(), 40, 14, juce::Justification::centredLeft, false);
    if (signalOut)
        g.drawText ("In " + (inDb > -99.0f ? juce::String (inDb, 1) : juce::String ("-")) + " dB    Out " + (outDb > -99.0f ? juce::String (outDb, 1) : juce::String ("-")) + " dB peak",
                    (int) r.getRight() - 260, (int) r.getY(), 260, 14, juce::Justification::centredRight, false);
    else
    {
        g.setFont (fontOf (14.0f));
        g.setColour (faint);
        g.drawText ("Play your track to see your vocal", r.toNearestInt().withHeight ((int) (r.getHeight() * 0.5f) - 10), juce::Justification::centredBottom, false); // above the flat line
    }
    if (proc.isMasteringMode())
    {
        g.setFont (fontOf (13.0f, true));
        g.setColour (pinkNeon);
        g.drawText ("This GlitchKid is in Mastering mode: the vocal controls below are not in use. Switch it off on MASTER.",
                    r.toNearestInt().removeFromBottom (18), juce::Justification::centred, false);
    }
}
}
