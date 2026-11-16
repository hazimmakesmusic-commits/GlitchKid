#include "MainScreen.h"
#include "NeonKit.h"
#include "PlaygroundWidgets.h"
#include "../Macros/MacroEngine.h"
#include "../Presets/FactoryPresets.h"
#include "Brand.h"
#include "Theme.h"

namespace glitchkid::ui
{
using namespace nk;

namespace
{
const char* const keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String noteOf (float midi)
{
    const int n = (int) std::lround (midi);
    return juce::String (keyNames[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}
const char* const moduleName[MainScreen::numModules] = { "AUTOTUNE", "CLEAN LAB", "AIR LAB", "LIMITER" };
const char* const moduleSwitchId[MainScreen::numModules] = { GlitchKidProcessor::tuneOnId, GlitchKidProcessor::cleanOnId, GlitchKidProcessor::airOnId, GlitchKidProcessor::limOnId };
}

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
    choices (keyBox, GlitchKidProcessor::tuneKeyId, keyAtt, "The key AUTOTUNE pulls toward.");
    choices (scaleBox, GlitchKidProcessor::tuneScaleId, scaleAtt, "The scale AUTOTUNE pulls toward (Chromatic = every note).");
    keyBox.setTitle ("Key");
    scaleBox.setTitle ("Scale");

    static const char* switchTip[numModules] = {
        "AUTOTUNE on or off (Pitch Tune). While on it adds about 31 ms of latency, which your DAW compensates.",
        "CLEAN LAB on or off: rumble, low-mid build-up, harsh resonances and sharp S sounds, measured on your voice.",
        "AIR LAB on or off: MID AIR (clarity) and HIGH AIR (air), each following the voice and easing off on S sounds and noise.",
        "LIMITER on or off: catches peaks above the ceiling at the very end of the chain." };
    for (int m = 0; m < numModules; ++m)
    {
        auto& s = switches[(size_t) m];
        s.setClickingTogglesState (true);
        s.setTitle (juce::String (moduleName[m]) + " on/off");
        s.setTooltip (switchTip[m]);
        s.onStateChange = [this, &s]
        {
            const auto t = s.getToggleState() ? "ON" : "OFF";
            if (s.getButtonText() == t)
                return;
            s.setButtonText (t);
            refreshText(); // the module's light and status follow at once, not at the next tick
            repaint();
        };
        if (auto* prm = proc.apvts.getParameter (moduleSwitchId[m]))
            switchAtt[(size_t) m] = std::make_unique<juce::ButtonParameterAttachment> (*prm, s);
        addAndMakeVisible (s);
    }

    // the journey: SOUND, BUILD MY MIX, BEAT
    genreBox.addItem ("No profile: a clean start", 1);
    for (int i = 0; i < FactoryPresets::count; ++i)
        genreBox.addItem (FactoryPresets::info (i).name, i + 2);
    genreBox.setTitle ("Sound");
    genreBox.setTooltip ("The starting sound Build My Mix fits to your vocal. Choosing one changes nothing yet: press BUILD MY MIX, or load it from the preset menu to hear it as saved.");
    genreBox.setColour (juce::ComboBox::backgroundColourId, surface);
    genreBox.setColour (juce::ComboBox::outlineColourId, hairline);
    genreBox.setColour (juce::ComboBox::textColourId, white);
    genreBox.onChange = [this]
    {
        const int chosen = genreBox.getSelectedId() - 2; // id 1 = no profile (-1)
        if (chosen != profile && genreBox.getSelectedId() > 0)
        {
            profile = chosen;
            if (hooks.chooseProfile) hooks.chooseProfile (chosen);
            refreshText();
            repaint (journey);
        }
    };
    addAndMakeVisible (genreBox);
    buildButton.setTooltip ("BUILD MY MIX: listens to your vocal for a few seconds (press play in your DAW), then fits the chosen sound to what it heard. UNDO goes back.");
    buildButton.getProperties().set ("secondary", true);
    buildButton.onClick = [this] { if (hooks.buildMix) hooks.buildMix(); };
    beatButton.setTooltip ("Beat Lock in Advanced Mixing: compares your vocal with the beat on the Instrumental input and suggests small, reviewable changes.");
    beatButton.onClick = [this] { if (hooks.openBeat) hooks.openBeat(); };
    advancedButton.setTooltip ("Every detailed control: CLEAN, TUNE, TONE, DYNAMICS, SPACE, CREATIVE, BEAT, REFERENCE, MASTER, presets and A/B. HOME in its rail comes back here.");
    advancedButton.getProperties().set ("secondary", true);
    advancedButton.setToggleState (true, juce::dontSendNotification); // drawn lit: the way into the detailed pages
    advancedButton.onClick = [this] { if (hooks.openAdvanced) hooks.openAdvanced(); };
    useKeyButton.setTooltip ("Set AUTOTUNE's key and scale to the key Kid estimated from your vocal file. Nothing changes until you press it.");
    useKeyButton.onClick = [this]
    {
        if (suggestedKey < 0)
            return;
        setParam (GlitchKidProcessor::tuneKeyId, (float) (suggestedKey % 12));
        setParam (GlitchKidProcessor::tuneScaleId, suggestedKey >= 12 ? 2.0f : 1.0f); // 1 Major, 2 Minor (the parameter's own choices)
        refreshText();
    };
    mandala = proc.getUiFlag ("uiMandala", false);
    mandalaButton.setToggleState (mandala, juce::dontSendNotification);
    mandalaButton.setTooltip ("Show the live view as a kaleidoscope: the same real input and output levels, mirrored eight ways. It turns slowly while audio plays and MOTION is on.");
    mandalaButton.onClick = [this]
    {
        mandala = ! mandala;
        mandalaButton.setToggleState (mandala, juce::dontSendNotification);
        proc.setUiFlag ("uiMandala", mandala);
        repaint (visual);
    };
    for (auto* b : { &buildButton, &beatButton, &advancedButton, &mandalaButton, &useKeyButton })
        addAndMakeVisible (b);
    sync();
    refreshText();
}

MainScreen::~MainScreen()
{
    stopTimer();
    if (holding) // the window closed mid-hold: never leave the track on ORIGINAL (directly on the parameter: the editor is being torn down)
        setParam (GlitchKidProcessor::bypassId, 0.0f);
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
    using P = GlitchKidProcessor;
    auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
    auto macro = [this] (DialItem& d, macros::Macro m)
    {
        d.get = [this, m] { return 100.0f * macros::read (m, proc.captureSoundState()); };
        d.set = [this, m] (float v) { auto s = proc.captureSoundState(); macros::apply (m, v / 100.0f, s); proc.applyParameterChanges (s); };
    };
    struct Spec { const char* name; const char* what; const char* param; const char* switchOn; };
    const Spec spec[numDials] = {
        { "CORRECTION", "How strongly AUTOTUNE pulls each note to the key and scale (Pitch Tune strength). Dragging it switches AUTOTUNE on.", P::tuneStrengthId, P::tuneOnId },
        { "INTENSITY", "How much CLEAN LAB corrects. Dragging it switches CLEAN LAB on.", P::cleanAmountId, P::cleanOnId },
        { "HIGH AIR", "AIR LAB's HIGH AIR: brilliance and air that adapt to your voice and ease off on S sounds. Dragging it switches AIR LAB on.", P::airAirId, P::airOnId },
        { "CEILING", "The highest level the LIMITER lets through. Dragging it switches the LIMITER on.", P::limCeilingId, P::limOnId },
        { "TONE", "Darker to brighter: the air (8 kHz) and presence (3 kHz) EQ bands together. The middle is flat.", nullptr, nullptr },
        { "PUNCH", "How firmly the compressor holds your voice: steadier, more upfront.", nullptr, nullptr },
        { "SPACE", "The SPACE reverb, echo, width and doubles together.", nullptr, nullptr },
        { "MID AIR", "AIR LAB's MID AIR: clarity and presence that adapt to your voice and ease off on harsh moments. Dragging it switches AIR LAB on.", P::airPresenceId, P::airOnId },
        { "MIX", "Processed against your untouched voice.", P::mixId, nullptr },
        { "OUTPUT", "Output level after GlitchKid (before the limiter).", P::gainId, nullptr } };

    macro (dials[tone], macros::Macro::tone);
    dials[tone].text = [] (float v) { const int a = juce::roundToInt (std::abs (v - 50.0f) * 2.0f); return a < 3 ? juce::String ("Flat") : (v > 50.0f ? "Brighter " : "Darker ") + juce::String (a); };
    macro (dials[punch], macros::Macro::punch);
    macro (dials[space], macros::Macro::space);
    dials[space].text = [pct] (float v) { return v < 0.5f ? juce::String ("Dry") : pct (v); };
    dials[ceiling].text = [] (float v) { return juce::String (v, 1) + " dB"; };
    dials[output].text = [] (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; };

    const float macroDefaults[3] = { 50.0f, 0.0f, 0.0f }; // tone, punch, space
    for (int i = 0; i < numDials; ++i)
    {
        auto& d = dials[(size_t) i];
        d.name = spec[i].name;
        d.what = spec[i].what;
        d.switchOn = spec[i].switchOn;
        if (! d.text)
            d.text = pct;
        d.slider.setLookAndFeel (&knob);
        d.slider.setTitle (d.name);
        d.slider.setTooltip (d.what);
        d.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        if (spec[i].param != nullptr)
        {
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (spec[i].param)))
            {
                d.attachment = std::make_unique<juce::SliderParameterAttachment> (*prm, d.slider);
                d.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            }
        }
        else
        {
            d.slider.setRange (0.0, 100.0, 0.1);
            d.slider.setDoubleClickReturnValue (true, macroDefaults[i - tone]);
            d.slider.setValue (d.get(), juce::dontSendNotification);
            d.slider.onValueChange = [this, i] { auto& x = dials[(size_t) i]; x.set ((float) x.slider.getValue()); repaint (basics); };
        }
        d.slider.onDragStart = [this, i]
        {
            auto& x = dials[(size_t) i];
            x.dragging = true;
            if (x.switchOn != nullptr && ! on (x.switchOn))
                setParam (x.switchOn, 1.0f);
        };
        d.slider.onDragEnd = [this, i] { dials[(size_t) i].dragging = false; };
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

// The macro dials read back from the parameters, so they always agree with the detailed pages (the single-parameter dials and switches follow by attachment).
void MainScreen::sync()
{
    for (auto& d : dials)
        if (d.attachment == nullptr && ! d.slider.isMouseButtonDown())
        {
            const double v = d.get();
            if (std::abs (v - d.slider.getValue()) > 0.05)
                d.slider.setValue (v, juce::dontSendNotification);
        }
}

// Every line of text comes from a parameter or from what the DSP reports; nothing is guessed.
void MainScreen::refreshText()
{
    using P = GlitchKidProcessor;
    const bool dry = on (P::bypassId);
    for (int m = 0; m < numModules; ++m)
        active[(size_t) m] = on (moduleSwitchId[m]) && ! dry;

    // the headline: what reaches GlitchKid and what you hear
    if (proc.isMasteringMode())
        headline = "Mastering mode is on in this GlitchKid: the vocal controls are not in use. Switch it off on MASTER in Advanced Mixing.";
    else if (stillTicks > 30)
        headline = "No audio is reaching GlitchKid: press play in your DAW";
    else if (! signalOut)
        headline = "Silence on the input. Play your vocal.";
    else if (dry)
        headline = proc.isLevelMatchedCompare() ? "ORIGINAL: your untouched vocal, at the processed loudness" : "ORIGINAL: your untouched vocal";
    else
    {
        juce::StringArray on4;
        for (int m = 0; m < numModules; ++m)
            if (active[(size_t) m]) on4.add (moduleName[m]);
        headline = "Hearing your vocal, processed" + (on4.isEmpty() ? juce::String() : ":  " + on4.joinIntoString (", "));
    }

    using SS = GlitchKidProcessor::SourceStatus;
    const auto st = proc.getInstrumentalStatus();
    beatText = st == SS::live ? "Beat playing" : st == SS::connectedSilent ? "Beat input connected, silent" : "No beat connected";
    if (proc.isStutterTempoFromHost()) // the DAW's tempo, as the host reports it (GlitchKid does not detect tempo)
        beatText += "\nHost tempo " + juce::String (proc.getStutterTempoBpm(), 1) + " BPM";
    static const char* nextLine[6] = { "Play your vocal in your DAW", "Choose a sound", "Press BUILD MY MIX and let the vocal play",
                                       "Refine below and compare, then add your beat (optional)", "Fit to beat, compare with ORIGINAL", "Compare, then SAVE it as a preset" };
    nextText = nextLine[nextStep()];

    // AUTOTUNE: the sung note and where it is pulled (the real detector), and a key Kid estimated from a vocal file
    pitchText = {};
    if (on (P::tuneOnId))
    {
        const float sung = proc.getTuneDetectedMidi(), target = proc.getTuneTargetMidi();
        if (sung >= 0.0f && target >= 0.0f)
        {
            const int cents = (int) std::lround ((sung - std::round (sung)) * 100.0f);
            pitchText = noteOf (sung) + (cents >= 0 ? " +" : " ") + juce::String (cents) + " cents  ->  " + noteOf (target);
        }
    }
    status[autotune] = ! on (P::tuneOnId) ? "Off" : pitchText.isNotEmpty() ? "Sung " + pitchText : "Listening for a sung note...";
    int tonic = 0;
    bool minor = false;
    suggestedKey = hooks.fileKey && hooks.fileKey (tonic, minor) ? juce::jlimit (0, 11, tonic) + (minor ? 12 : 0) : -1;
    keyText = suggestedKey < 0 ? "No key suggestion: analyse a vocal file with Kid" : "Your vocal file: " + juce::String (keyNames[suggestedKey % 12]) + (minor ? " minor" : " major");
    const bool keyDiffers = suggestedKey >= 0 && (juce::roundToInt (value (P::tuneKeyId)) != suggestedKey % 12 || juce::roundToInt (value (P::tuneScaleId)) != (minor ? 2 : 1));
    if (useKeyButton.isVisible() != keyDiffers)
        useKeyButton.setVisible (keyDiffers);

    // CLEAN LAB: what it is really applying (its own telemetry)
    if (! on (P::cleanOnId))
        status[cleanLab] = "Off: your voice passes through untouched";
    else
    {
        const auto a = proc.getCleanApplied();
        juce::StringArray parts;
        if (a.rumbleAmount > 0.02f) parts.add ("rumble");
        if (a.mudDb > 0.05f) parts.add ("mud -" + juce::String (a.mudDb, 1) + " dB");
        int res = 0;
        for (int i = 0; i < CleanLab::numRes; ++i) res += a.resDb[i] > 0.05f ? 1 : 0;
        if (res > 0) parts.add (juce::String (res) + (res == 1 ? " resonance" : " resonances"));
        if (a.essGrDb > 0.05f) parts.add ("S -" + juce::String (a.essGrDb, 1) + " dB");
        status[cleanLab] = a.learnedSeconds < 3.0f ? "Learning your voice: " + juce::String (a.learnedSeconds, 1) + " s heard"
                         : parts.isEmpty()        ? "Listening: nothing to fix right now"
                                                  : "Fixing " + parts.joinIntoString (", ");
    }
    status[airLab] = ! on (P::airOnId) ? "Off" : "Mid air " + juce::String (juce::roundToInt (value (P::airPresenceId))) + " %,  high air " + juce::String (juce::roundToInt (value (P::airAirId))) + " %";
    status[limiter] = ! on (P::limOnId) ? "Off: peaks are not caught"
                    : limGrDb > 0.1f    ? "Catching peaks: -" + juce::String (limGrDb, 1) + " dB"
                                        : "Holding below " + juce::String (value (P::limCeilingId), 1) + " dB: nothing to catch";
}

void MainScreen::setProfile (int p)
{
    profile = juce::jlimit (-1, FactoryPresets::count - 1, p);
    genreBox.setSelectedId (profile + 2, juce::dontSendNotification);
}

void MainScreen::setBuild (const juce::String& buttonText, const juce::String& statusText, bool listening, bool applied)
{
    if (buttonText == buildButton.getButtonText() && statusText == buildFullText && listening == buildListening && applied == buildApplied)
        return;
    buildFullText = statusText;
    buildButton.setButtonText (buttonText);
    buildButton.setToggleState (listening, juce::dontSendNotification); // lit while it really listens
    // HOME shows the first sentence of the shared status (the state and the real progress); SOUNDS shows all of it
    const int stop = statusText.indexOf (". ");
    buildStatus = stop > 0 ? statusText.substring (0, stop + 1) : statusText;
    buildListening = listening;
    buildApplied = applied;
    refreshText();
    repaint (journey);
}

// The next step of the work, from the real state only: is audio arriving, is a sound chosen, is a mix built and still loaded, is a beat connected.
int MainScreen::nextStep() const
{
    using SS = GlitchKidProcessor::SourceStatus;
    if (stillTicks > 30 || ! signalOut)
        return buildListening ? 2 : 0;
    if (! buildApplied)
        return profile < 0 && ! buildListening ? 1 : 2;
    if (proc.getInstrumentalStatus() == SS::notConnected)
        return 3;
    return proc.isPresetModified() ? 5 : 4;
}

void MainScreen::visibilityChanged()
{
    if (isShowing()) startTimerHz (30);
    else stopTimer();
}

void MainScreen::mouseDown (const juce::MouseEvent& e) { press (e.getPosition()); }
void MainScreen::mouseUp (const juce::MouseEvent&) { release(); }

// Press and hold the vocal to hear the ORIGINAL; let go to hear GlitchKid again (the Bypass parameter, exactly as the ORIGINAL / PROCESSED buttons).
// If ORIGINAL is already selected, holding changes nothing.
void MainScreen::press (juce::Point<int> at)
{
    if (visual.contains (at) && ! on (GlitchKidProcessor::bypassId) && hooks.holdOriginal)
    {
        holding = true;
        hooks.holdOriginal (true);
        refreshText();
        repaint (visual);
    }
}

void MainScreen::release()
{
    if (! holding)
        return;
    holding = false;
    if (hooks.holdOriginal)
        hooks.holdOriginal (false);
    refreshText();
    repaint (visual);
}

void MainScreen::timerCallback()
{
    ++tick;
    bool moved = false;
    auto level = [this, &moved] (const analysis::DisplayTap& tap, juce::uint32& last, float& db)
    {
        const auto wc = tap.writeCount();
        const bool moving = wc != last;
        moved = moved || moving;
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
    stillTicks = moved ? 0 : juce::jmin (stillTicks + 1, 1000);
    signalOut = envOut[0] > 0.0f || envIn[0] > 0.0f;
    bloom = motion ? bloom + 0.035f * (envOut[0] - bloom) : 0.0f; // slow at 30 Hz; held dark with MOTION off
    if (motion && envOut[0] > 0.0f)
        spin = std::fmod (spin + juce::MathConstants<float>::twoPi / (30.0f * 24.0f), juce::MathConstants<float>::twoPi); // one turn in 24 s
    if (tick % 3 == 0)
    {
        refreshText();
        repaint (modulesArea);
    }
    if (tick % 6 == 0)
    {
        motion = proc.getUiFlag ("uiMotion", true);
        sync();
        repaint (basics);
    }
    repaint (visual);
}

// ---- layout ----
void MainScreen::resized()
{
    const int W = getWidth();
    // top bar: the editor's preset selector, SAVE and A/B sit at x 180..760 and UNDO / MOTION at the right edge
    advancedButton.setBounds (W - 16 - 70 - 4 - 46 - 12 - 172, 12, 172, 28);

    // the journey column, then the vocal (under the bar and the editor's help line, y 54..72)
    journey = { 16, 76, 228, 236 };
    genreBox.setBounds (journey.getX() + 12, journey.getY() + 30, journey.getWidth() - 24, 28);
    buildButton.setBounds (journey.getX() + 12, journey.getY() + 92, journey.getWidth() - 24, 32);
    beatButton.setBounds (journey.getRight() - 12 - 92, journey.getY() + 186, 92, 22);
    visual = { journey.getRight() + 8, 76, W - 16 - journey.getRight() - 8, 236 };
    ribbon = visual.reduced (56, 40);
    mandalaButton.setBounds (visual.getRight() - 56 - 100, visual.getY() + 8, 100, 22); // top right of the vocal (the beat status is in the journey)

    modulesArea = { 24, 320, W - 48, 182 };
    const int mw = modulesArea.getWidth() / numModules;
    for (int m = 0; m < numModules; ++m)
    {
        auto r = modules[(size_t) m] = { modulesArea.getX() + m * mw, modulesArea.getY(), mw, modulesArea.getHeight() };
        switches[(size_t) m].setBounds (r.getRight() - 16 - 58, r.getY() + 10, 58, 24);
        dials[(size_t) m].slider.setBounds (r.getX() + 16, r.getY() + 50, 82, 82);
    }
    const auto at = modules[autotune];
    keyBox.setBounds (at.getX() + 112, at.getY() + 50, 56, 24);
    scaleBox.setBounds (at.getX() + 172, at.getY() + 50, at.getWidth() - 172 - 16, 24);
    useKeyButton.setBounds (at.getRight() - 16 - 52, at.getBottom() - 34, 52, 22);

    basics = { 24, 512, W - 48, 160 };
    const int x0 = basics.getX(), y0 = basics.getY() + 40;
    for (int i = 0; i < 4; ++i) // SHAPE: TONE, PUNCH, SPACE, PRESENCE
        dials[(size_t) (tone + i)].slider.setBounds (x0 + 40 + i * 146, y0, 76, 76);
    for (int i = 0; i < 2; ++i) // LEVEL: MIX, OUTPUT
        dials[(size_t) (mix + i)].slider.setBounds (x0 + 650 + i * 124, y0, 76, 76);
    compare = { basics.getRight() - 176, y0 + 4, 176, 66 }; // the editor's ORIGINAL / PROCESSED
}

// ---- painting ----
void MainScreen::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();
    const bool dry = on (GlitchKidProcessor::bypassId);

    // the top bar: the name, a hairline under it
    // the GLITCHKID logo: emblem, wordmark and the tagline (vector marks from Assets/Brand)
    brand::drawEmblem (g, juce::Rectangle<float> (16.0f, 9.0f, 38.0f, 32.0f), juce::Justification::centredLeft);
    brand::drawWordmark (g, juce::Rectangle<float> (58.0f, 11.0f, 140.0f, 18.0f));
    brand::drawTagline (g, juce::Rectangle<float> (59.0f, 33.0f, 138.0f, 11.0f), 7.5f);
    g.setColour (hairline);
    g.fillRect (0, 52, getWidth(), 1);

    // a faint ultraviolet bloom behind the vocal: the screen's one light source
    const auto rc = ribbon.toFloat().getCentre();
    {
        juce::Graphics::ScopedSaveState glowState (g);
        g.reduceClipRegion (visual);
        g.addTransform (juce::AffineTransform::scale (1.0f, 0.36f, rc.x, rc.y));
        const float R = (float) ribbon.getWidth() * 0.6f;
        g.setGradientFill (juce::ColourGradient ((dry ? dim : uv).withAlpha (0.14f), rc.x, rc.y, uv.withAlpha (0.0f), rc.x + R, rc.y, true));
        g.fillEllipse (rc.x - R, rc.y - R, 2.0f * R, 2.0f * R);
        for (int i = 1; i <= 24; ++i) // static ripples, fading outward
        {
            const float rr = 38.0f * (float) i;
            g.setColour ((i % 2 ? uv : pinkNeon).withAlpha ((dry ? 0.04f : 0.09f) * (1.0f - (float) i / 26.0f)));
            g.drawEllipse (rc.x - rr, rc.y - rr, 2.0f * rr, 2.0f * rr, 1.4f);
        }
    }
    if (bloom > 0.002f && ! dry) // the level-following glow
    {
        juce::Graphics::ScopedSaveState glowState (g);
        g.reduceClipRegion (visual);
        const float R = (float) ribbon.getWidth() * 0.5f;
        g.addTransform (juce::AffineTransform::scale (1.0f, ((float) ribbon.getHeight() * 0.5f + 24.0f) / R, rc.x, rc.y));
        g.setGradientFill (juce::ColourGradient (uv.interpolatedWith (pinkNeon, 0.45f).withAlpha (0.30f * bloom), rc.x, rc.y, uv.withAlpha (0.0f), rc.x + R, rc.y, true));
        g.fillEllipse (rc.x - R, rc.y - R, 2.0f * R, 2.0f * R);
    }
    paintRibbon (g);
    if (! mandala)
        paintCore (g);
    paintOverlays (g);
    paintJourney (g);

    // the four modules: quiet cards; a working module gets the profile's accent corner and a gradient edge
    for (int m = 0; m < numModules; ++m)
    {
        const auto r = modules[(size_t) m];
        const auto card = r.toFloat().reduced (5.0f, 2.0f);
        g.setGradientFill (juce::ColourGradient (surface, card.getX(), card.getY(), bgTop, card.getX(), card.getBottom(), false));
        g.fillRoundedRectangle (card, 12.0f);
        if (active[(size_t) m])
        {
            g.setGradientFill (theme::primaryGradient (card.getTopLeft(), card.getBottomRight(), 0.65f));
            g.drawRoundedRectangle (card.reduced (0.5f), 12.0f, 1.2f);
            juce::Path corner; // the accent corner: a small wedge of the profile gradient inside the card's top-left curve
            corner.startNewSubPath (card.getX() + 0.5f, card.getY() + 20.0f);
            corner.lineTo (card.getX() + 0.5f, card.getY() + 12.0f);
            corner.quadraticTo (card.getX() + 0.5f, card.getY() + 0.5f, card.getX() + 12.0f, card.getY() + 0.5f);
            corner.lineTo (card.getX() + 20.0f, card.getY() + 0.5f);
            corner.closeSubPath();
            g.setGradientFill (theme::primaryGradient (card.getTopLeft(), card.getTopLeft().translated (20.0f, 20.0f)));
            g.fillPath (corner);
        }
        else
        {
            g.setColour (hairline.brighter (0.15f));
            g.drawRoundedRectangle (card.reduced (0.5f), 12.0f, 1.0f);
        }
        g.setFont (fontOf (14.0f, true));
        g.setColour (active[(size_t) m] ? white : dim);
        g.drawText (moduleName[m], r.getX() + 24, r.getY() + 10, 150, 24, juce::Justification::centredLeft, false);
        const auto& d = dials[(size_t) m];
        const auto b = d.slider.getBounds();
        g.setFont (fontOf (11.5f, true));
        g.setColour (dim);
        g.drawText (d.name, b.getX() - 20, b.getBottom() + 2, b.getWidth() + 40, 14, juce::Justification::centred, false);
        g.setFont (fontOf (13.0f));
        g.setColour (active[(size_t) m] ? uvSoft : faint);
        g.drawText (d.text ((float) d.slider.getValue()), b.getX() - 20, b.getBottom() + 16, b.getWidth() + 40, 16, juce::Justification::centred, false);

        auto text = juce::Rectangle<int> (r.getX() + 112, r.getY() + (m == autotune ? 80 : 52), r.getWidth() - 112 - 16, 64);
        g.setFont (fontOf (12.5f));
        g.setColour (active[(size_t) m] ? textBright.interpolatedWith (textDim, 0.4f) : faint);
        g.drawFittedText (status[(size_t) m], text, juce::Justification::topLeft, 3, 0.9f);
        if (m == limiter && active[(size_t) m]) // the limiter's real gain reduction, 0..12 dB
        {
            const auto bar = juce::Rectangle<float> ((float) text.getX(), (float) r.getBottom() - 28.0f, (float) text.getWidth(), 4.0f);
            g.setColour (surfaceHi);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (pinkNeon);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, limGrDb / 12.0f)), 2.0f);
        }
        if (m == autotune)
        {
            g.setFont (fontOf (11.5f));
            g.setColour (suggestedKey >= 0 ? uvSoft : faint);
            g.drawFittedText (keyText, r.getX() + 112, r.getBottom() - 34, r.getWidth() - 112 - 16 - (useKeyButton.isVisible() ? 58 : 0), 22, juce::Justification::centredLeft, 2, 0.85f);
        }
    }

    // the basics: SHAPE, LEVEL, COMPARE
    g.setColour (hairline);
    g.fillRect (basics.getX(), basics.getY() - 4, basics.getWidth(), 1);
    g.setFont (fontOf (11.5f, true));
    g.setColour (dim);
    g.drawText ("SHAPE YOUR VOCAL", basics.getX() + 16, basics.getY() + 6, 200, 14, juce::Justification::centredLeft, false);
    g.drawText ("LEVEL", basics.getX() + 626, basics.getY() + 6, 120, 14, juce::Justification::centredLeft, false);
    g.drawText ("COMPARE", compare.getX(), basics.getY() + 6, 120, 14, juce::Justification::centredLeft, false);
    for (int i = tone; i < numDials; ++i)
    {
        const auto& d = dials[(size_t) i];
        const auto b = d.slider.getBounds();
        g.setFont (fontOf (12.5f, true));
        g.setColour (white);
        g.drawText (d.name, b.getX() - 30, b.getY() - 18, b.getWidth() + 60, 14, juce::Justification::centred, false);
        g.setFont (fontOf (13.0f));
        g.setColour (uvSoft);
        g.drawText (d.text ((float) d.slider.getValue()), b.getX() - 30, b.getBottom() + 2, b.getWidth() + 60, 16, juce::Justification::centred, false);
    }
}

