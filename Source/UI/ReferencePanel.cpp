#include "ReferencePanel.h"

namespace glitchkid::ui
{
namespace
{
constexpr float maxVocalSeconds = 20.0f, minVocalActiveSeconds = 4.0f;
using Profile = analysis::SpectralProfile;

juce::String confidenceText (automode::Confidence c)
{
    switch (c)
    {
        case automode::Confidence::low: return "Confidence: low";
        case automode::Confidence::medium: return "Confidence: medium (a full mix is never rated higher)";
        default: return "Confidence: high";
    }
}

juce::String bandName (const juce::String& id)
{
    if (id == "eq_b1_gain") return "Body bell";
    if (id == "eq_b2_gain") return "Presence bell";
    if (id == "eq_hi_gain") return "High shelf";
    return id;
}
}

// ---- ToneDiffView ----------------------------------------------------------------------------------------------------------------------

void ToneDiffView::set (const std::array<float, Profile::numBands>& d, int b, bool v)
{
    if (v != valid || b != bands || d != diff)
    {
        diff = d;
        bands = b;
        valid = v;
        repaint();
    }
}

void ToneDiffView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0d11));
    g.fillPath (chamferPath (r, 6.0f));
    g.setColour (border);
    g.strokePath (chamferPath (r.reduced (0.5f), 6.0f), juce::PathStrokeType (1.0f));
    auto area = r.reduced (10.0f, 8.0f);
    auto labels = area.removeFromBottom (14.0f);
    if (! valid || bands < 2)
    {
        g.setColour (textDim);
        g.setFont (fontOf (11.5f));
        g.drawText ("Analyse a reference and your vocal to see the difference.", getLocalBounds(), juce::Justification::centred, false);
        return;
    }
    const float step = area.getWidth() / (float) Profile::numBands, mid = area.getCentreY(), half = area.getHeight() * 0.5f;
    g.setColour (border);
    g.fillRect (area.getX(), mid, area.getWidth(), 1.0f);
    for (int b = 0; b < bands; ++b)
    {
        const float v = juce::jlimit (-6.0f, 6.0f, diff[(size_t) b]) / 6.0f, h = std::abs (v) * half;
        g.setColour ((v >= 0.0f ? violet : pink).withAlpha (0.9f));
        g.fillRect (area.getX() + (float) b * step + 2.0f, v >= 0.0f ? mid - h : mid, juce::jmax (2.0f, step - 4.0f), juce::jmax (1.0f, h));
    }
    g.setColour (textDim);
    g.setFont (fontOf (10.5f));
    for (int b : { 0, 7, 14, 18 })
        if (b < bands)
        {
            const float hz = Profile::bandCentreHz (b);
            g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k" : juce::String ((int) hz), juce::Rectangle<float> (area.getX() + (float) b * step - 14.0f, labels.getY(), step + 28.0f, labels.getHeight()), juce::Justification::centred, false);
        }
}

// ---- ReferencePanel --------------------------------------------------------------------------------------------------------------------

