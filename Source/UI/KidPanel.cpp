#include "KidPanel.h"
#include "PlaygroundWidgets.h"

namespace glitchkid::ui
{
namespace
{
const juce::Colour kidPink (0xffe14cbb), kidPinkSoft (0xffffc6ea), shapeCol (0xffcbbcff), spaceCol (0xff8d84ff), finishCol (0xffcfc8da), amber (0xffffb54c),
                   plum (0xff1a0f1b), stageBg (0xff0c0a11);
const char* const stationName[17] = { "Tune", "Formant", "Harmony", "Mutation", "Doubler", "EQ", "Clean", "Comp", "De-ess", "Warmth", "Air", "Stutter", "Echo", "Dimension", "Gain", "Mix", "Limiter" };
const char* const stageName[18] = { "", "Pitch Tune", "Formant", "Harmony", "Mutation", "Doubler", "4-band EQ", "CLEAN corrective", "Compressor", "CLEAN de-esser",
                                    "Saturation", "AIR LAB", "Stutter", "Echo", "Dimension", "Gain", "Mix", "Output Limiter" };
const char* const dimName[kid::numDims][2] = { { "BRIGHTER", "DARKER" }, { "WARMER", "COOLER" }, { "MORE PRESENT", "FURTHER BACK" }, { "HARDER", "SOFTER" },
                                               { "WIDER", "NARROWER" }, { "MORE SPACE", "CLOSER" }, { "CLEANER", "CLEANER" }, { "STRANGER", "STRANGER" } };
juce::Colour worldOf (int stage)
{
    switch (stage)
    {
        case 2: case 3: case 4: case 12: return kidPink;
        case 5: case 13: case 14: return spaceCol;
        case 15: case 16: case 17: return finishCol;
        default: return shapeCol;
    }
}
constexpr int maxLines = 60;
}

// ---- the push-to-talk button ----
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
    say ("I'm KID. Hold the mic, or type, and tell me what you hear in your head: \"warmer\", \"wider but keep me upfront\", or just \"I don't know, improve it\". "
         "I show you every change before anything happens.");

    mic.setTitle ("Talk to KID");
    mic.setDescription ("Push to talk: hold to speak, or tap once to start and again to stop.");
    mic.setWantsKeyboardFocus (true);
    mic.onPress = [this] { startRecording(); };
    mic.onRelease = [this] { stopRecording (false); };
    addAndMakeVisible (mic);

    input.setFont (fontOf (14.0f));
    input.setTextToShowWhenEmpty ("...or type to KID", textDim);
    input.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b090e));
    input.setColour (juce::TextEditor::outlineColourId, border);
    input.setColour (juce::TextEditor::textColourId, textBright);
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
            safe->say (ok ? juce::String ("Speech is ready. Hold the mic and talk to me.") : msg);
            safe->refreshButtons();
            safe->repaint();
        });
        refreshButtons();
        repaint();
    };
    applyButton.onClick = [this] { apply(); };
    cancelButton.onClick = [this] { cancelTake(); };
    analyseButton.onClick = [this] { pendingText = {}; analysisStartCount = proc.getAnalyzer().completedCount(); proc.startSourceAnalysis(); analysisStartedMs = juce::Time::getMillisecondCounter(); phase = Phase::analysing; refreshButtons(); repaint(); };
    const char* tabNames[3] = { "TAKE", "VOCAL READ", "LIMITS" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText (tabNames[i]);
        tabButtons[i].setClickingTogglesState (false);
        tabButtons[i].onClick = [this, i] { tab = (Tab) i; takeScroll = 0.0f; refreshButtons(); repaint(); };
        addAndMakeVisible (tabButtons[i]);
    }
    for (auto* b : { &sendButton, &setupButton, &applyButton, &cancelButton, &analyseButton })
        addAndMakeVisible (b);
    sendButton.setTooltip ("Send what you typed to KID.");
    applyButton.setTooltip ("Apply the ticked changes. UNDO (top right) brings your previous sound back.");
    cancelButton.setTooltip ("Drop KID's take. Your sound has not changed.");
    refreshChips();
    refreshButtons();
}

KidPanel::~KidPanel()
{
    stopTimer();
    commandMic.stop();
}

