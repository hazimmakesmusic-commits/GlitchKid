#include "MasteringScreen.h"
#include "PlaygroundWidgets.h"
#include "../Analysis/ReferenceAnalyzer.h"

namespace glitchkid::ui
{
using namespace nk;
namespace
{
constexpr float silent = master::LoudnessMeter::silence;
juce::String lufsText (float v) { return v > -70.0f ? juce::String (v, 1) : juce::String ("--"); }
juce::String dbText (float v) { return v > -100.0f ? juce::String (v, 1) : juce::String ("--"); }
}

void MasteringScreen::Tile::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (on ? uv.withAlpha (0.14f) : over ? surfaceHi : surface);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (on ? uv : hairline);
    g.drawRoundedRectangle (r, 12.0f, on ? 1.6f : 1.0f);
    if (on)
    {
        g.setColour (uv.withAlpha (0.08f));
        g.drawRoundedRectangle (r.expanded (2.5f), 14.0f, 3.0f);
    }
    g.setFont (fontOf (15.0f, true));
    g.setColour (white);
    g.drawText (title, r.reduced (16.0f, 12.0f).removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.0f));
    g.setColour (on ? uvSoft : dim);
    g.drawFittedText (description, r.reduced (16.0f, 12.0f).withTrimmedTop (26.0f).toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (uvSoft.withAlpha (0.7f));
        g.drawRoundedRectangle (r.expanded (1.0f), 13.0f, 1.0f);
    }
}

MasteringScreen::MasteringScreen (GlitchKidProcessor& p, Hooks h) : proc (p), hooks (std::move (h))
{
    setOpaque (true);
    struct Spec { const char* id; const char* name; bool large; std::function<juce::String (float)> text; const char* tip; };
    const Spec specs[5] = {
        { GlitchKidProcessor::masterIntensityId, "INTENSITY", true, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; },
          "How much of the style is used: its EQ, glue compression, saturation and width. 0 leaves only the limiter." },
        { GlitchKidProcessor::masterLoudnessId, "LOUDNESS", true, [] (float v) { return "+" + juce::String (v, 1) + " dB"; },
          "Drive into the true-peak limiter. More is louder and less dynamic; watch LIMITING on the right. Peaks never pass -1 dBTP." },
        { GlitchKidProcessor::masterToneId, "TONE", true, [] (float v) { const int a = juce::roundToInt (std::abs (v)); return a < 3 ? juce::String ("Flat") : (v > 0 ? "Brighter " : "Darker ") + juce::String (a); },
          "Tilts the balance around 1 kHz: up to 2.5 dB more or less top, with the opposite on the lows." },
        { GlitchKidProcessor::masterWidthId, "WIDTH", true, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; },
          "Stereo width above 120 Hz. The mono sum never changes, the low end is never widened, and the side can't outgrow the middle." },
        { GlitchKidProcessor::masterOutputId, "OUTPUT", false, [] (float v) { return juce::String (v, 1) + " dB"; },
          "Output level after the limiter (it only goes down, so the ceiling holds)." } };
    for (size_t i = 0; i < dials.size(); ++i)
    {
        auto& d = dials[i];
        d.name = specs[i].name;
        d.large = specs[i].large;
        d.text = specs[i].text;
        d.slider.setLookAndFeel (&knob);
        d.slider.setTitle (d.name);
        d.slider.setTooltip (specs[i].tip);
        d.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (specs[i].id)))
        {
            d.attachment = std::make_unique<juce::SliderParameterAttachment> (*prm, d.slider);
            d.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        }
        d.slider.textFromValueFunction = [this, i] (double v) { return dials[i].text ((float) v); };
        d.slider.onValueChange = [this] { repaint (dialRow); repaint (outputBox); };
        addAndMakeVisible (d.slider);
    }
    for (int s = 0; s < master::numStyles; ++s)
    {
        auto& t = tiles[(size_t) s];
        t.title = master::styleName ((master::Style) s);
        t.description = master::styleDescription ((master::Style) s);
        t.setTitle (t.title);
        t.setTooltip (t.title + ": " + t.description + " A starting point: listen and adjust.");
        t.onClick = [this, s] { setParam (GlitchKidProcessor::masterStyleId, (float) s); sync(); };
        addAndMakeVisible (t);
    }
    backButton.setTooltip ("Back to the vocal instrument.");
    backButton.onClick = [this] { if (hooks.back) hooks.back(); };
    modeButton.setTooltip ("Mastering mode: this GlitchKid masters the finished mix instead of processing a vocal. Use it on your master bus (a separate instance from your vocal one).");
    modeButton.onClick = [this] { setParam (GlitchKidProcessor::masterOnId, proc.isMasteringMode() ? 0.0f : 1.0f); proc.requestMasterMeterReset(); sync(); };
    originalButton.getProperties().set ("secondary", true);
    originalButton.setTooltip ("The untouched mix, time-aligned and played at the master's loudness (kept below clipping), so louder never wins.");
    masterButton.setTooltip ("The master.");
    originalButton.onClick = [this] { setParam (GlitchKidProcessor::bypassId, 1.0f); sync(); };
    masterButton.onClick = [this] { setParam (GlitchKidProcessor::bypassId, 0.0f); sync(); };
    loadButton.setTooltip ("Measure a finished mix file (" + analysis::referenceFormatNames() + ") and get it ready to export. The file is only read.");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a finished mix", juce::File(), analysis::referenceFileWildcard());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<MasteringScreen> (this)] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult() != juce::File()) safe->load (fc.getResult()); });
    };
    liveButton.setTooltip ("Forget the loaded file and show the mix playing through this track.");
    liveButton.onClick = [this] { mixFile = juce::File(); fileReport = master::MixReport(); status = juce::String(); sync(); };
    exportButton.setTooltip ("Render the loaded mix through these exact settings to a 24-bit WAV at its own sample rate (offline, same length, time-aligned). Your mix file is never changed.");
    exportButton.onClick = [this]
    {
        if (! mixFile.existsAsFile()) return;
        chooser = std::make_unique<juce::FileChooser> ("Export the master", mixFile.getSiblingFile (mixFile.getFileNameWithoutExtension() + " (GlitchKid master).wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe = juce::Component::SafePointer<MasteringScreen> (this)] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult() != juce::File()) safe->exportTo (fc.getResult().withFileExtension ("wav")); });
    };
    resetButton.setTooltip ("Start the loudness measurement again (for example at the top of the song).");
    resetButton.onClick = [this] { proc.requestMasterMeterReset(); };
    for (auto* b : { &backButton, &modeButton, &originalButton, &masterButton, &loadButton, &liveButton, &exportButton, &resetButton })
        addAndMakeVisible (b);
    sync();
}

MasteringScreen::~MasteringScreen()
{
    stopTimer();
    job.stopJob();
    for (auto& d : dials)
        d.slider.setLookAndFeel (nullptr);
}

float MasteringScreen::value (const char* id) const
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        return r->convertFrom0to1 (r->getValue());
    return 0.0f;
}