ReferencePanel::ReferencePanel (GlitchKidProcessor& p, Hooks h) : processor (p), hooks (std::move (h))
{
    auto text = [this] (juce::Label& l, float size, bool bold, juce::Colour c, juce::Justification j = juce::Justification::topLeft)
    {
        l.setFont (fontOf (size, bold));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (j);
        l.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (l);
    };
    auto button = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonOnColourId, violet);
        b.setColour (juce::TextButton::textColourOffId, textBright);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b.setTooltip (tip);
        addAndMakeVisible (b);
    };
    for (auto* c : std::initializer_list<juce::Component*> { &title, &step1, &step2, &step3 })
        addAndMakeVisible (c);
    title.setColour (textBright);
    text (notice, 11.5f, false, textDim);
    notice.setText ("A finished song is a full mix: vocal, instruments and mastering. Reference Match compares its overall tone, 200 Hz to 12 kHz, with your vocal and proposes small "
                    "changes to three EQ gains (body, presence, high shelf). It is a hint toward that tone, not an exact vocal match. Only use audio you have the right to use.", juce::dontSendNotification);
    text (fileName, 12.0f, true, textBright, juce::Justification::centredLeft);
    fileName.setText ("No file chosen", juce::dontSendNotification);
    text (refStatus, 11.5f, false, textDim);
    text (vocalStatus, 11.5f, false, textDim);
    vocalStatus.setText ("Play your vocal through the plug-in, then press Analyze my vocal.", juce::dontSendNotification);
    text (confidence, 12.5f, true, textBright, juce::Justification::centredLeft);
    text (confidenceWhy, 11.5f, false, textDim);
    text (intensityLabel, 12.0f, true, textBright, juce::Justification::centredLeft);
    intensityLabel.setText ("Match intensity", juce::dontSendNotification);
    text (resultStatus, 11.5f, false, textBright);
    text (diffCaption, 11.0f, false, textDim, juce::Justification::centredLeft);
    diffCaption.setText ("Reference minus your vocal as it sounds now, in dB (violet: reference is stronger)", juce::dontSendNotification);

    button (chooseButton, "Choose an audio file you have the right to use (" + analysis::referenceFormatNames() + "). The file is only read, never copied or stored.");
    button (analyseRefButton, "Measure the tone of the chosen file. This runs in the background and can be cancelled.");
    button (cancelRefButton, "Stop the analysis.");
    button (vocalButton, "Measure your vocal from the plug-in's input while the host plays. Press again to finish early.");
    button (applyButton, "Apply the ticked changes to the EQ. UNDO puts everything back.");
    button (cancelButton, "Discard this proposal. Nothing was changed.");
    button (resetButton, "Forget both analyses and the proposal. Nothing is changed.");
    button (undoButton, "Put back the sound you had before Reference Match changed it.");
    autoSection.setColour (juce::ToggleButton::tickColourId, violet);
    autoSection.setToggleState (true, juce::dontSendNotification);
    autoSection.setTooltip ("Analyse the loudest 20 seconds of the file, or turn this off to choose where the section starts.");
    addAndMakeVisible (autoSection);
    sectionStart.setSliderStyle (juce::Slider::LinearHorizontal);
    sectionStart.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    sectionStart.setTextValueSuffix (" s");
    sectionStart.setRange (0.0, 1.0, 0.5);
    sectionStart.setColour (juce::Slider::trackColourId, violet);
    sectionStart.setTooltip ("Where in the file the analysed section starts.");
    addAndMakeVisible (sectionStart);
    intensity.setSliderStyle (juce::Slider::LinearHorizontal);
    intensity.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    intensity.setTextValueSuffix (" %");
    intensity.setRange (0.0, 100.0, 1.0);
    intensity.setValue (60.0, juce::dontSendNotification);
    intensity.setColour (juce::Slider::trackColourId, violet);
    intensity.setTooltip ("How much of the measured tonal difference is requested. Even at 100 % only half of it is, because a full mix is not a vocal.");
    intensity.onValueChange = [this] { propose(); };
    addAndMakeVisible (intensity);
    addAndMakeVisible (refProgress);
    addAndMakeVisible (vocalProgress);
    addAndMakeVisible (diffView);
    diffView.setTooltip ("Real numbers from the two analyses: the reference's tone minus your vocal's tone as it sounds with your current EQ, per third-octave band.");

    for (int i = 0; i < rows; ++i)
    {
        rowTick[i].setColour (juce::ToggleButton::tickColourId, violet);
        rowTick[i].setTooltip ("Include this change when you press Apply selected. Untick it to reject it.");
        rowTick[i].onClick = [this, i] { if ((size_t) i < ticked.size()) ticked[(size_t) i] = rowTick[i].getToggleState(); };
        addAndMakeVisible (rowTick[i]);
        text (rowTitle[i], 12.5f, true, textBright, juce::Justification::centredLeft);
        text (rowReason[i], 11.0f, false, textDim);
    }
    chooseButton.onClick = [this] { chooseFile(); };
    analyseRefButton.onClick = [this] { analyseReference(); };
    cancelRefButton.onClick = [this] { job.cancel(); };
    vocalButton.onClick = [this] { vocalListening ? finishVocal() : startVocal(); };
    applyButton.onClick = [this] { applySelected(); };
    cancelButton.onClick = [this] { proposal = {}; proposalFresh = false; ticked.clear(); refreshRows(); resultStatus.setText ("Proposal discarded. Nothing was changed.", juce::dontSendNotification); };
    resetButton.onClick = [this] { resetAll(); };
    undoButton.onClick = [this] { if (hooks.requestUndo) hooks.requestUndo(); applied = false; resultStatus.setText ("Your previous sound is back.", juce::dontSendNotification); };
    autoSection.onClick = [this] { sectionStart.setVisible (! autoSection.getToggleState() && fileSeconds > 0.0); resized(); };
    sectionStart.setVisible (false);
    refreshRows();
}

