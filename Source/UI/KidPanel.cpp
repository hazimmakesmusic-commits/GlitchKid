#include "KidPanel.h"
#include "PlaygroundWidgets.h"
#include "../Analysis/ReferenceAnalyzer.h"

namespace glitchkid::ui
{
namespace
{
// Simple neon: near-black, charcoal surfaces, one ultraviolet accent, pink only for "live" (recording, ORIGINAL), white type.
const juce::Colour bg (0xff08070b), surface (0xff121117), surfaceHi (0xff1a1820), hairline (0xff24212c), accent (0xff9b5cff), accentSoft (0xffc4a6ff),
                   liveCol (0xffe14cbb), white (0xfff4f2f7), dim (0xff8d8798), faint (0xff5d5868);
constexpr int advancedStage[5] = { 1, 7, 6, 8, 13 }; // Tune, Clean (corrective), Tone (EQ), Dynamics (compressor), Space (echo)

void glow (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c, float strength)
{
    for (int i = 3; i >= 1; --i) // a few soft rings: cheap, and drawn only around the one active control
    {
        g.setColour (c.withAlpha (strength * 0.05f * (float) (4 - i)));
        g.fillRoundedRectangle (r.expanded ((float) i * 3.0f), radius + (float) i * 3.0f);
    }
}
}

void KidPanel::Pill::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = getToggleState();
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    if (main)
    {
        if (isEnabled()) glow (g, r, rad, accent, over ? 1.4f : 1.0f);
        g.setColour (accent.withAlpha (alpha).brighter (down ? 0.0f : over ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (white.withAlpha (alpha));
        g.setFont (fontOf (14.5f, true));
    }
    else
    {
        if (on)
        {
            g.setColour (accent.withAlpha (0.22f * alpha));
            g.fillRoundedRectangle (r, rad);
            g.setColour (accent.withAlpha (alpha));
            g.drawRoundedRectangle (r, rad, 1.2f);
        }
        else
        {
            g.setColour ((over ? surfaceHi : surface).withAlpha (alpha));
            g.fillRoundedRectangle (r, rad);
            g.setColour (hairline.withAlpha (alpha));
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
        g.setColour ((on ? white : over ? white : juce::Colour (0xffd6d2de)).withAlpha (alpha));
        g.setFont (fontOf (13.0f, on));
    }
    g.drawFittedText (getButtonText(), getLocalBounds().reduced (10, 0), juce::Justification::centred, 1, 0.85f);
    if (hasKeyboardFocus (false))
    {
        g.setColour (accentSoft.withAlpha (0.8f));
        g.drawRoundedRectangle (r.expanded (1.0f), rad + 1.0f, 1.0f);
    }
}

// ---- the push-to-talk button ----// ---- the push-to-talk button ----
void KidPanel::MicButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    juce::ColourGradient ring (accent, r.getX(), r.getBottom(), liveCol, r.getRight(), r.getY(), false);
    ring.addColour (0.7, juce::Colour (0xffc9a6ff));
    if (live)
    {
        g.setColour (liveCol.withAlpha (0.25f));
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

KidPanel::KidPanel (GlitchKidProcessor& p, Hooks h) : proc (p), hooks (std::move (h)), director (p.makeRangeLookup())
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    lastApplied = proc.captureSoundState();
    director.begin (lastApplied);
    tapScratch.resize (2048);
    say ("Hey, I'm Kid. Load your vocal (or play your track), then tell me the sound you want, like \"a polished, emotional melodic rap vocal with smooth tuning and a little atmosphere\". "
         "Or just press BUILD MY MIX and I'll fit a mix to your recording.");

    mic.setTitle ("Talk to Kid");
    mic.setDescription ("Push to talk: hold to speak, or tap once to start and again to stop.");
    mic.setWantsKeyboardFocus (true);
    mic.onPress = [this] { startRecording(); };
    mic.onRelease = [this] { stopRecording (false); };
    addAndMakeVisible (mic);

    input.setFont (fontOf (15.0f));
    input.setTextToShowWhenEmpty ("Tell Kid what you want, or ask why...", faint);
    input.setColour (juce::TextEditor::backgroundColourId, surface);
    input.setColour (juce::TextEditor::outlineColourId, hairline);
    input.setColour (juce::TextEditor::focusedOutlineColourId, accent);
    input.setColour (juce::TextEditor::textColourId, white);
    input.setIndents (14, 0);
    input.setJustification (juce::Justification::centredLeft);
    input.setTitle ("Message to Kid");
    input.getProperties().set ("rawText", true);
    input.onReturnKey = [this] { send (input.getText(), false); input.clear(); };
    addAndMakeVisible (input);

    sendButton.onClick = [this] { send (input.getText(), false); input.clear(); };
    sendButton.setTooltip ("Send to Kid.");
    setupButton.setTooltip ("Downloads Kid's speech model once (about 142 MB, from the whisper.cpp project), checks it, and keeps it on this computer. Speech then works offline.");
    setupButton.onClick = [this]
    {
        phase = Phase::downloading;
        say ("Setting up speech: downloading the model (about 142 MB) and checking it. You can keep typing.");
        downloader.start ([safe = juce::Component::SafePointer<KidPanel> (this)] (bool ok, juce::String msg)
        {
            if (safe == nullptr) return;
            safe->phase = Phase::ready;
            safe->say (ok ? juce::String ("Speech is ready. Hold the mic and talk to me.") : msg);
            safe->refreshButtons();
            safe->repaint();
        });
        refreshButtons();
    };
    loadButton.setTooltip ("Analyse a vocal recording (" + analysis::referenceFormatNames() + "). It is read, not played or copied: you hear GlitchKid by playing your track in the DAW.");
    loadButton.onClick = [this] { chooseFile(); };
    originalButton.setTooltip ("Your untouched vocal (the real bypass), played at the processed loudness so the louder version never wins.");
    processedButton.setTooltip ("Your vocal through GlitchKid.");
    originalButton.onClick = [this] { setCompare (true); };
    processedButton.onClick = [this] { setCompare (false); };
    mainButton.setTooltip ("Kid listens to your vocal (the loaded file, or the live input while your project plays), diagnoses it and builds a mix with GlitchKid's own effects. UNDO goes back.");
    mainButton.onClick = [this]
    {
        if (built && ! fileReport.ok())
            haveFacts = false; // UPDATE with the live input: listen again, the take or the performance may have changed
        send (chosenProfile > 0 && ! built ? "Build my mix: " + juce::String (kid::producerProfile (chosenProfile).name) : juce::String ("Build my mix"), false);
    };
    undoButton.setTooltip ("Undo Kid's last change.");
    undoButton.onClick = [this] { undoStep(); };
    for (int i = 0; i < 5; ++i)
    {
        advanced[i].setTooltip ("Open the detailed " + advanced[i].getButtonText().toLowerCase() + " controls. KID in the side rail brings you back.");
        advanced[i].onClick = [this, i] { if (hooks.openStage) hooks.openStage (advancedStage[i]); };
        addAndMakeVisible (advanced[i]);
    }
    backButton.setTooltip ("Back to the GlitchKid instrument.");
    backButton.onClick = [this] { if (hooks.back) hooks.back(); };
    for (auto* b : { &sendButton, &setupButton, &loadButton, &originalButton, &processedButton, &mainButton, &undoButton, &backButton })
        addAndMakeVisible (b);
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
    proc.setLevelMatchedCompare (isVisible() && getParentComponent() != nullptr); // ORIGINAL on this screen is loudness-matched; everywhere else bypass is exact
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

juce::String KidPanel::lastKidLineForTest() const
{
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (it->kid) return it->text;
    return {};
}

void KidPanel::pressForTest (const juce::String& text)
{
    for (auto* c : getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == text && b->isEnabled() && b->onClick)
        {
            b->onClick();
            return;
        }
}

KidPanel::Status KidPanel::status() const
{
    if (phase == Phase::recording) return Status::listening;
    if (phase == Phase::analysing) return signalPresent ? Status::analyzing : Status::listening;
    if (fileJob.isRunning() || fileWaiting) return Status::analyzing;
    if (phase == Phase::transcribing || phase == Phase::downloading || (appliedAtMs != 0 && juce::Time::getMillisecondCounter() - appliedAtMs < 700)) return Status::processing;
    return Status::ready;
}

juce::String KidPanel::statusText() const
{
    switch (status())
    {
        case Status::listening: return phase == Phase::recording ? "Listening to you" : "Listening for your vocal";
        case Status::analyzing: return fileJob.isRunning() || fileWaiting ? "Analyzing your file" : "Analyzing your vocal";
        case Status::processing: return phase == Phase::transcribing ? "Understanding" : phase == Phase::downloading ? "Setting up speech" : "Processing";
        case Status::ready: break;
    }
    return "Ready";
}

juce::StringArray KidPanel::suggestions() const
{
    if (! built)
        return { "Make me sound like The Kid LAROI", "Polished, emotional melodic rap", "Analyze my vocal" };
    return { "Less reverb", "Make the tuning harder", "Why did you change the high mids?" };
}

void KidPanel::refreshChips()
{
    const auto s = suggestions();
    chips.clear();
    for (auto& t : s)
    {
        auto* b = chips.add (new Pill (t));
        b->setTooltip ("Say this to Kid.");
        b->onClick = [this, t] { send (t, false); };
        addAndMakeVisible (b);
    }
    resized();
}

void KidPanel::refreshButtons()
{
    const bool busy = phase == Phase::analysing || phase == Phase::transcribing || phase == Phase::recording;
    const bool speechOk = kid::Recogniser::available() && kid::SpeechModel::isInstalled() && phase != Phase::downloading;
    mic.setEnabled ((speechOk && ! busy) || phase == Phase::recording);
    mic.live = phase == Phase::recording;
    setupButton.setVisible (kid::Recogniser::available() && ! kid::SpeechModel::isInstalled() && phase != Phase::downloading);
    input.setEnabled (! busy);
    sendButton.setEnabled (! busy);
    for (auto* c : chips) c->setEnabled (! busy);
    mainButton.setEnabled (! busy);
    mainButton.setButtonText (built ? "UPDATE MY MIX" : "BUILD MY MIX");
    undoButton.setEnabled (! history.empty() && ! busy);
    loadButton.setEnabled (! fileJob.isRunning() && ! fileWaiting);
    const bool dry = value ("bypass") >= 0.5f;
    originalButton.setToggleState (dry, juce::dontSendNotification);
    processedButton.setToggleState (! dry, juce::dontSendNotification);
    mic.repaint();
}

void KidPanel::setCompare (bool original)
{
    proc.setLevelMatchedCompare (isVisible() && getParentComponent() != nullptr);
    setParam ("bypass", original ? 1.0f : 0.0f);
    refreshButtons();
    repaint (vocalBox);
}

// ---- the conversation ----
void KidPanel::send (const juce::String& raw, bool spoken)
{
    const auto text = raw.trim();
    if (text.isEmpty() || phase == Phase::analysing || phase == Phase::transcribing)
        return;
    lines.push_back ({ false, text, spoken });
    scroll = 1.0e9f;
    pendingText = text;
    pendingSpoken = spoken;
    const auto f = kid::parse (text);
    const bool soundRequest = f.ask == kid::IntentFrame::Ask::none && f.understood() && ! f.reset;
    if (! haveFacts && (soundRequest || kid::Director::needsAnalysis (f)))
    {
        startListening();
        return;
    }
    finishRequest();
}

void KidPanel::startListening()
{
    auto& an = proc.getAnalyzer();
    analysisStartCount = an.completedCount();
    if (! an.isListening())
        proc.startSourceAnalysis();
    analysisStartedMs = juce::Time::getMillisecondCounter();
    phase = Phase::analysing;
    say ("Let me hear your vocal first: press play in your project (2-8 seconds of singing or rapping), or load the vocal file.");
    refreshButtons();
    repaint();
}

void KidPanel::finishRequest()
{
    phase = Phase::ready;
    if (pendingText.isNotEmpty())
    {
        auto f = kid::parse (pendingText);
        pendingText = {};
        if (f.ask != kid::IntentFrame::Ask::none && ! f.anyOps())
            say (director.answer (f));
        else
            run (f);
    }
    refreshChips();
    refreshButtons();
    repaint();
}

// One request becomes the mix: the session starts on the first one, the profile's foundation comes from the existing Build My Mix engine, Kid's plan is
// rebuilt from the starting sound and applied in one step that UNDO can take back.
void KidPanel::run (kid::IntentFrame f)
{
    if (f.profile > 0)
        chosenProfile = f.profile;
    const auto before = proc.captureSoundState();
    const auto memoryBefore = director;
    if (! built)
    {
        director.begin (before); // the session's starting sound: every plan is built from here
        manualIds.clear();
    }
    const bool firstMix = ! built && ! f.reset && f.understood();
    if (firstMix && f.profile == 0 && ! f.anyOps() && f.tune == 0 && f.cleanLevel == 0)
        f.buildMix = true; // "analyze and fix it": a whole mix fitted to the recording
    if (f.buildMix || f.profile > 0 || (firstMix && ! f.anyOps()))
    {
        const int prof = f.profile > 0 ? f.profile : chosenProfile;
        const auto& lead = fileReport.ok() && fileReport.tone.status == analysis::VocalAnalysis::Status::ready ? fileReport.tone : lastAnalysis;
        director.setFoundation (kid::buildFoundation (prof, director.start(), proc.factoryDefaultState(), lead, proc.makeRangeLookup())); // relative to the starting sound, like every row
        if (f.buildMix && f.profile == 0 && prof > 0 && ! built) f.profile = prof;
    }
    const auto line = director.respond (f, haveFacts ? &facts : nullptr);
    applyPlan (before, memoryBefore);
    say (line + (manualIds.empty() ? juce::String() : " (I left the " + juce::String ((int) manualIds.size()) + " control" + (manualIds.size() == 1 ? "" : "s") + " you moved by hand alone.)"));
}

void KidPanel::applyPlan (const SoundState& before, const kid::Director& memoryBefore)
{
    const auto target = director.plan (before, manualIds);
    bool changes = false;
    for (auto& e : target.values)
        changes = changes || std::abs (before.get (e.id, e.value) - e.value) > 1.0e-4f;
    if (! changes && ! built)
        return;
    history.push_back ({ before, memoryBefore });
    if (history.size() > 50) history.erase (history.begin());
    if (hooks.beforeApply) hooks.beforeApply (before);
    proc.applyParameterChanges (target); // the plug-in's normal path: host gestures, automation, saved state
    lastApplied = proc.captureSoundState();
    built = true;
    appliedAtMs = juce::Time::getMillisecondCounter();
    if (hooks.status) hooks.status ("Kid updated your mix.");
}

void KidPanel::undoStep()
{
    if (history.empty())
        return;
    auto [state, memory] = history.back();
    history.pop_back();
    proc.applyParameterChanges (state);
    director = memory;
    lastApplied = proc.captureSoundState();
    if (history.empty())
    {
        built = false;
        manualIds.clear();
    }
    say (history.empty() ? "Undone: you're back to your sound before my mix." : "Undone: back one step.");
    refreshChips();
    refreshButtons();
}

// A control moved by hand (or by automation) is the artist's from now on: Kid leaves it alone. A big jump (a preset, A/B) starts a new session instead.
void KidPanel::watchManualMoves()
{
    const auto now = proc.captureSoundState();
    juce::StringArray moved;
    for (auto& e : now.values)
        if (kid::Director::isAllowed (e.id) && std::abs (lastApplied.get (e.id, e.value) - e.value) > 1.0e-4f)
            moved.add (e.id);
    lastApplied = now;
    if (moved.isEmpty() || ! built)
        return;
    if (moved.size() > 12)
    {
        built = false;
        manualIds.clear();
        history.clear();
        say ("Your sound changed a lot (a preset or A/B?). My next mix starts from this sound.");
        refreshChips();
    }
    else
        for (auto& id : moved)
            manualIds.insert (id);
    refreshButtons();
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
    refreshButtons();
    repaint();
}

void KidPanel::fileDone (const analysis::VocalFileReport& r)
{
    fileWaiting = false;
    if (! r.ok())
    {
        say (r.message);
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
    facts.peakDb = r.peakDb;
    facts.rmsDb = t.rmsDb;
    facts.crestDb = t.crestDb;
    for (size_t i = 0; i < facts.bandFraction.size(); ++i)
        facts.bandFraction[i] = t.bandFraction[i];
    facts.keyReliable = r.keyReliable;
    facts.keyTonic = r.keyTonic;
    facts.keyMinor = r.keyMinor;
    facts.noiseKnown = r.noiseFloorKnown;
    facts.noiseFloorDb = r.noiseFloorDb;
    facts.voicedShare = r.voicedShare;
    facts.offNoteCents = r.offNoteCents;
    facts.shapeKnown = r.shape.valid;
    facts.shapeDb = r.shape.shapeDb;
    haveFacts = facts.valid;
    if (! haveFacts)
    {
        say ("I read " + r.fileName + ", but there wasn't enough voice in it to judge the tone.");
        refreshButtons();
        return;
    }
    director.setFacts (facts);
    juce::StringArray heard;
    std::vector<const kid::Finding*> top;
    for (auto& x : director.diagnosis().items)
        if (x.id != "key") top.push_back (&x);
    std::stable_sort (top.begin(), top.end(), [] (const kid::Finding* a, const kid::Finding* b) { return a->severity > b->severity; });
    for (size_t i = 0; i < top.size() && i < 3; ++i) heard.add (top[i]->plain);
    say ("I've got " + r.fileName + " (" + juce::String (r.analysedSeconds, 1) + " s). " + (heard.isEmpty() ? juce::String ("It's in good shape to mix.") : heard.joinIntoString (" "))
         + (r.keyReliable ? " Key: probably " + analysis::keyName (r.keyTonic, r.keyMinor) + "." : juce::String())
         + (built ? " Press UPDATE MY MIX to fit the mix to this take." : " Tell me the sound you want, or press BUILD MY MIX."));
    if (pendingText.isNotEmpty() && phase == Phase::analysing)
        finishRequest();
    refreshButtons();
    repaint();
}

void KidPanel::startRecording()
{
    if (! kid::Recogniser::available() || ! kid::SpeechModel::isInstalled() || recogniser.isBusy())
    {
        say (kid::Recogniser::available() ? "Speech isn't set up yet: press Set up speech (a one-time download), or type." : "This build has no speech. Type, or tap a suggestion.");
        refreshButtons();
        repaint();
        return;
    }
    juce::String err;
    if (! commandMic.start (err))
    {
        say (err.isNotEmpty() ? "I can't hear you: " + err + " You can still type." : "I can't hear you. You can still type.");
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
        phase = Phase::ready;
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
        safe->phase = Phase::ready;
        if (text.isEmpty())
        {
            safe->say (error);
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
        const auto waited = juce::Time::getMillisecondCounter() - analysisStartedMs;
        if (an.completedCount() > analysisStartCount && ! an.isListening())
        {
            lastAnalysis = an.fetch();
            if (! fileReport.ok()) // a loaded file stays the reference for the measurements
            {
                facts = {};
                facts.valid = lastAnalysis.status == analysis::VocalAnalysis::Status::ready;
                facts.activeSeconds = lastAnalysis.activeSeconds;
                facts.sibilanceIndex = lastAnalysis.sibilanceIndex;
                facts.dynamicRangeDb = lastAnalysis.dynamicRangeDb;
                facts.clippedSamples = lastAnalysis.clippedSamples;
                facts.peakDb = lastAnalysis.peakDb;
                facts.rmsDb = lastAnalysis.rmsDb;
                facts.crestDb = lastAnalysis.crestDb;
                for (size_t i = 0; i < facts.bandFraction.size(); ++i)
                    facts.bandFraction[i] = lastAnalysis.bandFraction[i];
                facts.mudHz = value ("clean_on") >= 0.5f ? proc.getCleanApplied().mudHz : 0.0f;
                haveFacts = facts.valid;
                director.setFacts (facts);
            }
            if (lastAnalysis.status != analysis::VocalAnalysis::Status::ready)
                say ("I couldn't hear enough of your vocal, so I'm going on what you told me. Press play (or load the file), then UPDATE MY MIX to fit it to your voice.");
            finishRequest();
        }
        else if (waited > 15000 && an.isListening())
            proc.stopSourceAnalysis(); // finish with whatever was heard; the result arrives on a later tick
        else if (waited > 22000)
        {
            say ("Nothing came through to analyse, so I'm going on what you told me. Press play in your project (or load the file), then UPDATE MY MIX.");
            finishRequest();
        }
    }
    if (tick % 6 == 0)
    {
        watchManualMoves();
        if (originalButton.getToggleState() != (value ("bypass") >= 0.5f))
            refreshButtons();
    }
    repaint (topBar);
    repaint (waveArea);
    if (phase == Phase::analysing || phase == Phase::transcribing || phase == Phase::recording)
        repaint (chatBox);
}

// ---- layout ----
void KidPanel::resized()
{
    const int W = getWidth(), cx0 = 170, cx1 = W - 170;
    topBar = { 0, 0, W, 64 };
    backButton.setBounds (178, 18, 104, 28);
    vocalBox = { 32, 76, W - 64, 116 };
    loadButton.setBounds (vocalBox.getX() + 18, vocalBox.getY() + 18, 132, 32);
    waveArea = { vocalBox.getX() + 168, vocalBox.getY() + 14, vocalBox.getWidth() - 168 - 252, vocalBox.getHeight() - 40 };
    originalButton.setBounds (vocalBox.getRight() - 236, vocalBox.getY() + 18, 108, 32);
    processedButton.setBounds (vocalBox.getRight() - 124, vocalBox.getY() + 18, 108, 32);

    chatBox = { cx0, 208, cx1 - cx0, 312 };
    inputBox = { cx0, 532, cx1 - cx0, 48 };
    mic.setBounds (inputBox.getX(), inputBox.getY(), 48, 48);
    sendButton.setBounds (inputBox.getRight() - 76, inputBox.getY() + 6, 76, 36);
    input.setBounds (inputBox.getX() + 58, inputBox.getY() + 2, inputBox.getWidth() - 58 - 86, 44);

    mainButton.setBounds (cx1 - 210, 594, 210, 46);
    undoButton.setBounds (mainButton.getX() - 82, 600, 72, 34);
    int x = cx0;
    for (auto* c : chips)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (fontOf (13.0f), c->getButtonText()) + 30;
        const bool fits = x + w <= undoButton.getX() - 10;
        c->setBounds (x, 602, w, 30);
        c->setVisible (fits);
        x += w + 8;
    }

    footBox = { 32, 648, W - 64, 24 };
    int ax = footBox.getX() + 74;
    for (auto& a : advanced)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (fontOf (13.0f), a.getButtonText()) + 24;
        a.setBounds (ax, footBox.getY(), w, 24);
        ax += w + 6;
    }
    setupButton.setBounds (footBox.getRight() - 130, footBox.getY(), 130, 24);
}

// ---- painting ----
void KidPanel::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // top bar: the wordmark and Kid's status, nothing else
    g.setFont (fontOf (22.0f, true));
    g.setColour (white);
    g.drawText ("GLITCHKID", 32, 18, 200, 28, juce::Justification::centredLeft, false);
    {
        const auto st = status();
        const auto text = statusText();
        const int w = (int) juce::GlyphArrangement::getStringWidth (fontOf (13.0f, true), text) + 40;
        const juce::Rectangle<float> pill ((float) (getWidth() - 32 - w), 19.0f, (float) w, 26.0f);
        const juce::Colour c = st == Status::ready ? accentSoft : st == Status::listening ? liveCol : accent;
        g.setColour (surface);
        g.fillRoundedRectangle (pill, 13.0f);
        g.setColour (hairline);
        g.drawRoundedRectangle (pill, 13.0f, 1.0f);
        const float pulse = st == Status::ready ? 1.0f : 0.55f + 0.45f * std::sin ((float) tick * 0.25f);
        const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ pill.getX() + 16.0f, pill.getCentreY() });
        g.setColour (c.withAlpha (0.25f * pulse));
        g.fillEllipse (dot.expanded (4.0f));
        g.setColour (c.withAlpha (pulse));
        g.fillEllipse (dot);
        g.setColour (white);
        g.setFont (fontOf (13.0f, true));
        g.drawText (text, pill.withTrimmedLeft (28.0f).toNearestInt(), juce::Justification::centredLeft, false);
    }
    paintVocal (g);
    paintChat (g);

    // the input line
    g.setFont (fontOf (12.0f));
    g.setColour (faint);
    g.drawText ("Advanced", footBox.getX(), footBox.getY(), 70, footBox.getHeight(), juce::Justification::centredLeft, false);
    if (! setupButton.isVisible())
        g.drawText (kid::Recogniser::available() && kid::SpeechModel::isInstalled() ? "Kid and speech run on this computer" : "Kid runs on this computer: no internet needed",
                    footBox.getRight() - 320, footBox.getY(), 320, footBox.getHeight(), juce::Justification::centredRight, false);
}

void KidPanel::paintVocal (juce::Graphics& g)
{
    const auto box = vocalBox.toFloat();
    g.setColour (surface);
    g.fillRoundedRectangle (box, 14.0f);
    g.setColour (hairline);
    g.drawRoundedRectangle (box.reduced (0.5f), 14.0f, 1.0f);

    // under the load button: what the waveform is
    g.setFont (fontOf (12.0f));
    g.setColour (dim);
    const juce::String source = fileWaiting || fileJob.isRunning() ? "Reading: " + juce::String (juce::roundToInt (fileJob.progress() * 100.0f)) + " %"
                              : fileReport.ok() ? fileReport.fileName : juce::String ("Live output");
    g.drawFittedText (source, vocalBox.getX() + 20, vocalBox.getY() + 58, 132, 16, juce::Justification::centredLeft, 1, 0.8f);
    if (fileReport.ok())
        g.drawFittedText (juce::String (fileReport.analysedSeconds, 1) + " s" + (fileReport.keyReliable ? ", " + analysis::keyName (fileReport.keyTonic, fileReport.keyMinor) : juce::String()),
                          vocalBox.getX() + 20, vocalBox.getY() + 76, 132, 16, juce::Justification::centredLeft, 1, 0.8f);

    // the waveform: the loaded file's real peaks, or the live output level (what leaves GlitchKid, newest on the right)
    const auto w = waveArea.toFloat();
    const float mid = w.getCentreY(), half = w.getHeight() * 0.5f - 2.0f;
    if (fileReport.ok() && ! fileReport.overview.empty())
    {
        const int n = (int) fileReport.overview.size();
        g.setColour (accent.withAlpha (0.85f));
        for (int px = 0; px < (int) w.getWidth(); px += 2)
        {
            const float a = fileReport.overview[(size_t) juce::jlimit (0, n - 1, px * n / juce::jmax (1, (int) w.getWidth()))];
            const float h = juce::jmax (0.6f, a * half);
            g.fillRect (w.getX() + (float) px, mid - h, 1.2f, 2.0f * h);
        }
    }
    else
    {
        const int n = (int) env.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = env[(size_t) (n - 1 - i)];
            const float h = juce::jmax (0.6f, a * half);
            g.setColour ((signalPresent ? accent : faint).withAlpha (0.25f + 0.75f * (float) i / (float) n));
            g.fillRect (w.getX() + (float) i * w.getWidth() / (float) n, mid - h, 1.6f, 2.0f * h);
        }
    }
    g.setFont (fontOf (11.5f));
    g.setColour (faint);
    g.drawText (fileReport.ok() ? "Analysed, not played: play your track in the DAW to hear it" : signalPresent ? "Live: what GlitchKid is sending out" : "No signal: press play in your project",
                waveArea.getX(), waveArea.getBottom() + 4, waveArea.getWidth(), 16, juce::Justification::centredLeft, false);
    g.drawText ("Matched loudness", originalButton.getX(), originalButton.getBottom() + 8, processedButton.getRight() - originalButton.getX(), 16, juce::Justification::centred, false);
}