void MasteringScreen::setParam (const char* id, float plain)
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
    {
        r->beginChangeGesture();
        r->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, r->convertTo0to1 (plain)));
        r->endChangeGesture();
    }
}

void MasteringScreen::pressForTest (const juce::String& text)
{
    for (auto* c : getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && (b->getButtonText() == text || b->getTitle() == text) && b->isEnabled() && b->onClick)
        {
            b->onClick();
            return;
        }
}

void MasteringScreen::load (const juce::File& f)
{
    if (! job.analyse (f, [safe = juce::Component::SafePointer<MasteringScreen> (this), f] (const master::MixReport& r)
    {
        if (safe == nullptr) return;
        safe->waiting = false;
        if (r.ok) { safe->mixFile = f; safe->fileReport = r; safe->status = "Loaded " + r.fileName + " (" + juce::String (r.seconds, 1) + " s). Choose a style, then Export master."; }
        else safe->status = r.message;
        safe->sync();
    }))
        return;
    waiting = true;
    status = "Measuring " + f.getFileName() + "...";
    sync();
}

void MasteringScreen::exportTo (const juce::File& dest)
{
    if (! mixFile.existsAsFile() || ! job.render (mixFile, dest, proc.readMasterControls(), [safe = juce::Component::SafePointer<MasteringScreen> (this)] (const master::RenderResult& r)
    {
        if (safe == nullptr) return;
        safe->waiting = false;
        safe->status = r.message;
        safe->sync();
    }))
        return;
    waiting = true;
    status = "Rendering the master...";
    sync();
}