ReferencePanel::~ReferencePanel()
{
    cancelEverything();
}

void ReferencePanel::cancelEverything()
{
    job.stopJob();
    vocalListening = false;
    vocalAcc.reset();
}

void ReferencePanel::resized()
{
    const int lx = 24, lw = 430, rx = 486, rw = 450;
    title.setBounds (lx, 8, 250, 24);
    notice.setBounds (lx, 36, lw, 62);
    step1.setBounds (lx, 108, lw, 20);
    chooseButton.setBounds (lx, 132, 120, 28);
    fileName.setBounds (lx + 130, 132, lw - 130, 28);
    autoSection.setBounds (lx, 166, 200, 26);
    sectionStart.setBounds (lx + 206, 166, lw - 206, 26);
    analyseRefButton.setBounds (lx, 198, 150, 28);
    cancelRefButton.setBounds (lx + 158, 198, 80, 28);
    refProgress.setBounds (lx + 248, 206, lw - 248, 12);
    refStatus.setBounds (lx, 230, lw, 34);
    step2.setBounds (lx, 270, lw, 20);
    vocalButton.setBounds (lx, 294, 160, 28);
    vocalProgress.setBounds (lx + 170, 302, lw - 170, 12);
    vocalStatus.setBounds (lx, 326, lw, 34);
    diffCaption.setBounds (lx, 366, lw, 16);
    diffView.setBounds (lx, 384, lw, 66);

    step3.setBounds (rx, 8, rw, 20);
    confidence.setBounds (rx, 34, rw, 20);
    confidenceWhy.setBounds (rx, 56, rw, 52);
    for (int i = 0; i < rows; ++i)
    {
        const int y = 114 + i * 62;
        rowTick[i].setBounds (rx, y, 26, 24);
        rowTitle[i].setBounds (rx + 30, y, rw - 30, 22);
        rowReason[i].setBounds (rx + 30, y + 22, rw - 30, 38);
    }
    intensityLabel.setBounds (rx, 308, 140, 24);
    intensity.setBounds (rx + 140, 308, rw - 140, 24);
    applyButton.setBounds (rx, 346, 150, 34);
    cancelButton.setBounds (rx + 158, 346, 90, 34);
    resetButton.setBounds (rx + 256, 346, 90, 34);
    undoButton.setBounds (rx, 388, 170, 28);
    resultStatus.setBounds (rx, 420, rw, 34);
}

void ReferencePanel::paint (juce::Graphics& g)
{
    drawPanel (g, juce::Rectangle<float> (16.0f, 2.0f, 446.0f, 452.0f));
    drawPanel (g, juce::Rectangle<float> (478.0f, 2.0f, 466.0f, 452.0f));
}

