#include "KidPanel.h"
#include "PlaygroundWidgets.h"
#include "../Analysis/ReferenceAnalyzer.h"
#include "../Macros/MacroEngine.h"

namespace glitchkid::ui
{
namespace
{
// GlitchKid's own palette for the producer screen: near-black and plum, with ultraviolet, magenta and pink accents.
const juce::Colour kidPink (0xffe14cbb), kidPinkSoft (0xffffc6ea), ultra (0xff7d3cff), lilac (0xffb5aeff), amber (0xffffb54c),
                   plum (0xff1d0f22), plumDeep (0xff120a16), ink (0xff0b0910), mint (0xff62e0a6);
const char* const stageName[18] = { "", "Pitch Tune", "Formant", "Harmony", "Mutation", "Doubler", "4-band EQ", "CLEAN corrective", "Compressor", "CLEAN de-esser",
                                    "Saturation", "AIR LAB", "Stutter", "Echo", "Dimension", "Gain", "Mix", "Output Limiter" };
const char* const dimName[kid::numDims][2] = { { "BRIGHTER", "DARKER" }, { "WARMER", "COOLER" }, { "MORE PRESENT", "FURTHER BACK" }, { "HARDER", "SOFTER" },
                                               { "WIDER", "NARROWER" }, { "MORE SPACE", "CLOSER" }, { "CLEANER", "CLEANER" }, { "STRANGER", "STRANGER" } };
const char* const intensityName[3] = { "GENTLE", "BALANCED", "AGGRESSIVE" };
constexpr int advancedStage[6] = { 1, 7, 8, 6, 13, 4 }; // Tune, Clean, Dynamics, Tone, Space, Creative (LAB)
const char* const advancedName[6] = { "TUNE", "CLEAN", "DYNAMICS", "TONE", "SPACE", "CREATIVE" };

// A tactile panel: a plum-to-ink face, a lit top edge and a dark lower lip, so the screen reads as one physical instrument.
void bezel (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour top, juce::Colour bottom, float radius = 14.0f)
{
    const auto f = r.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (f.translated (0.0f, 2.0f), radius);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, f.getY(), bottom, 0.0f, f.getBottom(), false));
    g.fillRoundedRectangle (f, radius);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (f.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (f.getX() + radius, f.getY() + 1.0f, f.getWidth() - 2.0f * radius, 1.0f);
}

void caption (juce::Graphics& g, const juce::String& t, int x, int y, int w, juce::Colour c = textDim)
{
    g.setFont (fontOf (11.5f, true));
    g.setColour (c);
    g.drawText (t, x, y, w, 14, juce::Justification::centredLeft, true);
}
}

// ---- the push-to-talk button ----// ---- the push-to-talk button ----
void KidPanel::MicButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    juce::ColourGradient ring (juce::Colour (0xff7d3cff), r.getX(), r.getBottom(), kidPink, r.getRight(), r.getY(), false);
    ring.addColour (0.7, juce::Colour (0xffc9a6ff));
    if (live)
    {
        g.setColour (kidPink.withAlpha (0.25f));
        g.fillEllipse (r.expanded (1.5f));
    }
    g.setGradientFill (ring);
    g.fillEllipse (r);
    g.setColour (live ? juce::Colour (0xff2a0f24) : juce::Colour (0xff0f0b13));
    g.fillEllipse (r.reduced (3.0f));
    if (! isEnabled())
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (r);
    }
    // the microphone glyph
    const auto c = r.getCentre();
    const float s = r.getWidth() / 64.0f;
    g.setColour (juce::Colours::white.withAlpha (isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle (c.x - 6 * s, c.y - 15 * s, 12 * s, 20 * s, 6 * s, 2.2f * s);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y + 1 * s, 11 * s, 11 * s, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.5f, true);
    g.strokePath (arc, juce::PathStrokeType (2.2f * s));
    g.drawLine (c.x, c.y + 12 * s, c.x, c.y + 17 * s, 2.2f * s);
    g.drawLine (c.x - 5 * s, c.y + 17 * s, c.x + 5 * s, c.y + 17 * s, 2.2f * s);
}

void KidPanel::MicButton::mouseDown (const juce::MouseEvent&)
{
    if (! isEnabled())
        return;
    if (live && tapMode) // second tap: stop
    {
        tapMode = false;
        downAt = 0;
        if (onRelease) onRelease();
        return;
    }
    downAt = juce::Time::getMillisecondCounter();
    if (onPress) onPress();
}

void KidPanel::MicButton::mouseUp (const juce::MouseEvent&)
{
    if (downAt == 0 || ! live)
        return;
    if (juce::Time::getMillisecondCounter() - downAt < 250) // a short press: tap mode, keep listening until the next press
        tapMode = true;
    else if (onRelease)
        onRelease();
    downAt = 0;
}

bool KidPanel::MicButton::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey)
    {
        if (live) { tapMode = false; if (onRelease) onRelease(); }
        else if (isEnabled() && onPress) { tapMode = true; onPress(); }
        return true;
    }
    return false;
}

// ---- the panel ----
KidPanel::KidPanel (GlitchKidProcessor& p, Hooks h) : proc (p), hooks (std::move (h)), director (p.makeRangeLookup())
{
    setWantsKeyboardFocus (true);
    lastSeen = proc.captureSoundState();
    director.begin (lastSeen);
    tapScratch.resize (2048);
    say ("I'm KID, your vocal producer. Load your vocal or play your track, then tell me the sound in your head: \"warmer\", \"wide melodic rap but keep me upfront\", "
         "a profile below, or just \"I don't know, improve it\". I show you every change before anything happens.");

    mic.setTitle ("Talk to KID");
    mic.setDescription ("Push to talk: hold to speak, or tap once to start and again to stop.");
    mic.setWantsKeyboardFocus (true);
    mic.onPress = [this] { startRecording(); };
    mic.onRelease = [this] { stopRecording (false); };
    addAndMakeVisible (mic);

    input.setFont (fontOf (14.0f));
    input.setTextToShowWhenEmpty ("...or type to KID: \"make it more emotional and less robotic\"", textDim);
    input.setColour (juce::TextEditor::backgroundColourId, ink);
    input.setColour (juce::TextEditor::outlineColourId, border);
    input.setColour (juce::TextEditor::textColourId, textBright);
    input.setTitle ("Command for KID");
    input.getProperties().set ("rawText", true);
    input.onReturnKey = [this] { send (input.getText(), false); input.clear(); };
    addAndMakeVisible (input);
    sendButton.onClick = [this] { send (input.getText(), false); input.clear(); };
    setupButton.setTooltip ("Downloads KID's speech model once (about 142 MB, from the whisper.cpp project), checks it, and keeps it on this computer. Speech then works offline.");
    setupButton.onClick = [this]
    {
        phase = Phase::downloading;
        say ("Setting up speech: downloading the model (about 142 MB) and checking it. Typing works meanwhile.");
        downloader.start ([safe = juce::Component::SafePointer<KidPanel> (this)] (bool ok, juce::String msg)
        {
            if (safe == nullptr) return;
            safe->phase = safe->director.proposal().empty() ? Phase::ready : Phase::review;
            if (ok) safe->say ("Speech is ready. Hold the mic and talk to me.");
            else safe->fail (msg);
            safe->refreshButtons();
            safe->repaint();
        });
        refreshButtons();
        repaint();
    };
    applyButton.onClick = [this] { apply(); };
    cancelButton.onClick = [this] { cancelTake(); };
    analyseButton.setTooltip ("Listen to the vocal on this track while your project plays (2-8 s of singing or rapping). GlitchKid only hears this track, not your whole project.");
    analyseButton.onClick = [this] { pendingText = {}; analysisStartCount = proc.getAnalyzer().completedCount(); proc.startSourceAnalysis(); analysisStartedMs = juce::Time::getMillisecondCounter(); phase = Phase::analysing; refreshButtons(); repaint(); };
    loadButton.setTooltip ("Analyse a vocal recording (" + analysis::referenceFormatNames() + "). It is read, not played and not copied: you still hear GlitchKid by playing your track in the DAW.");
    loadButton.onClick = [this] { chooseFile(); };
    const char* tabNames[3] = { "TAKE", "VOCAL READ", "LIMITS" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText (tabNames[i]);
        tabButtons[i].onClick = [this, i] { tab = (Tab) i; takeScroll = 0.0f; refreshButtons(); repaint(); };
        addAndMakeVisible (tabButtons[i]);
    }
    for (int i = 0; i < kid::numProfiles; ++i)
    {
        const auto& prof = kid::producerProfile (i + 1);
        profileButtons[i].setButtonText (juce::String (prof.name).toUpperCase());
        profileButtons[i].setTooltip (juce::String (prof.tagline) + " An original GlitchKid direction, not anyone's vocal chain.");
        profileButtons[i].onClick = [this, i] { chosenProfile = i + 1; send ("Give me the " + juce::String (kid::producerProfile (i + 1).name) + " sound", false); };
        addAndMakeVisible (profileButtons[i]);
    }
    buildButton.setTooltip ("Analyses your vocal (the loaded file, or the live input), builds a whole mix from the chosen profile with GlitchKid's own effects, and shows every change before Apply. UNDO goes back.");
    buildButton.onClick = [this] { send (chosenProfile > 0 ? "Build my mix: " + juce::String (kid::producerProfile (chosenProfile).name) : juce::String ("Build my mix"), false); };
    cleanButton.setTooltip ("CLEAN LAB only: rumble, low-mid build-up, resonances and harsh S's. It is not noise reduction and does not remove reverb or clipping.");
    cleanButton.onClick = [this] { send (cleanIntensity == 0 ? "Clean my vocal gently" : cleanIntensity == 2 ? "Clean my vocal aggressively" : "Clean my vocal", false); };
    for (int i = 0; i < 3; ++i)
    {
        intensityButtons[i].setButtonText (intensityName[i]);
        intensityButtons[i].setTooltip (i == 0 ? "Gentle: CLEAN at 35 %, each section held back." : i == 1 ? "Balanced: CLEAN at 60 %." : "Aggressive: CLEAN at 90 %. Can thin your tone: compare with ORIGINAL.");
        intensityButtons[i].onClick = [this, i] { cleanIntensity = i; refreshButtons(); };
        addAndMakeVisible (intensityButtons[i]);
    }
    originalButton.setTooltip ("Hear your untouched vocal (the real bypass, delay-matched). With LEVEL MATCH on it plays at the processed loudness, so louder never wins.");
    processedButton.setTooltip ("Hear GlitchKid.");
    originalButton.onClick = [this] { setCompare (true); };
    processedButton.onClick = [this] { setCompare (false); };
    originalButton.setColour (juce::TextButton::buttonOnColourId, kidPink.darker (0.45f));
    processedButton.setColour (juce::TextButton::buttonOnColourId, ultra);
    matchButton.setTooltip ("Level match: ORIGINAL is played at the long-term loudness GlitchKid measured for PROCESSED (up to 24 dB). Off: ORIGINAL is the exact input.");
    matchButton.onClick = [this] { levelMatch = ! levelMatch; proc.setLevelMatchedCompare (levelMatch && isVisible() && getParentComponent() != nullptr); refreshButtons(); repaint (compareBox); };
    for (int i = 0; i < 6; ++i)
    {
        advancedButtons[i].setButtonText (advancedName[i]);
        advancedButtons[i].setTooltip ("Open the detailed " + juce::String (advancedName[i]).toLowerCase() + " controls. KID is the way back.");
        advancedButtons[i].onClick = [this, i] { if (hooks.openStage) hooks.openStage (advancedStage[i]); };
        addAndMakeVisible (advancedButtons[i]);
    }
    for (auto* b : { &sendButton, &setupButton, &applyButton, &cancelButton, &analyseButton, &loadButton, &buildButton, &cleanButton, &originalButton, &processedButton, &matchButton })
        addAndMakeVisible (b);
    sendButton.setTooltip ("Send what you typed to KID.");
    applyButton.setTooltip ("Apply the ticked changes. UNDO (top right) brings your previous sound back.");
    cancelButton.setTooltip ("Drop KID's take. Your sound has not changed.");
    setupDials();
    refreshChips();
    refreshButtons();
}