void MasteringScreen::sync()
{
    const bool on = proc.isMasteringMode(), busy = job.isRunning() || waiting;
    modeButton.setButtonText (on ? "Mastering mode: on" : "Mastering mode: off");
    modeButton.setToggleState (on, juce::dontSendNotification);
    const bool dry = value (GlitchKidProcessor::bypassId) >= 0.5f;
    originalButton.setToggleState (dry, juce::dontSendNotification);
    masterButton.setToggleState (! dry, juce::dontSendNotification);
    const int style = juce::roundToInt (value (GlitchKidProcessor::masterStyleId));
    for (int s = 0; s < master::numStyles; ++s)
        tiles[(size_t) s].setToggleState (s == style, juce::dontSendNotification);
    loadButton.setEnabled (! busy);
    exportButton.setEnabled (! busy && mixFile.existsAsFile());
    if (liveButton.isVisible() != mixFile.existsAsFile())
    {
        liveButton.setVisible (mixFile.existsAsFile());
        resized();
    }
    verdict = mixFile.existsAsFile() ? fileReport.message
            : on ? master::suitability (inInt > -70.0f ? inInt : inSt, inTp, inClips, inCorr, true)
                 : juce::String ("Mastering mode is off: this GlitchKid is processing a vocal. To master, put a GlitchKid on your master bus and switch Mastering mode on, or load a mix file.");
    repaint();
}

void MasteringScreen::visibilityChanged()
{
    if (isShowing()) startTimerHz (15);
    else stopTimer();
}

void MasteringScreen::timerCallback()
{
    const auto& m = proc.getMastering();
    const auto& in = m.inputMeter();
    const auto& out = m.outputMeter();
    inSt = in.shortTermLufs(); inInt = in.integratedLufs(); inTp = in.truePeakDb(); inPk = in.samplePeakDb(); inCorr = in.correlation(); inClips = in.clippedSamples();
    outSt = out.shortTermLufs(); outInt = out.integratedLufs(); outTp = out.truePeakDb(); outClips = out.clippedSamples();
    gr = m.limiterReductionDb();
    comp = m.compressorReductionDb();
    match = m.compareGainDb();
    sync();
}

// ---- layout ----
void MasteringScreen::resized()
{
    const int W = getWidth();
    const int cy = 21;
    backButton.setBounds (W - 48 - 104 - 104 - 4 - 200 - 12 - 84, cy, 84, 30);
    modeButton.setBounds (W - 48 - 104 - 104 - 4 - 200, cy, 196, 30);
    originalButton.setBounds (W - 48 - 104 - 104, cy, 100, 30);
    masterButton.setBounds (W - 48 - 104, cy, 104, 30);

    inputBox = { 48, 96, 296, 248 };
    styleBox = { 368, 96, 384, 248 };
    outputBox = { 776, 96, W - 48 - 776, 248 };
    for (int s = 0; s < master::numStyles; ++s)
        tiles[(size_t) s].setBounds (styleBox.getX() + (s % 2) * 196, styleBox.getY() + 24 + (s / 2) * 112, 188, 104);

    dialRow = { 48, 372, W - 96, 200 };
    const int slot = (styleBox.getRight() - 48) / 4;
    for (int i = 0; i < 4; ++i)
        dials[(size_t) i].slider.setBounds (48 + i * slot + slot / 2 - 66, dialRow.getY() + 34, 132, 132);
    dials[4].slider.setBounds (outputBox.getCentreX() - 48, dialRow.getY() + 52, 96, 96);

    footer = { 48, 608, W - 96, 40 };
    loadButton.setBounds (footer.getX(), footer.getY() + 5, 116, 30);
    liveButton.setBounds (footer.getX() + 124, footer.getY() + 5, 128, 30);
    exportButton.setBounds (footer.getX() + (liveButton.isVisible() ? 260 : 124), footer.getY() + 5, 136, 30);
    resetButton.setBounds (footer.getRight() - 128, footer.getY() + 5, 128, 30);
}