void KidPanel::visibilityChanged()
{
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

juce::String KidPanel::lastKidLineForTest() const
{
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (it->kid) return it->text;
    return {};
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

juce::StringArray KidPanel::suggestions() const
{
    const bool spoke = std::any_of (lines.begin(), lines.end(), [] (const Line& l) { return ! l.kid; });
    const auto& rows = director.proposal();
    const auto& d = director.direction();
    if (! spoke)
        return { "Make my vocal wider and more atmospheric", "Clean up the muddy frequencies", "Make it darker and more aggressive",
                 "Give me a natural melodic rap vocal", "Make my ad-libs sound distant", "I don't know what it needs. Improve it" };
    if (! rows.empty() && d[kid::width] > 0 && director.kept().count ("front") == 0) return { "Too much. Make it subtle", "Keep the vocal upfront", "More" };
    if (! rows.empty() && d[kid::bright] < 0 && director.kept().count ("presence") == 0) return { "A little less", "Keep the presence", "Make it warmer" };
    if (! rows.empty()) return { "A little less", "More", "Keep the vocal upfront", "Start over" };
    return { "Make it warmer", "Make it wider but keep it upfront", "Make it experimental", "I don't know what it needs. Improve it" };
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
    applyButton.setEnabled (director.selectedCount() > 0 && ! busy);
    applyButton.setButtonText ("APPLY " + juce::String (director.selectedCount()));
    cancelButton.setEnabled (! director.proposal().empty() && ! busy);
    for (int i = 0; i < 3; ++i) tabButtons[i].setToggleState ((int) tab == i, juce::dontSendNotification);
    analyseButton.setVisible (tab == Tab::read && ! (lastAnalysis.status == analysis::VocalAnalysis::Status::ready) && phase != Phase::analysing);
    mic.repaint();
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
        say (director.respond (kid::parse (pendingText), haveFacts ? &facts : nullptr));
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
    say ("Applied " + juce::String (n) + (n == 1 ? " change" : " changes") + ". UNDO (top right) brings your previous sound back. What next?");
    if (hooks.status) hooks.status ("KID applied " + juce::String (n) + (n == 1 ? " change." : " changes."));
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
        if (an.completedCount() > analysisStartCount && ! an.isListening())
        {
            lastAnalysis = an.fetch();
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
            if (! facts.valid && pendingText.isEmpty())
                say ("I couldn't hear enough of your vocal. Press play in your project and try again.");
            finishRequest();
        }
        else if (juce::Time::getMillisecondCounter() - analysisStartedMs > 25000 && an.isListening())
            proc.stopSourceAnalysis(); // finish with whatever was heard; the result arrives on a later tick
        else if (juce::Time::getMillisecondCounter() - analysisStartedMs > 40000)
        {
            say ("Nothing came through to analyse. Press play in your project, then ask again.");
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
            repaint();
        }
    }
    repaint (stageBox);
    repaint (cmdBox);
    repaint (pathBox);
    if (phase == Phase::downloading || phase == Phase::analysing)
        repaint (takeBox);
}

// ---- layout ----
void KidPanel::resized()
{
    stageBox = { 12, 8, 568, 300 };
    voiceBox = stageBox.withWidth (200);
    scriptBox = stageBox.withTrimmedLeft (200).reduced (16, 12);
    takeBox = { 592, 8, 356, 300 };
    auto t = takeBox;
    auto head = t.removeFromTop (38).reduced (10, 6);
    auto foot = t.removeFromBottom (46).reduced (10, 8);
    takeBody = t.reduced (12, 6);
    for (int i = 2; i >= 0; --i)
        tabButtons[i].setBounds (head.removeFromRight (i == 1 ? 92 : 62));
    applyButton.setBounds (foot.removeFromRight (100));
    foot.removeFromRight (8);
    cancelButton.setBounds (foot.removeFromRight (84));
    analyseButton.setBounds (takeBody.getX(), takeBody.getY() + 64, 170, 28);

    cmdBox = { 12, 318, 936, 104 };
    mic.setBounds (cmdBox.getX() + 14, cmdBox.getCentreY() - 34, 68, 68);
    waveBox = { cmdBox.getX() + 100, cmdBox.getY() + 40, 400, 30 };
    auto right = juce::Rectangle<int> (cmdBox.getRight() - 420, cmdBox.getY() + 10, 408, cmdBox.getHeight() - 20);
    auto row = right.removeFromTop (30);
    if (setupButton.isVisible() || (kid::Recogniser::available() && ! kid::SpeechModel::isInstalled()))
    {
        setupButton.setBounds (row.removeFromRight (128));
        row.removeFromRight (6);
    }
    sendButton.setBounds (row.removeFromRight (64));
    row.removeFromRight (6);
    input.setBounds (row);
    right.removeFromTop (6);
    // the suggestions wrap over two rows
    int x = right.getX(), y = right.getY();
    for (auto* c : chips)
    {
        const int w = juce::jmin (right.getWidth(), (int) juce::GlyphArrangement::getStringWidth (fontOf (12.5f), c->getButtonText()) + 22);
        if (x + w > right.getRight()) { x = right.getX(); y += 26; }
        c->setBounds (x, y, w, 22);
        c->setVisible (y + 22 <= cmdBox.getBottom() - 2);
        x += w + 5;
    }
    pathBox = { 12, 432, 936, 104 };
    const int n = 17, gap = 4, stx = pathBox.getX() + 14, stw = (pathBox.getWidth() - 28 - 14 - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
        stationHits[(size_t) i] = { stx + i * (stw + gap), pathBox.getY() + 40, stw, 50 };
}

// ---- painting ----
void KidPanel::paint (juce::Graphics& g)
{
    // stage
    g.setColour (stageBg);
    g.fillRoundedRectangle (stageBox.toFloat(), 16.0f);
    g.setColour (juce::Colour (0xff241f2e));
    g.drawRoundedRectangle (stageBox.toFloat().reduced (0.5f), 16.0f, 1.0f);
    g.setColour (juce::Colour (0xff1f1b28));
    g.fillRect (voiceBox.getRight(), stageBox.getY() + 1, 1, stageBox.getHeight() - 2);
    paintVoice (g, voiceBox.toFloat());
    paintScript (g, scriptBox);

    // KID's take
    g.setGradientFill (juce::ColourGradient (plum, 0.0f, (float) takeBox.getY(), juce::Colour (0xff100c13), 0.0f, (float) takeBox.getY() + 140.0f, false));
    g.fillRoundedRectangle (takeBox.toFloat(), 16.0f);
    g.setColour (juce::Colour (0xff4a2140));
    g.drawRoundedRectangle (takeBox.toFloat().reduced (0.5f), 16.0f, 1.0f);
    g.setColour (kidPinkSoft);
    g.setFont (fontOf (14.0f, true));
    g.drawText ("KID'S TAKE", takeBox.getX() + 14, takeBox.getY() + 8, 130, 22, juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff2e1a2c));
    g.fillRect (takeBox.getX() + 1, takeBox.getY() + 38, takeBox.getWidth() - 2, 1);
    g.fillRect (takeBox.getX() + 1, takeBox.getBottom() - 46, takeBox.getWidth() - 2, 1);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (takeBody);
        rowHits.clear();
        if (tab == Tab::take) paintTake (g, takeBody);
        else if (tab == Tab::read) paintRead (g, takeBody);
        else paintLimits (g, takeBody);
    }
    paintCommand (g);
    paintPath (g);
}

