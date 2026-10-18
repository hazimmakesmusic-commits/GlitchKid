#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/Finite.h"

namespace
{
// Formant is "on" (adds latency, runs the shifter) only when it is at least half a parameter step (0.05 st) away from 0; the
// 0.1 st grid can leave float residue at "0", so anything closer is exactly neutral.
bool formantIsOn (float semitones) { return std::abs (semitones) >= 0.05f; }
}

juce::AudioProcessorValueTreeState::ParameterLayout GlitchKidProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { gainId, 1 }, "Gain",
        NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { bypassId, 1 }, "Bypass", false));

    struct BandInfo { const char* name; float fMin, fMax, fDefault, fCentre; };
    const BandInfo info[ParametricEq::numBands] = {
        { "Low Shelf", 20.0f,  500.0f,   100.0f,  100.0f },
        { "Bell 1",    100.0f, 5000.0f,  400.0f,  700.0f },
        { "Bell 2",    500.0f, 16000.0f, 3000.0f, 3000.0f },
        { "High Shelf", 1000.0f, 18000.0f, 8000.0f, 6000.0f } };

    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        NormalisableRange<float> fr (info[b].fMin, info[b].fMax, 1.0f);
        fr.setSkewForCentre (info[b].fCentre);

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].freq, 1 },
            String (info[b].name) + " Freq", fr, info[b].fDefault,
            AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].gain, 1 },
            String (info[b].name) + " Gain", NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));
        if (eqIds[b].q != nullptr)
        {
            NormalisableRange<float> qr (0.3f, 10.0f, 0.01f);
            qr.setSkewForCentre (1.5f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { eqIds[b].q, 1 },
                String (info[b].name) + " Q", qr, 1.0f));
        }
    }

    // Compressor. Ratio defaults to 1:1, so a fresh instance does not change the sound.
    auto skewed = [] (float lo, float hi, float step, float centre)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compThresholdId, 1 }, "Comp Threshold",
        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compRatioId, 1 }, "Comp Ratio",
        skewed (1.0f, 20.0f, 0.1f, 4.0f), 1.0f, AudioParameterFloatAttributes().withLabel (":1")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compAttackId, 1 }, "Comp Attack",
        skewed (0.1f, 100.0f, 0.1f, 10.0f), 10.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compReleaseId, 1 }, "Comp Release",
        skewed (10.0f, 1000.0f, 1.0f, 100.0f), 100.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compKneeId, 1 }, "Comp Knee",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { compMakeupId, 1 }, "Comp Makeup",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { satDriveId, 1 }, "Saturation Drive",
        NormalisableRange<float> (0.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { stutterOnId, 1 }, "Stutter On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { stutterRateId, 1 }, "Stutter Rate",
        StringArray { "1/4", "1/8", "1/16", "1/32" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { stutterMixId, 1 }, "Stutter Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));

    StringArray divisions;
    for (int i = 0; i < LiquidEcho::numDivisions; ++i)
        divisions.add (LiquidEcho::divisionName (i));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { echoOnId, 1 }, "Echo On", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { echoSyncId, 1 }, "Echo Sync", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { echoTimeId, 1 }, "Echo Time",
        skewed (1.0f, 2000.0f, 1.0f, 300.0f), 350.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { echoDivId, 1 }, "Echo Division", divisions, 4));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { echoFbId, 1 }, "Echo Feedback",
        NormalisableRange<float> (0.0f, LiquidEcho::maxFeedbackPct, 0.1f), 35.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { echoToneId, 1 }, "Echo Tone",
        skewed (400.0f, 20000.0f, 1.0f, 3000.0f), 6000.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { echoWidthId, 1 }, "Echo Width",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { echoMixId, 1 }, "Echo Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { tuneOnId, 1 }, "Tune On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { tuneKeyId, 1 }, "Tune Key",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    StringArray scales;
    for (int i = 0; i < glitchkid::pitch::ScaleQuantizer::numScales; ++i)
        scales.add (glitchkid::pitch::ScaleQuantizer::name ((glitchkid::pitch::ScaleQuantizer::Scale) i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { tuneScaleId, 1 }, "Tune Scale", scales, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tuneStrengthId, 1 }, "Tune Strength",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tuneSpeedId, 1 }, "Tune Speed",
        skewed (0.0f, 400.0f, 1.0f, 40.0f), 20.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { formantShiftId, 1 }, "Formant Shift",
        NormalisableRange<float> (glitchkid::formant::FormantShifter::minShiftSemitones, glitchkid::formant::FormantShifter::maxShiftSemitones, 0.1f),
        0.0f, AudioParameterFloatAttributes().withLabel ("st")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { mutOnId, 1 }, "Mutation On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mutAmountId, 1 }, "Mutation",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mutCharacterId, 1 }, "Mutation Character",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mutBlendId, 1 }, "Mutation Blend",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { harmOnId, 1 }, "Harmony On", false));
    {
        StringArray intervals;
        for (int i = 0; i < glitchkid::harmony::numIntervals; ++i)
            intervals.add (glitchkid::harmony::intervalInfo (i).name);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { harmIntervalId, 1 }, "Harmony Interval", intervals,
                                                            glitchkid::harmony::defaultInterval));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { harmLevelId, 1 }, "Harmony Level",
        NormalisableRange<float> (-24.0f, 0.0f, 0.1f), -9.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { dblOnId, 1 }, "Double On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dblAmountId, 1 }, "Double",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dblWidthId, 1 }, "Double Width",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dblDetuneId, 1 }, "Double Detune",
        NormalisableRange<float> (0.0f, VocalDoubler::maxDetuneCents, 0.1f), 8.0f, AudioParameterFloatAttributes().withLabel ("cents")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dblTimingId, 1 }, "Double Timing",
        NormalisableRange<float> (VocalDoubler::minTimingMs, VocalDoubler::maxTimingMs, 0.1f), 18.0f, AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { dimOnId, 1 }, "Dimension On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dimWidthId, 1 }, "Dimension Width",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dimDetuneId, 1 }, "Dimension Detune",
        NormalisableRange<float> (0.0f, 20.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("cents")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dimDepthId, 1 }, "Dimension Depth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { dimMixId, 1 }, "Dimension Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mixId, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

