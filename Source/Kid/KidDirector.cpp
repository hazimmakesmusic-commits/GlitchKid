#include "KidDirector.h"
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
    { "\\b(rapper|rap|bars)\\b",                                                   { 0, 0, 0.5f, 0.5f, 0, 0, 0, 0 }, nullptr } };

struct Keep { const char* pattern; const char* a; const char* b; };
const Keep keeps[] = {
    { "\\bkeep (?:the |my |it |its |it's )?(?:vocal |voice )?(?:upfront|up front|in front|forward)\\b", "front", "presence" },
    { "\\b(?:but|still|and) (?:keep )?(?:it |the vocal |me )?(?:upfront|up front|in front)\\b", "front", "presence" },
    { "\\bkeep (?:the |my |its |it )?(?:presence|clarity|words clear)\\b", "presence", nullptr },
    { "\\bkeep (?:the |my )?character\\b", "char", nullptr },
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
    return anyOps() || scale > 0.0f || more || improve || reset || ! locks.isEmpty() || role.isNotEmpty() || ! flags.isEmpty() || ! notes.isEmpty();
}

IntentFrame parse (const juce::String& text)
{
    IntentFrame f;
    auto t = normalise (text);
    std::string m;
    for (const auto& k : keeps)
        if (find (t, k.pattern, &m))
        {
            f.locks.addIfNotAlreadyThere (k.a);
            if (k.b != nullptr)
                f.locks.addIfNotAlreadyThere (k.b);
            f.heard.add (juce::String (m).trim());
            t = std::regex_replace (t, std::regex (k.pattern), " ", std::regex_constants::format_first_only);
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
    if (find (t, "\\b(too much|a little less|bit less|less|tone it down|pull it back|pull back|subtle|subtler|not so much|too far|too strong)\\b", &m))
    {
        f.scale = find (t, "\\bsubtler?\\b") ? 0.4f : find (t, "(little less|bit less)") ? 0.7f : 0.5f;
        f.heard.add (m);
        t = std::regex_replace (t, std::regex ("\\b(too much|a little less|bit less|less|tone it down|pull it back|pull back|make it subtle|subtle|subtler|not so much|too far|too strong)\\b"), " ");
    }
    f.more = find (t, "\\b(more|stronger|further|push it|even more)\\b");
    if (find (t, "\\bad-? ?libs?\\b|\\bbacking\\b", &m)) { f.role = "adlib"; f.heard.add (m); }
    else if (find (t, "\\b(lead|main vocal)\\b")) f.role = "lead";
    if (find (t, "\\breverb\\b"))
    {
        f.notes.add ("GlitchKid has no reverb, so I build space with Echo and Dimension.");
        f.ops[depth] += mag;
        f.heard.add ("reverb");
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
    if (find (t, "\\b(autotune|auto-tune|tune|tuned|tuning|pitch|in key)\\b")) f.notes.add ("Tuning stays yours: Pitch Tune is on the TUNE page.");
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------

Director::Director (automode::RangeLookup r) : ranges (std::move (r)) {}

bool Director::isAllowed (const juce::String& id)
{
    return ! (id.startsWith ("tune_") || id.startsWith ("lim_") || id == "gain" || id == "mix" || id == "bypass") && id.isNotEmpty();
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

bool Director::needsAnalysis (const IntentFrame& f) { return f.improve || f.ops[clean] > 0.0f; }

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
    }
    if (f.scale > 0.0f && ! touched.empty())
        for (auto d : touched)
            dir[(size_t) d] = round2 (dir[(size_t) d] * f.scale); // "a little less dark": scale what is there
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
    std::map<juce::String, juce::String> W;
    const auto& d = dir;
    const bool front = locks.count ("front") > 0, adlib = roleName == "adlib", keepPresence = locks.count ("presence") > 0;
    auto B = [this] (const char* id) { return base.get (id, 0.0f); };
    auto cur = [&] (const char* id) { auto it = T.find (id); return it != T.end() ? it->second : B (id); };
    auto put = [&] (const char* id, float v, const char* why)
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
        w = w.isEmpty() || w == why ? juce::String (why) : w + "; " + why;
    };
    auto add = [&] (const char* id, float dv, const char* why) { put (id, cur (id) + dv, why); };
    auto k = [] (float x) { return juce::jmin (1.0f, std::abs (x) / 1.5f); }; // how far toward a goal: 1.5 units of direction = all the way
    auto toward = [&] (const char* id, float goal, float amount, const char* why) { const float c = cur (id); if (std::abs (goal - c) > 1e-6f) put (id, c + (goal - c) * amount, why); };
    auto atMost = [&] (const char* id, float cap, const char* why) { if (cur (id) > cap) put (id, cap, why); };

    if (std::abs (d[bright]) > 1e-6f)
    {
        add ("eq_hi_gain", 3.0f * d[bright], d[bright] > 0 ? "Air shelf up: more top end" : "Air shelf down: less top end");
        if (d[bright] > 0 || ! keepPresence)
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
        if (locks.count ("warm") == 0 || d[warm] > 0) add ("eq_lo_gain", d[warm], "Low body");
    }
    if (std::abs (d[presence]) > 1e-6f)
        add ("eq_b2_gain", 1.5f * d[presence], d[presence] > 0 ? "Presence band: the words cut through" : "Presence down: the voice steps back");
    if (d[punch] > 0)
    {
        if (cur ("comp_ratio") < 6.0f) toward ("comp_ratio", 6.0f, k (d[punch]), "Firmer compression: steadier and harder");
        if (cur ("comp_attack") > 2.0f) toward ("comp_attack", 2.0f, k (d[punch]), "Faster attack grabs the front of words");
        add ("comp_thresh", -3.0f * d[punch], "The compressor works harder");
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
        if (B ("dbl_on") >= 0.5f && d[width] <= -0.75f) put ("dbl_on", 0.0f, "Doubler off: one voice in the middle");
        else if (B ("dbl_on") >= 0.5f) add ("dbl_amount", 15.0f * d[width], "Quieter doubles");
        if (B ("dim_on") >= 0.5f && d[width] <= -0.75f) put ("dim_on", 0.0f, "Dimension off");
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
    if (d[depth] < 0)
    {
        if (B ("echo_on") >= 0.5f) put ("echo_on", 0.0f, "Echo off: nothing behind you");
        if (B ("dim_on") >= 0.5f && ! (d[width] > 0)) put ("dim_on", 0.0f, "Dimension off: close and direct");
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
        if (flags.count ("mud") > 0)
        {
            put ("eq_b1_freq", 300.0f, "Body band moved to 300 Hz, where the mud is");
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
            out.push_back ({ id, from, v, W[id] });
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

juce::String Director::respond (IntentFrame f, const AnalysisFacts* facts)
{
    if (f.improve && facts != nullptr && facts->valid)
    {
        if (facts->sibilanceIndex > 0.25f) { f.ops[clean] = juce::jmax (f.ops[clean], 1.0f); f.flags.addIfNotAlreadyThere ("ess"); }
        if (facts->bandFraction[1] > 0.35f) { f.ops[clean] = juce::jmax (f.ops[clean], 1.0f); f.flags.addIfNotAlreadyThere ("mud"); }
        if (facts->dynamicRangeDb > 12.0f) f.ops[punch] += 0.5f;
        if (facts->bandFraction[3] + facts->bandFraction[4] < 0.25f) f.ops[bright] += 0.5f;
        if (facts->clippedSamples > 0) f.notes.add ("Your recording clips. Lower the input level when you record; I don't fix clipping with Gain.");
    }
    updateDirection (f);
    rebuild();
    return reply (f, facts);
}

juce::String Director::reply (const IntentFrame& f, const AnalysisFacts* facts) const
{
    juce::StringArray parts;
    if (f.reset) parts.add ("Clean slate. Your sound is untouched.");
    const bool touched = f.anyOps();
    if (f.improve)
    {
        if (facts != nullptr && facts->valid)
            parts.add ("I listened to " + juce::String (facts->activeSeconds, 1) + " s of your vocal. "
                       + juce::String (facts->bandFraction[1] > 0.35f ? (facts->mudHz > 0.0f ? "It gets muddy around " + juce::String (juce::roundToInt (facts->mudHz)) + " Hz. " : juce::String ("There is a build-up in the low mids (120-500 Hz). ")) : juce::String())
                       + juce::String (facts->sibilanceIndex > 0.25f ? "The S's are sharp. " : "")
                       + juce::String (facts->dynamicRangeDb > 12.0f ? "The level jumps about. " : "")
                       + "Here's my take.");
        else
            parts.add ("I need a few seconds of you singing or rapping first: press play in your project, then ask again.");
    }
    else if (touched)
    {
        juce::StringArray w;
        for (int d = 0; d < numDims; ++d)
            if (std::abs (f.ops[(size_t) d]) > 1e-6f)
                w.addIfNotAlreadyThere (dimWords[d][f.ops[(size_t) d] > 0 ? 0 : 1]);
        auto s = f.scale > 0.0f ? juce::String ("Eased that back.") : w.joinIntoString (", ") + ".";
        parts.add (s.substring (0, 1).toUpperCase() + s.substring (1));
    }
    if (f.scale > 0.0f && ! touched)
        parts.add (f.scale <= 0.4f ? "Made it subtle: about 40 % of what it was." : f.scale <= 0.5f ? "Pulled it back by half." : "Eased it off a little.");
    if (f.more && ! touched) parts.add ("Pushed it further.");
    if (f.locks.contains ("front")) parts.add ("You stay in front: the width comes from the doubles, echo stays low and Presence isn't touched.");
    else if (f.locks.contains ("presence")) parts.add ("Presence stays where it is; the change comes from the top end only.");
    if (f.locks.contains ("char")) parts.add ("Your Lab effects stay exactly as they are.");
    if (f.role == "adlib") parts.add ("For the ad-libs: further back, thinner, with longer throws.");
    if (f.flags.contains ("natural")) parts.add ("Keeping it natural: light saturation, no Lab effects.");
    parts.addArray (f.notes);
    if (! f.understood())
        return "I didn't hear a sound in that. Try \"warmer\", \"wider\", \"more distant\", or \"I don't know, improve it\".";
    const int n = selectedCount();
    if (! rows.empty()) parts.add (juce::String (n) + (n == 1 ? " change" : " changes") + " ready. Apply when it's right, or tell me what to change.");
    else parts.add ("That lands on your current sound: nothing to change.");
    return parts.joinIntoString (" ");
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