KidPanel::~KidPanel()
{
    stopTimer();
    commandMic.stop();
    fileJob.stopJob();
    proc.setLevelMatchedCompare (false);
}

void KidPanel::visibilityChanged()
{
    proc.setLevelMatchedCompare (levelMatch && isVisible() && getParentComponent() != nullptr); // matching belongs to this screen's compare switch; everywhere else bypass is exact
    if (isShowing()) startTimerHz (30);
    else
    {
        stopTimer();
        if (phase == Phase::recording)
            stopRecording (true);
    }
}

float KidPanel::value (const char* id) const
{
    if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
        return r->convertFrom0to1 (r->getValue());
    return 0.0f;
}

void KidPanel::setParam (const char* id, float plain)
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

void KidPanel::setupDials()
{
    auto pct = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
    auto macroDial = [this] (macros::Macro m)
    {
        return std::pair<std::function<float()>, std::function<void (float)>> (
            [this, m] { return 100.0f * macros::read (m, proc.captureSoundState()); },
            [this, m] (float v) { auto s = proc.captureSoundState(); macros::apply (m, v / 100.0f, s); proc.applyParameterChanges (s); });
    };
    auto& clean = dials[0];
    clean.name = "CLEAN";
    clean.what = "CLEAN LAB amount: rumble, low-mid build-up, resonances and harsh S's. 0 switches CLEAN off. It is not noise reduction.";
    clean.accent = mint;
    clean.stage = 7;
    clean.get = [this] { return value ("clean_on") >= 0.5f ? value ("clean_amount") : 0.0f; };
    clean.set = [this] (float v) { if (v >= 0.5f) { setParam ("clean_amount", v); setParam ("clean_on", 1.0f); } else setParam ("clean_on", 0.0f); };
    clean.text = [pct] (float v) { return v < 0.5f ? juce::String ("Off") : pct (v); };

    auto& tune = dials[1];
    tune.name = "TUNE";
    tune.what = "Pitch Tune strength toward the key and scale set on TUNE. 0 switches it off. Pitch Tune adds about 31 ms of latency while on.";
    tune.accent = lilac;
    tune.stage = 1;
    tune.get = [this] { return value ("tune_on") >= 0.5f ? value ("tune_strength") : 0.0f; };
    tune.set = [this] (float v) { if (v >= 0.5f) { setParam ("tune_strength", v); setParam ("tune_on", 1.0f); } else setParam ("tune_on", 0.0f); };
    tune.text = [this, pct] (float v)
    {
        if (v < 0.5f) return juce::String ("Off");
        static const char* style[] = { "Natural", "Light", "Hard" };
        const int s = macros::readTuneStyle (proc.captureSoundState());
        return pct (v) + (s >= 0 ? juce::String (" ") + style[s] : juce::String());
    };

    auto& punch = dials[2];
    punch.name = "STYLE";
    punch.what = "Punch: how firmly the compressor holds your voice (the PUNCH macro: ratio, threshold, attack, release, makeup).";
    punch.accent = kidPink;
    punch.stage = 8;
    std::tie (punch.get, punch.set) = macroDial (macros::Macro::punch);
    punch.text = [pct] (float v) { return pct (v) + " punch"; };

    auto& space = dials[3];
    space.name = "SPACE";
    space.what = "Echo, Dimension and doubles together (the SPACE macro). GlitchKid has no reverb: space is delay and width.";
    space.accent = ultra.brighter (0.3f);
    space.stage = 13;
    std::tie (space.get, space.set) = macroDial (macros::Macro::space);
    space.text = pct;

    auto& presence = dials[4];
    presence.name = "PRESENCE";
    presence.what = "The EQ presence band (eq_b2_gain): where words cut through.";
    presence.accent = kidPinkSoft;
    presence.stage = 6;
    presence.get = [this] { return value ("eq_b2_gain"); };
    presence.set = [this] (float v) { setParam ("eq_b2_gain", v); };
    presence.text = [] (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; };

    auto& mixDial = dials[5];
    mixDial.name = "MIX";
    mixDial.what = "Processed vs. untouched voice (the Mix parameter). KID never moves this: it is yours.";
    mixDial.accent = juce::Colour (0xffcfc8da);
    mixDial.stage = 16;
    mixDial.get = [this] { return value ("mix"); };
    mixDial.set = [this] (float v) { setParam ("mix", v); };
    mixDial.text = pct;

    for (size_t i = 0; i < dials.size(); ++i)
    {
        auto& d = dials[i];
        juce::NormalisableRange<double> range (0.0, 100.0, 0.1);
        if (i == 4)
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("eq_b2_gain")))
                range = { (double) r->getNormalisableRange().start, (double) r->getNormalisableRange().end, 0.1 };
        d.slider.setNormalisableRange (range);
        d.slider.setTitle (d.name);
        d.slider.setTooltip (d.what);
        d.slider.setColour (juce::Slider::rotarySliderFillColourId, d.accent);
        d.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        d.slider.setValue (d.get(), juce::dontSendNotification);
        d.slider.onValueChange = [this, i] { auto& x = dials[i]; x.set ((float) x.slider.getValue()); repaint (dialBox); };
        d.slider.onDragStart = [this, i] { dials[i].dragging = true; };
        d.slider.onDragEnd = [this, i] { dials[i].dragging = false; };
        d.slider.textFromValueFunction = [this, i] (double v) { return dials[i].text ((float) v); }; // what a screen reader announces
        addAndMakeVisible (d.slider);
    }
}

void KidPanel::syncDials()
{
    for (auto& d : dials)
        if (! d.dragging && ! d.slider.isMouseButtonDown())
        {
            const double v = d.get();
            if (std::abs (v - d.slider.getValue()) > 0.05)
                d.slider.setValue (v, juce::dontSendNotification);
        }
}

juce::String KidPanel::lastKidLineForTest() const
{
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (it->kid) return it->text;
    return {};
}

void KidPanel::pressForTest (const juce::String& text)
{
    for (auto* c : getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text && b->isEnabled() && b->onClick)
        {
            b->onClick();
            return;
        }
}

juce::String KidPanel::phaseLabel() const
{
    switch (phase)
    {
        case Phase::recording: return "LISTENING TO YOU";
        case Phase::transcribing: return "UNDERSTANDING";
        case Phase::analysing: return "ANALYSING YOUR VOCAL";
        case Phase::review: return "READY TO APPLY";
        case Phase::downloading: return "SETTING UP SPEECH";
        case Phase::ready: break;
    }
    if (fileJob.isRunning() || fileWaiting) return "READING YOUR FILE";
    if (! kid::Recogniser::available()) return "READY (TYPE TO KID)";
    return "READY";
}

juce::String KidPanel::phaseHint() const
{
    switch (phase)
    {
        case Phase::recording: return "Release to send. Esc cancels. Mic: " + commandMic.deviceName();
        case Phase::transcribing: return "Turning your words into text, on this computer...";
        case Phase::analysing: return "Press play in your project: KID listens to 2-8 s of your vocal (" + juce::String (proc.getAnalyzer().liveActiveSeconds(), 1) + " s so far).";
        case Phase::review: return "Apply to hear it (UNDO goes back), untick anything you don't want, or keep talking.";
        case Phase::downloading: return "Downloading the speech model: " + juce::String (juce::roundToInt (downloader.progress() * 100.0f)) + " %";
        case Phase::ready: break;
    }
    if (! kid::Recogniser::available()) return "This build has no speech. Type, or tap a suggestion.";
    if (! kid::SpeechModel::isInstalled()) return "Speech needs a one-time download (SET UP SPEECH). Typing works now.";
    return "Hold to talk, or tap. Your command mic is separate from the track.";
}