GlitchKidProcessor::GlitchKidProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GlitchKidState", createLayout())
{
    setLatencySamples (Saturation::latencySamples);
    gainDb = apvts.getRawParameterValue (gainId);
    bypass = apvts.getRawParameterValue (bypassId);
    for (int b = 0; b < ParametricEq::numBands; ++b)
    {
        eqParams[b].freq = apvts.getRawParameterValue (eqIds[b].freq);
        eqParams[b].gain = apvts.getRawParameterValue (eqIds[b].gain);
        eqParams[b].q    = eqIds[b].q != nullptr ? apvts.getRawParameterValue (eqIds[b].q) : nullptr;
    }
    const char* ids[] = { compThresholdId, compRatioId, compAttackId, compReleaseId, compKneeId, compMakeupId };
    for (int i = 0; i < 6; ++i)
        compParams[i] = apvts.getRawParameterValue (ids[i]);
    satDrive = apvts.getRawParameterValue (satDriveId);
    mixPercent = apvts.getRawParameterValue (mixId);
    stutterOn = apvts.getRawParameterValue (stutterOnId);
    stutterRate = apvts.getRawParameterValue (stutterRateId);
    stutterMix = apvts.getRawParameterValue (stutterMixId);
    const char* echoIds[] = { echoOnId, echoSyncId, echoTimeId, echoDivId, echoFbId, echoToneId, echoWidthId, echoMixId };
    for (int i = 0; i < 8; ++i)
        echoParams[i] = apvts.getRawParameterValue (echoIds[i]);
    const char* dimIds[] = { dimOnId, dimWidthId, dimDetuneId, dimDepthId, dimMixId };
    for (int i = 0; i < 5; ++i)
        dimParams[i] = apvts.getRawParameterValue (dimIds[i]);
    const char* dblIds[] = { dblOnId, dblAmountId, dblWidthId, dblDetuneId, dblTimingId };
    for (int i = 0; i < 5; ++i)
        dblParams[i] = apvts.getRawParameterValue (dblIds[i]);
    const char* harmIds[] = { harmOnId, harmIntervalId, harmLevelId };
    for (int i = 0; i < 3; ++i)
        harmParams[i] = apvts.getRawParameterValue (harmIds[i]);
    const char* mutIds[] = { mutOnId, mutAmountId, mutCharacterId, mutBlendId };
    for (int i = 0; i < 4; ++i)
        mutParams[i] = apvts.getRawParameterValue (mutIds[i]);
    const char* tuneIds[] = { tuneOnId, tuneKeyId, tuneScaleId, tuneStrengthId, tuneSpeedId };
    for (int i = 0; i < 5; ++i)
        tuneParams[i] = apvts.getRawParameterValue (tuneIds[i]);
    formantShift = apvts.getRawParameterValue (formantShiftId);

    if (juce::MessageManager::existsAndIsCurrentThread())
        startTimerHz (10); // keeps the reported latency in step with the Tune switch (never touches the audio thread)
}

GlitchKidProcessor::~GlitchKidProcessor()
{
    stopTimer();
}

glitchkid::pitch::PitchCorrector::Settings GlitchKidProcessor::readTuneSettings() const
{
    glitchkid::pitch::PitchCorrector::Settings s;
    s.key = (int) std::lround (tuneParams[1]->load());
    s.scale = (glitchkid::pitch::ScaleQuantizer::Scale) juce::jlimit (0, glitchkid::pitch::ScaleQuantizer::numScales - 1,
                                                                      (int) std::lround (tuneParams[2]->load()));
    s.strength = tuneParams[3]->load() * 0.01f;
    s.retuneSpeedMs = tuneParams[4]->load();
    return s;
}

int GlitchKidProcessor::getWantedLatencySamples() const
{
    return Saturation::latencySamples + (tuneParams[0]->load() >= 0.5f ? pitchLatency : 0)
           + (formantIsOn (formantShift->load()) ? formantLatency : 0) + (harmParams[0]->load() >= 0.5f ? harmonyLatency : 0);
}

void GlitchKidProcessor::updateReportedLatency()
{
    setLatencySamples (getWantedLatencySamples());
}