void ReferencePanel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference audio file you have the right to use", juce::File(), analysis::referenceFileWildcard()); // exactly the formats this build can decode
    juce::Component::SafePointer<ReferencePanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        safe->file = fc.getResult();
        safe->fileName.setText (safe->file.getFileName(), juce::dontSendNotification);
        safe->referenceProfile = {};
        safe->refResult = {};
        safe->fileSeconds = 0.0;
        safe->fileSeconds = analysis::referenceFileSeconds (safe->file);
        safe->sectionStart.setRange (0.0, juce::jmax (1.0, safe->fileSeconds - 20.0), 0.5);
        safe->sectionStart.setVisible (! safe->autoSection.getToggleState() && safe->fileSeconds > 0.0);
        safe->refStatus.setText (safe->fileSeconds > 0.0 ? "Ready to analyse (" + juce::String (safe->fileSeconds, 1) + " s long). Press Analyze reference."
                                                                         : "This file could not be opened as audio. Formats that can be opened here: " + analysis::referenceFormatNames() + ".", juce::dontSendNotification);
        safe->propose();
    });
}

void ReferencePanel::analyseReference()
{
    if (job.isRunning())
        return;
    analysis::ReferenceSection section;
    section.automatic = autoSection.getToggleState();
    section.startSeconds = sectionStart.getValue();
    refStatus.setText ("Analysing...", juce::dontSendNotification);
    refProgress.setValue (0.0f, false);
    juce::Component::SafePointer<ReferencePanel> safe (this);
    if (! job.start (file, section, [safe] (const analysis::ReferenceResult& r) { if (safe != nullptr) safe->onReferenceDone (r); }))
        refStatus.setText ("An analysis is already running.", juce::dontSendNotification);
}

void ReferencePanel::onReferenceDone (const analysis::ReferenceResult& r)
{
    refResult = r;
    refProgress.setValue (r.status == analysis::ReferenceResult::Status::ok ? 1.0f : 0.0f, false);
    referenceProfile = r.status == analysis::ReferenceResult::Status::ok ? r.profile : analysis::SpectralProfile {};
    refStatus.setText (r.status == analysis::ReferenceResult::Status::ok
                           ? r.message + " Section: " + juce::String (r.sectionStartSeconds, 1) + " s to " + juce::String (r.sectionStartSeconds + r.sectionLengthSeconds, 1) + " s."
                           : r.message,
                       juce::dontSendNotification);
    propose();
}

void ReferencePanel::startVocal()
{
    vocalRate = processor.getSampleRate() > 8000.0 ? processor.getSampleRate() : 48000.0;
    vocalAcc = std::make_unique<analysis::SpectralAccumulator> (vocalRate);
    vocalLastCount = processor.getInputTap().writeCount();
    vocalListening = true;
    vocalSecondsHeard = 0.0f;
    vocalProfile = {};
    vocalButton.setButtonText ("Finish now");
    vocalStatus.setText ("Listening: play your vocal through the plug-in (the host must be playing).", juce::dontSendNotification);
    propose();
}

void ReferencePanel::finishVocal()
{
    if (! vocalListening || vocalAcc == nullptr)
        return;
    vocalListening = false;
    vocalButton.setButtonText ("Analyze my vocal");
    vocalProfile = vocalAcc->finish (1);
    vocalAcc.reset();
    if (! vocalProfile.valid || vocalProfile.activeSeconds < minVocalActiveSeconds)
    {
        vocalStatus.setText ("Not enough of your vocal was heard (" + juce::String (vocalProfile.valid ? vocalProfile.activeSeconds : 0.0f, 1) + " s; at least " + juce::String ((int) minVocalActiveSeconds)
                                 + " s is needed). Play the vocal through the plug-in and try again.", juce::dontSendNotification);
        vocalProfile = {};
    }
    else
        vocalStatus.setText ("Analysed " + juce::String (vocalProfile.activeSeconds, 1) + " s of your vocal (the plug-in's input, before processing).", juce::dontSendNotification);
    vocalProgress.setValue (vocalProfile.valid ? 1.0f : 0.0f, false);
    propose();
}