// The vocal core: the emblem in a ring at the centre of the live view. The ring's light follows the real output level (the same slow average as the
// bloom, dark with no audio or with MOTION off); with ORIGINAL selected it is grey. Nothing here moves on its own.
void MainScreen::paintCore (juce::Graphics& g)
{
    const bool dry = on (GlitchKidProcessor::bypassId);
    const auto c = ribbon.toFloat().getCentre();
    const float R = (float) ribbon.getHeight() * 0.44f;
    const auto ring = juce::Rectangle<float> (2.0f * R, 2.0f * R).withCentre (c);
    g.setColour (bgBottom.withAlpha (0.86f)); // the core: the waveform passes behind it
    g.fillEllipse (ring);
    const float lit = dry ? 0.0f : juce::jlimit (0.0f, 1.0f, bloom * 1.6f);
    if (lit > 0.01f)
        for (auto [w, a] : { std::pair { 14.0f, 0.10f }, std::pair { 7.0f, 0.22f } }) // the glow, as strong as the real level
        {
            g.setGradientFill (theme::primaryGradient (ring.getTopRight(), ring.getBottomLeft(), a * lit));
            g.drawEllipse (ring, w);
        }
    if (dry)
        g.setColour (dim.withAlpha (0.6f));
    else
        g.setGradientFill (theme::primaryGradient (ring.getTopRight(), ring.getBottomLeft(), 0.55f + 0.45f * lit));
    g.drawEllipse (ring, 2.0f);
    g.setColour (hairline.brighter (0.3f));
    g.drawEllipse (ring.reduced (7.0f), 1.0f);
    for (int k = 0; k < 4; ++k) // four ticks: the core's compass marks (static)
    {
        const float a = (float) k * juce::MathConstants<float>::halfPi;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.drawLine ({ c + d * (R + 4.0f), c + d * (R + 10.0f) }, 1.0f);
    }
    brand::drawEmblem (g, ring.reduced (R * 0.42f), juce::Justification::centred, dry ? 0.6f : 1.0f);
}