DoublerSettings GlitchKidProcessor::readDoublerSettings() const
{
    DoublerSettings s;
    s.on = dblParams[0]->load() >= 0.5f;
    s.amountPct = dblParams[1]->load();
    s.widthPct = dblParams[2]->load();
    s.detuneCents = dblParams[3]->load();
    s.timingMs = dblParams[4]->load();
    return s;
}

glitchkid::harmony::HarmonyVoice::Settings GlitchKidProcessor::readHarmonySettings() const
{
    glitchkid::harmony::HarmonyVoice::Settings s;
    s.semitones = glitchkid::harmony::intervalInfo ((int) std::lround (harmParams[1]->load())).semitones;
    s.levelDb = harmParams[2]->load();
    return s;
}

MutationSettings GlitchKidProcessor::readMutationSettings() const
{
    MutationSettings s;
    s.on = mutParams[0]->load() >= 0.5f;
    s.mutationPct = mutParams[1]->load();
    s.characterPct = mutParams[2]->load();
    s.blendPct = mutParams[3]->load();
    return s;
}

DimensionSettings GlitchKidProcessor::readDimensionSettings() const
{
    DimensionSettings s;
    s.on = dimParams[0]->load() >= 0.5f;
    s.widthPct = dimParams[1]->load();
    s.detuneCents = dimParams[2]->load();
    s.depthPct = dimParams[3]->load();
    s.mixPct = dimParams[4]->load();
    return s;
}

EchoSettings GlitchKidProcessor::readEchoSettings() const
{
    EchoSettings s;
    s.on = echoParams[0]->load() >= 0.5f;
    s.sync = echoParams[1]->load() >= 0.5f;
    s.timeMs = echoParams[2]->load();
    s.division = (int) std::lround (echoParams[3]->load());
    s.feedbackPct = echoParams[4]->load();
    s.toneHz = echoParams[5]->load();
    s.widthPct = echoParams[6]->load();
    s.mixPct = echoParams[7]->load();
    s.bpm = currentBpm;
    return s;
}

namespace
{
constexpr const char* egoIdKey = "alterEgo";
constexpr const char* egoChaosKey = "alterEgoChaos";
constexpr const char* egoSeedKey = "alterEgoSeed";
}

GlitchKidProcessor::AlterEgoSelection GlitchKidProcessor::getAlterEgoSelection() const
{
    AlterEgoSelection s;
    const auto& st = apvts.state;
    s.ego = AlterEgos::indexOfId (st.getProperty (egoIdKey, juce::String()).toString().toRawUTF8());
    s.chaos = juce::jlimit (0.0f, 1.0f, (float) (double) st.getProperty (egoChaosKey, 0.0));
    s.seed = (std::uint32_t) (int) st.getProperty (egoSeedKey, 0);
    if (s.ego < 0)
        s.seed = 0;
    return s;
}

int GlitchKidProcessor::applyAlterEgo (int ego, float chaos, std::uint32_t seed)
{
    if (ego < 0 || ego >= AlterEgos::count)
        return 0;

    chaos = juce::jlimit (0.0f, 1.0f, chaos);
    AlterEgos::Value values[AlterEgos::numOwned];
    const int n = AlterEgos::compute (ego, chaos, seed, values);

    apvts.state.setProperty (egoIdKey, AlterEgos::info (ego).id, nullptr);
    apvts.state.setProperty (egoChaosKey, (double) chaos, nullptr);
    apvts.state.setProperty (egoSeedKey, (int) seed, nullptr);

    return AlterEgos::applyValues (values, n, [this] (const char* id, float value)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
        if (p == nullptr)
            return false; // a missing parameter (an effect that does not exist) is skipped, never an error
        p->beginChangeGesture(); // lets the host record this as a normal automatable change
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
        return true;
    });
}

int GlitchKidProcessor::setChaos (float chaos)
{
    const auto s = getAlterEgoSelection();
    if (s.ego < 0)
    {
        apvts.state.setProperty (egoChaosKey, (double) juce::jlimit (0.0f, 1.0f, chaos), nullptr); // remembered for the next selection
        return 0;
    }
    return applyAlterEgo (s.ego, chaos, s.seed);
}

int GlitchKidProcessor::surprise (std::uint32_t seed)
{
    const auto s = getAlterEgoSelection();
    return s.ego < 0 ? 0 : applyAlterEgo (s.ego, s.chaos, seed == 0 ? 1u : seed);
}

bool GlitchKidProcessor::isAlterEgoActive() const
{
    const auto s = getAlterEgoSelection();
    if (s.ego < 0)
        return false;

    AlterEgos::Value values[AlterEgos::numOwned];
    const int n = AlterEgos::compute (s.ego, s.chaos, s.seed, values);
    for (int i = 0; i < n; ++i)
    {
        if (! values[i].active)
            continue; // this Alter Ego leaves that parameter alone, so it cannot make the selection "not match"
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (values[i].id));
        if (p == nullptr)
            continue; // a missing parameter cannot make the selection "not match"
        const float step = p->getNormalisableRange().interval;
        const float tolerance = step > 0.0f ? 0.5f * step + 1.0e-4f : 1.0e-3f;
        if (std::abs (p->convertFrom0to1 (p->getValue()) - values[i].value) > tolerance)
            return false;
    }
    return true;
}