void KidPanel::paintVoice (juce::Graphics& g, juce::Rectangle<float> box)
{
    g.setColour (textBright);
    g.setFont (fontOf (11.5f, true));
    g.drawText ("YOUR VOICE", box.getX() + 12, box.getY() + 10, 160, 14, juce::Justification::centredLeft, false);
    const float cx = box.getCentreX(), cy = box.getCentreY() - 6.0f, R0 = box.getWidth() * 0.2f, A = box.getWidth() * 0.18f;
    const bool dry = value ("bypass") >= 0.5f;
    if (! signalPresent)
    {
        g.setColour (juce::Colour (0xff4a4458));
        g.drawEllipse (cx - R0, cy - R0, 2 * R0, 2 * R0, 1.5f);
        g.setColour (textDim);
        g.setFont (fontOf (13.0f, true));
        g.drawText ("No signal", juce::Rectangle<float> (cx - 60, cy - 10, 120, 20), juce::Justification::centred, false);
        g.setFont (fontOf (11.5f));
        g.drawFittedText ("Press play in your project", juce::Rectangle<int> ((int) box.getX() + 10, (int) box.getBottom() - 34, (int) box.getWidth() - 20, 16), juce::Justification::centred, 1);
        return;
    }
    constexpr int N = 90;
    const float ratio = dry ? 1.0f : value ("comp_ratio"), sat = dry ? 0.0f : value ("sat_drive") / 18.0f;
    const float fsc = dry ? 1.0f : 1.0f - value ("formant_shift") / 12.0f * 0.2f;
    auto amp = [&] (int i) { float s = 0; int c = 0; for (int j = i - 2; j <= i + 2; ++j) if (j >= 0 && j < (int) env.size()) { s += env[(size_t) j]; ++c; } return c ? s / (float) c : 0.0f; };
    auto contour = [&] (float scale, float extra, float stretch, float dx)
    {
        juce::Path p;
        for (int k = -N; k <= N; ++k)
        {
            const int i = std::abs (k);
            const float th = -juce::MathConstants<float>::halfPi + (float) k / (float) N * juce::MathConstants<float>::pi;
            float a = std::pow (amp (i), 1.0f / (1.0f + (ratio - 1.0f) * 0.16f));
            a += sat * 0.12f * std::sin ((float) i * 2.6f) * a;
            const float r = (R0 * fsc + A * a + extra) * scale;
            const juce::Point<float> pt (cx + dx + std::cos (th) * r * stretch, cy + std::sin (th) * r);
            if (k == -N) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    };
    if (! dry)
    {
        if (value ("dim_on") >= 0.5f)
        {
            const float rr = R0 + A + value ("dim_width") * 0.25f;
            juce::ColourGradient haze (spaceCol.withAlpha (0.22f * value ("dim_mix") / 100.0f), cx, cy, spaceCol.withAlpha (0.0f), cx + rr + 20, cy, true);
            g.setGradientFill (haze);
            g.fillEllipse (cx - rr - 20, cy - rr - 14, 2 * (rr + 20), 2 * (rr + 14));
        }
        if (value ("echo_on") >= 0.5f)
        {
            const int n = juce::jmax (1, juce::roundToInt (1.0f + value ("echo_fb") / 15.0f));
            for (int k2 = 1; k2 <= n; ++k2)
            {
                const float a = std::pow (value ("echo_fb") / 100.0f, (float) (k2 - 1)) * value ("echo_mix") / 100.0f * 0.9f + 0.05f;
                g.setColour (spaceCol.withAlpha (juce::jmin (0.8f, a)));
                g.strokePath (contour (1.0f, 9.0f * (float) k2, 1.0f + value ("echo_width") / 100.0f * 0.2f * (float) k2 / (float) n, 0.0f), juce::PathStrokeType (1.1f));
            }
        }
        if (value ("dbl_on") >= 0.5f)
            for (float sd : { -1.0f, 1.0f })
            {
                g.setColour (juce::Colour (0xffb5aeff).withAlpha (0.12f + value ("dbl_amount") / 100.0f * 0.35f));
                g.strokePath (contour (1.0f, 0.0f, 1.0f, sd * (3.0f + value ("dbl_width") / 100.0f * 18.0f)), juce::PathStrokeType (1.3f));
            }
    }
    const auto body = contour (1.0f, 0.0f, 1.0f, 0.0f);
    const float bright = dry ? 0.0f : juce::jlimit (-1.0f, 1.0f, (value ("eq_hi_gain") + value ("eq_b2_gain") * 0.5f) / 8.0f);
    g.setGradientFill (juce::ColourGradient (dry ? juce::Colour (0x29c8c4ce) : juce::Colour::fromFloatRGBA ((203 + 30 * bright) / 255.0f, (188 + 40 * bright) / 255.0f, 1.0f, 0.2f + 0.12f * bright),
                                             cx, cy - R0 - A, dry ? juce::Colour (0x0f787480) : juce::Colour (0x1a5a28aa), cx, cy + R0 + A, false));
    g.fillPath (body);
    g.setColour (dry ? juce::Colour (0xffa19bab) : juce::Colour (0xffece4d6));
    g.strokePath (body, juce::PathStrokeType (dry ? 1.5f : 2.0f));
    for (int i = 0; i < 3; ++i)
    {
        g.setColour ((dry ? juce::Colour (0xffc8c4ce) : juce::Colour (0xffece4d6)).withAlpha (0.12f + 0.04f * (float) i));
        g.strokePath (contour (0.78f - 0.19f * (float) i, 0.0f, 1.0f, 0.0f), juce::PathStrokeType (1.0f));
    }
    if (! dry)
    {
        if (value ("harm_on") >= 0.5f)
        {
            static const int semi[9] = { -12, -7, -4, -3, 3, 4, 5, 7, 12 };
            juce::Path dashed;
            const float dl[2] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, contour (1.0f + (float) semi[juce::jlimit (0, 8, (int) value ("harm_interval"))] / 12.0f * 0.3f, 0, 1, 0), dl, 2);
            g.setColour (kidPink.withAlpha (0.4f + (value ("harm_level") + 24.0f) / 24.0f * 0.5f));
            g.fillPath (dashed);
        }
        if (value ("mut_on") >= 0.5f)
        {
            g.setColour (kidPink.withAlpha (0.4f + 0.5f * value ("mut_blend") / 100.0f));
            for (int k = -N; k <= N; k += 6)
                if ((k * 7 + tick / 4) % 3 != 0)
                {
                    const float th = -juce::MathConstants<float>::halfPi + (float) k / (float) N * juce::MathConstants<float>::pi;
                    const float r = R0 * fsc + A * amp (std::abs (k)) + (6.0f + 16.0f * value ("mut_amount") / 100.0f) * (((k * 13 + tick / 3) % 7) / 7.0f - 0.3f);
                    juce::Path seg;
                    seg.addCentredArc (cx, cy, r, r, 0.0f, th + juce::MathConstants<float>::halfPi - 0.05f, th + juce::MathConstants<float>::halfPi + 0.05f, true);
                    g.strokePath (seg, juce::PathStrokeType (2.2f));
                }
        }
        if (value ("stutter_on") >= 0.5f)
        {
            const int slices = 4 << juce::jlimit (0, 3, (int) value ("stutter_rate"));
            g.setColour (kidPinkSoft.withAlpha (0.3f + 0.6f * value ("stutter_mix") / 100.0f));
            for (int q = 0; q < slices; q += 2)
            {
                juce::Path a;
                const float a0 = (float) q / (float) slices * juce::MathConstants<float>::twoPi;
                a.addCentredArc (cx, cy, R0 * fsc - 7, R0 * fsc - 7, 0.0f, a0, a0 + juce::MathConstants<float>::pi / (float) slices * 0.9f, true);
                g.strokePath (a, juce::PathStrokeType (2.6f));
            }
        }
        if (value ("lim_on") >= 0.5f)
        {
            const float rc = R0 + A * juce::Decibels::decibelsToGain (value ("lim_ceiling")) + 4.0f;
            juce::Path c, dashed;
            c.addEllipse (cx - rc, cy - rc, 2 * rc, 2 * rc);
            const float dl[2] = { 2.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, c, dl, 2);
            g.setColour (finishCol.withAlpha (0.45f));
            g.fillPath (dashed);
        }
    }
    g.setFont (fontOf (11.5f));
    g.setColour (textDim);
    int active = 0;
    for (int s = 1; s <= 17; ++s) active += stageOn (s) ? 1 : 0;
    g.drawFittedText (dry ? juce::String ("DRY: the untouched voice") : "PROCESSED: " + juce::String (active) + " of 17 stages active",
                      juce::Rectangle<int> ((int) box.getX() + 10, (int) box.getBottom() - 24, (int) box.getWidth() - 20, 16), juce::Justification::centredLeft, 1);
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
        as.append (l.text, l.kid ? fontOf (15.0f, ! thinking) : fontOf (19.0f), l.kid ? (thinking ? juce::Colour (0xffb9a8c8) : kidPinkSoft) : textBright);
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
                                             { "\"experimental\"", "LAB: Mutation #4 or Stutter #12" }, { "never", "Pitch Tune, Gain, Mix, Limiter, Bypass" } };
        for (auto& m : map)
        {
            g.setFont (fontOf (12.5f, true));
            g.setColour (kidPinkSoft);
            g.drawText (m[0], area.getX(), (int) y, 104, 16, juce::Justification::centredLeft, false);
            g.setFont (fontOf (12.5f));
            g.setColour (textDim);
            g.drawText (m[1], area.getX() + 106, (int) y, area.getWidth() - 106, 16, juce::Justification::centredLeft, true);
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
        g.drawText (name, area.getX() + 22, (int) y, area.getWidth() - 22 - 150, 18, juce::Justification::centredLeft, true);
        g.setFont (fontOf (12.5f));
        g.setColour (textDim);
        g.drawText (show (r.from) + "  >", area.getRight() - 150, (int) y, 76, 18, juce::Justification::centredRight, true);
        g.setFont (fontOf (12.5f, true));
        g.setColour (r.selected ? textBright : textDim);
        g.drawText (show (r.to), area.getRight() - 72, (int) y, 72, 18, juce::Justification::centredRight, true);
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

void KidPanel::paintRead (juce::Graphics& g, juce::Rectangle<int> area)
{
    float y = (float) area.getY() - takeScroll;
    auto row = [&] (const juce::String& label, const juce::String& v, const juce::String& source, bool measured)
    {
        g.setFont (fontOf (12.5f));
        g.setColour (juce::Colour (0xffcdbfd2));
        g.drawText (label, area.getX(), (int) y, area.getWidth() - 150, 17, juce::Justification::centredLeft, true);
        g.setColour (measured ? textBright : textDim);
        g.drawText (v, area.getRight() - 150, (int) y, 150, 17, juce::Justification::centredRight, true);
        y += 17;
        g.setColour (measured ? juce::Colour (0xff62e0a6) : textDim);
        g.drawText (source, area.getX(), (int) y, area.getWidth(), 15, juce::Justification::centredLeft, true);
        y += 21;
    };
    const auto& a = lastAnalysis;
    if (a.status != analysis::VocalAnalysis::Status::ready)
    {
        g.setFont (fontOf (13.0f));
        g.setColour (textDim);
        g.drawFittedText (phase == Phase::analysing ? phaseHint()
                                                    : "KID listens to your vocal while your project plays (2-8 s of singing or rapping), then shows what it measured.",
                          juce::Rectangle<int> (area.getX(), (int) y, area.getWidth(), 56), juce::Justification::topLeft, 3);
        return;
    }
    row ("Listened to", juce::String (a.activeSeconds, 1) + " s of " + juce::String (a.totalSeconds, 1) + " s", "measured", true);
    row ("Input peak", juce::String (a.peakDb, 1) + " dBFS", "measured", true);
    row ("Clipped / near-clipped samples", juce::String (a.clippedSamples) + " / " + juce::String (a.nearClippedSamples), "measured", true);
    row ("Level (RMS of the sung parts)", juce::String (a.rmsDb, 1) + " dBFS", "measured", true);
    row ("Crest factor", juce::String (a.crestDb, 1) + " dB", "measured", true);
    row ("Dynamic range (P90 - P10)", juce::String (a.dynamicRangeDb, 1) + " dB", "measured", true);
    static const char* const bandNames[5] = { "< 120 Hz", "120-500 Hz", "500 Hz-2 kHz", "2-5 kHz", "> 5 kHz" };
    for (int i = 0; i < 5; ++i)
        row (juce::String ("Energy ") + bandNames[i], juce::String (juce::roundToInt (a.bandFraction[(size_t) i] * 100.0f)) + " %", "measured", true);
    row ("Sibilance index (rough)", juce::String (a.sibilanceIndex, 2), "measured", true);
    if (value ("tune_on") >= 0.5f && proc.getTuneDetectedMidi() > 0.0f)
        row ("Pitch (now)", juce::MidiMessage::getMidiNoteName (juce::roundToInt (proc.getTuneDetectedMidi()), true, true, 3), "Pitch Tune's live read-out", true);
    else
        row ("Pitch", "-", "only while Pitch Tune is on and you sing", false);
    row ("Pitch range and stability", "-", "not measured by this version", false);
    row ("Stereo width / correlation", "-", "not measured: the analyser is mono", false);
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
    row ("Never touched", "Pitch Tune (5), Gain, Mix, Output Limiter (4), Bypass");
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

void KidPanel::paintCommand (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff150f1d), (float) cmdBox.getX(), 0.0f, juce::Colour (0xff100d15), (float) cmdBox.getRight(), 0.0f, false));
    g.fillRoundedRectangle (cmdBox.toFloat(), 16.0f);
    g.setColour (juce::Colour (0xff3b2a4a));
    g.drawRoundedRectangle (cmdBox.toFloat().reduced (0.5f), 16.0f, 1.0f);
    g.setFont (fontOf (14.0f, true));
    g.setColour (phase == Phase::recording ? kidPinkSoft : textBright);
    g.drawText (phaseLabel(), waveBox.getX(), cmdBox.getY() + 12, 400, 20, juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.5f));
    g.setColour (textDim);
    g.drawFittedText (phaseHint(), juce::Rectangle<int> (waveBox.getX(), waveBox.getBottom() + 4, waveBox.getWidth(), 28), juce::Justification::topLeft, 2);
    // the waveform: its form follows the state
    const auto w = waveBox.toFloat();
    const float mid = w.getCentreY();
    if (phase == Phase::recording)
    {
        juce::ColourGradient grad (juce::Colour (0xff8d84ff), w.getX(), 0, juce::Colour (0xfff6eefa), w.getRight(), 0, false);
        grad.addColour (0.55, kidPink);
        g.setGradientFill (grad);
        const int n = (int) micLevels.size();
        for (int i = 0; i < n; ++i)
        {
            const float lv = juce::jmin (1.0f, micLevels[(size_t) (n - 1 - i)] * 3.0f);
            const float a = juce::jmax (1.5f, lv * w.getHeight() * 0.48f);
            g.fillRect (w.getX() + (float) i / (float) (n - 1) * (w.getWidth() - 3.0f), mid - a, 2.6f, 2 * a);
        }
    }
    else if (phase == Phase::transcribing || phase == Phase::downloading)
    {
        for (int i = 0; i < 28; ++i)
        {
            const float x = w.getX() + (float) i / 27.0f * w.getWidth();
            const float y = mid + std::sin ((float) i * 0.55f - (float) tick * 0.2f) * w.getHeight() * 0.18f;
            g.setColour (kidPink.withAlpha (0.3f + 0.7f * (std::sin ((float) i * 0.5f - (float) tick * 0.17f) + 1.0f) / 2.0f));
            g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        }
        if (phase == Phase::downloading)
        {
            g.setColour (juce::Colour (0xff2a2336));
            g.fillRect (w.getX(), w.getBottom() + 2, w.getWidth(), 3.0f);
            g.setColour (kidPink);
            g.fillRect (w.getX(), w.getBottom() + 2, w.getWidth() * downloader.progress(), 3.0f);
        }
    }
    else if (phase == Phase::analysing)
    {
        const float prog = juce::jlimit (0.0f, 1.0f, proc.getAnalyzer().liveActiveSeconds() / 8.0f);
        for (float x = w.getX(); x < w.getRight(); x += 2.0f)
        {
            const float a = (0.25f + 0.75f * std::abs (std::sin (x * 0.045f) * std::sin (x * 0.013f + 1.0f))) * w.getHeight() * 0.4f;
            g.setColour (x - w.getX() < prog * w.getWidth() ? juce::Colour (0xffb5aeff) : juce::Colour (0xff3a3359));
            g.fillRect (x, mid - a, 1.2f, 2 * a);
        }
    }
    else
    {
        juce::Path p;
        for (float x = 0; x <= w.getWidth(); x += 2.0f)
        {
            const float y = mid + (phase == Phase::review ? std::sin (x * 0.05f + (float) tick * 0.07f) * std::sin (x * 0.011f) * w.getHeight() * 0.32f
                                                         : std::sin (x * 0.06f + (float) tick * 0.05f) * std::sin (x * 0.009f) * 2.5f);
            if (x < 1.0f) p.startNewSubPath (w.getX(), y); else p.lineTo (w.getX() + x, y);
        }
        g.setColour (phase == Phase::review ? kidPinkSoft.withAlpha (0.8f) : juce::Colour (0xff4a4458));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }
}