// The journey: three numbered steps, the one to do next lit; every line is the editor's real state (build) or the real input (beat).
void MainScreen::paintJourney (juce::Graphics& g)
{
    const auto j = journey.toFloat();
    g.setColour (surface.withAlpha (0.7f));
    g.fillRoundedRectangle (j, 10.0f);
    g.setColour (hairline);
    g.drawRoundedRectangle (j.reduced (0.5f), 10.0f, 1.0f);
    const int next = nextStep();
    auto step = [&] (int n, const char* name, int y, bool lit)
    {
        const auto badge = juce::Rectangle<float> ((float) journey.getX() + 12.0f, (float) y, 18.0f, 18.0f);
        g.setColour (lit ? uv : surfaceHi);
        g.fillEllipse (badge);
        g.setColour (lit ? white : dim);
        g.setFont (fontOf (11.0f, true));
        g.drawText (juce::String (n), badge, juce::Justification::centred, false);
        g.setFont (fontOf (12.0f, true));
        g.setColour (lit ? white : dim);
        g.drawText (name, journey.getX() + 38, y, 160, 18, juce::Justification::centredLeft, false);
    };
    const int x = journey.getX() + 12, w = journey.getWidth() - 24;
    step (1, "SOUND", journey.getY() + 8, next == 1);
    step (2, "BUILD MY MIX", journey.getY() + 68, next == 2);
    g.setFont (fontOf (11.5f));
    g.setColour (buildListening ? white : dim);
    g.drawFittedText (buildStatus, x, journey.getY() + 128, w, 46, juce::Justification::topLeft, 3, 0.9f);
    step (3, "BEAT", journey.getY() + 164, next == 4);
    g.setFont (fontOf (11.5f));
    g.setColour (beatText.startsWith ("Beat playing") ? good : dim);
    g.drawFittedText (beatText, x, journey.getY() + 186, w - 98, 22, juce::Justification::centredLeft, 2, 0.85f);
    // NEXT: one line
    g.setColour (hairline);
    g.fillRect (x, journey.getBottom() - 30, w, 1);
    g.setFont (fontOf (11.5f, true));
    g.setColour (uvSoft);
    g.drawFittedText ("NEXT:  " + nextText, x, journey.getBottom() - 26, w, 20, juce::Justification::centredLeft, 1, 0.75f);
}