// ---- Sounds, presets and A/B ------------------------------------------------------------------------------------------------------

namespace
{
constexpr const char* presetNameKey = "presetName";
constexpr const char* abActiveKey = "abActive";
const char* abSlotKey (int slot) { return slot == 0 ? "abSlotA" : "abSlotB"; }

float toolerance (const juce::RangedAudioParameter& p)
{
    const float step = p.getNormalisableRange().interval;
    return step > 0.0f ? 0.5f * step + 1.0e-4f : 1.0e-3f;
}
}

glitchkid::SoundState GlitchKidProcessor::captureSoundState() const
{
    glitchkid::SoundState s;
    for (auto* q : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            if (glitchkid::SoundState::isSoundParameter (r->paramID))
                s.values.push_back ({ r->paramID, r->convertFrom0to1 (r->getValue()) });
    const auto sel = getAlterEgoSelection();
    if (sel.ego >= 0)
    {
        s.egoId = AlterEgos::info (sel.ego).id;
        s.egoChaos = sel.chaos;
        s.egoSeed = (int) sel.seed;
    }
    return s;
}

glitchkid::SoundState GlitchKidProcessor::factoryDefaultState() const
{
    glitchkid::SoundState s;
    for (auto* q : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            if (glitchkid::SoundState::isSoundParameter (r->paramID))
                s.values.push_back ({ r->paramID, r->convertFrom0to1 (r->getDefaultValue()) });
    return s;
}

int GlitchKidProcessor::applyParameterChanges (const glitchkid::SoundState& s)
{
    int set = 0;
    for (const auto& e : s.values)
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (e.id));
        if (r == nullptr || ! glitchkid::SoundState::isSoundParameter (r->paramID) || ! std::isfinite (e.value))
            continue;
        if (std::abs (r->convertFrom0to1 (r->getValue()) - e.value) <= toolerance (*r))
            continue;
        r->beginChangeGesture();
        r->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, r->convertTo0to1 (e.value)));
        r->endChangeGesture();
        ++set;
    }
    return set;
}

bool GlitchKidProcessor::getUiFlag (const char* key, bool fallback) const
{
    const auto v = apvts.state.getProperty (key);
    return v.isVoid() ? fallback : (bool) v;
}

void GlitchKidProcessor::setUiFlag (const char* key, bool value)
{
    apvts.state.setProperty (key, value, nullptr);
}

int GlitchKidProcessor::applySoundState (const glitchkid::SoundState& s)
{
    int set = 0;
    for (auto* q : getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (q);
        if (r == nullptr || ! glitchkid::SoundState::isSoundParameter (r->paramID))
            continue;
        float value = s.get (r->paramID, r->convertFrom0to1 (r->getDefaultValue())); // older states miss newer parameters: use the default
        if (! std::isfinite (value))
            value = r->convertFrom0to1 (r->getDefaultValue());
        r->beginChangeGesture(); // lets the host record this as a normal automatable change
        r->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, r->convertTo0to1 (value)));
        r->endChangeGesture();
        ++set;
    }

    // The Alter Ego selection (only a known Alter Ego is accepted; anything else clears it).
    const int ego = s.egoId.isEmpty() ? -1 : AlterEgos::indexOfId (s.egoId.toRawUTF8());
    apvts.state.setProperty (egoIdKey, ego >= 0 ? juce::String (AlterEgos::info (ego).id) : juce::String(), nullptr);
    apvts.state.setProperty (egoChaosKey, (double) (ego >= 0 ? juce::jlimit (0.0f, 1.0f, s.egoChaos) : 0.0f), nullptr);
    apvts.state.setProperty (egoSeedKey, ego >= 0 ? s.egoSeed : 0, nullptr);
    return set;
}

bool GlitchKidProcessor::matchesSoundState (const glitchkid::SoundState& s) const
{
    for (const auto& e : s.values)
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (e.id)))
            if (std::abs (r->convertFrom0to1 (r->getValue()) - e.value) > toolerance (*r))
                return false;
    return true;
}

void GlitchKidProcessor::applyPreset (const juce::String& name, const glitchkid::SoundState& s)
{
    applySoundState (s);
    apvts.state.setProperty (presetNameKey, name, nullptr);
    apvts.state.setProperty (abActiveKey, -1, nullptr); // the live sound is the preset now; both A/B snapshots are kept
    presetBaseline = captureSoundState();
    hasPresetBaseline = true;
}

void GlitchKidProcessor::notePresetSaved (const juce::String& name, const glitchkid::SoundState& s)
{
    apvts.state.setProperty (presetNameKey, name, nullptr);
    presetBaseline = s;
    hasPresetBaseline = true;
}

void GlitchKidProcessor::clearPresetName()
{
    apvts.state.setProperty (presetNameKey, juce::String(), nullptr);
    hasPresetBaseline = false;
}

juce::String GlitchKidProcessor::getPresetName() const
{
    return apvts.state.getProperty (presetNameKey, juce::String()).toString();
}

bool GlitchKidProcessor::isPresetModified() const
{
    return hasPresetBaseline && ! matchesSoundState (presetBaseline);
}

bool GlitchKidProcessor::hasAbSlot (int slot) const
{
    return slot >= 0 && slot < numAbSlots && apvts.state.getProperty (abSlotKey (slot), juce::String()).toString().isNotEmpty();
}