// ---- painting ----
void MasteringScreen::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();
    {
        juce::Graphics::ScopedSaveState s (g); // the screen's one light source, behind the styles
        const auto c = styleBox.toFloat().getCentre();
        g.addTransform (juce::AffineTransform::scale (1.0f, 0.45f, c.x, c.y));
        g.setGradientFill (juce::ColourGradient (uv.withAlpha (0.09f), c.x, c.y, uv.withAlpha (0.0f), c.x + 520.0f, c.y, true));
        g.fillEllipse (c.x - 520.0f, c.y - 520.0f, 1040.0f, 1040.0f);
    }
    // header
    g.setFont (fontOf (24.0f, true));
    g.setColour (white);
    const float mw = juce::GlyphArrangement::getStringWidth (fontOf (24.0f, true), "GLITCHKID");
    g.drawText ("GLITCHKID", 48, 20, (int) mw + 4, 30, juce::Justification::centredLeft, false);
    g.setGradientFill (juce::ColourGradient (uv, 48.0f, 0.0f, pinkNeon, 48.0f + mw, 0.0f, false));
    g.fillRoundedRectangle (48.0f, 52.0f, mw, 2.0f, 1.0f);
    g.setFont (fontOf (11.0f, true));
    g.setColour (uvSoft);
    g.drawText ("MASTERING LAB", 48 + (int) mw + 14, 30, 160, 14, juce::Justification::centredLeft, false);
    g.setColour (hairline);
    g.fillRect (0, 72, getWidth(), 1);

    auto heading = [&g] (const juce::String& t, juce::Rectangle<int> r) { g.setFont (fontOf (11.5f, true)); g.setColour (dim); g.drawText (t, r.getX(), r.getY(), 300, 16, juce::Justification::centredLeft, false); };
    heading ("INPUT", inputBox);
    heading ("STYLE", styleBox);
    heading ("OUTPUT", outputBox);
    paintInput (g);
    paintOutput (g);

    // dials: name above, value below
    for (auto& d : dials)
    {
        const auto b = d.slider.getBounds();
        g.setFont (fontOf (d.large ? 13.5f : 12.5f, true));
        g.setColour (white);
        g.drawText (d.name, b.getX() - 30, b.getY() - 22, b.getWidth() + 60, 16, juce::Justification::centred, false);
        g.setFont (fontOf (d.large ? 14.0f : 13.0f));
        g.setColour (uvSoft);
        g.drawText (d.text ((float) d.slider.getValue()), b.getX() - 30, b.getBottom() + 4, b.getWidth() + 60, 18, juce::Justification::centred, false);
    }
    g.setColour (hairline);
    g.fillRect (outputBox.getX() - 12, dialRow.getY() + 8, 1, 180);

    // footer: what the last action did
    g.setFont (fontOf (12.0f));
    g.setColour (status.startsWith ("Saved") ? good : dim);
    const int sx = exportButton.getRight() + 16;
    juce::String shown = status;
    if (job.isRunning()) shown += "  " + juce::String (juce::roundToInt (job.progress() * 100.0f)) + " %";
    g.drawFittedText (shown, sx, footer.getY(), resetButton.getX() - 12 - sx, footer.getHeight(), juce::Justification::centredLeft, 2, 0.85f);
}

void MasteringScreen::paintInput (juce::Graphics& g)
{
    const auto r = inputBox.withTrimmedTop (24);
    const bool file = mixFile.existsAsFile();
    const float lufs = file ? fileReport.integratedLufs : (inInt > -70.0f ? inInt : inSt);
    const float tp = file ? fileReport.truePeakDb : inTp, pk = file ? fileReport.samplePeakDb : inPk, corr = file ? fileReport.correlation : inCorr;
    const int clips = file ? fileReport.clippedSamples : inClips;
    g.setFont (fontOf (12.0f));
    g.setColour (faint);
    g.drawFittedText (file ? "File: " + fileReport.fileName : juce::String ("Live: the mix playing through this track"), r.getX(), r.getY(), r.getWidth(), 16, juce::Justification::centredLeft, 1, 0.85f);
    g.setFont (fontOf (34.0f, true));
    g.setColour (white);
    g.drawText (lufsText (lufs), r.getX(), r.getY() + 20, 140, 40, juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.0f));
    g.setColour (dim);
    g.drawText (file ? "LUFS integrated" : (inInt > -70.0f ? "LUFS integrated" : "LUFS short-term"), r.getX() + 142, r.getY() + 34, 150, 16, juce::Justification::centredLeft, false);
    g.drawText ("Peak " + dbText (pk) + " dBFS    True peak " + dbText (tp) + " dBTP", r.getX(), r.getY() + 66, r.getWidth(), 16, juce::Justification::centredLeft, false);
    // clipping
    g.setColour (clips > 0 ? bad : good);
    g.fillEllipse ((float) r.getX(), (float) r.getY() + 91.0f, 8.0f, 8.0f);
    g.setColour (dim);
    g.drawText (clips > 0 ? juce::String (clips) + " clipped samples" : juce::String ("No clipping"), r.getX() + 14, r.getY() + 87, 200, 16, juce::Justification::centredLeft, false);
    // stereo: correlation from -1 (out of phase) to +1 (mono)
    const juce::Rectangle<float> bar ((float) r.getX(), (float) r.getY() + 118.0f, (float) r.getWidth(), 4.0f);
    g.setColour (hairline);
    g.fillRoundedRectangle (bar, 2.0f);
    const float x = bar.getX() + (corr + 1.0f) * 0.5f * bar.getWidth();
    g.setColour (corr < 0.0f ? bad : uv);
    g.fillEllipse (x - 5.0f, bar.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour (faint);
    g.drawText ("Out of phase", (int) bar.getX(), (int) bar.getBottom() + 6, 120, 14, juce::Justification::centredLeft, false);
    g.drawText ("Mono", (int) bar.getRight() - 60, (int) bar.getBottom() + 6, 60, 14, juce::Justification::centredRight, false);
    g.setColour (dim);
    g.drawText ("Stereo correlation " + juce::String (corr, 2), r.getX(), r.getY() + 146, r.getWidth(), 16, juce::Justification::centredLeft, false);
    // the verdict
    g.setFont (fontOf (12.5f));
    g.setColour (verdict.startsWith ("Ready") ? good : verdict.startsWith ("Mastering mode is off") || verdict.startsWith ("Nothing") ? dim : warn);
    g.drawFittedText (verdict, r.getX(), r.getY() + 168, r.getWidth(), 56, juce::Justification::topLeft, 4, 0.9f);
}