// The overlays on the vocal: what reaches GlitchKid, the beat input, the input and output meters, the pitch, the limiter ceiling. All measured or set.
void MainScreen::paintOverlays (juce::Graphics& g)
{
    const bool dry = on (GlitchKidProcessor::bypassId);
    g.setFont (fontOf (13.0f, true));
    g.setColour (dry ? warn : signalOut ? white : dim);
    g.drawFittedText (headline, visual.getX() + 56, visual.getY() + 8, visual.getWidth() - 56 - 56 - 112, 22, juce::Justification::centredLeft, 1, 0.8f);

    // the meters: input (cyan, as everywhere) at the left, output at the right; -60..0 dBFS
    auto meter = [&g, this] (int x, float db, juce::Colour c, const char* label)
    {
        const auto r = juce::Rectangle<float> ((float) x, (float) ribbon.getY(), 6.0f, (float) ribbon.getHeight());
        g.setColour (surfaceHi);
        g.fillRoundedRectangle (r, 3.0f);
        const float f = db > -60.0f ? juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) : 0.0f;
        g.setColour (c);
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * f), 3.0f);
        g.setFont (fontOf (11.0f, true));
        g.setColour (dim);
        g.drawText (label, (int) r.getCentreX() - 20, ribbon.getBottom() + 4, 40, 14, juce::Justification::centred, false);
        g.setFont (fontOf (11.0f));
        g.drawText (db > -99.0f ? juce::String (db, 1) : juce::String ("-"), (int) r.getCentreX() - 24, ribbon.getBottom() + 17, 48, 14, juce::Justification::centred, false);
    };
    meter (visual.getX() + 22, inDb, cyan.withAlpha (0.8f), "IN");
    meter (visual.getRight() - 28, outDb, dry ? dim : pinkNeon, "OUT");

    if (pitchText.isNotEmpty())
    {
        g.setFont (fontOf (20.0f, true));
        g.setColour (white.withAlpha (dry ? 0.4f : 0.9f));
        g.drawText (pitchText, visual.getCentreX() - 200, visual.getBottom() - 32, 400, 26, juce::Justification::centred, false);
    }
    else if (! signalOut || stillTicks > 30)
    {
        g.setFont (fontOf (14.0f));
        g.setColour (faint);
        g.drawText (stillTicks > 30 ? "Waiting for audio" : "Play your track to see your vocal", visual.withTop (visual.getBottom() - 26), juce::Justification::centred, false); // under the core
    }
    if (signalOut && ! holding && ! dry)
    {
        g.setFont (fontOf (11.5f));
        g.setColour (faint);
        g.drawText ("Press and hold to hear the original", visual.getX() + 56, visual.getBottom() - 22, 240, 16, juce::Justification::centredLeft, false);
    }
}