glitchkid::SoundState GlitchKidProcessor::getAbSlot (int slot) const
{
    glitchkid::SoundState s;
    if (slot >= 0 && slot < numAbSlots)
        if (auto xml = juce::XmlDocument::parse (apvts.state.getProperty (abSlotKey (slot), juce::String()).toString()))
            s.readFrom (*xml);
    return s;
}

void GlitchKidProcessor::captureAbSlot (int slot)
{
    if (slot < 0 || slot >= numAbSlots)
        return;
    juce::XmlElement xml ("Sound");
    captureSoundState().writeTo (xml);
    apvts.state.setProperty (abSlotKey (slot), xml.toString (juce::XmlElement::TextFormat().singleLine()), nullptr);
    apvts.state.setProperty (abActiveKey, slot, nullptr);
}

bool GlitchKidProcessor::recallAbSlot (int slot)
{
    if (! hasAbSlot (slot))
        return false;
    const auto s = getAbSlot (slot);
    if (s.values.empty())
        return false; // a damaged slot is treated as empty
    applySoundState (s);
    apvts.state.setProperty (abActiveKey, slot, nullptr);
    return true;
}

bool GlitchKidProcessor::copyAbSlot (int from, int to)
{
    if (from == to || ! hasAbSlot (from) || to < 0 || to >= numAbSlots)
        return false;
    apvts.state.setProperty (abSlotKey (to), apvts.state.getProperty (abSlotKey (from)), nullptr);
    return true;
}

int GlitchKidProcessor::getAbActive() const
{
    const int a = (int) apvts.state.getProperty (abActiveKey, -1);
    return hasAbSlot (a) ? a : -1;
}

bool GlitchKidProcessor::isAbSlotModified() const
{
    const int a = getAbActive();
    return a >= 0 && ! matchesSoundState (getAbSlot (a));
}

CompressorSettings GlitchKidProcessor::readCompressorSettings() const
{
    return { compParams[0]->load(), compParams[1]->load(), compParams[2]->load(),
             compParams[3]->load(), compParams[4]->load(), compParams[5]->load() };
}

bool GlitchKidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == layouts.getMainInputChannelSet();
}

// Target linear gain. The 0.1 dB parameter grid can leave float residue (e.g. 9e-7 dB) at "0 dB" on
// some CPUs; anything within half a step of 0 dB is exactly unity so the default is bit-exact pass-through.
static float targetGain (float db)
{
    return std::abs (db) < 0.05f ? 1.0f : juce::Decibels::decibelsToGain (db);
}

// Share of the processed signal (1 = fully processed, 0 = dry). The 0.1 % parameter grid can leave float
// residue at the ends, so anything within half a step of 0 or 100 % is exactly 0 or 1.
static float wetAmount (bool bypassed, float percent)
{
    if (bypassed || percent < 0.05f)
        return 0.0f;
    return percent >= 99.95f ? 1.0f : percent * 0.01f;
}

void GlitchKidProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    analyzer.prepare (sampleRate);
    smoothedGain.reset (sampleRate, 0.02); // 20 ms ramps
    smoothedGain.setCurrentAndTargetValue (targetGain (gainDb->load()));
    wetMix.reset (sampleRate, 0.02);
    wetMix.setCurrentAndTargetValue (wetAmount (bypass->load() >= 0.5f, mixPercent->load()));

    for (int b = 0; b < ParametricEq::numBands; ++b) // prepare() starts the smoothers at these values
        eq.setBand (b, { eqParams[b].freq->load(), eqParams[b].gain->load(),
                         eqParams[b].q != nullptr ? eqParams[b].q->load() : 0.7071f });
    eq.prepare (sampleRate);

    compressor.setSettings (readCompressorSettings());
    compressor.prepare (sampleRate);

    saturation.setDriveDb (satDrive->load());
    saturation.prepare (sampleRate, juce::jmax (samplesPerBlock, 512));
    dryDelay.prepare (Saturation::latencySamples);
    stutter.prepare (sampleRate);

    pitchLatency = glitchkid::pitch::PitchCorrector::latencyFor (sampleRate);
    pitch.prepare (sampleRate);
    pitch.setSettings (readTuneSettings());
    pitchDryDelay.prepare (pitchLatency);
    tuneBlend.reset (sampleRate, 0.02);
    tuneBlend.setCurrentAndTargetValue (tuneParams[0]->load() >= 0.5f ? 1.0f : 0.0f);

    formantLatency = glitchkid::formant::FormantShifter::latencyFor (sampleRate);
    formant.prepare (sampleRate);
    formantDryDelay.prepare (formantLatency);
    formantBlend.reset (sampleRate, 0.02);
    formantBlend.setCurrentAndTargetValue (formantIsOn (formantShift->load()) ? 1.0f : 0.0f);
    harmonyLatency = glitchkid::harmony::HarmonyVoice::latencyFor (sampleRate);
    harmony.setSettings (readHarmonySettings());
    harmony.prepare (sampleRate);
    harmonyDryDelay.prepare (harmonyLatency);
    harmonyBlend.reset (sampleRate, 0.02);
    harmonyBlend.setCurrentAndTargetValue (harmParams[0]->load() >= 0.5f ? 1.0f : 0.0f);
    updateReportedLatency();

    doubler.setSettings (readDoublerSettings());
    doubler.prepare (sampleRate);

    echo.setSettings (readEchoSettings());
    echo.prepare (sampleRate);

    dimension.setSettings (readDimensionSettings());
    dimension.prepare (sampleRate);

    mutation.setSettings (readMutationSettings());
    mutation.prepare (sampleRate);

    dryBuffer.setSize (2, juce::jmax (samplesPerBlock, 512));
    dryBuffer2.setSize (2, dryBuffer.getNumSamples());
    undelayedBuffer.setSize (2, dryBuffer.getNumSamples());
    blendBuffer.assign ((size_t) dryBuffer.getNumSamples(), 0.0f);
    dryBuffer3.setSize (2, dryBuffer.getNumSamples());
    scratchBuffer.setSize (2, dryBuffer.getNumSamples());
    formantBlendBuffer.assign ((size_t) dryBuffer.getNumSamples(), 0.0f);
    dryBuffer4.setSize (2, dryBuffer.getNumSamples());
    harmonyBlendBuffer.assign ((size_t) dryBuffer.getNumSamples(), 0.0f);
}