void KidPanel::paintChat (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (chatBox);
    const auto area = chatBox;
    std::vector<Line> shown (lines.begin(), lines.end());
    if (phase == Phase::analysing || phase == Phase::transcribing)
        shown.push_back ({ true, phase == Phase::transcribing ? juce::String ("...") : statusText() + "...", false });
    struct Laid { juce::TextLayout tl; bool kid; float h; };
    std::vector<Laid> laid;
    float total = 0.0f;
    const float kidW = (float) area.getWidth() * 0.86f, youW = (float) area.getWidth() * 0.62f;
    for (auto& l : shown)
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.append (l.text, fontOf (l.kid ? 15.0f : 14.5f), l.kid ? white : juce::Colour (0xffdcd8e4));
        juce::TextLayout tl;
        tl.createLayout (as, l.kid ? kidW - 20.0f : juce::jmin (youW - 28.0f, juce::GlyphArrangement::getStringWidth (fontOf (14.5f), l.text) + 4.0f));
        const float h = tl.getHeight() + (l.kid ? 26.0f : 22.0f);
        laid.push_back ({ std::move (tl), l.kid, h });
        total += h + 14.0f;
    }
    scrollMax = juce::jmax (0.0f, total - (float) area.getHeight());
    scroll = juce::jlimit (0.0f, scrollMax, scroll);
    float y = (float) area.getY() - scroll + juce::jmax (0.0f, (float) area.getHeight() - total); // the newest message sits at the bottom
    for (auto& m : laid)
    {
        if (m.kid)
        {
            g.setColour (accent);
            g.fillEllipse ((float) area.getX(), y + 4.0f, 7.0f, 7.0f);
            g.setFont (fontOf (11.5f, true));
            g.setColour (accentSoft);
            g.drawText ("Kid", area.getX() + 14, (int) y, 60, 15, juce::Justification::centredLeft, false);
            m.tl.draw (g, { (float) area.getX() + 14.0f, y + 20.0f, kidW - 20.0f, m.tl.getHeight() });
        }
        else
        {
            const float w = m.tl.getWidth() + 28.0f;
            const juce::Rectangle<float> bubble ((float) area.getRight() - w, y, w, m.h);
            g.setColour (surfaceHi);
            g.fillRoundedRectangle (bubble, 14.0f);
            m.tl.draw (g, bubble.reduced (14.0f, 11.0f));
        }
        y += m.h + 14.0f;
    }
}

void KidPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (chatBox.contains (e.getPosition()))
    {
        scroll = juce::jlimit (0.0f, scrollMax, scroll - w.deltaY * 120.0f);
        repaint (chatBox);
    }
}
}