KidPanel::Mood KidPanel::mood() const
{
    const auto now = juce::Time::getMillisecondCounter();
    if (errorAtMs != 0 && now - errorAtMs < 3000) return Mood::error;
    if (phase == Phase::recording) return Mood::listening;
    if (phase == Phase::analysing || fileJob.isRunning() || fileWaiting) return Mood::analysing;
    if (phase == Phase::transcribing || phase == Phase::downloading) return Mood::building;
    if (appliedAtMs != 0 && now - appliedAtMs < 2500 && appliedAtMs >= builtAtMs) return Mood::complete;
    if (builtAtMs != 0 && now - builtAtMs < 900) return Mood::building;
    if (phase == Phase::review) return Mood::previewing;
    return Mood::idle;
}

juce::StringArray KidPanel::suggestions() const
{
    const bool spoke = std::any_of (lines.begin(), lines.end(), [] (const Line& l) { return ! l.kid; });
    const auto& rows = director.proposal();
    const auto& d = director.direction();
    if (! spoke)
        return { "Make this vocal sound like The Kid LAROI", "Make it more emotional and less robotic", "Give me a wide melodic rap vocal",
                 "Clean the harsh frequencies", "Make the tuning harder", "Keep it natural but make it sound expensive" };
    if (! rows.empty() && d[kid::width] > 0 && director.kept().count ("front") == 0) return { "Too much. Make it subtle", "Keep the vocal upfront", "More" };
    if (! rows.empty() && d[kid::bright] < 0 && director.kept().count ("presence") == 0) return { "A little less", "Keep the presence", "Make it warmer" };
    if (! rows.empty()) return { "A little less", "More", "Keep the vocal upfront", "Start over" };
    return { "Make it warmer", "Make the reverb more atmospheric", "Make the vocal sit better with the beat", "I don't know what it needs. Improve it" };
}

void KidPanel::refreshChips()
{
    const auto s = suggestions();
    chips.clear();
    for (auto& t : s)
    {
        auto* b = chips.add (new juce::TextButton (t));
        b->getProperties().set ("rawText", true);
        b->setTooltip ("Say this to KID.");
        b->onClick = [this, t] { send (t, false); };
        addAndMakeVisible (b);
    }
    resized();
}

void KidPanel::refreshButtons()
{
    const bool busy = phase == Phase::analysing || phase == Phase::transcribing || phase == Phase::recording;
    const bool speechOk = kid::Recogniser::available() && kid::SpeechModel::isInstalled() && phase != Phase::downloading;
    mic.setEnabled (speechOk && ! busy) ;
    mic.live = phase == Phase::recording;
    if (phase == Phase::recording) mic.setEnabled (true);
    setupButton.setVisible (kid::Recogniser::available() && ! kid::SpeechModel::isInstalled() && phase != Phase::downloading);
    input.setEnabled (! busy);
    sendButton.setEnabled (! busy);
    for (auto* c : chips) c->setEnabled (! busy);
    for (auto& b : profileButtons) b.setEnabled (! busy);
    for (int i = 0; i < kid::numProfiles; ++i) profileButtons[i].setToggleState (chosenProfile == i + 1, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) intensityButtons[i].setToggleState (cleanIntensity == i, juce::dontSendNotification);
    buildButton.setEnabled (! busy);
    cleanButton.setEnabled (! busy);
    loadButton.setEnabled (! fileJob.isRunning() && ! fileWaiting);
    analyseButton.setEnabled (! busy);
    applyButton.setEnabled (director.selectedCount() > 0 && ! busy);
    applyButton.setButtonText ("APPLY " + juce::String (director.selectedCount()));
    cancelButton.setEnabled (! director.proposal().empty() && ! busy);
    for (int i = 0; i < 3; ++i) tabButtons[i].setToggleState ((int) tab == i, juce::dontSendNotification);
    matchButton.setToggleState (levelMatch, juce::dontSendNotification);
    const bool dry = value ("bypass") >= 0.5f;
    originalButton.setToggleState (dry, juce::dontSendNotification);
    processedButton.setToggleState (! dry, juce::dontSendNotification);
    mic.repaint();
}

void KidPanel::setCompare (bool original)
{
    proc.setLevelMatchedCompare (levelMatch && isVisible() && getParentComponent() != nullptr);
    setParam ("bypass", original ? 1.0f : 0.0f);
    refreshButtons();
    repaint (compareBox);
}

void KidPanel::send (const juce::String& raw, bool spoken)
{
    const auto text = raw.trim();
    if (text.isEmpty() || phase == Phase::analysing || phase == Phase::transcribing)
        return;
    lines.push_back ({ false, text, spoken });
    scriptScroll = 1.0e9f;
    pendingText = text;
    pendingSpoken = spoken;
    const auto f = kid::parse (text);
    if (kid::Director::needsAnalysis (f) && ! haveFacts)
    {
        auto& an = proc.getAnalyzer();
        analysisStartCount = an.completedCount();
        if (! an.isListening())
            proc.startSourceAnalysis();
        analysisStartedMs = juce::Time::getMillisecondCounter();
        phase = Phase::analysing;
        refreshButtons();
        repaint();
        return;
    }
    finishRequest();
}

void KidPanel::finishRequest()
{
    if (pendingText.isNotEmpty())
    {
        auto f = kid::parse (pendingText);
        if (f.profile > 0)
            chosenProfile = f.profile;
        if (f.buildMix || f.profile > 0) // the profile's foundation from the existing Build My Mix engine, fitted to the vocal KID has read (the file first)
        {
            const int prof = f.profile > 0 ? f.profile : chosenProfile;
            const auto& lead = fileReport.ok() && fileReport.tone.status == analysis::VocalAnalysis::Status::ready ? fileReport.tone : lastAnalysis;
            director.setFoundation (kid::buildFoundation (prof, proc.captureSoundState(), proc.factoryDefaultState(), lead, proc.makeRangeLookup()));
            if (f.buildMix && f.profile == 0) f.profile = prof;
        }
        say (director.respond (f, haveFacts ? &facts : nullptr));
        if (! director.proposal().empty())
            builtAtMs = juce::Time::getMillisecondCounter();
        pendingText = {};
    }
    phase = director.proposal().empty() ? Phase::ready : Phase::review;
    takeScroll = 0.0f;
    if (! director.proposal().empty()) tab = Tab::take;
    refreshChips();
    refreshButtons();
    repaint();
}

void KidPanel::apply()
{
    const auto changes = director.selectedChanges();
    if (changes.values.empty())
        return;
    const auto before = proc.captureSoundState();
    if (hooks.beforeApply) hooks.beforeApply (before);
    proc.applyParameterChanges (changes); // the plug-in's normal path: host gestures, automation, then the DSP reads the parameters as always
    lastSeen = proc.captureSoundState();
    director.accepted (lastSeen);
    const int n = (int) changes.values.size();
    say ("Applied " + juce::String (n) + (n == 1 ? " change" : " changes") + ". Play it, flip ORIGINAL / PROCESSED to compare at the same loudness, and UNDO (top right) if you want your sound back. What next?");
    if (hooks.status) hooks.status ("KID applied " + juce::String (n) + (n == 1 ? " change." : " changes."));
    appliedAtMs = juce::Time::getMillisecondCounter();
    phase = Phase::ready;
    refreshChips();
    refreshButtons();
    repaint();
}

void KidPanel::cancelTake()
{
    if (director.proposal().empty())
        return;
    director.cancel();
    say ("Dropped it. Your sound hasn't changed.");
    phase = Phase::ready;
    refreshChips();
    refreshButtons();
    repaint();
}

void KidPanel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a vocal recording to analyse", juce::File(), analysis::referenceFileWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<KidPanel> (this)] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult() != juce::File())
            safe->loadFile (fc.getResult());
    });
}

void KidPanel::loadFile (const juce::File& f)
{
    if (! fileJob.start (f, [safe = juce::Component::SafePointer<KidPanel> (this)] (const analysis::VocalFileReport& r) { if (safe != nullptr) safe->fileDone (r); }))
        return;
    fileWaiting = true;
    say ("Reading " + f.getFileName() + ". I analyse it here; to hear GlitchKid on it, play it from your track in the DAW.");
    refreshButtons();
    repaint();
}

void KidPanel::fileDone (const analysis::VocalFileReport& r)
{
    fileWaiting = false;
    if (! r.ok())
    {
        fail (r.message);
        refreshButtons();
        repaint();
        return;
    }
    fileReport = r;
    facts = {};
    const auto& t = r.tone;
    facts.valid = t.status == analysis::VocalAnalysis::Status::ready;
    facts.fromFile = true;
    facts.activeSeconds = t.activeSeconds;
    facts.sibilanceIndex = t.sibilanceIndex;
    facts.dynamicRangeDb = t.dynamicRangeDb;
    facts.clippedSamples = r.clippedSamples;
    for (size_t i = 0; i < facts.bandFraction.size(); ++i)
        facts.bandFraction[i] = t.bandFraction[i];
    facts.keyReliable = r.keyReliable;
    facts.keyTonic = r.keyTonic;
    facts.keyMinor = r.keyMinor;
    facts.noiseKnown = r.noiseFloorKnown;
    facts.noiseFloorDb = r.noiseFloorDb;
    facts.voicedShare = r.voicedShare;
    facts.offNoteCents = r.offNoteCents;
    haveFacts = facts.valid;

    // what KID says about it: plain English from the measurements, each claim only when the number supports it
    juce::StringArray s;
    if (facts.valid)
    {
        if (t.sibilanceIndex > 0.25f) s.add ("The S's are sharp.");
        if (t.bandFraction[1] > 0.35f) s.add ("There's a build-up in the low mids.");
        if (t.bandFraction[3] + t.bandFraction[4] < 0.2f) s.add ("It's on the dull side.");
    }
    if (r.clippedSamples > 0) s.add ("It clips in " + juce::String (r.clippedSamples) + " samples: GlitchKid can't repair that, so re-record lower if you can.");
    if (r.noiseFloorKnown) s.add (r.noiseFloorDb > -55.0f ? "There's audible background between phrases (about " + juce::String (juce::roundToInt (r.noiseFloorDb)) + " dBFS); GlitchKid has no noise reduction."
                                                          : juce::String ("The background between phrases is quiet: clean enough to mix."));
    if (r.voicedShare > 0.5f && r.medianHz > 0.0f)
        s.add (r.offNoteCents < 10.0f ? "The pitch sits close to the notes." : r.offNoteCents < 20.0f ? "The pitch is mostly stable; a few notes could use gentle correction."
                                                                                                  : "Notes often drift off pitch: tuning would help.");
    s.add (r.keyReliable ? "My key estimate: " + analysis::keyName (r.keyTonic, r.keyMinor) + " (check it on TUNE)." : juce::String ("I can't tell the key reliably, so I won't guess one."));
    say ("I read " + r.fileName + " (" + juce::String (r.analysedSeconds, 1) + " s). " + s.joinIntoString (" ") + (facts.valid ? "" : " There wasn't enough voice for a tone read."));
    tab = Tab::read;
    takeScroll = 0.0f;
    refreshButtons();
    repaint();
}