bool KidPanel::stageOn (int s) const
{
    switch (s)
    {
        case 1: return value ("tune_on") >= 0.5f;
        case 2: return std::abs (value ("formant_shift")) > 1e-4f;
        case 3: return value ("harm_on") >= 0.5f;
        case 4: return value ("mut_on") >= 0.5f;
        case 5: return value ("dbl_on") >= 0.5f;
        case 6: return std::abs (value ("eq_lo_gain")) + std::abs (value ("eq_b1_gain")) + std::abs (value ("eq_b2_gain")) + std::abs (value ("eq_hi_gain")) > 1e-4f;
        case 7: return value ("clean_on") >= 0.5f;
        case 8: return value ("comp_ratio") > 1.001f;
        case 9: return value ("clean_on") >= 0.5f && value ("clean_ess") > 0.0f;
        case 10: return value ("sat_drive") > 0.0f;
        case 11: return value ("air_on") >= 0.5f;
        case 12: return value ("stutter_on") >= 0.5f;
        case 13: return value ("echo_on") >= 0.5f;
        case 14: return value ("dim_on") >= 0.5f;
        case 15: return std::abs (value ("gain")) > 1e-4f;
        case 16: return value ("mix") < 99.99f;
        case 17: return value ("lim_on") >= 0.5f;
        default: return false;
    }
}