float ReferencePanel::eqResponseDb (const SoundState& s, float hz) const
{
    const char* ids[4] = { "lo", "b1", "b2", "hi" };
    float f[4], gn[4], q[4];
    for (int i = 0; i < 4; ++i)
    {
        f[i] = s.get (juce::String ("eq_") + ids[i] + "_freq", 1000.0f);
        gn[i] = s.get (juce::String ("eq_") + ids[i] + "_gain", 0.0f);
        q[i] = s.get (juce::String ("eq_") + ids[i] + "_q", 0.8f);
    }
    return EqGraph::responseDb (f, gn, q, hz, vocalRate > 8000.0 ? vocalRate : 48000.0);
}

juce::uint32 ReferencePanel::eqSignature() const
{
    const auto s = processor.captureSoundState();
    juce::uint32 h = 2166136261u;
    for (const char* id : { "eq_lo_freq", "eq_lo_gain", "eq_b1_freq", "eq_b1_gain", "eq_b1_q", "eq_b2_freq", "eq_b2_gain", "eq_b2_q", "eq_hi_freq", "eq_hi_gain" })
        h = (h ^ (juce::uint32) juce::roundToInt (s.get (id, 0.0f) * 100.0f)) * 16777619u;
    return h;
}

void ReferencePanel::propose()
{
    signature = eqSignature();
    automode::MatchInput in;
    in.reference = referenceProfile;
    in.vocal = vocalProfile;
    in.intensity = (float) intensity.getValue() / 100.0f;
    in.current = processor.captureSoundState();
    in.currentEqDb = [this, cur = in.current] (float hz) { return eqResponseDb (cur, hz); };
    const auto old = proposal;
    proposal = automode::proposeReferenceInspiredEq (in, processor.makeRangeLookup()); // (it states what is missing when an analysis is not there yet)
    // keep the user's ticks for changes that are still proposed
    std::vector<bool> next (proposal.changes.size(), true);
    for (size_t i = 0; i < proposal.changes.size(); ++i)
        for (size_t j = 0; j < old.changes.size() && j < ticked.size(); ++j)
            if (old.changes[j].id == proposal.changes[i].id)
                next[i] = ticked[j];
    ticked = next;
    proposalFresh = true;
    applied = false;
    refreshRows();
}

void ReferencePanel::refreshRows()
{
    const bool have = referenceProfile.valid && vocalProfile.valid;
    diffView.set (proposal.differenceDb, proposal.bandsCompared, have && proposal.bandsCompared > 0);
    if (! have)
    {
        confidence.setText ("Confidence: not available yet", juce::dontSendNotification);
        confidenceWhy.setText ("Analyse a reference file and your vocal. Nothing is shown until both measurements exist.", juce::dontSendNotification);
    }
    else
    {
        confidence.setText (confidenceText (proposal.confidence), juce::dontSendNotification);
        juce::String why = proposal.message;
        for (const auto& r : proposal.confidenceReasons)
            why << "\n" << r;
        confidenceWhy.setText (why, juce::dontSendNotification);
    }
    for (int i = 0; i < rows; ++i)
    {
        const bool shown = (size_t) i < proposal.changes.size();
        rowTick[i].setVisible (shown);
        rowTitle[i].setVisible (shown);
        rowReason[i].setVisible (shown);
        if (! shown)
            continue;
        const auto& c = proposal.changes[(size_t) i];
        rowTitle[i].setText (bandName (c.id) + ":  " + juce::String (c.from, 1) + " dB  ->  " + juce::String (c.to, 1) + " dB", juce::dontSendNotification);
        rowReason[i].setText (c.reason, juce::dontSendNotification);
        rowTick[i].setToggleState ((size_t) i < ticked.size() && ticked[(size_t) i], juce::dontSendNotification);
    }
    const bool any = proposal.status == automode::MatchProposal::Status::ok && ! proposal.changes.empty();
    applyButton.setEnabled (any);
    cancelButton.setEnabled (any);
    resetButton.setEnabled (referenceProfile.valid || vocalProfile.valid || file != juce::File());
    undoButton.setEnabled (applied);
    analyseRefButton.setEnabled (file != juce::File() && fileSeconds > 0.0 && ! job.isRunning());
    cancelRefButton.setVisible (job.isRunning());
}