void KidPanel::startRecording()
{
    if (! kid::Recogniser::available() || ! kid::SpeechModel::isInstalled() || recogniser.isBusy())
    {
        say (kid::Recogniser::available() ? "Speech isn't set up yet: press SET UP SPEECH (a one-time download), or type." : "This build has no speech. Type, or tap a suggestion.");
        refreshButtons();
        repaint();
        return;
    }
    juce::String err;
    if (! commandMic.start (err))
    {
        fail (err.isNotEmpty() ? "I can't hear you: " + err + " You can still type." : "I can't hear you. You can still type.");
        refreshButtons();
        repaint();
        return;
    }
    micLevels.fill (0.0f);
    recordStartedMs = juce::Time::getMillisecondCounter();
    phase = Phase::recording;
    refreshButtons();
    repaint();
}

void KidPanel::stopRecording (bool cancel)
{
    if (phase != Phase::recording)
        return;
    commandMic.stop();
    mic.tapMode = false;
    auto samples = commandMic.take16k(); // the command audio lives only until it is transcribed
    if (cancel)
    {
        say ("Cancelled. Nothing was sent.");
        phase = director.proposal().empty() ? Phase::ready : Phase::review;
        refreshButtons();
        repaint();
        return;
    }
    phase = Phase::transcribing;
    refreshButtons();
    repaint();
    recogniser.transcribe (std::move (samples), [safe = juce::Component::SafePointer<KidPanel> (this)] (juce::String text, juce::String error)
    {
        if (safe == nullptr) return;
        safe->phase = safe->director.proposal().empty() ? Phase::ready : Phase::review;
        if (text.isEmpty())
        {
            safe->fail (error);
            safe->refreshButtons();
            safe->repaint();
            return;
        }
        safe->send (text, true);
    });
}

bool KidPanel::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && phase == Phase::recording)
    {
        stopRecording (true);
        return true;
    }
    return false;
}

void KidPanel::readTap()
{
    const auto& tap = proc.getOutputTap();
    const auto wc = tap.writeCount();
    const bool moving = wc != lastWriteCount;
    lastWriteCount = wc;
    tap.copyLatest (tapScratch.data(), (int) tapScratch.size());
    double sum = 0.0;
    for (auto v : tapScratch) sum += (double) v * v;
    const float rms = (float) std::sqrt (sum / (double) tapScratch.size());
    const float db = juce::Decibels::gainToDecibels (rms, -100.0f);
    signalPresent = moving && db > -70.0f;
    std::rotate (env.rbegin(), env.rbegin() + 1, env.rend()); // newest first
    env[0] = signalPresent ? juce::jlimit (0.03f, 1.0f, (db + 60.0f) / 54.0f) : 0.0f;
}

void KidPanel::timerCallback()
{
    ++tick;
    readTap();
    if (phase == Phase::recording)
    {
        std::rotate (micLevels.rbegin(), micLevels.rbegin() + 1, micLevels.rend());
        micLevels[0] = commandMic.level();
        if (juce::Time::getMillisecondCounter() - recordStartedMs > (juce::uint32) (kid::CommandMic::maxSeconds * 1000.0))
            stopRecording (false);
    }
    if (phase == Phase::analysing)
    {
        auto& an = proc.getAnalyzer();
        if (an.completedCount() > analysisStartCount && ! an.isListening())
        {
            lastAnalysis = an.fetch();
            if (! fileReport.ok()) // a loaded file stays the reference for the measurements; the live read is shown next to it
            {
                facts = {};
                facts.valid = lastAnalysis.status == analysis::VocalAnalysis::Status::ready;
                facts.activeSeconds = lastAnalysis.activeSeconds;
                facts.sibilanceIndex = lastAnalysis.sibilanceIndex;
                facts.dynamicRangeDb = lastAnalysis.dynamicRangeDb;
                facts.clippedSamples = lastAnalysis.clippedSamples;
                for (size_t i = 0; i < facts.bandFraction.size(); ++i)
                    facts.bandFraction[i] = lastAnalysis.bandFraction[i];
                facts.mudHz = value ("clean_on") >= 0.5f ? proc.getCleanApplied().mudHz : 0.0f;
                haveFacts = facts.valid;
            }
            if (lastAnalysis.status != analysis::VocalAnalysis::Status::ready && pendingText.isEmpty())
                fail ("I couldn't hear enough of your vocal. Press play in your project and try again.");
            finishRequest();
        }
        else if (juce::Time::getMillisecondCounter() - analysisStartedMs > 25000 && an.isListening())
            proc.stopSourceAnalysis(); // finish with whatever was heard; the result arrives on a later tick
        else if (juce::Time::getMillisecondCounter() - analysisStartedMs > 40000)
        {
            fail ("Nothing came through to analyse. Press play in your project, then ask again.");
            finishRequest();
        }
    }
    if (tick % 6 == 0) // a manual move, UNDO or a preset load changed the sound: KID starts from the sound you have now
    {
        const auto now = proc.captureSoundState();
        bool changed = now.values.size() != lastSeen.values.size();
        for (size_t i = 0; ! changed && i < now.values.size(); ++i)
            changed = now.values[i].id != lastSeen.values[i].id || std::abs (now.values[i].value - lastSeen.values[i].value) > 1e-4f;
        if (changed)
        {
            lastSeen = now;
            director.noteManualEdit (now);
            refreshButtons();
            repaint (takeBox);
        }
        syncDials();
        const bool dry = value ("bypass") >= 0.5f;
        if (originalButton.getToggleState() != dry)
            refreshButtons();
        repaint (compareBox);
        repaint (dialBox);
    }
    repaint (dispBox);
    repaint (srcBox);
    repaint (cmdBox);
    if (phase == Phase::downloading || phase == Phase::analysing)
        repaint (takeBox);
}

// ---- layout ----
void KidPanel::resized()
{
    srcBox = { 12, 8, 236, 260 };
    loadButton.setBounds (srcBox.getX() + 12, srcBox.getY() + 30, srcBox.getWidth() - 24, 28);
    analyseButton.setBounds (srcBox.getX() + 12, srcBox.getY() + 62, srcBox.getWidth() - 24, 24);
    waveArea = { srcBox.getX() + 12, srcBox.getY() + 94, srcBox.getWidth() - 24, 62 };

    dispBox = { 256, 8, 384, 260 };
    mascotBox = { dispBox.getX() + 8, dispBox.getY() + 8, 136, 136 };
    scriptBox = { dispBox.getX() + 16, dispBox.getY() + 150, dispBox.getWidth() - 32, dispBox.getHeight() - 158 };

    takeBox = { 648, 8, 300, 260 };
    auto t = takeBox;
    auto head = t.removeFromTop (36).reduced (8, 6);
    auto foot = t.removeFromBottom (42).reduced (10, 7);
    takeBody = t.reduced (12, 6);
    for (int i = 2; i >= 0; --i)
        tabButtons[i].setBounds (head.removeFromRight (i == 1 ? 90 : 58));
    applyButton.setBounds (foot.removeFromRight (96));
    foot.removeFromRight (8);
    cancelButton.setBounds (foot.removeFromRight (80));

    dialBox = { 12, 276, 628, 116 };
    const int dw = dialBox.getWidth() / numDials;
    for (int i = 0; i < numDials; ++i)
        dials[(size_t) i].slider.setBounds (dialBox.getX() + i * dw + dw / 2 - 36, dialBox.getY() + 21, 72, 72);

    compareBox = { 648, 276, 300, 116 };
    originalButton.setBounds (compareBox.getX() + 12, compareBox.getY() + 26, 134, 36);
    processedButton.setBounds (compareBox.getX() + 154, compareBox.getY() + 26, 134, 36);
    matchButton.setBounds (compareBox.getX() + 12, compareBox.getY() + 68, 96, 20);
    static const int aw[6] = { 38, 42, 60, 38, 42, 54 };
    for (int i = 0, ax = compareBox.getX() + 12; i < 6; ax += aw[i] + 1, ++i)
        advancedButtons[i].setBounds (ax, compareBox.getY() + 93, aw[i], 17);

    cmdBox = { 12, 400, 628, 136 };
    mic.setBounds (cmdBox.getX() + 12, cmdBox.getY() + 36, 64, 64);
    waveBox = { cmdBox.getX() + 88, cmdBox.getY() + 28, cmdBox.getWidth() - 100, 10 };
    auto row = juce::Rectangle<int> (cmdBox.getX() + 88, cmdBox.getY() + 44, cmdBox.getWidth() - 100, 28);
    if (setupButton.isVisible() || (kid::Recogniser::available() && ! kid::SpeechModel::isInstalled()))
    {
        setupButton.setBounds (row.removeFromRight (120));
        row.removeFromRight (6);
    }
    sendButton.setBounds (row.removeFromRight (60));
    row.removeFromRight (6);
    input.setBounds (row);
    const auto chipArea = juce::Rectangle<int> (cmdBox.getX() + 88, cmdBox.getY() + 80, cmdBox.getWidth() - 100, 50);
    int x = chipArea.getX(), y = chipArea.getY();
    for (auto* c : chips)
    {
        const int w = juce::jmin (chipArea.getWidth(), (int) juce::GlyphArrangement::getStringWidth (fontOf (12.5f), c->getButtonText()) + 22);
        if (x + w > chipArea.getRight()) { x = chipArea.getX(); y += 26; }
        c->setBounds (x, y, w, 22);
        c->setVisible (y + 22 <= cmdBox.getBottom() - 2);
        x += w + 5;
    }

    actionBox = { 648, 400, 300, 136 };
    for (int i = 0; i < kid::numProfiles; ++i)
        profileButtons[i].setBounds (actionBox.getX() + 12 + (i % 2) * 142, actionBox.getY() + 24 + (i / 2) * 28, 134, 24);
    buildButton.setBounds (actionBox.getX() + 12, actionBox.getY() + 82, actionBox.getWidth() - 24, 24);
    cleanButton.setBounds (actionBox.getX() + 12, actionBox.getY() + 110, 106, 22);
    const int iw[3] = { 50, 58, 64 };
    for (int i = 0, ix = actionBox.getX() + 122; i < 3; ix += iw[i] + 2, ++i)
        intensityButtons[i].setBounds (ix, actionBox.getY() + 110, iw[i], 22);
}