std::set<int> KidPanel::kidStages() const
{
    std::set<int> s;
    for (auto& r : director.proposal()) s.insert (kid::Director::stageOf (r.id));
    return s;
}

void KidPanel::paintPath (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0e0c12));
    g.fillRoundedRectangle (pathBox.toFloat(), 16.0f);
    g.setColour (juce::Colour (0xff241f2e));
    g.drawRoundedRectangle (pathBox.toFloat().reduced (0.5f), 16.0f, 1.0f);
    g.setFont (fontOf (12.5f, true));
    g.setColour (textBright);
    g.drawText ("THE PATH", pathBox.getX() + 14, pathBox.getY() + 10, 90, 16, juce::Justification::centredLeft, false);
    g.setFont (fontOf (12.5f));
    g.setColour (textDim);
    g.drawText ("Fixed order, left to right. A pink glow: KID wants to change that stage. Click a stage for its controls.", pathBox.getX() + 104, pathBox.getY() + 10, 640, 16,
                juce::Justification::centredLeft, true);
    const auto ks = kidStages();
    const auto& first = stationHits[0];
    g.setColour (juce::Colour (0xff3a3546));
    g.fillRect ((float) first.getX(), (float) first.getY() + 13.0f, (float) (stationHits[16].getRight() - first.getX()) + 6.0f, 2.0f);
    juce::Path arrow;
    arrow.addTriangle ((float) stationHits[16].getRight() + 6.0f, (float) first.getY() + 8.0f, (float) stationHits[16].getRight() + 14.0f, (float) first.getY() + 14.0f,
                       (float) stationHits[16].getRight() + 6.0f, (float) first.getY() + 20.0f);
    g.fillPath (arrow);
    for (int i = 0; i < 17; ++i)
    {
        const auto r = stationHits[(size_t) i];
        const auto box = r.withHeight (28).toFloat();
        const bool on = stageOn (i + 1), k = ks.count (i + 1) > 0;
        const auto col = worldOf (i + 1);
        if (k)
        {
            g.setColour (kidPink.withAlpha (0.35f));
            g.fillRoundedRectangle (box.expanded (3.0f), 10.0f);
        }
        g.setColour (on ? juce::Colour (0xff1d1a26) : juce::Colour (0xff14121a));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (k ? kidPink : on ? col : juce::Colour (0xff2f2b3a));
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, k ? 2.0f : 1.0f);
        g.setColour (col.withAlpha (on ? 1.0f : 0.45f));
        g.fillRect (box.getX() + 7.0f, box.getBottom() - 3.0f, box.getWidth() - 14.0f, 3.0f);
        g.setFont (fontOf (13.0f, true));
        g.setColour (on || k ? textBright : juce::Colour (0xff7a7487));
        g.drawText (juce::String (i + 1), box.toNearestInt(), juce::Justification::centred, false);
        g.setFont (fontOf (11.5f));
        g.setColour (k ? kidPinkSoft : on ? textBright : juce::Colour (0xff7a7487));
        g.drawFittedText (stationName[i], juce::Rectangle<int> (r.getX() - 4, r.getY() + 32, r.getWidth() + 8, 14), juce::Justification::centred, 1, 0.85f);
    }
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
    for (int i = 0; i < 17; ++i)
        if (stationHits[(size_t) i].contains (p) && hooks.openStage)
        {
            hooks.openStage (i + 1);
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