// Raise a peak value; only the audio thread writes, so a plain load/store is enough.
static void raisePeak (std::atomic<float>& peak, float value)
{
    if (value > peak.load (std::memory_order_relaxed))
        peak.store (value, std::memory_order_relaxed);
}

// Real-time thread: no allocation, locks or I/O here. Splits oversized host blocks so the
// preallocated dry buffer is always big enough.
void GlitchKidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Host tempo for Glitch Stutter. The host may supply no play head, no tempo, or nonsense; every such
    // case falls back to 120 BPM (see GlitchStutter::resolveTempo).
    bool hostHasTempo = false;
    double hostBpm = 0.0;
    if (auto* host = getPlayHead())
        if (auto position = host->getPosition())
            if (auto bpm = position->getBpm())
            {
                hostHasTempo = true;
                hostBpm = *bpm;
            }
    bool usedHost = false;
    currentBpm = GlitchStutter::resolveTempo (hostHasTempo, hostBpm, usedHost);
    shownTempo.store (currentBpm, std::memory_order_relaxed);
    tempoFromHost.store (usedHost, std::memory_order_relaxed);

    const int cap = dryBuffer.getNumSamples();
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += cap)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                        start, juce::jmin (cap, total - start));
        processChunk (chunk);
    }
}

// While Formant is not in use, still run the shifter at 0 semitones on a copy and throw the result away. That keeps its
// overlap-add full, so moving the control off 0 starts straight away with no fade-in; at 0 the frames skip the transforms
// (about 0.15 % of a core).
void GlitchKidProcessor::formantListen (const juce::AudioBuffer<float>& buffer, int numChannels, int dryChannels, int numSamples)
{
    for (int ch = 0; ch < dryChannels; ++ch)
        scratchBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    formant.setShiftSemitones (0.0f);
    formant.process (scratchBuffer.getArrayOfWritePointers(), numChannels, numSamples);
}

void GlitchKidProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Layouts are matched in/out, but never pass unrelated data through extra outputs.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // AutoMode listens to the raw input (before the clean-up below, so it can count invalid samples). It does nothing unless ANALYZE was pressed.
    analyzer.process (buffer.getArrayOfReadPointers(), juce::jmin (numChannels, getTotalNumInputChannels()), numSamples);

    // Non-finite input (NaN / +-Inf) is replaced by 0 once, up front, so it can never travel through the dry copy that Bypass and Mix use
    // (the stages also guard themselves). Finite audio is not touched.
    for (int ch = 0; ch < numChannels; ++ch)
        dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);

    const float inMag = buffer.getMagnitude (0, numSamples);
    raisePeak (inputPeak, inMag);

    // Dry copy, delayed by the saturation latency. Kept current on every block so that bypass and the
    // bypass crossfade stay time-aligned with the processed path.
    const int dryChannels = juce::jmin (numChannels, 2);
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    juce::AudioBuffer<float> dry (dryBuffer.getArrayOfWritePointers(), dryChannels, 0, numSamples);
    dryDelay.process (dry);

    // Pitch Tune: while it is on, the dry path is delayed by its latency too (crossfaded with the same 20 ms ramp that
    // blends the tuned signal in), so Mix and Bypass stay time-aligned with what the host was told.
    const bool tuneOn = tuneParams[0]->load() >= 0.5f;
    tuneBlend.setTargetValue (tuneOn ? 1.0f : 0.0f);
    const bool tuneActive = tuneOn || tuneBlend.isSmoothing();
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer2.copyFrom (ch, 0, dry, ch, 0, numSamples);
    juce::AudioBuffer<float> dryLate (dryBuffer2.getArrayOfWritePointers(), dryChannels, 0, numSamples);
    pitchDryDelay.process (dryLate); // always fed, so it is valid the moment Tune is switched on
    if (tuneActive)
    {
        for (int i = 0; i < numSamples; ++i)
            blendBuffer[(size_t) i] = tuneBlend.getNextValue();
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = blendBuffer[(size_t) i];
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float a = dry.getSample (ch, i), late = dryLate.getSample (ch, i);
                dry.setSample (ch, i, b >= 1.0f ? late : a + (late - a) * b);
            }
        }
    }

    // Formant: the same for its own latency, applied after the Pitch Tune step above (so with both on the dry path is delayed by both).
    const float formantSt = formantShift->load();
    const bool formantOn = formantIsOn (formantSt);
    formantBlend.setTargetValue (formantOn ? 1.0f : 0.0f);
    const bool formantActive = formantOn || formantBlend.isSmoothing();
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer3.copyFrom (ch, 0, dry, ch, 0, numSamples);
    juce::AudioBuffer<float> dryLate2 (dryBuffer3.getArrayOfWritePointers(), dryChannels, 0, numSamples);
    formantDryDelay.process (dryLate2); // always fed, so it is valid the moment Formant is moved off 0
    if (formantActive)
    {
        for (int i = 0; i < numSamples; ++i)
            formantBlendBuffer[(size_t) i] = formantBlend.getNextValue();
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = formantBlendBuffer[(size_t) i];
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float a = dry.getSample (ch, i), late = dryLate2.getSample (ch, i);
                dry.setSample (ch, i, b >= 1.0f ? late : a + (late - a) * b);
            }
        }
    }

    // Harmony: the same for its own latency (after the Formant step above, so with several stages on the dry path is delayed by all of them).
    const bool harmonyOn = harmParams[0]->load() >= 0.5f;
    harmonyBlend.setTargetValue (harmonyOn ? 1.0f : 0.0f);
    const bool harmonyActive = harmonyOn || harmonyBlend.isSmoothing();
    for (int ch = 0; ch < dryChannels; ++ch)
        dryBuffer4.copyFrom (ch, 0, dry, ch, 0, numSamples);
    juce::AudioBuffer<float> dryLate3 (dryBuffer4.getArrayOfWritePointers(), dryChannels, 0, numSamples);
    harmonyDryDelay.process (dryLate3); // always fed, so it is valid the moment Harmony is switched on
    if (harmonyActive)
    {
        for (int i = 0; i < numSamples; ++i)
            harmonyBlendBuffer[(size_t) i] = harmonyBlend.getNextValue();
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = harmonyBlendBuffer[(size_t) i];
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float a = dry.getSample (ch, i), late = dryLate3.getSample (ch, i);
                dry.setSample (ch, i, b >= 1.0f ? late : a + (late - a) * b);
            }
        }
    }

    const bool bypassed = bypass->load() >= 0.5f;
    const float wetTarget = wetAmount (bypassed, mixPercent->load());
    wetMix.setTargetValue (wetTarget);

    if (wetTarget <= 0.0f && ! wetMix.isSmoothing())
    {
        eq.reset();                 // settled bypass or 0 % mix: exact (delayed) dry signal, no processing cost
        compressor.reset();
        saturation.reset();
        stutter.reset();
        echo.reset();
        dimension.reset();
        mutation.reset();
        doubler.reset();
        harmony.feed (buffer.getArrayOfReadPointers(), numChannels, numSamples); // keep listening, so un-bypassing has no gap
        pitch.feed (buffer.getArrayOfReadPointers(), numChannels, numSamples); // keep listening, so un-bypassing has no gap
        formantListen (buffer, numChannels, dryChannels, numSamples);
        shownDetectedMidi.store (-1.0f, std::memory_order_relaxed);
        shownTargetMidi.store (-1.0f, std::memory_order_relaxed);
        for (int ch = 0; ch < dryChannels; ++ch)
            buffer.copyFrom (ch, 0, dry, ch, 0, numSamples);
        raisePeak (outputPeak, buffer.getMagnitude (0, numSamples));
        return;
    }

    const bool crossfading = wetMix.isSmoothing() || wetTarget < 1.0f; // blending dry and processed

    if (tuneActive)
    {
        // Pitch Tune comes first. Non-finite input is zeroed before the straight-through copy is taken, so a NaN can
        // never reach the blend (0 x NaN is NaN).
        for (int ch = 0; ch < numChannels; ++ch)
            dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
        for (int ch = 0; ch < dryChannels; ++ch)
            undelayedBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        pitch.setSettings (readTuneSettings());
        pitch.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = blendBuffer[(size_t) i];
            if (b >= 1.0f)
                continue;
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float u = undelayedBuffer.getSample (ch, i), t = buffer.getSample (ch, i);
                buffer.setSample (ch, i, u + (t - u) * b);
            }
        }
        const auto& tel = pitch.telemetry();
        shownDetectedMidi.store (tel.voiced ? tel.detectedMidi : -1.0f, std::memory_order_relaxed);
        shownTargetMidi.store (tel.voiced ? tel.targetMidi : -1.0f, std::memory_order_relaxed);
    }
    else
    {
        pitch.feed (buffer.getArrayOfReadPointers(), numChannels, numSamples); // idle: record only, no processing
        shownDetectedMidi.store (-1.0f, std::memory_order_relaxed);
        shownTargetMidi.store (-1.0f, std::memory_order_relaxed);
    }

    if (formantActive)
    {
        // Formant follows Pitch Tune. Same safety as above: zero non-finite input before the straight-through copy is taken.
        for (int ch = 0; ch < numChannels; ++ch)
            dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
        for (int ch = 0; ch < dryChannels; ++ch)
            undelayedBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        formant.setShiftSemitones (formantOn ? formantSt : 0.0f);
        formant.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = formantBlendBuffer[(size_t) i];
            if (b >= 1.0f)
                continue;
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float u = undelayedBuffer.getSample (ch, i), t = buffer.getSample (ch, i);
                buffer.setSample (ch, i, u + (t - u) * b);
            }
        }
    }
    else
        formantListen (buffer, numChannels, dryChannels, numSamples); // idle: keep the shifter's history, output discarded

    // Harmony: one fixed-interval voice from the (tuned, formant-shifted) lead. While on it delays the lead by its latency and adds the voice; the
    // straight-through copy crossfades it in and out like the other latency stages. While off it only listens (cheap).
    if (harmonyActive)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            dspguard::zeroNonFinite (buffer.getWritePointer (ch), numSamples);
        for (int ch = 0; ch < dryChannels; ++ch)
            undelayedBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        harmony.setSettings (readHarmonySettings());
        harmony.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float b = harmonyBlendBuffer[(size_t) i];
            if (b >= 1.0f)
                continue;
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const float u = undelayedBuffer.getSample (ch, i), t = buffer.getSample (ch, i);
                buffer.setSample (ch, i, u + (t - u) * b);
            }
        }
    }
    else
        harmony.feed (buffer.getArrayOfReadPointers(), numChannels, numSamples);

    // Vocal Mutation: after Pitch Tune and FORMANT (so they hear the clean voice), before the EQ and compressor (which can shape the new partials).
    mutation.setSettings (readMutationSettings());
    mutation.process (buffer);

    // Vocal Doubler: zero latency; after Mutation so the EQ and compressor treat the doubles as part of the vocal.
    doubler.setSettings (readDoublerSettings());
    doubler.process (buffer);

    for (int b = 0; b < ParametricEq::numBands; ++b)
        eq.setBand (b, { eqParams[b].freq->load(), eqParams[b].gain->load(),
                         eqParams[b].q != nullptr ? eqParams[b].q->load() : 0.7071f });
    eq.process (buffer);

    compressor.setSettings (readCompressorSettings());
    compressor.process (buffer);
    raisePeak (gainReductionPeak, compressor.getGainReductionDb());

    saturation.setDriveDb (satDrive->load());
    saturation.process (buffer);

    stutter.setSettings ({ stutterOn->load() >= 0.5f, (int) std::lround (stutterRate->load()),
                           stutterMix->load(), currentBpm });
    stutter.process (buffer);
    shownLoopLength.store (stutter.getLoopLengthSamples(), std::memory_order_relaxed);

    echo.setSettings (readEchoSettings());
    echo.process (buffer);

    dimension.setSettings (readDimensionSettings());
    dimension.process (buffer);

    smoothedGain.setTargetValue (targetGain (gainDb->load()));
    if (! smoothedGain.isSmoothing())
    {
        buffer.applyGain (smoothedGain.getTargetValue());
    }
    else
    {
        // Same ramp on every channel: step the smoother once per sample, not once per channel.
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothedGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    if (crossfading)
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = wetMix.getNextValue();
            for (int ch = 0; ch < dryChannels; ++ch)
            {
                float* out = buffer.getWritePointer (ch);
                out[i] = dry.getSample (ch, i) + (out[i] - dry.getSample (ch, i)) * m;
            }
        }

    raisePeak (outputPeak, buffer.getMagnitude (0, numSamples));
}