// ---- painting ----
void KidPanel::paint (juce::Graphics& g)
{
    paintSource (g);
    paintDisplay (g);

    // KID's take
    bezel (g, takeBox, plum, plumDeep);
    g.setColour (kidPinkSoft);
    g.setFont (fontOf (14.0f, true));
    g.drawText ("KID'S TAKE", takeBox.getX() + 14, takeBox.getY() + 8, 110, 22, juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff2e1a2c));
    g.fillRect (takeBox.getX() + 1, takeBox.getY() + 36, takeBox.getWidth() - 2, 1);
    g.fillRect (takeBox.getX() + 1, takeBox.getBottom() - 42, takeBox.getWidth() - 2, 1);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (takeBody);
        rowHits.clear();
        if (tab == Tab::take) paintTake (g, takeBody);
        else if (tab == Tab::read) paintRead (g, takeBody);
        else paintLimits (g, takeBody);
    }
    paintDials (g);
    paintCompare (g);
    paintCommand (g);
    paintActions (g);
}

void KidPanel::paintSource (juce::Graphics& g)
{
    bezel (g, srcBox, juce::Colour (0xff17121d), ink);
    caption (g, "YOUR VOCAL", srcBox.getX() + 14, srcBox.getY() + 10, 200, textBright);
    const auto w = waveArea.toFloat();
    g.setColour (juce::Colour (0xff09070c));
    g.fillRoundedRectangle (w, 6.0f);
    const float mid = w.getCentreY();
    if (fileReport.ok() && ! fileReport.overview.empty())
    {
        const int n = (int) fileReport.overview.size();
        juce::ColourGradient grad (ultra, w.getX(), 0.0f, kidPink, w.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        for (int px = 0; px < (int) w.getWidth(); ++px)
        {
            const float a = fileReport.overview[(size_t) juce::jlimit (0, n - 1, px * n / juce::jmax (1, (int) w.getWidth()))];
            const float hgt = juce::jmax (0.6f, a * (w.getHeight() * 0.5f - 3.0f));
            g.fillRect (w.getX() + (float) px, mid - hgt, 1.0f, 2.0f * hgt);
        }
        if (fileReport.clippedSamples > 0)
        {
            g.setColour (errorRed);
            g.fillRect (w.getX(), w.getY(), w.getWidth(), 2.0f);
        }
    }
    else
    {
        // live: the level of what leaves the plug-in, newest on the right
        const int n = (int) env.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = env[(size_t) (n - 1 - i)];
            const float hgt = juce::jmax (0.6f, a * (w.getHeight() * 0.5f - 3.0f));
            g.setColour ((signalPresent ? lilac : juce::Colour (0xff3a3546)).withAlpha (0.35f + 0.65f * (float) i / (float) n));
            g.fillRect (w.getX() + 2.0f + (float) i * (w.getWidth() - 4.0f) / (float) n, mid - hgt, 1.6f, 2.0f * hgt);
        }
    }
    juce::StringArray info;
    if (fileWaiting || fileJob.isRunning())
        info.add ("Reading the file: " + juce::String (juce::roundToInt (fileJob.progress() * 100.0f)) + " %");
    else if (fileReport.ok())
    {
        info.add (fileReport.fileName);
        info.add (juce::String (fileReport.analysedSeconds, 1) + " s, " + (fileReport.channels > 1 ? (fileReport.stereoIdentical ? "stereo file, mono content" : "stereo") : "mono")
                  + ", peak " + juce::String (fileReport.peakDb, 1) + " dBFS");
        info.add (fileReport.keyReliable ? "Key estimate: " + analysis::keyName (fileReport.keyTonic, fileReport.keyMinor) : juce::String ("Key: not reliable, not guessed"));
        info.add ("Analysed only. Play your track in the DAW to hear GlitchKid.");
    }
    else
    {
        info.add (signalPresent ? "Live input: GlitchKid hears this track" : "No signal: press play in your project");
        info.add ("Load a file for pitch, key, noise and stereo reads.");
        info.add ("It hears only this track, not your project.");
    }
    g.setFont (fontOf (12.0f));
    int y = waveArea.getBottom() + 8;
    for (int i = 0; i < info.size(); ++i)
    {
        g.setColour (i == 0 && fileReport.ok() ? textBright : textDim);
        g.drawFittedText (info[i], srcBox.getX() + 14, y, srcBox.getWidth() - 28, 16, juce::Justification::centredLeft, 1, 0.9f);
        y += i == info.size() - 1 ? 0 : 18;
    }
    if (lastAnalysis.status == analysis::VocalAnalysis::Status::ready)
    {
        g.setColour (mint);
        g.drawFittedText ("Live read: " + juce::String (lastAnalysis.activeSeconds, 1) + " s, " + juce::String (lastAnalysis.rmsDb, 1) + " dBFS RMS",
                          srcBox.getX() + 14, srcBox.getBottom() - 22, srcBox.getWidth() - 28, 14, juce::Justification::centredLeft, 1, 0.9f);
    }
}

void KidPanel::paintDisplay (juce::Graphics& g)
{
    bezel (g, dispBox, juce::Colour (0xff1a1022), ink, 18.0f);
    // the screen glass behind the mascot
    const auto m = mascotBox.toFloat();
    const auto md = mood();
    const juce::Colour glow = md == Mood::error ? errorRed : md == Mood::listening ? kidPink : md == Mood::analysing ? ultra : md == Mood::building ? kidPink
                            : md == Mood::complete ? mint : md == Mood::previewing ? lilac : violet;
    g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.28f), m.getCentreX(), m.getCentreY(), glow.withAlpha (0.0f), m.getRight() + 10.0f, m.getCentreY(), true));
    g.fillEllipse (m.expanded (8.0f));
    paintMascot (g, m, md);

    // the task, the direction, the progress
    const int tx = mascotBox.getRight() + 10, tw = dispBox.getRight() - 16 - tx;
    int y = dispBox.getY() + 16;
    g.setFont (fontOf (17.0f, true));
    g.setColour (md == Mood::error ? errorRed : textBright);
    g.drawFittedText (md == Mood::error ? juce::String ("THAT DIDN'T WORK") : phaseLabel(), tx, y, tw, 22, juce::Justification::centredLeft, 1, 0.8f);
    y += 26;
    juce::String task;
    if (phase == Phase::analysing) task = "Listening to your vocal on this track";
    else if (fileWaiting || fileJob.isRunning()) task = "Measuring levels, noise, pitch and key";
    else if (director.profile() > 0 || chosenProfile > 0) task = "Direction: " + juce::String (kid::producerProfile (director.profile() > 0 ? director.profile() : chosenProfile).name);
    else if (! director.proposal().empty()) task = "A take is waiting for you";
    else task = "Tell KID the sound you want";
    g.setFont (fontOf (13.0f, true));
    g.setColour (kidPinkSoft);
    g.drawFittedText (task, tx, y, tw, 18, juce::Justification::centredLeft, 1, 0.85f);
    y += 22;
    if (phase == Phase::analysing || fileWaiting || fileJob.isRunning())
    {
        const float prog = phase == Phase::analysing ? juce::jlimit (0.0f, 1.0f, proc.getAnalyzer().liveActiveSeconds() / 8.0f) : fileJob.progress();
        g.setColour (juce::Colour (0xff2a2336));
        g.fillRoundedRectangle ((float) tx, (float) y + 4.0f, (float) tw, 5.0f, 2.5f);
        g.setGradientFill (juce::ColourGradient (ultra, (float) tx, 0.0f, kidPink, (float) (tx + tw), 0.0f, false));
        g.fillRoundedRectangle ((float) tx, (float) y + 4.0f, juce::jmax (5.0f, (float) tw * prog), 5.0f, 2.5f);
        y += 16;
    }
    // the direction KID is following, as small pills
    juce::StringArray pills;
    for (int d = 0; d < kid::numDims; ++d)
        if (std::abs (director.direction()[(size_t) d]) > 1e-6f)
            pills.add (dimName[d][director.direction()[(size_t) d] > 0 ? 0 : 1]);
    if (director.tuneStyle() != 0) pills.add (director.tuneStyle() < 0 ? "TUNE OFF" : director.tuneStyle() == 1 ? "NATURAL TUNE" : director.tuneStyle() == 2 ? "LIGHT TUNE" : "HARD TUNE");
    if (director.cleanLevel() > 0) pills.add (juce::String ("CLEAN ") + intensityName[director.cleanLevel() - 1]);
    if (director.kept().count ("front")) pills.add ("YOU IN FRONT");
    if (director.natural()) pills.add ("NATURAL");
    int px = tx, py = y;
    g.setFont (fontOf (11.0f, true));
    for (auto& p : pills)
    {
        const int pw = (int) juce::GlyphArrangement::getStringWidth (fontOf (11.0f, true), displayCase (p)) + 14;
        if (px + pw > tx + tw) { px = tx; py += 20; }
        if (py + 16 > mascotBox.getBottom()) break;
        g.setColour (kidPink.withAlpha (0.18f));
        g.fillRoundedRectangle ((float) px, (float) py, (float) pw, 16.0f, 8.0f);
        g.setColour (kidPinkSoft);
        g.drawText (displayCase (p), px, py, pw, 16, juce::Justification::centred, false);
        px += pw + 4;
    }
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (scriptBox.getX(), scriptBox.getY() - 6, scriptBox.getWidth(), 1);
    paintScript (g, scriptBox);
}

