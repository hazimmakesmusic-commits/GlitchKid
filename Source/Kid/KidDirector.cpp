#include "KidDirector.h"
#include "../Analysis/VocalFileAnalysis.h"
#include "../Macros/MacroEngine.h"
#include <regex>

namespace glitchkid::kid
{
namespace
{
const char* const dimWords[numDims][2] = { { "brighter", "darker" }, { "warmer", "cooler" }, { "more present", "further back" }, { "harder", "softer" },
                                           { "wider", "narrower" }, { "more space", "closer" }, { "cleaner", "cleaner" }, { "stranger", "stranger" } };

struct Lex { const char* pattern; std::array<float, numDims> ops; const char* flag; };
// The lexicon (documented in design-prototypes/glitchkid-kid-vocal-director-v1/INTENT_PARAMETER_MAP.md). Order matters only for the words KID reports as heard.
const Lex lexicon[] = {
    { "\\b(brighter|bright|crisp|shiny|sparkle|sparkly)\\b",                       { 1, 0, 0, 0, 0, 0, 0, 0 }, nullptr },
    { "\\b(airy|air|breathy)\\b",                                                  { 1, 0, 0, 0, 0, 0, 0, 0 }, "air" },
    { "\\b(darker|dark|moody|murky)\\b",                                           { -1, 0, 0, 0, 0, 0, 0, 0 }, nullptr },
    { "\\b(warmer|warm|smooth|smoother|analog|analogue|silky)\\b",                 { 0, 1, 0, 0, 0, 0, 0, 0 }, nullptr },
    { "\\b(clear|clearer|clarity|intelligible|upfront|up front|in front|forward)\\b", { 0, 0, 1, 0, 0, 0, 0, 0 }, nullptr },
    { "\\b(aggressive|aggro|punchy|punch|energy|energetic|harder|hard|angry|hype)\\b", { 0, 0, 0, 1, 0, 0, 0, 0 }, nullptr },
    { "\\b(softer|soft|gentle|gentler|relaxed)\\b",                                { 0, 0, 0, -1, 0, 0, 0, 0 }, nullptr },
    { "\\b(wider|wide|width|stereo|bigger|big|huge)\\b",                           { 0, 0, 0, 0, 1, 0, 0, 0 }, nullptr },
    { "\\b(narrow|narrower|mono|centred|centered|focused)\\b",                     { 0, 0, 0, 0, -1, 0, 0, 0 }, nullptr },
    { "\\b(atmospheric|atmosphere|spacious|space|ambient)\\b",                     { 0, 0, 0, 0, 0, 1, 0, 0 }, nullptr },
    { "\\b(distant|far away|far|background)\\b",                                   { -0.5f, 0, -1, 0, 0, 1.5f, 0, 0 }, nullptr },
    { "\\b(dreamy|dream|floaty|floating|ethereal)\\b",                             { 0.5f, 0.5f, 0, 0, 0.5f, 1, 0, 0 }, nullptr },
    { "\\b(drier|dry|closer|close|intimate)\\b",                                   { 0, 0, 0, 0, -0.5f, -1, 0, 0 }, nullptr },
    { "\\b(cleaner|clean up|clean|tidy|muddy|mud|boxy)\\b",                        { 0, 0, 0, 0, 0, 0, 1, 0 }, nullptr },
    { "\\b(harsh|sibilant|essy|esses|sibilance)\\b",                               { 0, 0, 0, 0, 0, 0, 1, 0 }, "ess" },
    { "\\b(experimental|weird|alien|strange|mutate|mutated|robotic|robot|crazy|wild)\\b", { 0, 0, 0, 0, 0, 0, 0, 1 }, nullptr },
    { "\\b(glitchy|glitch|choppy|chop|stutter)\\b",                                { 0, 0, 0, 0, 0, 0, 0, 1 }, "glitch" },
    { "\\bmelodic\\b",                                                             { 0.5f, 0, 0, 0, 0.5f, 0.5f, 0, 0 }, nullptr },
    { "\\b(rapper|rap|bars)\\b",                                                   { 0, 0, 0.5f, 0.5f, 0, 0, 0, 0 }, nullptr },
    { "\\b(emotional|emotive|heartfelt|sad|vulnerable)\\b",                          { 0.25f, 0.5f, 0, 0, 0, 0.5f, 0, 0 }, nullptr },
    { "\\b(expensive|polished|glossy|professional|radio ready|finished)\\b",        { 0.5f, 0, 0.5f, 0.25f, 0, 0, 0.5f, 0 }, nullptr },
    { "\\bsits? (?:better )?(?:with|in|on) (?:the )?(?:beat|mix|track|instrumental)\\b", { 0, 0, 0.5f, 0, 0, 0, 0.5f, 0 }, nullptr } };

struct Pick { const char* pattern; int value; };
// Producer profiles by name, then artist references (a direction in that spirit, never a copy). Matched first and removed, so "melodic" or "rap" are not counted twice.
const Pick profileWords[] = {
    { "\\b(melodic pop-? ?rap|pop-? ?rap|melodic rap)\\b", 1 }, { "\\batmospheric rap\\b", 2 },
    { "\\b(intimate rap|intimate vocal|bedroom vocal)\\b", 3 }, { "\\b(experimental vocal|experimental profile)\\b", 4 } };
const Pick artistWords[] = {
    { "\\b(the kid laroi|kid laroi|laroi|juice wrld|post malone|lil tecca|iann dior)\\b", 1 },
    { "\\b(travis scott|don toliver|kid cudi)\\b", 2 },
    { "\\b(drake|j cole|frank ocean|mac miller)\\b", 3 },
    { "\\b(playboi carti|carti|yeat|bon iver)\\b", 4 } };
// Pitch Tune words, strongest meaning first (value = IntentFrame::tune).
const Pick tuneWords[] = {
    { "\\b(no auto-? ?tune|no tuning|without (?:auto-? ?tune|tuning)|tuning off|turn (?:off )?(?:the )?tuning(?: off)?)\\b", -1 },
    { "\\b(less robotic|not robotic|not so robotic|natural tuning|tuning (?:more )?natural|subtle tuning|gentle tuning|gentle correction|softer tuning|tuning softer)\\b", 1 },
    { "\\b(harder tuning|tuning (?:much |a lot |even )?harder|hard-? ?tune|hard tuned|more auto-? ?tune|heavy auto-? ?tune|robotic tuning)\\b", 3 },
    { "\\b(auto-? ?tune|tune it|tune me|tune my vocal|pitch correct(?:ion)?|correct (?:the |my )?pitch|in key|in tune|tuning)\\b", 2 } };

const ProducerProfile profiles[numProfiles] = {
    { "Melodic Pop-Rap", "Modern emotional melodic rap: smooth pitch correction, forward presence, controlled compression, a little saturation, tasteful echo and width.",
      "MODERN MELODIC RAP", 2, { 0.25f, 0.25f, 0.5f, 0, 0.25f, 0, 0, 0 }, nullptr },
    { "Atmospheric Rap", "Spacious, experimental modern rap: wide ambience, creative echo, more audible effects, and the low-mid build-up kept in check.",
      "DARK & ATMOSPHERIC", 2, { 0, 0, 0, 0, 0.75f, 0.75f, 0.5f, 0 }, "mud" },
    { "Intimate Rap", "Close, personal production: natural pitch correction, clear presence, controlled dynamics and almost no ambience.",
      "CLEAN & NATURAL", 1, { 0, 0.25f, 0.75f, 0.25f, -0.5f, -1.0f, 0, 0 }, "natural" },
    { "Experimental Vocal", "Unconventional textures: creative hard tuning, harder processing, Mutation, width and movement.",
      "HARD-TUNED RAP", 3, { 0, 0.5f, 0, 0.5f, 0.75f, 0.25f, 0, 1.0f }, nullptr } };

struct Keep { const char* pattern; const char* a; const char* b; };
const Keep keeps[] = {
    { "\\bkeep (?:the |my |it |its |it's )?(?:vocal |voice )?(?:upfront|up front|in front|forward)\\b", "front", "presence" },
    { "\\b(?:but|still|and) (?:keep )?(?:it |the vocal |me )?(?:upfront|up front|in front)\\b", "front", "presence" },
    { "\\bkeep (?:the |my |its |it )?(?:presence|clarity|words clear)\\b", "presence", nullptr },
    { "\\bkeep (?:the |my )?character\\b", "char", nullptr },
    { "\\bkeep (?:the |my )?(?:center|centre|middle|vocal) (?:strong|solid|focused|clear)\\b", "front", "presence" },
    { "\\bkeep (?:the |my |it )?(?:warmth|warm|body|low end)\\b", "warm", nullptr } };

bool find (const std::string& t, const char* pattern, std::string* match = nullptr)
{
    std::smatch m;
    if (! std::regex_search (t, m, std::regex (pattern)))
        return false;
    if (match != nullptr)
        *match = m.str (0);
    return true;
}

std::string normalise (const juce::String& text)
{
    auto s = text.toLowerCase().replace (juce::CharPointer_UTF8 ("\xe2\x80\x99"), "'"); // typographic apostrophe
    juce::String out;
    for (auto c : s)
        out << ((juce::CharacterFunctions::isLetterOrDigit (c) && c < 128) || c == '\'' || c == '-' ? juce::String::charToString (c) : juce::String (" "));
    while (out.contains ("  "))
        out = out.replace ("  ", " ");
    return " " + out.trim().toStdString() + " ";
}

float clampDir (float v) { return juce::jlimit (-2.0f, 2.0f, v); }
float round2 (float v) { return std::round (v * 100.0f) / 100.0f; }

int pick (std::string& t, const Pick* table, size_t n, juce::String* heard = nullptr)
{
    std::string m;
    for (size_t i = 0; i < n; ++i)
        if (find (t, table[i].pattern, &m))
        {
            if (heard != nullptr) *heard = juce::String (m).trim();
            t = std::regex_replace (t, std::regex (table[i].pattern), " ");
            return table[i].value;
        }
    return 0;
}
}

const ProducerProfile& producerProfile (int index) { return profiles[juce::jlimit (1, numProfiles, index) - 1]; }

juce::String artistDisclaimer (const juce::String& artist, int profile)
{
    const auto& p = producerProfile (profile);
    juce::StringArray words;
    words.addTokens (artist.toLowerCase(), " ", "");
    for (auto& w : words)
        w = w.substring (0, 1).toUpperCase() + (w == "laroi" ? juce::String ("AROI") : w.substring (1)); // "The Kid LAROI" is spelled that way
    return "I can't copy " + words.joinIntoString (" ") + "'s voice or vocal chain, and I don't claim to. I'll take it as a direction: " + juce::String (p.name)
         + " (" + juce::String (p.tagline).upToFirstOccurrenceOf (":", false, false).toLowerCase() + "), built only from GlitchKid's own effects.";
}

automode::MixProposal buildFoundation (int profile, const SoundState& current, const SoundState& factoryDefault, const analysis::VocalAnalysis& lead, const automode::RangeLookup& ranges)
{
    automode::MixInput in;
    in.profile = profile > 0 ? FactoryPresets::indexOfName (producerProfile (profile).foundation) : -1;
    in.current = current;
    in.factoryDefault = factoryDefault;
    in.lead = lead;
    in.allowProfileOnly = true;
    const bool heard = lead.status == analysis::VocalAnalysis::Status::ready || lead.status == analysis::VocalAnalysis::Status::tooShort;
    if (! heard)
        in.lead.status = analysis::VocalAnalysis::Status::tooShort; // nothing heard yet: the engine's own "profile as saved" path, not fitted to the voice
    auto p = automode::buildMix (in, ranges);
    if (p.usable && ! heard)
        p.message = "No vocal heard yet: the " + p.profileName + " foundation as saved, not fitted to your voice.";
    return p;
}

bool IntentFrame::anyOps() const
{
    for (auto v : ops)
        if (std::abs (v) > 1e-6f)
            return true;
    return false;
}

bool IntentFrame::understood() const
{
    return anyOps() || scale > 0.0f || more || improve || reset || ! locks.isEmpty() || role.isNotEmpty() || ! flags.isEmpty() || ! notes.isEmpty()
        || profile > 0 || tune != 0 || cleanLevel > 0 || buildMix || raw || towardProfile || ask != Ask::none;
}

namespace
{
// What a "why" question is about, and the parameters that belong to it.
struct Topic { const char* name; const char* pattern; const char* ids; };
const Topic topics[] = {
    { "tuning", "tun|pitch|autotune|auto-tune|key|scale|robotic", "tune_" },
    { "space", "reverb|echo|delay|space|atmospher|ambien|wet|dry", "echo_ dim_" },
    { "harsh", "harsh|high mids|high-mids|upper mids|upper-mids|presence|sharp|bite|3k|2k|4k", "eq_b2_ clean_res" },
    { "ess", "\\bs\\b|s sounds|sibilan|de-?ess|esses", "clean_ess" },
    { "lows", "low end|lows|bass|mud|muddy|low mids|low-mids|boxy|rumble|body|bottom", "eq_lo_ eq_b1_ clean_tone clean_amount clean_on" },
    { "air", "air|top end|highs|treble|bright|dark|dull", "eq_hi_ air_" },
    { "dynamics", "compress|dynamic|punch|level|squash|loud", "comp_" },
    { "warmth", "saturat|distort|grit|warm|drive", "sat_" },
    { "width", "wide|width|stereo|double|narrow|center|centre", "dbl_ dim_" },
    { "clean", "clean|noise|resonan|ring", "clean_" },
    { "effects", "mutation|stutter|harmony|formant|glitch|effect", "mut_ stutter_ harm_ formant_" } };
}

IntentFrame parse (const juce::String& text)
{
    IntentFrame f;
    auto t = normalise (text);
    std::string m;
    // questions first: they are answered from the plan and the analysis, and never change the sound
    const bool question = text.trim().endsWithChar ('?') || find (t, "^ (why|what|which|how|explain|is|are|did|tell me)\\b");
    if (find (t, "\\bwhy\\b"))
        f.ask = IntentFrame::Ask::why;
    else if (find (t, "\\b(what did you (?:do|change)|what (?:have you|has) changed|what changed|explain (?:what|your|the) |what you changed|show me (?:the )?changes|walk me through)"))
        f.ask = IntentFrame::Ask::changes;
    else if (find (t, "\\b(what key|which key|key is it|key am i|what scale)\\b"))
        f.ask = IntentFrame::Ask::key;
    else if (find (t, "\\b(analy[sz]e|analysis|diagnos[ei]s?|what'?s wrong|what is wrong|how does my (?:vocal|voice) sound|check my (?:vocal|voice)|listen to my (?:vocal|voice))\\b")
             || (question && find (t, "\\b(clip|clipping|noisy|noise|in tune|harsh|muddy)\\b")))
        f.ask = IntentFrame::Ask::analysis;
    else if (find (t, "\\b(what can you do|help|how do i|how does this work)\\b"))
        f.ask = IntentFrame::Ask::help;
    if (f.ask != IntentFrame::Ask::none)
    {
        for (const auto& tp : topics)
            if (find (t, (std::string ("(") + tp.pattern + ")").c_str()))
            {
                f.topic = tp.name;
                break;
            }
        return f;
    }
    for (const auto& k : keeps)
        if (find (t, k.pattern, &m))
        {
            f.locks.addIfNotAlreadyThere (k.a);
            if (k.b != nullptr)
                f.locks.addIfNotAlreadyThere (k.b);
            f.heard.add (juce::String (m).trim());
            t = std::regex_replace (t, std::regex (k.pattern), " ", std::regex_constants::format_first_only);
        }
    if (const int a = pick (t, artistWords, std::size (artistWords), &f.artist); a > 0)
        f.profile = a;
    else if (const int pr = pick (t, profileWords, std::size (profileWords)); pr > 0)
        f.profile = pr;
    else if (find (t, "\\b(sound like|sounds like|in the style of|style of)\\b"))
        f.notes.add ("I don't have a direction for that name. Pick a profile, or describe the sound: wide, dark, close, glitchy...");
    if (f.profile > 0) f.heard.add (f.artist.isNotEmpty() ? f.artist : juce::String (producerProfile (f.profile).name));
    if (find (t, "\\b(build my mix|build (?:me )?a mix|build the mix|mix it for me|make me a mix)\\b", &m))
    {
        f.buildMix = true;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\b(build my mix|build (?:me )?a mix|build the mix|mix it for me|make me a mix)\\b"), " ");
    }
    if (find (t, "\\bclean (?:up )?(?:my |the |this )?(?:vocal|voice)\\b", &m))
    {
        f.cleanLevel = find (t, "\\b(gentle|gently|light|lightly|subtle)\\b") ? 1 : find (t, "\\b(aggressive|aggressively|heavy|heavily|hard)\\b") ? 3 : 2;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\bclean (?:up )?(?:my |the |this )?(?:vocal|voice)\\b|\\b(gentle|gently|light|lightly|subtle|aggressive|aggressively|heavy|heavily|hard|balanced)\\b"), " ");
    }
    {
        juce::String w;
        f.tune = pick (t, tuneWords, std::size (tuneWords), &w);
        if (f.tune != 0) f.heard.add (w);
    }
    if (find (t, "\\b(more like (?:that|it|this|the reference|him|her|them|the profile)|closer to (?:that|the reference|the profile))\\b", &m))
    {
        f.towardProfile = true;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\b(more like (?:that|it|this|the reference|him|her|them|the profile)|closer to (?:that|the reference|the profile))\\b"), " ");
    }
    if (find (t, "\\b(more raw|rawer|raw|less polished|not so polished|less produced|unpolished|grittier and raw)\\b", &m))
    {
        f.raw = true;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\b(more raw|rawer|raw|less polished|not so polished|less produced|unpolished)\\b"), " ");
    }
    if (find (t, "\\b(start over|reset|forget that|forget it|from scratch)\\b"))
    {
        f.reset = true;
        f.heard.add ("start over");
    }
    if (find (t, "\\b(improve|better|what it needs|don't know|dont know|fix it|help me|sound good)\\b", &m))
    {
        f.improve = true;
        f.heard.add (m);
    }
    float mag = 1.0f;
    if (find (t, "\\b(a little|slightly|a bit|a touch)\\b")) mag = 0.5f;
    if (find (t, "\\b(much|way|really|very|super|a lot|lots)\\b")) mag = 1.5f;
    if (find (t, "\\b(too much|a little less|bit less|less|tone it down|pull it back|pull back|subtle|subtler|not so much|too far|too strong|reduce|lower|turn down|turn it down|halve|by half|by about half|half)\\b", &m))
    {
        f.scale = find (t, "\\bsubtler?\\b") ? 0.4f : find (t, "(little less|bit less|a little|slightly|a bit)") ? 0.7f : 0.5f;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\b(too much|a little less|bit less|less|tone it down|pull it back|pull back|make it subtle|subtle|subtler|not so much|too far|too strong|reduce|lower|turn down|turn it down|halve|by about half|by half|half)\\b"), " ");
    }
    f.more = find (t, "\\b(more|stronger|further|push it|even more)\\b");
    if (find (t, "\\bad-? ?libs?\\b|\\bbacking\\b", &m)) { f.role = "adlib"; f.heard.add (m); }
    else if (find (t, "\\b(lead|main vocal)\\b")) f.role = "lead";
    if (find (t, "\\b(reverb|echo|delay|ambience|ambiance)\\b", &m))
    {
        if (find (t, "\\breverb\\b"))
            f.notes.add ("GlitchKid has no reverb, so space comes from Echo and Dimension.");
        f.ops[depth] += mag;
        f.heard.add (m);
    }
    for (const auto& lx : lexicon)
    {
        if (! find (t, lx.pattern, &m))
            continue;
        f.heard.add (juce::String (m).trim());
        for (int d = 0; d < numDims; ++d)
            f.ops[(size_t) d] += lx.ops[(size_t) d] * mag;
        if (lx.flag != nullptr)
            f.flags.addIfNotAlreadyThere (lx.flag);
        if (lx.ops[clean] > 0.0f && find (t, "\\b(muddy|mud|boxy)\\b"))
            f.flags.addIfNotAlreadyThere ("mud");
    }
    if (find (t, "\\b(natural|transparent|realistic|honest)\\b", &m)) { f.flags.addIfNotAlreadyThere ("natural"); f.heard.add (m); }
    if (find (t, "\\b(louder|quieter|volume|loud)\\b")) f.notes.add ("Level stays yours: I never touch Gain, Mix or the limiter.");
    if (find (t, "\\bsits? (?:better )?(?:with|in|on) (?:the )?(?:beat|mix|track|instrumental)\\b") || find (t, "\\b(the beat|my beat|the instrumental)\\b"))
        f.notes.add ("I only hear this vocal (and the beat, if you route it to GlitchKid's instrumental input on BEAT), not your whole project.");
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------

Director::Director (automode::RangeLookup r) : ranges (std::move (r)) {}

bool Director::isAllowed (const juce::String& id)
{
    return ! (id.startsWith ("lim_") || id == "gain" || id == "mix" || id == "bypass") && id.isNotEmpty(); // level and the safety limiter stay the artist's
}

int Director::stageOf (const juce::String& id)
{
    if (id.startsWith ("tune_")) return 1;
    if (id == "formant_shift") return 2;
    if (id.startsWith ("harm_")) return 3;
    if (id.startsWith ("mut_")) return 4;
    if (id.startsWith ("dbl_")) return 5;
    if (id.startsWith ("eq_")) return 6;
    if (id == "clean_ess") return 9;
    if (id.startsWith ("clean_")) return 7;
    if (id.startsWith ("comp_")) return 8;
    if (id.startsWith ("sat_")) return 10;
    if (id.startsWith ("air_")) return 11;
    if (id.startsWith ("stutter_")) return 12;
    if (id.startsWith ("echo_")) return 13;
    if (id.startsWith ("dim_")) return 14;
    if (id == "gain") return 15;
    if (id == "mix") return 16;
    if (id.startsWith ("lim_")) return 17;
    return 0;
}

bool Director::needsAnalysis (const IntentFrame& f) { return f.improve || f.buildMix || f.ask == IntentFrame::Ask::analysis || f.ops[clean] > 0.0f; }

int Director::selectedCount() const
{
    int n = 0;
    for (const auto& r : rows)
        n += r.selected ? 1 : 0;
    return n;
}

void Director::begin (const SoundState& current)
{
    base = current;
    rebuild();
}

void Director::updateDirection (const IntentFrame& f)
{
    std::vector<Dim> touched;
    for (int d = 0; d < numDims; ++d)
        if (std::abs (f.ops[(size_t) d]) > 1e-6f)
            touched.push_back ((Dim) d);
    if (f.reset)
    {
        dir = {};
        locks.clear();
        flags.clear();
        roleName = "lead";
        unticked.clear();
        lastTouched.clear();
        foundation.clear();
        profileIndex = tuneChoice = cleanChoice = 0;
    }
    if (f.profile > 0)
    {
        const auto& p = producerProfile (f.profile);
        profileIndex = f.profile;
        dir = p.direction; // a profile replaces the direction; the words in the same sentence are added on top
        tuneChoice = p.tune;
        if (p.flag != nullptr) flags.insert (p.flag);
        lastTouched.clear();
        for (int d = 0; d < numDims; ++d)
            if (std::abs (dir[(size_t) d]) > 1e-6f)
                lastTouched.push_back ((Dim) d);
    }
    if (f.tune != 0) tuneChoice = f.tune;
    if (f.cleanLevel > 0) cleanChoice = f.cleanLevel;
    if (f.scale > 0.0f && ! touched.empty())
        for (auto d : touched) // "a little less dark": scale what is there; "less reverb" when KID never added any: move the other way, as far as the word says
            dir[(size_t) d] = std::abs (dir[(size_t) d]) > 0.1f ? round2 (dir[(size_t) d] * f.scale) : round2 (-(1.0f - f.scale));
    else
    {
        for (auto d : touched)
            dir[(size_t) d] = clampDir (dir[(size_t) d] + f.ops[(size_t) d]);
        if (f.scale > 0.0f)
            for (auto d : lastTouched)
                dir[(size_t) d] = round2 (dir[(size_t) d] * f.scale);
        if (f.more && touched.empty())
            for (auto d : lastTouched)
                dir[(size_t) d] = clampDir (round2 (dir[(size_t) d] * 1.5f));
    }
    if (f.towardProfile)
    {
        if (profileIndex > 0)
            for (int d = 0; d < numDims; ++d)
                dir[(size_t) d] = clampDir (round2 (dir[(size_t) d] + 0.5f * producerProfile (profileIndex).direction[(size_t) d]));
        else
            for (auto d : lastTouched)
                dir[(size_t) d] = clampDir (round2 (dir[(size_t) d] * 1.5f));
    }
    if (f.raw) // less polish: half the brightness, punch and cleanup KID added, natural tuning, no Lab effects
    {
        for (auto d : { bright, punch, clean, presence })
            if (dir[(size_t) d] > 0.0f) dir[(size_t) d] = round2 (dir[(size_t) d] * 0.5f);
        flags.insert ("natural");
        if (tuneChoice > 1) tuneChoice = 1;
        if (cleanChoice > 1) cleanChoice = 1;
    }
    if (f.flags.contains ("natural") && tuneChoice > 1)
        tuneChoice = 1;
    for (auto& l : f.locks) locks.insert (l);
    for (auto& fl : f.flags) flags.insert (fl);
    if (f.role.isNotEmpty())
        roleName = f.role;
    if (locks.count ("presence") > 0 && dir[presence] < 0.0f)
        dir[presence] = 0.0f;
    if (! touched.empty())
        lastTouched = touched;
}

std::vector<Change> Director::decide()
{
    refusedIds.clear();
    std::map<juce::String, float> T;
    std::map<juce::String, juce::String> W, D;
    const auto& d = dir;
    const bool front = locks.count ("front") > 0, adlib = roleName == "adlib", keepPresence = locks.count ("presence") > 0;
    auto B = [this] (const char* id) { return base.get (id, 0.0f); };
    auto cur = [&] (const char* id) { auto it = T.find (id); return it != T.end() ? it->second : B (id); };
    auto put = [&] (const juce::String& id, float v, const juce::String& why, const juce::String& detail = {})
    {
        automode::Range r;
        if (! isAllowed (id) || ! ranges || ! ranges (id, r) || ! std::isfinite (v))
        {
            refusedIds.addIfNotAlreadyThere (id); // the validator: allowlist, known id, finite value
            return;
        }
        v = juce::jlimit (r.lo, r.hi, v);
        if (r.snap)
            v = r.snap (v);
        T[id] = v;
        auto& w = W[id];
        w = w.isEmpty() || w == why ? why : w + "; " + why;
        if (detail.isNotEmpty())
        {
            auto& dd = D[id];
            dd = dd.isEmpty() || dd == detail ? detail : dd + " " + detail;
        }
    };
    for (const auto& c : foundation) // Build My Mix: the profile's foundation first; everything below moves from there
        put (c.id, c.to, "Foundation (" + foundationName + "): " + c.reason);
    auto add = [&] (const char* id, float dv, const juce::String& why, const juce::String& detail = {}) { put (id, cur (id) + dv, why, detail); };
    auto k = [] (float x) { return juce::jmin (1.0f, std::abs (x) / 1.5f); }; // how far toward a goal: 1.5 units of direction = all the way
    auto toward = [&] (const char* id, float goal, float amount, const char* why) { const float c = cur (id); if (std::abs (goal - c) > 1e-6f) put (id, c + (goal - c) * amount, why); };
    auto atMost = [&] (const char* id, float cap, const juce::String& why) { if (cur (id) > cap) put (id, cap, why); };
    auto atLeast = [&] (const char* id, float floor, const juce::String& why, const juce::String& detail = {}) { if (cur (id) < floor) put (id, floor, why, detail); };
    auto hzText = [] (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; };

    // ---- what the recording needs (the diagnosis), before any taste: corrections use only what was measured or estimated, each with its evidence ----
    const auto* fRumble = diag.find ("rumble");
    const auto* fMud = diag.find ("mud");
    const auto* fHarsh = diag.find ("harsh");
    const auto* fEss = diag.find ("sibilance");
    const auto* fDull = diag.find ("dull");
    const bool alreadyBright = diag.has ("bright"), squashed = diag.has ("compressed"), noisy = diag.has ("noise");
    if (fRumble != nullptr || fMud != nullptr || fEss != nullptr || (fHarsh != nullptr && fHarsh->narrow))
    {
        put ("clean_on", 1.0f, "CLEAN on to fix what I found in your recording");
        atLeast ("clean_amount", 55.0f, "CLEAN works firmly enough to matter");
    }
    if (fRumble != nullptr)
        atLeast ("clean_tone", 85.0f, "Rumble: " + fRumble->plain, fRumble->detail);
    if (fMud != nullptr)
    {
        atLeast ("clean_tone", 90.0f, "Mud: CLEAN's low-mid bell (wide, up to -3.5 dB) finds the exact frequency while it listens", fMud->detail);
        if (fMud->hz > 0.0f && fMud->severity >= 0.5f && std::abs (B ("eq_b1_gain")) < 0.05f) // strong and located: one gentle extra cut where it was measured
        {
            put ("eq_b1_freq", fMud->hz, "Body band moved to " + hzText (fMud->hz) + ", where the build-up is", fMud->detail);
            put ("eq_b1_q", 1.0f, "A wide bell, so it doesn't hollow out the voice");
            put ("eq_b1_gain", -2.0f, "A gentle 2 dB cut at " + hzText (fMud->hz) + ": less mud, the body stays");
        }
    }
    if (fHarsh != nullptr && std::abs (B ("eq_b2_gain")) < 0.05f && fHarsh->hz > 0.0f)
    {
        const float cut = fHarsh->narrow ? 2.0f + 2.0f * fHarsh->severity : 1.5f + 1.5f * fHarsh->severity;
        put ("eq_b2_freq", fHarsh->hz, fHarsh->narrow ? "Presence band on the sharp spot at " + hzText (fHarsh->hz) : "Presence band centred on the forward upper mids (" + hzText (fHarsh->hz) + ")", fHarsh->detail);
        put ("eq_b2_q", fHarsh->narrow ? 2.5f : 0.9f, fHarsh->narrow ? "A narrower bell: only the sharp spot, not your whole top" : "A wide bell: a broad tilt, not one ringing frequency");
        put ("eq_b2_gain", -cut, "Upper mids eased by " + juce::String (cut, 1) + " dB: smoother, without going dull");
        if (fHarsh->narrow) atLeast ("clean_res", 100.0f, "CLEAN's resonance finder catches narrow ringing while your voice moves (Q 6, up to -6 dB)");
    }
    else if (fHarsh != nullptr && fHarsh->hz <= 0.0f && std::abs (B ("eq_b2_gain")) < 0.05f) // the five-band live read: no frequency to aim at, so only a gentle wide cut at the band's default
        put ("eq_b2_gain", -1.5f, "Upper mids eased 1.5 dB (wide bell at " + hzText (cur ("eq_b2_freq")) + ")", fHarsh->detail);
    if (fEss != nullptr)
    {
        atLeast ("clean_ess", 100.0f, "De-esser: it only acts while an S is sounding (4.5-12.5 kHz, up to -9 dB)", fEss->detail);
        put ("clean_on", 1.0f, "CLEAN on to fix what I found in your recording");
    }
    if (fDull != nullptr && fHarsh == nullptr && d[bright] >= 0.0f)
        add ("eq_hi_gain", 1.5f + fDull->severity, "A gentle lift above 8 kHz brings back air", fDull->detail);
    if (const auto* fDyn = diag.find ("dynamic"))
    {
        if (cur ("comp_ratio") < 3.0f) put ("comp_ratio", 3.0f, "The level jumps around: steady 3:1 compression", fDyn->detail);
        if (known.rmsDb > -60.0f)
            put ("comp_thresh", juce::jlimit (-40.0f, -8.0f, known.rmsDb + 3.0f), "Threshold set from your real level (RMS " + juce::String (known.rmsDb, 1) + " dBFS): the compressor works on the loud words");
    }
    if (squashed) atMost ("comp_ratio", 2.0f, "Your take is already compressed: only light compression on top");
    if (noisy)
    {
        atMost ("comp_ratio", 3.0f, "Gentle compression: harder settings would lift the background noise");
        atMost ("comp_makeup", 2.0f, "Little makeup gain, so the background stays down");
    }

    if (std::abs (d[bright]) > 1e-6f)
    {
        if (d[bright] > 0 && alreadyBright)
            add ("eq_hi_gain", 1.2f * d[bright], "Air shelf up only a little: your vocal is already bright on top");
        else
            add ("eq_hi_gain", 3.0f * d[bright], d[bright] > 0 ? "Air shelf up: more top end" : "Air shelf down: less top end");
        if ((d[bright] > 0 && fHarsh == nullptr) || (d[bright] < 0 && ! keepPresence)) // no presence lift on upper mids that are already sharp
            add ("eq_b2_gain", d[bright], d[bright] > 0 ? "Presence lifts with the top" : "Presence eases with the top");
        if (flags.count ("air") > 0 && d[bright] > 0)
        {
            put ("air_on", 1.0f, "AIR LAB on: an open, breathy top");
            if (cur ("air_air") < 80.0f) toward ("air_air", 80.0f, k (d[bright]), "More AIR LAB air");
        }
    }
    if (std::abs (d[warm]) > 1e-6f)
    {
        add ("sat_drive", 1.5f * d[warm], "Saturation: density and warmth");
        if (d[warm] > 0 && fMud != nullptr) {} // warmth from saturation only: the low mids are already built up
        else if (locks.count ("warm") == 0 || d[warm] > 0) add ("eq_lo_gain", d[warm], "Low body");
    }
    if (std::abs (d[presence]) > 1e-6f)
    {
        if (d[presence] > 0 && fHarsh != nullptr)
            add ("eq_b2_gain", 0.6f * d[presence], "Presence band up only a little: your upper mids are already sharp");
        else
            add ("eq_b2_gain", 1.5f * d[presence], d[presence] > 0 ? "Presence band: the words cut through" : "Presence down: the voice steps back");
    }
    if (d[punch] > 0)
    {
        const float goal = squashed ? 2.5f : noisy ? 3.5f : 6.0f;
        if (cur ("comp_ratio") < goal) toward ("comp_ratio", goal, k (d[punch]), squashed ? "A little firmer (your take is already compressed)" : noisy ? "Firmer, but held back so the background noise stays down" : "Firmer compression: steadier and harder");
        if (cur ("comp_attack") > 2.0f) toward ("comp_attack", 2.0f, k (d[punch]), "Faster attack grabs the front of words");
        add ("comp_thresh", (noisy || squashed ? -1.5f : -3.0f) * d[punch], "The compressor works harder");
        add ("sat_drive", 1.5f * d[punch], "Saturation: grit and energy");
    }
    if (d[punch] < 0)
    {
        if (cur ("comp_ratio") > 1.0f) toward ("comp_ratio", 1.0f, k (d[punch]), "Gentler compression");
        if (cur ("comp_attack") < 40.0f) toward ("comp_attack", 40.0f, k (d[punch]), "Slower attack: a softer front");
    }
    if (d[width] > 0)
    {
        const float a = k (d[width]);
        put ("dbl_on", 1.0f, "Doubler on: two copies, left and right");
        if (cur ("dbl_width") < 92.0f) toward ("dbl_width", 92.0f, a, "Doubles spread wider");
        if (front) atMost ("dbl_amount", 28.0f, "Doubles kept under you: you stay in front");
        else if (cur ("dbl_amount") < 45.0f) toward ("dbl_amount", 45.0f, a, "Doubles loud enough to hear");
        put ("dim_on", 1.0f, "Dimension on: width and movement");
        if (cur ("dim_width") < 88.0f) toward ("dim_width", 88.0f, a, "Wider Dimension");
        if (front) atMost ("dim_mix", 30.0f, "Dimension kept light: you stay in front");
        else if (cur ("dim_mix") < 55.0f) toward ("dim_mix", 55.0f, a, "Dimension clearly audible");
    }
    if (d[width] < 0)
    {
        if (cur ("dbl_on") >= 0.5f && d[width] <= -0.75f) put ("dbl_on", 0.0f, "Doubler off: one voice in the middle");
        else if (cur ("dbl_on") >= 0.5f) put ("dbl_amount", cur ("dbl_amount") * (1.0f + d[width]), "Quieter doubles");
        if (cur ("dim_on") >= 0.5f && d[width] <= -0.75f) put ("dim_on", 0.0f, "Dimension off");
    }
    if (d[depth] > 0)
    {
        const float a = k (d[depth]);
        put ("echo_on", 1.0f, "Echo on: space behind the voice");
        put ("echo_sync", 1.0f, "Echo locked to your tempo");
        if (B ("echo_on") < 0.5f || adlib) put ("echo_div", adlib ? 1.0f : 4.0f, adlib ? "1/4: long throws for ad-libs" : "1/8: repeats that move with the beat");
        if (front) atMost ("echo_mix", 12.0f, "Echo kept low: you stay in front");
        else if (cur ("echo_mix") < (adlib ? 40.0f : 32.0f)) toward ("echo_mix", adlib ? 40.0f : 32.0f, a, "More echo: space around you");
        if (cur ("echo_fb") < 55.0f) toward ("echo_fb", 55.0f, a, "Repeats that linger");
        if (cur ("echo_tone") > 2200.0f) toward ("echo_tone", 2200.0f, a, "Darker repeats sit further back");
        if (! front)
        {
            put ("dim_on", 1.0f, "Dimension adds haze");
            if (cur ("dim_mix") < 55.0f) toward ("dim_mix", 55.0f, a, "Haze around the voice");
        }
        if (adlib)
        {
            add ("eq_lo_gain", -3.0f, "Ad-libs: thinner low end, behind the lead");
            add ("eq_hi_gain", -1.5f, "Ad-libs: a softer top, further away");
        }
        if (! keepPresence) add ("eq_b2_gain", -0.75f * d[depth], "Presence eased: the voice steps back");
    }
    if (d[depth] <= -0.75f)
    {
        if (cur ("echo_on") >= 0.5f) put ("echo_on", 0.0f, "Echo off: nothing behind you");
        if (cur ("dim_on") >= 0.5f && ! (d[width] > 0)) put ("dim_on", 0.0f, "Dimension off: close and direct");
    }
    else if (d[depth] < 0) // "less reverb": the space that is there, scaled down by as much as was asked
    {
        const float f = 1.0f + d[depth];
        if (cur ("echo_on") >= 0.5f) put ("echo_mix", cur ("echo_mix") * f, "Less echo: about " + juce::String (juce::roundToInt ((1.0f - f) * 100.0f)) + " % down");
        if (cur ("dim_on") >= 0.5f && ! (d[width] > 0)) put ("dim_mix", cur ("dim_mix") * f, "Less Dimension haze");
    }
    if (front && std::abs (d[width]) < 1e-6f && std::abs (d[depth]) < 1e-6f)
    {
        atMost ("echo_mix", 12.0f, "Echo kept low: you stay in front");
        atMost ("dim_mix", 30.0f, "Dimension kept light: you stay in front");
        atMost ("dbl_amount", 28.0f, "Doubles kept under you");
    }
    if (d[clean] > 0)
    {
        put ("clean_on", 1.0f, "CLEAN on: rumble, mud, resonances and harsh S");
        if (cur ("clean_amount") < 85.0f) toward ("clean_amount", 85.0f, k (d[clean]), "More cleaning");
        if (flags.count ("mud") > 0 && known.mudHz > 0.0f && fMud == nullptr) // only where CLEAN really measured it; otherwise CLEAN's own bell finds it
        {
            put ("eq_b1_freq", known.mudHz, "Body band moved to " + hzText (known.mudHz) + ", where CLEAN measured the mud");
            put ("eq_b1_gain", juce::jmin (B ("eq_b1_gain"), -2.0f * d[clean]), "Body band cut: less mud");
            put ("clean_tone", 100.0f, "Corrective EQ at full strength");
        }
        if (flags.count ("ess") > 0) put ("clean_ess", 100.0f, "De-esser at full strength");
    }
    if (d[exp] > 0 && locks.count ("char") == 0)
    {
        const float a = k (d[exp]);
        if (flags.count ("glitch") > 0)
        {
            put ("stutter_on", 1.0f, "Stutter on: tempo-locked slices");
            put ("stutter_rate", 2.0f, "1/16 slices");
            if (B ("stutter_on") < 0.5f) put ("stutter_mix", 30.0f + 20.0f * d[exp], "Mixed in, not replacing you");
        }
        else
        {
            put ("mut_on", 1.0f, "Mutation on: the voice bends into something new");
            if (cur ("mut_amount") < 80.0f) toward ("mut_amount", 80.0f, a, "More mutation");
            if (cur ("mut_character") < 75.0f) toward ("mut_character", 75.0f, a, "More extreme character");
            if (B ("mut_on") < 0.5f) put ("mut_blend", 40.0f + 20.0f * d[exp], "Some of your real voice stays");
        }
    }
    if (cleanChoice > 0) // Clean My Vocal: CLEAN LAB only (rumble, low-mid build-up, resonances, de-esser), conservative unless asked
    {
        static const char* level[] = { "Gentle", "Balanced", "Aggressive" };
        static const float amount[] = { 35.0f, 60.0f, 90.0f }, trim[] = { 70.0f, 100.0f, 100.0f };
        const int i = juce::jlimit (1, 3, cleanChoice) - 1;
        const juce::String why = "Clean My Vocal (" + juce::String (level[i]) + ")";
        put ("clean_on", 1.0f, why + ": CLEAN on");
        put ("clean_amount", amount[i], why + ": how hard CLEAN may work");
        for (auto* id : { "clean_tone", "clean_res", "clean_ess" })
            put (id, trim[i], why + juce::String (i == 0 ? ": each section held back" : ": each section at full strength"));
    }
    if (tuneChoice > 0)
    {
        static const char* why[] = { "Natural tuning: gentle correction with a slow glide", "Light tuning: firm but smooth correction", "Hard tuning: every note snaps to the scale" };
        SoundState t;
        macros::apply ((macros::TuneStyle) (juce::jlimit (1, 3, tuneChoice) - 1), t);
        for (auto* id : { "tune_on", "tune_strength", "tune_speed" })
            put (id, t.get (id, 0.0f), why[juce::jlimit (1, 3, tuneChoice) - 1]);
        if (keyKnown) // only from a reliable estimate of a loaded vocal file
        {
            put ("tune_key", (float) keyRoot, "Key estimated from your vocal file");
            put ("tune_scale", keyIsMinor ? 2.0f : 1.0f, "Scale estimated from your vocal file");
        }
    }
    else if (tuneChoice < 0 && B ("tune_on") >= 0.5f)
        put ("tune_on", 0.0f, "Pitch Tune off: your natural pitch");
    if (flags.count ("natural") > 0)
    {
        if (cur ("sat_drive") > 2.0f) put ("sat_drive", 2.0f, "Natural: light saturation only");
        if (cur ("echo_on") >= 0.5f) atMost ("echo_mix", 12.0f, "Natural: echo stays subtle");
        if (locks.count ("char") == 0)
            for (auto* id : { "mut_on", "stutter_on", "harm_on" })
                if (B (id) >= 0.5f) put (id, 0.0f, "Natural: Lab effect off");
    }

    std::vector<Change> out;
    for (auto& [id, v] : T)
    {
        const float from = base.get (id, v);
        if (std::abs (v - from) > 1e-4f)
            out.push_back ({ id, from, v, W[id], D[id] });
    }
    return out;
}

void Director::rebuild()
{
    auto next = decide();
    for (auto& r : next)
    {
        r.selected = unticked.count (r.id) == 0;
        const auto it = previous.find (r.id);
        r.tag = manual.count (r.id) > 0 ? Change::Tag::manual
              : it == previous.end() ? Change::Tag::added
              : r.to > it->second + 1e-4f ? Change::Tag::up
              : r.to < it->second - 1e-4f ? Change::Tag::down : Change::Tag::same;
    }
    removedIds.clear();
    for (auto& [id, v] : previous)
        if (std::none_of (next.begin(), next.end(), [&] (const Change& c) { return c.id == id; }))
            removedIds.add (id);
    std::stable_sort (next.begin(), next.end(), [] (const Change& a, const Change& b) { return stageOf (a.id) < stageOf (b.id); });
    rows = std::move (next);
    previous.clear();
    for (auto& r : rows)
        previous[r.id] = r.to;
}

void Director::setFacts (const AnalysisFacts& f)
{
    if (! f.valid)
        return;
    known = f;
    diag = diagnose (f);
    if (f.keyReliable)
    {
        keyKnown = true;
        keyRoot = f.keyTonic;
        keyIsMinor = f.keyMinor;
    }
    rebuild();
}

juce::String Director::respond (IntentFrame f, const AnalysisFacts* facts)
{
    if (facts != nullptr && facts->valid)
        setFacts (*facts);
    if (f.ask != IntentFrame::Ask::none && ! f.anyOps())
        return answer (f); // a question: the sound does not change
    if ((f.improve || f.buildMix) && known.valid && diag.has ("offpitch") && f.tune == 0 && tuneChoice == 0)
        f.tune = 1; // notes drift: gentle correction, never more unless asked
    updateDirection (f);
    rebuild();
    return reply (f, facts);
}

juce::String Director::reply (const IntentFrame& f, const AnalysisFacts*) const
{
    if (! f.understood())
        return "I didn't catch a sound in that. Try \"warmer\", \"less reverb\", \"make the tuning harder\", or ask me \"why did you change the high mids?\"";
    juce::StringArray parts;
    if (f.reset) parts.add ("Back to where we started: your sound before my mix.");
    if (f.artist.isNotEmpty()) parts.add (artistDisclaimer (f.artist, f.profile));
    else if (f.profile > 0) parts.add ("Going for " + juce::String (producerProfile (f.profile).name) + ": " + producerProfile (f.profile).tagline);
    const bool touched = f.anyOps();
    if (f.improve || f.buildMix)
    {
        if (known.valid)
        {
            juce::StringArray heard;
            std::vector<const Finding*> top;
            for (auto& x : diag.items)
                if (x.id != "key") top.push_back (&x);
            std::stable_sort (top.begin(), top.end(), [] (const Finding* a, const Finding* b) { return a->severity > b->severity; });
            for (size_t i = 0; i < top.size() && i < 3; ++i) heard.add (top[i]->plain);
            parts.add ((known.fromFile ? juce::String ("I read your file. ") : "I listened to " + juce::String (known.activeSeconds, 1) + " s of your vocal. ")
                       + (heard.isEmpty() ? juce::String ("Nothing in the recording needs fixing.") : heard.joinIntoString (" ")));
        }
        else
            parts.add ("I haven't heard your vocal yet, so this isn't fitted to it: press play in your project (or load the vocal file), then UPDATE MY MIX.");
    }
    else if (touched)
    {
        juce::StringArray w;
        for (int d = 0; d < numDims; ++d)
            if (std::abs (f.ops[(size_t) d]) > 1e-6f)
                w.addIfNotAlreadyThere (dimWords[d][f.ops[(size_t) d] > 0 ? 0 : 1]);
        auto s = f.scale > 0.0f ? juce::String ("Less of that: ") + (f.scale <= 0.4f ? "about 40 % of what it was." : f.scale <= 0.5f ? "about half." : "eased a little.")
                                : w.joinIntoString (", ") + ".";
        parts.add (s.substring (0, 1).toUpperCase() + s.substring (1));
    }
    if (f.scale > 0.0f && ! touched)
        parts.add (f.scale <= 0.4f ? "Made it subtle: about 40 % of what it was." : f.scale <= 0.5f ? "Pulled it back by half." : "Eased it off a little.");
    if (f.more && ! touched) parts.add ("Pushed it further.");
    if (f.towardProfile) parts.add (profileIndex > 0 ? "Further toward " + juce::String (producerProfile (profileIndex).name) + "." : juce::String ("Pushed it further."));
    if (f.raw) parts.add ("Less polish: half the brightness, punch and cleanup I added, natural tuning, no Lab effects.");
    if (f.locks.contains ("front")) parts.add ("You stay in front: width from the doubles, echo kept low, Presence untouched.");
    else if (f.locks.contains ("presence")) parts.add ("Presence stays where it is.");
    if (f.locks.contains ("char")) parts.add ("Your Lab effects stay as they are.");
    if (f.role == "adlib") parts.add ("For the ad-libs: further back, thinner, with longer throws.");
    if (f.flags.contains ("natural") && ! f.raw) parts.add ("Keeping it natural: light saturation, gentle tuning, no Lab effects.");
    if (f.tune != 0 || f.profile > 0 || ((f.improve || f.buildMix) && tuneChoice > 0))
    {
        static const char* style[] = { "natural", "light", "hard" };
        if (tuneChoice < 0) parts.add ("Pitch correction off.");
        else if (tuneChoice > 0)
            parts.add ("Pitch correction: " + juce::String (style[juce::jlimit (1, 3, tuneChoice) - 1]) + ", "
                       + (keyKnown ? "in " + analysis::keyName (keyRoot, keyIsMinor) + " (my estimate from your file: check it on TUNE)."
                                   : juce::String ("in the key set on TUNE: I can't tell your key yet, so load the vocal file or set it there.")));
    }
    if (f.cleanLevel > 0)
    {
        static const char* level[] = { "gentle", "balanced", "aggressive" };
        parts.add ("Cleanup (" + juce::String (level[juce::jlimit (1, 3, cleanChoice) - 1]) + "): rumble, low-mid build-up, resonances and sharp S's. It isn't noise reduction: background noise, room sound and clipping stay.");
        if (cleanChoice >= 3) parts.add ("Aggressive can thin your tone: compare with ORIGINAL.");
    }
    if ((f.improve || f.buildMix || f.profile > 0) && ! rows.empty()) parts.add (summary());
    parts.addArray (f.notes);
    if (rows.empty() && ! f.reset) parts.add ("That's already where your sound is: nothing to change.");
    return parts.joinIntoString (" ");
}

juce::String Director::answer (const IntentFrame& f) const
{
    using Ask = IntentFrame::Ask;
    auto rowText = [] (const Change& r)
    {
        auto t = r.reason + (r.detail.isNotEmpty() ? " (" + r.detail.trimCharactersAtEnd (".") + ")" : juce::String());
        return t.endsWithChar ('.') ? t : t + ".";
    };
    switch (f.ask)
    {
        case Ask::why:
        {
            juce::StringArray prefixes;
            for (const auto& tp : topics)
                if (f.topic == tp.name) prefixes.addTokens (tp.ids, " ", "");
            std::vector<const Change*> hits;
            for (auto& r : rows)
            {
                bool match = prefixes.isEmpty();
                for (auto& p : prefixes) match = match || r.id.startsWith (p);
                if (r.selected && match) hits.push_back (&r);
            }
            std::stable_sort (hits.begin(), hits.end(), [] (const Change* a, const Change* b) // the main move first: a switch or a gain, with its evidence
            {
                auto rank = [] (const Change* c) { return (c->detail.isNotEmpty() ? 0 : 2) + (c->id.endsWith ("_on") || c->id.endsWith ("_gain") || c->id.endsWith ("_strength") ? 0 : 1); };
                return rank (a) < rank (b);
            });
            juce::StringArray said;
            for (auto* r : hits)
                if (said.size() < 3) said.addIfNotAlreadyThere (rowText (*r));
            if (said.isEmpty())
                return f.topic.isNotEmpty() ? "I didn't change the " + f.topic + " in this mix." + (rows.empty() ? juce::String (" There's no mix from me yet.") : juce::String())
                                            : juce::String ("There's nothing in my mix to explain yet.");
            return said.joinIntoString (" ");
        }
        case Ask::changes:
        {
            if (rows.empty()) return "I haven't changed anything yet.";
            juce::StringArray why;
            for (auto& r : rows)
                if (r.selected && why.size() < 4) why.addIfNotAlreadyThere (r.reason + ".");
            return summary() + " The main reasons: " + why.joinIntoString (" ") + " Ask me why about any part.";
        }
        case Ask::analysis:
        {
            if (! known.valid) return "I haven't heard your vocal yet: load the vocal file, or press play in your project, then ask again.";
            juce::StringArray out;
            out.add ("Measured: " + juce::String (known.activeSeconds, 1) + " s of voice" + (known.peakDb > -120.0f ? ", peaks at " + juce::String (known.peakDb, 1) + " dBFS" : juce::String())
                     + (known.rmsDb > -120.0f ? ", level " + juce::String (known.rmsDb, 1) + " dBFS RMS" : juce::String()) + ".");
            for (auto& x : diag.items)
                out.add (juce::String (x.measured ? "Measured: " : "Estimate: ") + x.plain);
            if (diag.empty()) out.add ("Nothing stands out: the recording is in good shape to mix.");
            if (! known.fromFile) out.add ("This is the live five-band read; load the vocal file for pitch, key, noise and a finer tone read.");
            return out.joinIntoString (" ");
        }
        case Ask::key:
            if (keyKnown) return "Probably " + analysis::keyName (keyRoot, keyIsMinor) + ": an estimate from the held notes in your file. Check it on TUNE before you rely on it.";
            return known.fromFile ? "I can't tell the key reliably from this take (rap, short phrases and few different notes make it ambiguous), so I won't guess. Set it on TUNE."
                                  : "Load your vocal file and I'll estimate it. The live input doesn't give me enough to name a key.";
        case Ask::help:
            return "Tell me the sound you want (\"polished emotional melodic rap with a little atmosphere\", \"less reverb\", \"make the tuning harder\") and I build it from GlitchKid's own effects, "
                   "fitted to what I measured in your vocal. Ask \"why\" about any change, \"what did you change\", or \"analyze my vocal\". I run on this computer; no internet is needed.";
        case Ask::none: break;
    }
    return {};
}

void Director::setFoundation (const automode::MixProposal& p)
{
    foundation.clear();
    foundationName = {};
    if (! p.usable)
        return;
    foundation = p.changes;
    foundationName = p.profileName;
}

juce::String Director::summary() const
{
    auto row = [this] (const char* id) -> const Change* { for (auto& r : rows) if (r.selected && r.id == id) return &r; return nullptr; };
    auto any = [this] (const char* prefix) { for (auto& r : rows) if (r.selected && r.id.startsWith (prefix)) return true; return false; };
    juce::StringArray did;
    if (auto* on = row ("tune_on"); on != nullptr && on->to < 0.5f) did.add ("switched pitch correction off");
    else if (any ("tune_"))
    {
        const auto* st = row ("tune_strength");
        const float strength = st != nullptr ? st->to : base.get ("tune_strength", 75.0f);
        did.add (juce::String (strength <= 55.0f ? "added gentle" : strength <= 80.0f ? "added smooth" : "added hard") + " pitch correction");
    }
    if (any ("clean_")) did.add ("cleaned rumble, mud, resonances and sharp S's");
    if (any ("comp_"))
    {
        const auto* ratio = row ("comp_ratio");
        did.add (ratio != nullptr && ratio->to < ratio->from ? "eased the compression" : "controlled the dynamics");
    }
    if (any ("eq_")) did.add ("shaped the tone");
    if (auto* sat = row ("sat_drive")) did.add (sat->to > sat->from ? "added saturation" : "pulled the saturation back");
    if (any ("air_")) did.add ("opened up the top");
    if (any ("dbl_") || any ("dim_"))
    {
        const auto* d1 = row ("dbl_on"); const auto* d2 = row ("dim_on");
        did.add ((d1 != nullptr && d1->to < 0.5f) || (d2 != nullptr && d2->to < 0.5f) ? "narrowed the image" : "widened it with doubles and Dimension");
    }
    if (any ("echo_"))
    {
        const auto* e = row ("echo_on");
        did.add (e != nullptr && e->to < 0.5f ? "took the echo out" : "added echo for space");
    }
    for (auto [id, what] : { std::pair { "mut_on", "Mutation" }, std::pair { "stutter_on", "Stutter" }, std::pair { "harm_on", "Harmony" } })
        if (auto* r = row (id))
            did.add ((r->to >= 0.5f ? juce::String ("added ") : juce::String ("switched off ")) + what);
    if (did.isEmpty())
        return "Nothing to change.";
    auto text = did.size() == 1 ? did[0] : did.joinIntoString (", ", 0, did.size() - 1) + " and " + did[did.size() - 1];
    return text.substring (0, 1).toUpperCase() + text.substring (1) + ".";
}

void Director::setSelected (const juce::String& id, bool on)
{
    if (on) unticked.erase (id); else unticked.insert (id);
    for (auto& r : rows)
        if (r.id == id)
            r.selected = on;
}

void Director::noteManualEdit (const SoundState& now)
{
    if (rows.empty())
    {
        base = now;
        return;
    }
    for (auto& r : rows)
        if (std::abs (now.get (r.id, r.from) - r.from) > 1e-4f)
        {
            unticked.insert (r.id);
            manual.insert (r.id);
        }
    base = now;
    rebuild();
}

SoundState Director::plan (const SoundState& now, const std::set<juce::String>& keepAsIs) const
{
    SoundState s = base; // the session's starting sound, with the plan on top
    for (auto& r : rows)
        if (r.selected)
            s.set (r.id, r.to);
    for (auto& e : s.values) // level, the limiter, Bypass and anything moved by hand stay exactly as they are now
        if (! isAllowed (e.id) || keepAsIs.count (e.id) > 0)
            e.value = now.get (e.id, e.value);
    return s;
}

SoundState Director::selectedChanges() const
{
    SoundState s;
    for (auto& r : rows)
        if (r.selected)
            s.set (r.id, r.to);
    return s;
}

void Director::accepted (const SoundState& now)
{
    base = now;
    dir = {};
    lastTouched.clear();
    unticked.clear();
    manual.clear();
    previous.clear();
    rows.clear();
    removedIds.clear();
    foundation.clear();
    foundationName = {};
    profileIndex = tuneChoice = cleanChoice = 0;
}

void Director::cancel() { accepted (base); }

void Director::startOver()
{
    accepted (base);
    locks.clear();
    flags.clear();
    roleName = "lead";
}
}