void ReferencePanel::applySelected()
{
    if (proposal.status != automode::MatchProposal::Status::ok)
        return;
    std::vector<automode::MixChange> changes;
    const auto cur = processor.captureSoundState();
    const auto after = automode::applyMatchChanges (proposal, ticked, cur, processor.makeRangeLookup(), &changes);
    if (changes.empty())
    {
        resultStatus.setText ("Nothing was changed: no change is ticked.", juce::dontSendNotification);
        return;
    }
    if (hooks.beforeApply)
        hooks.beforeApply (cur);
    processor.applyParameterChanges (after);
    applied = true;
    juce::String t = "Applied " + juce::String ((int) changes.size()) + " change" + (changes.size() == 1 ? "" : "s") + ": ";
    for (const auto& c : changes)
        t << bandName (c.id) << " " << juce::String (c.from, 1) << " to " << juce::String (c.to, 1) << " dB;  ";
    resultStatus.setText (t.trimEnd().trimCharactersAtEnd (";") + ". Undo puts your previous sound back.", juce::dontSendNotification);
    if (hooks.status)
        hooks.status ("Reference Match: " + juce::String ((int) changes.size()) + " EQ change(s) applied. Undo puts them back.");
    propose();
    applied = true;
    refreshRows();
}

void ReferencePanel::resetAll()
{
    job.stopJob();
    vocalListening = false;
    vocalAcc.reset();
    vocalButton.setButtonText ("Analyze my vocal");
    file = juce::File();
    fileSeconds = 0.0;
    referenceProfile = {};
    vocalProfile = {};
    refResult = {};
    proposal = {};
    ticked.clear();
    fileName.setText ("No file chosen", juce::dontSendNotification);
    refStatus.setText ({}, juce::dontSendNotification);
    vocalStatus.setText ("Play your vocal through the plug-in, then press Analyze my vocal.", juce::dontSendNotification);
    resultStatus.setText ("Both analyses were cleared. Nothing was changed.", juce::dontSendNotification);
    refProgress.setValue (0.0f, false);
    vocalProgress.setValue (0.0f, false);
    sectionStart.setVisible (false);
    refreshRows();
}

void ReferencePanel::tick (float)
{
    if (job.isRunning())
    {
        refProgress.setValue (job.progress(), false);
        cancelRefButton.setVisible (true);
        analyseRefButton.setEnabled (false);
    }
    else if (cancelRefButton.isVisible())
        refreshRows();
    if (vocalListening && vocalAcc != nullptr)
    {
        const auto& tap = processor.getInputTap();
        const juce::uint32 now = tap.writeCount(), fresh = now - vocalLastCount;
        if (fresh > 0)
        {
            std::vector<float> buf ((size_t) std::min<juce::uint32> (fresh, analysis::DisplayTap::size));
            if (fresh > analysis::DisplayTap::size)
                vocalAcc->resetStream(); // a gap: the newest samples are not contiguous with the partial frame
            tap.copyLatest (buf.data(), (int) buf.size());
            vocalAcc->pushSamples (buf.data(), (int) buf.size());
            vocalLastCount = now;
        }
        vocalSecondsHeard = vocalAcc->framesSeconds();
        vocalProgress.setValue (juce::jmin (1.0f, vocalSecondsHeard / maxVocalSeconds), false);
        if (vocalSecondsHeard >= maxVocalSeconds)
            finishVocal();
    }
    if (proposalFresh && eqSignature() != signature && referenceProfile.valid && vocalProfile.valid && ! applied)
        propose(); // your EQ moved: the proposal is recomputed against it
}

void ReferencePanel::setProfilesForTest (const analysis::SpectralProfile& ref, const analysis::SpectralProfile& voc)
{
    referenceProfile = ref;
    vocalProfile = voc;
    propose();
}
}