// GlitchKid's mascot: an original little speaker-headed kid with a visor screen, headphone cups and an antenna. Drawn with paths only, on the message thread.
void KidPanel::paintMascot (juce::Graphics& g, juce::Rectangle<float> r, Mood md)
{
    const float s = r.getWidth() / 136.0f, t = (float) tick;
    const float bob = md == Mood::idle ? std::sin (t * 0.08f) * 1.5f * s : md == Mood::complete ? -std::abs (std::sin (t * 0.3f)) * 4.0f * s : 0.0f;
    const auto c = r.getCentre().translated (0.0f, bob + 6.0f * s);
    const juce::Colour shell (0xff2a1d33), shellHi (0xff4a3358), visorCol (0xff07050a);
    const juce::Colour eye = md == Mood::error ? errorRed : md == Mood::complete ? mint : md == Mood::listening ? kidPink : md == Mood::analysing ? lilac : kidPinkSoft;

    // antenna
    const juce::Point<float> base (c.x + 8.0f * s, c.y - 40.0f * s), tip (c.x + 16.0f * s + (md == Mood::building ? std::sin (t * 0.4f) * 3.0f * s : 0.0f), c.y - 58.0f * s);
    g.setColour (shellHi);
    g.drawLine ({ base, tip }, 2.2f * s);
    const float pulse = 0.5f + 0.5f * std::sin (t * (md == Mood::idle ? 0.07f : 0.25f));
    g.setColour (eye.withAlpha (0.25f + 0.35f * pulse));
    g.fillEllipse (juce::Rectangle<float> (12.0f * s, 12.0f * s).withCentre (tip));
    g.setColour (eye);
    g.fillEllipse (juce::Rectangle<float> (5.5f * s, 5.5f * s).withCentre (tip));

    // body stub with a speaker grille
    const auto body = juce::Rectangle<float> (46.0f * s, 22.0f * s).withCentre (c.translated (0.0f, 44.0f * s));
    g.setGradientFill (juce::ColourGradient (shellHi, body.getX(), body.getY(), shell, body.getX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 9.0f * s);
    g.setColour (visorCol.withAlpha (0.7f));
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (juce::Rectangle<float> (26.0f * s, 2.0f * s).withCentre (body.getCentre().translated (0.0f, (float) i * 5.0f * s)), 1.0f * s);

    // headphone cups
    for (float side : { -1.0f, 1.0f })
    {
        const auto cup = juce::Rectangle<float> (16.0f * s, 30.0f * s).withCentre (c.translated (side * 47.0f * s, 2.0f * s));
        g.setColour (shell.brighter (0.15f));
        g.fillRoundedRectangle (cup, 7.0f * s);
        g.setColour (ultra.withAlpha (0.8f));
        g.fillRoundedRectangle (cup.reduced (4.0f * s, 8.0f * s), 3.0f * s);
    }
    juce::Path band;
    band.addCentredArc (c.x, c.y - 4.0f * s, 49.0f * s, 44.0f * s, 0.0f, -1.25f, 1.25f, true);
    g.setColour (shellHi);
    g.strokePath (band, juce::PathStrokeType (3.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // head
    const auto head = juce::Rectangle<float> (84.0f * s, 70.0f * s).withCentre (c);
    g.setGradientFill (juce::ColourGradient (shellHi, head.getX(), head.getY(), shell, head.getX(), head.getBottom(), false));
    g.fillRoundedRectangle (head, 24.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (head.reduced (1.0f), 24.0f * s, 1.2f * s);

    // the visor, with GlitchKid's signature: a brief colour-split glitch now and then (and constantly on an error)
    auto visor = head.reduced (10.0f * s, 14.0f * s).translated (0.0f, -1.0f * s);
    const bool glitch = md == Mood::error || (tick % 150) < 4;
    if (glitch)
    {
        g.setColour (kidPink.withAlpha (0.5f));
        g.fillRoundedRectangle (visor.translated (-2.0f * s, 0.0f), 13.0f * s);
        g.setColour (ultra.withAlpha (0.5f));
        g.fillRoundedRectangle (visor.translated (2.0f * s, 0.0f), 13.0f * s);
    }
    g.setColour (visorCol);
    g.fillRoundedRectangle (visor, 13.0f * s);
    g.setColour (eye.withAlpha (0.18f));
    g.drawRoundedRectangle (visor, 13.0f * s, 1.0f * s);

    // eyes and mouth, by mood
    const float ex = 13.0f * s, ey = visor.getCentreY() - 3.0f * s;
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (visor.toNearestInt());
    g.setColour (eye);
    auto pill = [&] (float x, float w, float h) { g.fillRoundedRectangle (juce::Rectangle<float> (w, h).withCentre ({ x, ey }), juce::jmin (w, h) * 0.5f); };
    switch (md)
    {
        case Mood::idle:
        {
            const bool blink = (tick % 120) < 4;
            for (float side : { -1.0f, 1.0f }) pill (visor.getCentreX() + side * ex, 7.0f * s, blink ? 1.6f * s : 13.0f * s);
            break;
        }
        case Mood::listening:
            for (float side : { -1.0f, 1.0f })
                g.fillEllipse (juce::Rectangle<float> (13.0f * s, 13.0f * s).withCentre ({ visor.getCentreX() + side * ex, ey }));
            break;
        case Mood::analysing:
        {
            for (float side : { -1.0f, 1.0f }) pill (visor.getCentreX() + side * ex, 9.0f * s, 6.0f * s);
            const float sx = visor.getX() + (0.5f + 0.5f * std::sin (t * 0.12f)) * visor.getWidth();
            g.setColour (lilac.withAlpha (0.55f));
            g.fillRect (sx - 1.5f * s, visor.getY(), 3.0f * s, visor.getHeight());
            break;
        }
        case Mood::building:
            for (float side : { -1.0f, 1.0f })
            {
                const juce::Point<float> e (visor.getCentreX() + side * ex, ey);
                g.drawEllipse (juce::Rectangle<float> (12.0f * s, 12.0f * s).withCentre (e), 1.6f * s);
                const float a = t * 0.3f * side;
                g.fillEllipse (juce::Rectangle<float> (4.0f * s, 4.0f * s).withCentre (e.translated (std::cos (a) * 6.0f * s, std::sin (a) * 6.0f * s)));
            }
            break;
        case Mood::previewing:
            for (float side : { -1.0f, 1.0f })
            {
                pill (visor.getCentreX() + side * ex, 11.0f * s, 11.0f * s);
                g.setColour (visorCol);
                g.fillEllipse (juce::Rectangle<float> (5.0f * s, 5.0f * s).withCentre ({ visor.getCentreX() + side * ex + 3.0f * s, ey })); // looking at KID's take
                g.setColour (eye);
            }
            break;
        case Mood::complete:
            for (float side : { -1.0f, 1.0f })
            {
                juce::Path arc;
                arc.addCentredArc (visor.getCentreX() + side * ex, ey + 3.0f * s, 6.0f * s, 6.0f * s, 0.0f, -1.3f, 1.3f, true);
                g.strokePath (arc, juce::PathStrokeType (2.4f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            break;
        case Mood::error:
            for (float side : { -1.0f, 1.0f })
            {
                const juce::Point<float> e (visor.getCentreX() + side * ex, ey);
                g.drawLine ({ e.translated (-4.5f * s, -4.5f * s), e.translated (4.5f * s, 4.5f * s) }, 2.2f * s);
                g.drawLine ({ e.translated (-4.5f * s, 4.5f * s), e.translated (4.5f * s, -4.5f * s) }, 2.2f * s);
            }
            break;
    }
    // the mouth: a little waveform that follows the mood (the live command level while you talk)
    juce::Path mouth;
    const float my = visor.getBottom() - 7.0f * s, mw = 22.0f * s;
    for (int i = 0; i <= 20; ++i)
    {
        const float u = (float) i / 20.0f, x = visor.getCentreX() - mw * 0.5f + u * mw;
        float a = 0.6f * s;
        if (md == Mood::listening) a = juce::jmin (1.0f, micLevels[0] * 4.0f) * 4.0f * s + 0.6f * s;
        else if (md == Mood::building || md == Mood::analysing) a = 1.8f * s;
        const float yv = md == Mood::complete ? my - std::sin (u * juce::MathConstants<float>::pi) * 3.0f * s
                       : md == Mood::error ? my + std::sin (u * 12.0f) * 1.2f * s : my + std::sin (u * 14.0f + t * 0.3f) * a;
        if (i == 0) mouth.startNewSubPath (x, yv); else mouth.lineTo (x, yv);
    }
    g.setColour (eye.withAlpha (0.8f));
    g.strokePath (mouth, juce::PathStrokeType (1.6f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (md == Mood::listening) // sound arriving
        for (int k = 0; k < 2; ++k)
        {
            const float ph = std::fmod (t * 0.05f + (float) k * 0.5f, 1.0f);
            g.setColour (kidPink.withAlpha (0.5f * (1.0f - ph)));
            g.drawEllipse (juce::Rectangle<float> ((30.0f + 30.0f * ph) * s, (30.0f + 30.0f * ph) * s).withCentre ({ visor.getCentreX(), ey }), 1.2f * s);
        }
}

void KidPanel::paintScript (juce::Graphics& g, juce::Rectangle<int> area)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (area);
    std::vector<std::pair<juce::TextLayout, int>> layouts; // layout, label colour index
    float total = 0.0f;
    std::vector<Line> shown (lines.begin(), lines.end());
    if (phase == Phase::analysing || phase == Phase::transcribing)
        shown.push_back ({ true, phaseHint() });
    for (size_t i = 0; i < shown.size(); ++i)
    {
        const auto& l = shown[i];
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        const bool thinking = l.kid && i == shown.size() - 1 && (phase == Phase::analysing || phase == Phase::transcribing);
        as.append (l.text, l.kid ? fontOf (13.5f, ! thinking) : fontOf (15.5f), l.kid ? (thinking ? juce::Colour (0xffb9a8c8) : kidPinkSoft) : textBright);
        juce::TextLayout tl;
        tl.createLayout (as, (float) area.getWidth() - (l.kid ? 14.0f : 0.0f));
        total += 18.0f + tl.getHeight() + 14.0f;
        layouts.emplace_back (std::move (tl), (int) i);
    }
    scriptMax = juce::jmax (0.0f, total - (float) area.getHeight());
    scriptScroll = juce::jlimit (0.0f, scriptMax, scriptScroll);
    float y = (float) area.getY() - scriptScroll;
    for (auto& [tl, idx] : layouts)
    {
        const auto& l = shown[(size_t) idx];
        g.setFont (fontOf (11.5f, true));
        g.setColour (l.kid ? kidPink : textDim);
        g.drawText (l.kid ? juce::String ("KID") : juce::String (l.spoken ? "YOU  (SPOKEN)" : "YOU  (TYPED)"), area.getX() + (l.kid ? 14 : 0), (int) y, 220, 14, juce::Justification::centredLeft, false);
        y += 18.0f;
        if (l.kid)
        {
            g.setColour (kidPink);
            g.fillRect ((float) area.getX(), y - 16.0f, 2.0f, tl.getHeight() + 16.0f);
        }
        tl.draw (g, { (float) area.getX() + (l.kid ? 14.0f : 0.0f), y, (float) area.getWidth() - (l.kid ? 14.0f : 0.0f), tl.getHeight() });
        y += tl.getHeight() + 14.0f;
    }
}

void KidPanel::paintTake (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& rows = director.proposal();
    float y = (float) area.getY() - takeScroll;
    auto text = [&] (const juce::String& t, juce::Font f, juce::Colour c, float indent = 0.0f)
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.append (t, f, c);
        juce::TextLayout tl;
        tl.createLayout (as, (float) area.getWidth() - indent);
        tl.draw (g, { (float) area.getX() + indent, y, (float) area.getWidth() - indent, tl.getHeight() });
        y += tl.getHeight();
    };
    // the direction KID is following, and what it keeps in mind
    juce::StringArray chipsText;
    for (int d = 0; d < kid::numDims; ++d)
        if (std::abs (director.direction()[(size_t) d]) > 1e-6f)
            chipsText.add (juce::String (dimName[d][director.direction()[(size_t) d] > 0 ? 0 : 1]) + "  " + juce::String::repeatedString ("+", juce::jlimit (1, 4, juce::roundToInt (std::abs (director.direction()[(size_t) d]) * 2.0f))));
    if (director.kept().count ("front")) chipsText.add ("YOU IN FRONT (kept)");
    else if (director.kept().count ("presence")) chipsText.add ("PRESENCE (kept)");
    if (director.kept().count ("char")) chipsText.add ("CHARACTER (kept)");
    if (director.kept().count ("warm")) chipsText.add ("WARMTH (kept)");
    if (director.role() == "adlib") chipsText.add ("AD-LIBS");
    if (director.natural()) chipsText.add ("NATURAL");
    if (rows.empty())
    {
        text (lines.size() > 1 ? "Nothing on the table." : "Tell KID what you hear in your head.", fontOf (16.0f, true), textBright);
        y += 6;
        text ("KID turns what you say into changes to GlitchKid's own controls, shows you every value, and changes nothing until you Apply.", fontOf (13.0f), textDim);
        y += 10;
        if (! chipsText.isEmpty())
        {
            text ("Kept in mind: " + chipsText.joinIntoString (", "), fontOf (12.5f, true), kidPinkSoft);
            y += 8;
        }
        g.setFont (fontOf (11.5f, true));
        g.setColour (textDim);
        g.drawText ("WHAT KID REACHES FOR", area.getX(), (int) y, area.getWidth(), 14, juce::Justification::centredLeft, false);
        y += 18;
        static const char* const map[][2] = { { "\"airy\"", "EQ air shelf #6, AIR LAB #11" }, { "\"clean it up\"", "CLEAN #7 + #9, EQ body band" }, { "\"wider\"", "Doubler #5, Dimension #14" },
                                             { "\"darker\"", "EQ air shelf and presence #6" }, { "\"aggressive\"", "Compressor #8, Saturation #10" }, { "\"distant\"", "Echo #13, Dimension, EQ" },
                                             { "\"experimental\"", "LAB: Mutation #4 or Stutter #12" }, { "\"harder tuning\"", "Pitch Tune #1 (key only from your file)" },
                                             { "never", "Gain, Mix, Limiter, Bypass" } };
        for (auto& m : map)
        {
            g.setFont (fontOf (12.5f, true));
            g.setColour (kidPinkSoft);
            g.drawText (m[0], area.getX(), (int) y, 96, 16, juce::Justification::centredLeft, false);
            g.setFont (fontOf (12.5f));
            g.setColour (textDim);
            g.drawText (m[1], area.getX() + 98, (int) y, area.getWidth() - 98, 16, juce::Justification::centredLeft, true);
            y += 19;
        }
        takeMax = juce::jmax (0.0f, y + takeScroll - (float) area.getBottom());
        return;
    }
    if (! chipsText.isEmpty())
    {
        text (chipsText.joinIntoString ("   "), fontOf (12.5f, true), kidPinkSoft);
        y += 4;
    }
    std::set<int> stages;
    for (auto& r : rows) stages.insert (kid::Director::stageOf (r.id));
    juce::StringArray mods;
    for (int s : stages) mods.add ("#" + juce::String (s) + " " + stageName[s]);
    text ("Adjusting " + mods.joinIntoString (", "), fontOf (12.5f), textDim);
    y += 6;
    for (auto& r : rows)
    {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (r.id));
        const auto show = [prm] (float v) { return prm == nullptr ? juce::String (v, 2) : (prm->getText (prm->convertTo0to1 (v), 24) + " " + prm->getLabel()).trim(); };
        const int s = kid::Director::stageOf (r.id);
        const juce::String name = juce::String (stageName[s]) + ": " + (prm != nullptr ? prm->getName (40) : r.id);
        const float top = y;
        g.setColour (juce::Colour (0xff2a1828));
        g.fillRect ((float) area.getX(), y, (float) area.getWidth(), 1.0f);
        y += 6;
        const juce::Rectangle<int> box (area.getX(), (int) y + 1, 15, 15);
        g.setColour (r.selected ? juce::Colour (0xffa83bee) : juce::Colour (0xff3a3546));
        g.fillRoundedRectangle (box.toFloat(), 3.0f);
        if (r.selected)
        {
            g.setColour (juce::Colours::white);
            juce::Path tickMark;
            tickMark.startNewSubPath ((float) box.getX() + 3.5f, (float) box.getY() + 8.0f);
            tickMark.lineTo ((float) box.getX() + 6.5f, (float) box.getY() + 11.0f);
            tickMark.lineTo ((float) box.getX() + 12.0f, (float) box.getY() + 4.0f);
            g.strokePath (tickMark, juce::PathStrokeType (2.0f));
        }
        g.setFont (fontOf (13.0f, true));
        g.setColour (r.selected ? textBright : textDim);
        g.drawText (name, area.getX() + 22, (int) y, area.getWidth() - 22 - 128, 18, juce::Justification::centredLeft, true);
        g.setFont (fontOf (12.5f));
        g.setColour (textDim);
        g.drawText (show (r.from) + "  >", area.getRight() - 128, (int) y, 66, 18, juce::Justification::centredRight, true);
        g.setFont (fontOf (12.5f, true));
        g.setColour (r.selected ? textBright : textDim);
        g.drawText (show (r.to), area.getRight() - 60, (int) y, 60, 18, juce::Justification::centredRight, true);
        y += 19;
        juce::String tag;
        switch (r.tag)
        {
            case kid::Change::Tag::added: tag = "NEW - "; break;
            case kid::Change::Tag::up: tag = "ADJUSTED UP - "; break;
            case kid::Change::Tag::down: tag = "ADJUSTED DOWN - "; break;
            case kid::Change::Tag::manual: tag = "YOU CHANGED THIS - "; break;
            case kid::Change::Tag::same: break;
        }
        text (tag + r.reason + " - " + r.id, fontOf (12.5f), r.tag == kid::Change::Tag::manual ? amber : textDim, 22.0f);
        y += 6;
        rowHits.push_back ({ juce::Rectangle<int> (area.getX(), (int) top, area.getWidth(), (int) (y - top)), r.id });
    }
    if (! director.removed().isEmpty())
    {
        juce::StringArray names;
        for (auto& id : director.removed())
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id)))
                names.add (prm->getName (40));
        y += 6;
        text ("No longer changing: " + names.joinIntoString (", ") + ".", fontOf (12.5f), textDim);
    }
    takeMax = juce::jmax (0.0f, y + takeScroll - (float) area.getBottom());
}

void KidPanel::paintLimits (juce::Graphics& g, juce::Rectangle<int> area)
{
    float y = (float) area.getY() - takeScroll;
    auto row = [&] (const juce::String& label, const juce::String& v)
    {
        g.setFont (fontOf (12.5f));
        g.setColour (juce::Colour (0xffcdbfd2));
        g.drawText (label, area.getX(), (int) y, area.getWidth(), 16, juce::Justification::centredLeft, true);
        y += 16;
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.append (v, fontOf (12.5f, true), textBright);
        juce::TextLayout tl;
        tl.createLayout (as, (float) area.getWidth());
        tl.draw (g, { (float) area.getX(), y, (float) area.getWidth(), tl.getHeight() });
        y += tl.getHeight() + 9;
    };
    int allowed = 0;
    for (auto* q : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            allowed += kid::Director::isAllowed (r->paramID) ? 1 : 0;
    row ("Parameters KID may propose", juce::String (allowed) + " of " + juce::String (proc.getParameters().size()));
    row ("Never touched", "Gain, Mix, Output Limiter (4), Bypass. Pitch Tune key and scale only from a reliable estimate of your loaded file");
    row ("Not in GlitchKid", "noise reduction, reverb, clipping repair, timing correction: KID says so instead of pretending");
    row ("Every value", "clamped to the parameter's own range and step");
    row ("Nothing changes", "until you press Apply. UNDO goes back one step.");
    row ("Your manual moves", "win: KID unticks that change and starts from your sound");
    row ("Command microphone", "push-to-talk only, a separate input; what you say never enters the vocal chain and is deleted after it is turned into text");
    row ("Speech", ! kid::Recogniser::available() ? juce::String ("not in this build")
                 : kid::SpeechModel::isInstalled() ? "on this computer (" + kid::SpeechModel::file().getFullPathName() + "); works offline"
                 : juce::String ("not set up (SET UP SPEECH downloads the model once, about 142 MB)"));
    row ("Refused by the validator", director.refused().isEmpty() ? juce::String ("none") : director.refused().joinIntoString (", "));
    takeMax = juce::jmax (0.0f, y + takeScroll - (float) area.getBottom());
}

void KidPanel::paintRead (juce::Graphics& g, juce::Rectangle<int> area)
{
    float y = (float) area.getY() - takeScroll;
    auto para = [&] (const juce::String& t, juce::Colour c, float size = 12.5f, bool bold = false)
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.append (t, fontOf (size, bold), c);
        juce::TextLayout tl;
        tl.createLayout (as, (float) area.getWidth());
        tl.draw (g, { (float) area.getX(), y, (float) area.getWidth(), tl.getHeight() });
        y += tl.getHeight() + 5.0f;
    };
    if (fileReport.ok())
    {
        para ("FROM YOUR FILE", textBright, 11.5f, true);
        for (auto& l : analysis::describeVocalFile (fileReport))
        {
            const bool measured = l.startsWith ("MEASURED");
            para ((measured ? "Measured: " : "Estimate: ") + l.fromFirstOccurrenceOf ("  ", false, false).trim(), measured ? mint : textDim);
        }
        y += 6.0f;
    }
    const auto& a = lastAnalysis;
    if (a.status == analysis::VocalAnalysis::Status::ready)
    {
        para ("FROM THE LIVE INPUT (measured)", textBright, 11.5f, true);
        para ("Listened to " + juce::String (a.activeSeconds, 1) + " s of " + juce::String (a.totalSeconds, 1) + " s. Peak " + juce::String (a.peakDb, 1) + " dBFS, RMS "
              + juce::String (a.rmsDb, 1) + " dBFS, crest " + juce::String (a.crestDb, 1) + " dB, level movement " + juce::String (a.dynamicRangeDb, 1) + " dB.", mint);
        static const char* const bandNames[5] = { "< 120 Hz", "120-500 Hz", "500 Hz-2 kHz", "2-5 kHz", "> 5 kHz" };
        juce::StringArray bands;
        for (int i = 0; i < 5; ++i)
            bands.add (juce::String (bandNames[i]) + " " + juce::String (juce::roundToInt (a.bandFraction[(size_t) i] * 100.0f)) + " %");
        para ("Energy: " + bands.joinIntoString (", ") + ". Clipped samples: " + juce::String (a.clippedSamples) + ". Sibilance index (rough): " + juce::String (a.sibilanceIndex, 2) + ".", mint);
        para ("The live analyser is mono and does not read pitch, key or background noise: load a file for those.", textDim);
    }
    if (! fileReport.ok() && a.status != analysis::VocalAnalysis::Status::ready)
    {
        para (phase == Phase::analysing ? phaseHint()
                                        : "Load a vocal file (levels, clipping, noise between phrases, pitch, a key estimate when it is clear, stereo) or press LISTEN LIVE while your project plays. "
                                          "Measurements and estimates are labelled.", textDim, 13.0f);
    }
    takeMax = juce::jmax (0.0f, y + takeScroll - (float) area.getBottom());
}

void KidPanel::paintDials (juce::Graphics& g)
{
    bezel (g, dialBox, juce::Colour (0xff1b1424), ink);
    const int dw = dialBox.getWidth() / numDials;
    for (int i = 0; i < numDials; ++i)
    {
        const auto& d = dials[(size_t) i];
        const int x = dialBox.getX() + i * dw;
        g.setFont (fontOf (12.5f, true));
        g.setColour (textBright);
        g.drawText (d.name, x, dialBox.getY() + 7, dw, 14, juce::Justification::centred, false);
        g.setFont (fontOf (12.0f));
        g.setColour (d.accent);
        g.drawFittedText (d.text ((float) d.slider.getValue()), x + 4, dialBox.getY() + 94, dw - 8, 16, juce::Justification::centred, 1, 0.85f);
        if (i > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (x, dialBox.getY() + 12, 1, dialBox.getHeight() - 24);
        }
    }
}

void KidPanel::paintCompare (juce::Graphics& g)
{
    bezel (g, compareBox, juce::Colour (0xff1b1424), ink);
    caption (g, "BEFORE / AFTER", compareBox.getX() + 14, compareBox.getY() + 8, 200, textBright);
    const float db = proc.getCompareGainDb();
    juce::String t;
    if (! levelMatch) t = "Off: ORIGINAL is the exact input";
    else if (std::abs (db) < 0.05f) t = "On: no level difference measured yet";
    else t = "On: ORIGINAL plays " + juce::String (std::abs (db), 1) + " dB " + (db > 0.0f ? "louder" : "quieter") + " to match";
    g.setFont (fontOf (11.5f));
    g.setColour (levelMatch ? lilac : textDim);
    g.drawFittedText (t, compareBox.getX() + 114, compareBox.getY() + 68, compareBox.getWidth() - 126, 20, juce::Justification::centredLeft, 1, 0.8f);
}

void KidPanel::paintCommand (juce::Graphics& g)
{
    bezel (g, cmdBox, juce::Colour (0xff160f1d), ink);
    g.setFont (fontOf (13.5f, true));
    g.setColour (phase == Phase::recording ? kidPinkSoft : textBright);
    const auto label = displayCase (phaseLabel());
    const int lw = (int) juce::GlyphArrangement::getStringWidth (fontOf (13.5f, true), label) + 12;
    g.drawText (label, waveBox.getX(), cmdBox.getY() + 8, lw, 18, juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.0f));
    g.setColour (textDim);
    g.drawFittedText (phaseHint(), waveBox.getX() + lw, cmdBox.getY() + 8, waveBox.getWidth() - lw, 18, juce::Justification::centredLeft, 1, 0.8f);
    // a thin strip under the label: the command mic while you talk, a moving line while KID works, flat at rest
    const auto w = waveBox.toFloat();
    const float mid = w.getCentreY();
    if (phase == Phase::recording)
    {
        juce::ColourGradient grad (lilac, w.getX(), 0, juce::Colour (0xfff6eefa), w.getRight(), 0, false);
        grad.addColour (0.55, kidPink);
        g.setGradientFill (grad);
        const int n = (int) micLevels.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::jmax (1.0f, juce::jmin (1.0f, micLevels[(size_t) (n - 1 - i)] * 3.0f) * w.getHeight() * 0.5f);
            g.fillRect (w.getX() + (float) i / (float) (n - 1) * (w.getWidth() - 3.0f), mid - a, 2.6f, 2 * a);
        }
    }
    else
    {
        const bool busy = phase == Phase::transcribing || phase == Phase::downloading || phase == Phase::analysing;
        juce::Path p;
        for (float x = 0; x <= w.getWidth(); x += 3.0f)
        {
            const float y = mid + (busy ? std::sin (x * 0.05f - (float) tick * 0.2f) * w.getHeight() * 0.4f : 0.0f);
            if (x < 1.0f) p.startNewSubPath (w.getX(), y); else p.lineTo (w.getX() + x, y);
        }
        g.setColour (busy ? kidPink.withAlpha (0.8f) : juce::Colour (0xff2e2838));
        g.strokePath (p, juce::PathStrokeType (1.4f));
        if (phase == Phase::downloading)
        {
            g.setColour (kidPink);
            g.fillRect (w.getX(), w.getBottom(), w.getWidth() * downloader.progress(), 2.0f);
        }
    }
}

void KidPanel::paintActions (juce::Graphics& g)
{
    bezel (g, actionBox, plum, plumDeep);
    caption (g, "SOUND PROFILES  (original directions, not anyone's chain)", actionBox.getX() + 14, actionBox.getY() + 6, actionBox.getWidth() - 28, kidPinkSoft);
}

// ---- input ----
void KidPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (tab == Tab::take && takeBody.contains (p))
        for (auto& [r, id] : rowHits)
            if (r.contains (p))
            {
                for (auto& row : director.proposal())
                    if (row.id == id)
                    {
                        director.setSelected (id, ! row.selected);
                        break;
                    }
                refreshButtons();
                repaint (takeBox);
                return;
            }
}

void KidPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float dy = -w.deltaY * 120.0f;
    if (scriptBox.contains (e.getPosition()))
        scriptScroll = juce::jlimit (0.0f, scriptMax, scriptScroll + dy);
    else if (takeBody.contains (e.getPosition()))
        takeScroll = juce::jlimit (0.0f, takeMax, takeScroll + dy);
    repaint();
}
}