void MasteringScreen::paintOutput (juce::Graphics& g)
{
    const auto r = outputBox.withTrimmedTop (24);
    const bool live = proc.isMasteringMode();
    g.setFont (fontOf (34.0f, true));
    g.setColour (live ? white : faint);
    g.drawText (live ? lufsText (outSt) : juce::String ("--"), r.getX(), r.getY() + 4, 140, 40, juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.0f));
    g.setColour (dim);
    g.drawText ("LUFS short-term", r.getX() + 142, r.getY() + 18, 150, 16, juce::Justification::centredLeft, false);
    g.drawText ("Integrated " + (live ? lufsText (outInt) : juce::String ("--")) + " LUFS", r.getX(), r.getY() + 50, r.getWidth(), 16, juce::Justification::centredLeft, false);
    // true peak against the ceiling
    const juce::Rectangle<float> tpBar ((float) r.getX(), (float) r.getY() + 80.0f, (float) r.getWidth(), 6.0f);
    g.setColour (hairline);
    g.fillRoundedRectangle (tpBar, 3.0f);
    auto at = [&tpBar] (float db) { return tpBar.getX() + juce::jlimit (0.0f, 1.0f, (db + 24.0f) / 24.0f) * tpBar.getWidth(); };
    if (live && outTp > -100.0f)
    {
        g.setColour (outTp > -0.9f ? bad : uv);
        g.fillRoundedRectangle (tpBar.withRight (at (outTp)), 3.0f);
    }
    g.setColour (pinkNeon);
    g.fillRect (at (-1.0f) - 1.0f, tpBar.getY() - 4.0f, 2.0f, tpBar.getHeight() + 8.0f);
    g.setColour (dim);
    g.drawText ("True peak " + (live ? dbText (outTp) : juce::String ("--")) + " dBTP    ceiling -1.0", r.getX(), r.getY() + 92, r.getWidth(), 16, juce::Justification::centredLeft, false);
    // limiting and glue
    g.drawText ("Limiting " + juce::String (live ? gr : 0.0f, 1) + " dB    Glue " + juce::String (live ? comp : 0.0f, 1) + " dB", r.getX(), r.getY() + 114, r.getWidth(), 16, juce::Justification::centredLeft, false);
    if (live && gr > 6.0f)
    {
        g.setColour (warn);
        g.drawFittedText ("Heavy limiting: expect less punch. Lower Loudness if it sounds squashed.", r.getX(), r.getY() + 134, r.getWidth(), 30, juce::Justification::topLeft, 2, 0.9f);
    }
    g.setColour (outClips > 0 ? bad : good);
    g.fillEllipse ((float) r.getX(), (float) r.getY() + 172.0f, 8.0f, 8.0f);
    g.setColour (dim);
    g.drawText (outClips > 0 ? "Clipped: " + juce::String (outClips) + " samples" : juce::String ("No clipping"), r.getX() + 14, r.getY() + 168, 200, 16, juce::Justification::centredLeft, false);
    if (live && std::abs (match) > 0.05f)
        g.drawText ("Original is matched " + juce::String (match, 1) + " dB", r.getX(), r.getY() + 190, r.getWidth(), 16, juce::Justification::centredLeft, false);
}
}