juce::AudioProcessorEditor* GlitchKidProcessor::createEditor()
{
    return new GlitchKidEditor (*this);
}

void GlitchKidProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GlitchKidProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Ignore anything that is not our own state (corrupt or foreign data).
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            hasPresetBaseline = false; // the restored sound is not known to match any preset on disk
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchKidProcessor();
}

float GlitchKidProcessor::getAutoIntensity() const
{
    const auto v = apvts.state.getProperty (autoIntensityKey);
    return v.isVoid() ? 0.6f : juce::jlimit (0.0f, 1.0f, (float) (double) v);
}

void GlitchKidProcessor::setAutoIntensity (float v)
{
    apvts.state.setProperty (autoIntensityKey, (double) juce::jlimit (0.0f, 1.0f, std::isfinite (v) ? v : 0.6f), nullptr);
}

glitchkid::automode::Result GlitchKidProcessor::previewAutoMode (float intensity) const
{
    auto rangeOf = [this] (const juce::String& id, glitchkid::automode::Range& out)
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
        if (r == nullptr)
            return false;
        const auto range = r->getNormalisableRange();
        out = { range.start, range.end, [range] (float v) { return range.snapToLegalValue (v); } };
        return true;
    };
    return glitchkid::automode::compute (analyzer.fetch(), glitchkid::automode::Profile::astroMode, intensity, captureSoundState(), rangeOf);
}

glitchkid::automode::Result GlitchKidProcessor::applyAutoMode (float intensity)
{
    auto result = previewAutoMode (intensity);
    if (result.usable && result.changedCount > 0)
        applyPreset (autoModePresetName, result.sound); // every value is set through the normal parameter calls; the marker shows "*" once the user edits
    return result;
}