void MainScreen::paintRibbon (juce::Graphics& g)
{
    using P = GlitchKidProcessor;
    const bool dry = on (P::bypassId);
    const auto A = dry ? dim : uv, B = dry ? textDim : pinkNeon; // ORIGINAL: the same real levels, without GlitchKid's colour
    const auto r = ribbon.toFloat();
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.46f;
    const int n = (int) envOut.size();
    auto xAt = [&] (int i) { return r.getX() + (float) (n - 1 - i) / (float) (n - 1) * r.getWidth(); }; // newest on the right
    auto smooth = [] (const std::array<float, 240>& e, int i) { const int a = juce::jmax (0, i - 1), b = juce::jmin ((int) e.size() - 1, i + 1); return (e[(size_t) a] + 2.0f * e[(size_t) i] + e[(size_t) b]) * 0.25f; };

    if (on (P::limOnId)) // the limiter's ceiling on the same 60 dB scale; lit while it is catching peaks
    {
        const float f = juce::jlimit (0.0f, 1.0f, (value (P::limCeilingId) + 60.0f) / 60.0f) * amp;
        g.setColour (! dry && limGrDb > 0.1f ? pinkNeon.withAlpha (0.8f) : faint.withAlpha (0.6f));
        const float dashes[] = { 4.0f, 4.0f };
        for (float y : { mid - f, mid + f })
            g.drawDashedLine ({ r.getX(), y, r.getRight(), y }, dashes, 2, 1.0f);
        g.setFont (fontOf (10.5f));
        g.drawText ("CEILING", (int) r.getRight() - 60, (int) (mid - f) - 13, 60, 12, juce::Justification::centredRight, false);
    }

    if (mandala)
    {
        paintMandala (g);
        return;
    }
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
    const bool live = envOut[0] > 0.0f;
    const float drift = live && motion && ! dry ? 0.5f + 0.5f * std::sin ((float) tick * juce::MathConstants<float>::twoPi / 240.0f) : 0.0f; // 8 s colour drift while audio plays
    const auto midColour = A.interpolatedWith (B, 0.55f * drift);
    juce::ColourGradient fill (A.withAlpha (0.05f), r.getX(), 0.0f, B.withAlpha (0.42f), r.getRight(), 0.0f, false);
    fill.addColour (0.62, midColour.withAlpha (0.36f));
    g.setGradientFill (fill);
    g.fillPath (out);
    juce::Path lower (edge);
    lower.applyTransform (juce::AffineTransform::verticalFlip (2.0f * mid));
    juce::ColourGradient line (A.withAlpha (0.3f), r.getX(), 0.0f, B, r.getRight(), 0.0f, false);
    line.addColour (0.62, midColour);
    if (live && ! dry)
    {
        // echo contours: the real output edge repeated outward, fading, like rings in water
        for (auto [scale, alpha] : { std::pair { 1.22f, 0.30f }, std::pair { 1.46f, 0.18f }, std::pair { 1.72f, 0.09f } })
            for (auto* p : { &edge, &lower })
            {
                g.setGradientFill (line);
                g.setOpacity (alpha);
                g.strokePath (*p, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved), juce::AffineTransform::scale (1.0f, scale, 0.0f, mid));
            }
        g.setOpacity (1.0f);
        if (on (P::airOnId)) // AIR LAB: a bright rim on the output edge, as strong as the air it adds (its AIR and MIX settings)
        {
            const float airAmt = juce::jlimit (0.0f, 1.0f, value (P::airAirId) / 100.0f * value (P::airMixId) / 100.0f);
            g.setColour (white.withAlpha (0.55f * airAmt));
            for (auto* p : { &edge, &lower })
                g.strokePath (*p, juce::PathStrokeType (1.0f), juce::AffineTransform::scale (1.0f, 1.08f, 0.0f, mid));
        }
        for (auto* p : { &edge, &lower }) // a chromatic split on the edge itself
        {
            g.setColour (cyan.withAlpha (0.38f));
            g.strokePath (*p, juce::PathStrokeType (1.4f), juce::AffineTransform::translation (-2.0f, -2.0f));
            g.setColour (pinkNeon.withAlpha (0.45f));
            g.strokePath (*p, juce::PathStrokeType (1.4f), juce::AffineTransform::translation (2.0f, 2.0f));
        }
    }
    for (auto* p : { &edge, &lower })
    {
        g.setGradientFill (line);
        g.setOpacity (0.22f);
        g.strokePath (*p, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setOpacity (1.0f);
        g.strokePath (*p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // what comes in: a faint line, so you see what GlitchKid did to it
    juce::Path in;
    for (int i = n - 1; i >= 0; --i)
    {
        const float y = mid - smooth (envIn, i) * amp;
        if (i == n - 1) in.startNewSubPath (xAt (i), y); else in.lineTo (xAt (i), y);
    }
    g.setColour (cyan.withAlpha (0.42f)); // cyan: the input, and only the input
    g.strokePath (in, juce::PathStrokeType (1.1f));
    if (live)
    {
        const float y = smooth (envOut, 0) * amp;
        g.setColour (B.withAlpha (0.3f));
        g.fillEllipse (r.getRight() - 6.0f, mid - y - 6.0f, 12.0f, 12.0f);
        g.setColour (B);
        g.fillEllipse (r.getRight() - 3.0f, mid - y - 3.0f, 6.0f, 6.0f);
    }
}

// The mandala: the ribbon's own level history (the last 2 s, newest at each petal's axis, oldest where the mirrored wedges meet) bent around a circle and
// mirrored eight ways, so its outline is the shape of the last phrases. Smaller mirrored reflections sit either side. Nothing here is invented: with no
// audio it is a small still seed.
void MainScreen::paintMandala (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (visual.reduced (40, 30));
    const bool dry = on (GlitchKidProcessor::bypassId);
    const auto A = dry ? dim : uv, B = dry ? textDim : pinkNeon;
    const auto r = ribbon.toFloat();
    const auto c = r.getCentre();
    const float R = r.getHeight() * 0.5f + 2.0f, r0 = 9.0f;
    constexpr int folds = 8, perWedge = 20, N = 2 * folds * perWedge, span = 60; // each wedge: the last 2 s (60 frames), newest on the petal axis
    auto shape = [&] (const std::array<float, 240>& e)
    {
        std::array<float, perWedge + 1> lv {};
        for (int k = 0; k <= perWedge; ++k) // a short average around each point, so a phrase reads as a petal, not as fuzz
        {
            const int centre = k * (span - 1) / perWedge;
            float sum = 0.0f;
            int cnt = 0;
            for (int i = juce::jmax (0, centre - 3); i <= juce::jmin ((int) e.size() - 1, centre + 3); ++i) { sum += e[(size_t) i]; ++cnt; }
            const float v = sum / (float) cnt;
            lv[(size_t) k] = v * v; // squared: quiet and loud parts differ more (the same real level, a steeper curve)
        }
        juce::Path p;
        for (int j = 0; j < N; ++j)
        {
            const int w = j / perWedge, k = j % perWedge;
            const int idx = (w % 2 == 0) ? k : perWedge - k; // mirror every other wedge, so the wedges meet seamlessly
            const float a = (float) j * juce::MathConstants<float>::twoPi / (float) N;
            const float rad = r0 + lv[(size_t) idx] * (R - r0);
            const juce::Point<float> pt (std::sin (a) * rad, -std::cos (a) * rad);
            if (j == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    };
    const auto outShape = shape (envOut), inShape = shape (envIn);
    const bool live = envOut[0] > 0.0f && ! dry;

    auto flower = [&] (juce::Point<float> centre, float scale, float alpha, float turn, bool flip)
    {
        const bool main = scale >= 1.0f; // the reflections get only a fill and an outline (cheap)
        auto at = juce::AffineTransform::rotation (turn).scaled (flip ? -scale : scale, scale).translated (centre);
        juce::Path o (outShape);
        o.applyTransform (at);
        g.setGradientFill (juce::ColourGradient (B.withAlpha (0.50f * alpha), centre.x, centre.y, A.withAlpha (0.10f * alpha), centre.x + R * scale, centre.y, true));
        g.fillPath (o);
        if (live && main)
            for (auto [k, a] : { std::pair { 1.16f, 0.30f }, std::pair { 1.34f, 0.16f } }) // echo contours
            {
                g.setColour (A.interpolatedWith (B, 0.4f).withAlpha (a * alpha));
                g.strokePath (o, juce::PathStrokeType (1.0f), juce::AffineTransform::scale (k, k, centre.x, centre.y));
            }
        juce::ColourGradient edge (A.withAlpha (alpha), centre.x - R * scale, centre.y, B.withAlpha (alpha), centre.x + R * scale, centre.y, false);
        g.setGradientFill (edge);
        if (main)
        {
            g.setOpacity (0.25f);
            g.strokePath (o, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setOpacity (1.0f);
        }
        g.strokePath (o, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (live && main)
        {
            g.setColour (cyan.withAlpha (0.35f * alpha));
            g.strokePath (o, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (-2.0f, -2.0f));
            g.setColour (pinkNeon.withAlpha (0.40f * alpha));
            g.strokePath (o, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (2.0f, 2.0f));
        }
        juce::Path in (inShape);
        in.applyTransform (at);
        g.setColour (cyan.withAlpha (0.5f * alpha)); // the input, in cyan as on the ribbon
        g.strokePath (in, juce::PathStrokeType (1.1f));
    };
    // the reflections first (mirrored, turning the other way), then the flower itself
    for (auto [d, scale, alpha] : { std::tuple { 2, 0.42f, 0.22f }, std::tuple { 1, 0.66f, 0.45f } })
        for (int side : { -1, 1 })
            flower ({ c.x + (float) (side * d) * R * 2.15f, c.y }, scale, alpha, -spin, (d % 2) == 1);
    flower (c, 1.0f, 1.0f, spin, false);
}
}
