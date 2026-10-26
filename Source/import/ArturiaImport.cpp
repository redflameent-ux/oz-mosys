#include "ArturiaImport.h"
#include "../Params.h"
#include <set>
#include <optional>
#include <algorithm>
#include <cstdlib>
#include <limits>

using namespace juce;

namespace ff::import
{
namespace
{
// ---------------------------------------------------------------------------------------
//  Boost text-archive reading
//  The file is one big run of space-separated tokens. A std::string is written as its byte
//  length, a space, then exactly that many raw bytes (so the text may contain spaces). We do
//  not need the full schema: every parameter we care about is a float written as
//  "<len> <Name> <value>", so we find a named float by searching for that exact prefix.
// ---------------------------------------------------------------------------------------
struct Archive
{
    const char* data = nullptr;
    size_t size = 0;

    Archive (const MemoryBlock& mb) : data (static_cast<const char*> (mb.getData())), size (mb.getSize()) {}

    // Find "<len> <name> " and return the following whitespace-delimited token as a double.
    std::optional<double> getRaw (const String& name) const
    {
        const int len = name.length();
        const String needle = String (len) + " " + name + " ";
        const char* nd = needle.toRawUTF8();
        const size_t nl = (size_t) needle.length();
        const char* const end = data + size;
        for (const char* q = data; q + nl <= end; )
        {
            const char* m = std::search (q, end, nd, nd + nl);
            if (m == end) break;
            // the length digits must not be the tail of a longer number (e.g. "114" vs "14")
            const bool boundaryOk = (m == data) || ! CharacterFunctions::isDigit (*(m - 1));
            if (boundaryOk)
            {
                const char* v = m + nl;
                char buf[48]; int bi = 0;
                while (v < end && bi < 47 && ! CharacterFunctions::isWhitespace (*v)) buf[bi++] = *v++;
                buf[bi] = 0;
                if (bi > 0) return std::strtod (buf, nullptr);
            }
            q = m + 1;
        }
        return {};
    }

    std::optional<float> getF (const String& name) const
    {
        if (auto d = getRaw (name)) return (float) *d;
        return {};
    }
    float get (const String& name, float def) const { return getF (name).value_or (def); }
    bool has (const String& name) const { return getRaw (name).has_value(); }

    // Offset of a length-prefixed token "<len> <token>" (the token must end at a space), or -1.
    // Same robust search as getRaw, so it is immune to the header's number/string ambiguity.
    int tokenOffset (const String& token, int window) const
    {
        const String needle = String (token.length()) + " " + token;
        const char* nd = needle.toRawUTF8();
        const size_t nl = (size_t) needle.length();
        const char* const end = data + jmin ((size_t) window, size);
        for (const char* q = data; q + nl + 1 <= data + size && q < end; )
        {
            const char* m = std::search (q, data + size, nd, nd + nl);
            if (m >= end) break;
            const bool boundaryOk = (m == data) || ! CharacterFunctions::isDigit (*(m - 1));
            if (boundaryOk && *(m + nl) == ' ') return (int) (m - data);
            q = m + 1;
        }
        return -1;
    }

    // Pull the length-prefixed strings out of the header (where the metadata lives), so we can
    // spot the sound type / characteristic tags. Stops after `maxBytes` (before the big param list).
    StringArray headStrings (int maxBytes) const
    {
        StringArray out;
        const int cap = jmin ((int) size, maxBytes);
        int i = 0;
        while (i < cap)
        {
            if (CharacterFunctions::isDigit (data[i]))
            {
                int j = i; int64 len = 0;
                while (j < cap && CharacterFunctions::isDigit (data[j])) { len = len * 10 + (data[j] - '0'); ++j; }
                if (j < (int) size && data[j] == ' ' && len >= 1 && len <= 200 && (int64) (j + 1) + len <= (int64) size)
                {
                    const char* s = data + j + 1;
                    bool printable = true, hasLetter = false;
                    for (int k = 0; k < (int) len; ++k)
                    {
                        const unsigned char c = (unsigned char) s[k];
                        if (c < 32 || c > 126) { printable = false; break; }
                        if (CharacterFunctions::isLetter ((juce_wchar) c)) hasLetter = true;
                    }
                    // a real metadata string always has a letter; this keeps us aligned past the
                    // header's bare numbers (version, counts) instead of eating them as strings
                    if (printable && hasLetter) { out.add (String (CharPointer_UTF8 (s), (size_t) len)); i = (int) (j + 1 + len); continue; }
                }
            }
            ++i;
        }
        return out;
    }

    // The preset's display name. In the .labx it is serialized as "<len> <name> <len> <bank> ...",
    // right after the archive header integers (e.g. "...archive 10 0 7 0 7 19 A Sound from Saturn
    // 10 PROVIDENCE"). Those header ints create false length-prefixed fragments, so we take the
    // first length-prefixed letter-string that is IMMEDIATELY followed by another one (the bank
    // name) - the integer fragments are never followed by a second valid string.
    String presetName() const
    {
        static const char sig[] = "serialization::archive";
        const char* const dend = data + size;
        const char* p = std::search (data, dend, sig, sig + 22);
        if (p == dend) return {};
        p += 22;
        const char* const win = data + (size_t) jmin (size, (size_t) 2000);

        // read a length-prefixed letter-string at `at`; set `after` to where parsing should resume
        auto readLP = [&] (const char* at, String& str, const char*& after) -> bool
        {
            while (at < win && *at == ' ') ++at;
            after = at < win ? at + 1 : win;
            if (at >= win || ! CharacterFunctions::isDigit (*at)) return false;
            int64 len = 0; const char* q = at;
            while (q < win && CharacterFunctions::isDigit (*q)) { len = len * 10 + (*q - '0'); ++q; }
            after = q;                                           // past the integer token
            if (q >= win || *q != ' ' || len < 2 || len > 100 || (const char*) (q + 1 + len) > dend) return false;
            const char* s = q + 1;
            bool hasLetter = false;
            for (int k = 0; k < (int) len; ++k)
            {
                const unsigned char c = (unsigned char) s[k];
                if (c < 32 || c > 126) return false;
                if (CharacterFunctions::isLetter ((juce_wchar) c)) hasLetter = true;
            }
            if (! hasLetter) return false;
            str = String (CharPointer_UTF8 (s), (size_t) len);
            after = s + len;
            return true;
        };

        // scan every offset so a header integer can't consume the name's length prefix; the name is
        // the first letter-string immediately followed by a second one (the bank).
        for (const char* pos = p; pos < win; ++pos)
        {
            String s1; const char* a1;
            if (! readLP (pos, s1, a1) || s1.startsWith ("serialization")) continue;
            String s2; const char* a2;
            if (! readLP (a1, s2, a2)) continue;
            // strip an extra "<len> " wrapper when the leading number IS the remainder's length
            // (e.g. "4 Beno" -> "Beno"), but keep real names that just start with a number.
            const String head = s1.upToFirstOccurrenceOf (" ", false, false);
            const String rest = s1.fromFirstOccurrenceOf (" ", false, false).trim();
            if (head.containsOnly ("0123456789") && head.getIntValue() == rest.length())
                return rest;
            return s1.trim();
        }
        return {};
    }
};

// Arturia envelope / time knobs (0..1) -> seconds. A few banks store seconds directly; anything
// over ~1.5 is taken as literal seconds, everything else as a normalised knob on a soft curve.
float arcTime (float v)
{
    v = jlimit (0.0f, 30.0f, v);
    if (v > 1.5f) return jlimit (0.0f, 15.0f, v);
    return 12.0f * v * v;
}

// ---- sound category (from the Arturia "type" + the preset / engine name) ----
enum class Cat { Bass, Lead, Pad, Keys, Piano, Organ, Brass, Strings, Pluck, Guitar, Bell, Mallet,
                 Seq, Arp, Perc, Drums, Vocal, Wind, SFX };

bool any (const String& s, std::initializer_list<const char*> words)
{
    for (auto* w : words) if (s.containsIgnoreCase (w)) return true;
    return false;
}

Cat classify (const String& type, const String& name, const String& tags)
{
    const String t = type;
    if (any (t, { "Bass" }))                                   return Cat::Bass;
    if (any (t, { "Lead" }))                                   return Cat::Lead;
    if (any (t, { "Pad" }))                                    return Cat::Pad;
    if (any (t, { "Organ" }))                                  return Cat::Organ;
    if (any (t, { "Piano", "Electric Piano", "E-Piano" }))     return Cat::Piano;
    if (any (t, { "Keys", "Keyboard", "Clav" }))               return Cat::Keys;
    if (any (t, { "Brass", "Horn" }))                          return Cat::Brass;
    if (any (t, { "String", "Ensemble" }))                     return Cat::Strings;
    if (any (t, { "Guitar" }))                                 return Cat::Guitar;
    if (any (t, { "Pluck" }))                                  return Cat::Pluck;
    if (any (t, { "Bell" }))                                   return Cat::Bell;
    if (any (t, { "Mallet", "Marimba", "Vibra", "Xylo" }))     return Cat::Mallet;
    if (any (t, { "Sequence", "Seq " }))                       return Cat::Seq;
    if (any (t, { "Arp" }))                                    return Cat::Arp;
    if (any (t, { "Drum", "Kick", "Snare", "Hat", "Clap" }))   return Cat::Drums;
    if (any (t, { "Perc" }))                                   return Cat::Perc;
    if (any (t, { "Vocal", "Voice", "Choir", "Vox" }))         return Cat::Vocal;
    if (any (t, { "Wind", "Woodwind", "Flute", "Reed" }))      return Cat::Wind;
    if (any (t, { "SFX", "Sound Effect", "FX", "Noise", "Soundtrack", "Atmosphere", "Drone", "Texture" })) return Cat::SFX;
    // fall back to words in the name / tags
    const String s = name + " " + tags;
    if (any (s, { "bass", "808", "sub" }))                     return Cat::Bass;
    if (any (s, { "pad", "atmos", "drone", "texture", "ambient", "evolv" })) return Cat::Pad;
    if (any (s, { "lead", "solo" }))                           return Cat::Lead;
    if (any (s, { "pluck", "stab" }))                          return Cat::Pluck;
    if (any (s, { "bell", "chime", "glass", "music box" }))    return Cat::Bell;
    if (any (s, { "key", "rhodes", "wurli", "piano", "epiano" })) return Cat::Keys;
    if (any (s, { "organ" }))                                  return Cat::Organ;
    if (any (s, { "string" }))                                 return Cat::Strings;
    if (any (s, { "brass", "horn" }))                          return Cat::Brass;
    if (any (s, { "arp", "seq" }))                             return Cat::Seq;
    if (any (s, { "choir", "vox", "vocal" }))                  return Cat::Vocal;
    if (any (s, { "fx", "noise", "riser", "impact", "hit" }))  return Cat::SFX;
    return Cat::Lead;
}

String catTag (Cat c)
{
    switch (c)
    {
        case Cat::Bass: return "bass"; case Cat::Lead: return "lead"; case Cat::Pad: return "pad";
        case Cat::Keys: return "keys"; case Cat::Piano: return "keys"; case Cat::Organ: return "organ";
        case Cat::Brass: return "brass"; case Cat::Strings: return "strings"; case Cat::Pluck: return "pluck";
        case Cat::Guitar: return "pluck"; case Cat::Bell: return "bell"; case Cat::Mallet: return "mallet";
        case Cat::Seq: return "sequence"; case Cat::Arp: return "arp"; case Cat::Perc: return "percussion";
        case Cat::Drums: return "percussion"; case Cat::Vocal: return "vocal"; case Cat::Wind: return "breath";
        case Cat::SFX: return "fx";
    }
    return {};
}

struct Builder
{
    std::map<String, float> v;
    void set (const String& id, float x) { v[id] = x; }
};

// Pick the best OZ engine for a category and set up a usable starting voice. Pigments then
// refines the knobs on top; Analog Lab / V Collection lives with just this plus any shared params.
void setupCategory (Builder& b, Cat c, bool& arpOn)
{
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);
    arpOn = false;

    auto wt = [&] (int table, float pos, int uni, float det)
    {
        b.set ("wt_on", 1.0f); b.set ("wt_table", (float) table); b.set ("wt_pos", pos);
        b.set ("wt_unison", (float) uni); b.set ("wt_detune", det);
    };
    auto an = [&] (float shape, float sub, float noise)
    {
        b.set ("an_on", 1.0f); b.set ("an_shape", shape); b.set ("an_sub", sub); b.set ("an_noise", noise);
    };
    auto res = [&] (int type) { b.set ("res_on", 1.0f); b.set ("res_type", (float) type); };
    auto gr  = [&] (int src)  { b.set ("gr_on", 1.0f); b.set ("gr_source", (float) src); b.set ("gr_density", 25.0f); };

    // a sensible amp envelope per category (overwritten by the real one when we have it)
    float a = 0.005f, d = 0.4f, s = 0.8f, r = 0.4f;

    switch (c)
    {
        case Cat::Bass:    an (0.9f, 0.5f, 0.0f); s = 0.9f; r = 0.2f; b.set ("flt_cutoff", 0.45f); break;
        case Cat::Lead:    an (1.0f, 0.0f, 0.0f); s = 0.9f; break;
        case Cat::Pad:     wt (1, 0.5f, 5, 0.3f); a = 0.8f; r = 1.6f; b.set ("flt_cutoff", 0.6f); break;
        case Cat::Keys:    res (18); d = 1.2f; s = 0.0f; r = 0.4f; break;        // E-Piano
        case Cat::Piano:   res (18); d = 1.6f; s = 0.0f; r = 0.5f; break;
        case Cat::Organ:   wt (5, 0.5f, 1, 0.1f); s = 1.0f; r = 0.15f; break;    // Dark Organ
        case Cat::Brass:   an (1.0f, 0.0f, 0.0f); a = 0.06f; s = 0.85f; break;
        case Cat::Strings: wt (1, 0.5f, 5, 0.25f); a = 0.25f; r = 0.9f; break;
        case Cat::Pluck:   res (0); d = 0.9f; s = 0.0f; r = 0.5f; break;         // Plucked String
        case Cat::Guitar:  res (0); d = 1.2f; s = 0.0f; r = 0.6f; break;
        case Cat::Bell:    res (7); d = 3.0f; s = 0.0f; r = 1.2f; break;         // Tingsha / Bell
        case Cat::Mallet:  res (17); d = 1.2f; s = 0.0f; r = 0.5f; break;        // Marimba
        case Cat::Seq:     an (1.0f, 0.2f, 0.0f); d = 0.3f; s = 0.4f; r = 0.2f; arpOn = true; break;
        case Cat::Arp:     wt (0, 0.66f, 1, 0.1f); d = 0.3f; s = 0.3f; r = 0.2f; arpOn = true; break;
        case Cat::Perc:    res (10); d = 0.5f; s = 0.0f; r = 0.3f; break;        // Hand Drum
        case Cat::Drums:   res (10); d = 0.4f; s = 0.0f; r = 0.2f; break;
        case Cat::Vocal:   gr (0); a = 0.3f; r = 1.0f; break;                    // Breathing Choir
        case Cat::Wind:    gr (2); a = 0.15f; r = 0.6f; break;                   // Breath & Wind
        case Cat::SFX:     gr (3); a = 0.4f; r = 1.8f; b.set ("rev_mix", 0.25f); break;  // Rain & Water
    }
    b.set ("amp_a", a); b.set ("amp_d", d); b.set ("amp_s", s); b.set ("amp_r", r);
    b.set ("flt_on", 1.0f);
}

int filterTypeFor (const Archive& ar)
{
    // Filter1_ModuleType is an index into Arturia's filter models; 0 is the Mini ladder.
    const float t = ar.get ("Filter1_ModuleType", 1.0f);
    if (t < 0.5f) return 0;   // Ladder LP 24
    // if a multi-mode filter exposes a HP/BP/notch mode, honour it
    if (auto mode = ar.getF ("Filter1_MultiModeFilter_Mode"))
    {
        if (*mode > 0.66f) return 3;   // High Pass
        if (*mode > 0.33f) return 2;   // Band Pass
    }
    return 1;                  // LP 12
}

// ---- Pigments: refine the OZ voice with the patch's real parameters ----
void refinePigments (Builder& b, const Archive& ar)
{
    // tune (CoarseTune / FineTune are 0..1 with 0.5 = centre)
    if (auto ct = ar.getF ("Engine1_VA3Osc_CoarseTune"))
    {
        const int semis = jlimit (-24, 24, roundToInt ((*ct - 0.5f) * 48.0f));
        const int oct = jlimit (-3, 3, (int) std::floor ((semis + 6) / 12.0));
        b.set ("an_oct", (float) oct); b.set ("wt_oct", (float) oct);
    }

    // oscillator knobs onto whichever engine the category chose
    const bool useWt = b.v.count ("wt_on") && b.v["wt_on"] > 0.5f;
    const bool useAn = b.v.count ("an_on") && b.v["an_on"] > 0.5f;
    if (useWt)
    {
        if (auto m = ar.getF ("Engine1_WTOsc_Morph"))        b.set ("wt_pos", jlimit (0.0f, 1.0f, *m));
        if (auto u = ar.getF ("Engine1_WTOsc_UnisonVoices")) b.set ("wt_unison", (float) jlimit (1, 7, roundToInt (*u > 1.0f ? *u : *u * 8.0f)));
        if (auto dt = ar.getF ("Engine1_WTOsc_UnisonDetune"))b.set ("wt_detune", jlimit (0.0f, 1.0f, *dt));
    }
    else if (useAn)
    {
        if (auto w = ar.getF ("Engine1_VA3Osc_Osc1Wave"))
            b.set ("an_shape", *w < 0.25f ? 0.0f : *w < 0.6f ? 1.0f : 2.0f);
        if (auto pw = ar.getF ("Engine1_VA3Osc_Osc1PulseWidth")) b.set ("an_pw", jlimit (0.05f, 0.95f, *pw));
        if (auto n = ar.getF ("Engine1_VA3Osc_NoiseGain"))       b.set ("an_noise", jlimit (0.0f, 1.0f, *n));
    }

    // filter
    if (auto cut = ar.getF ("Filter1_Cutoff"))
    {
        b.set ("flt_on", 1.0f);
        b.set ("flt_type", (float) filterTypeFor (ar));
        b.set ("flt_cutoff", jlimit (0.0f, 1.0f, *cut));
        b.set ("flt_res", jlimit (0.0f, 0.95f, ar.get ("Filter1_Resonance", 0.1f)));
    }

    // envelopes: Env1 = amp, Env2 -> Env 2, Env3 -> Env 3
    auto env = [&] (const String& src, const String& dst, float sDef)
    {
        if (auto a = ar.getF (src + "_Attack"))
        {
            b.set (dst + "_a", arcTime (*a));
            b.set (dst + "_d", jmax (0.001f, arcTime (ar.get (src + "_Decay", 0.3f))));
            b.set (dst + "_s", jlimit (0.0f, 1.0f, ar.get (src + "_Sustain", sDef)));
            b.set (dst + "_r", jmax (0.005f, arcTime (ar.get (src + "_Release", 0.2f))));
        }
    };
    env ("Env1", "amp", 0.85f);
    env ("Env2", "e2", 0.0f);
    env ("Env3", "e3", 0.5f);

    // LFO 1 rate (0..1 -> Hz)
    if (auto lr = ar.getF ("LFO1_RateUnSynced"))
        b.set ("lfo1_rate", jlimit (0.01f, 30.0f, 0.1f * std::pow (300.0f, jlimit (0.0f, 1.0f, *lr))));

    // effects: each of the three slots has a Dry/Wet; map it to the closest OZ effect
    float rev = 0.0f, dly = 0.0f, cho = 0.0f;
    for (int n = 1; n <= 3; ++n)
    {
        const String p = "FX" + String (n);
        const float wet = ar.get (p + "_Dry_Wet", 0.0f);
        if (wet < 0.03f || ar.get (p + "_Bypass", 0.0f) > 0.5f) continue;
        if (ar.get (p + "_Chorus_Depth", 0.0f) > 0.05f)         cho = jmax (cho, jlimit (0.0f, 0.5f, wet * 0.8f));
        else if (ar.get (p + "_Delay_Feedback", 0.0f) > 0.05f)  dly = jmax (dly, jlimit (0.0f, 0.35f, wet * 0.7f));
        else if (ar.get (p + "_Phaser_LFOAmount", 0.0f) > 0.05f) cho = jmax (cho, jlimit (0.0f, 0.4f, wet * 0.6f));
        else                                                     rev = jmax (rev, jlimit (0.0f, 0.4f, wet * 0.6f));  // reverb / space
    }
    if (rev > 0) b.set ("rev_mix", rev);
    if (dly > 0) b.set ("dly_mix", dly);
    if (cho > 0) b.set ("cho_mix", cho);
}

// ---- Analog Lab / V Collection: read the real parameters the .labx actually stores ----
// Analog Lab prefixes every parameter with "_Slot1_". Some are a universal wrapper (the arp),
// others are the underlying engine's own knobs and vary by engine. We map the ones that are
// safe across engines: the arp, the FX sends (additive - a missing one just means no FX), and
// the amp envelope. We deliberately do NOT force a filter cutoff here: a cutoff knob that belongs
// to a different engine can read 0 and would silence the preset, so the per-category default stands.
void refineAnalogLab (Builder& b, const Archive& ar)
{
    // amp envelope (Env1 is the VCA envelope on the VA-style engines)
    if (auto a = ar.getF ("_Slot1_Env1_Attack"))
    {
        b.set ("amp_a", arcTime (*a));
        b.set ("amp_d", jmax (0.001f, arcTime (ar.get ("_Slot1_Env1_Decay", 0.3f))));
        b.set ("amp_s", jlimit (0.0f, 1.0f, ar.get ("_Slot1_Env1_Sustain", 0.8f)));
        b.set ("amp_r", jmax (0.005f, arcTime (ar.get ("_Slot1_Env1_Release", 0.2f))));
    }

    // FX sends (additive and safe: absent / zero simply means no effect)
    if (auto rl = ar.getF ("_Slot1_AMOUNT_LEFT_REVERB"))
    {
        const float r = (*rl + ar.get ("_Slot1_AMOUNT_RIGHT_REVERB", *rl)) * 0.5f;
        if (r > 0.02f) b.set ("rev_mix", jlimit (0.0f, 0.5f, r));
    }
    if (auto ch = ar.getF ("_Slot1_Chorus Amount"))
        if (*ch > 0.02f) b.set ("cho_mix", jlimit (0.0f, 0.5f, *ch));

    // arpeggiator - the one truly universal Analog Lab wrapper, the same across every engine
    if (ar.get ("_Slot1_ArpSeq_On", 0.0f) > 0.5f)
    {
        b.set ("arp_on", 1.0f);
        if (auto rate = ar.getF ("_Slot1_ArpSeq_RateUnSynced"))
            b.set ("arp_rate", (float) jlimit (0, 5, roundToInt (jlimit (0.0f, 1.0f, *rate) * 5.0f)));
    }
}

// The sound "type" is a string field in the header. We find it the robust way (as a
// length-prefixed token), trying each known Arturia type and taking the earliest hit, rather
// than walking the header (whose bare numbers are ambiguous with string lengths).
String detectType (const Archive& ar)
{
    static const char* types[] = { "Bass", "Lead", "Pad", "Keys", "Keyboard", "Organ", "Piano", "Electric Piano",
        "Brass", "Strings", "Pluck", "Guitar", "Bell", "Bells & Mallets", "Mallets", "Sequence", "Arpeggio",
        "Percussion", "Drums", "SFX", "Sound Effects", "Soundtrack", "Winds", "Voice", "Vocal", "Synth", "Clavinet" };
    String best; int bestAt = std::numeric_limits<int>::max();
    for (auto* t : types)
    {
        const int at = ar.tokenOffset (t, 2000);
        if (at >= 0 && at < bestAt) { bestAt = at; best = t; }
    }
    return best;
}
} // namespace

bool convertArturiaPreset (const MemoryBlock& mb, const String& name, const String& bank,
                           AudioProcessorValueTreeState& apvts, Converted& out)
{
    if (mb.getSize() < 64) return false;
    Archive ar (mb);

    // --- metadata from the header ---
    const String type = detectType (ar);
    String comment;
    StringArray tags;
    for (auto& s : ar.headStrings (1200))                          // the longest line = the comment
        if (s.length() > comment.length() && s.length() > 24 && s.containsChar (' ') && ! s.equalsIgnoreCase (type))
            comment = s;

    const bool pigments = ar.has ("Engine1_ModuleType") || ar.has ("Filter1_Cutoff") || ar.has ("Engine1_WTOsc_Morph");
    const Cat c = classify (type, name, tags.joinIntoString (" "));

    Builder b;
    bool arpOn = false;
    setupCategory (b, c, arpOn);
    if (pigments) refinePigments (b, ar);
    else          refineAnalogLab (b, ar);
    if (arpOn) b.set ("arp_on", 1.0f);

    // --- write it up ---
    out = {};
    out.name = name;
    out.bank = bank;
    out.author = String ("Imported (") + (pigments ? "Pigments" : "Arturia") + ")";
    tags.insert (0, catTag (c));
    if (type.isNotEmpty()) tags.addIfNotAlreadyThere (type.toLowerCase());
    tags.insert (0, pigments ? "pigments" : "arturia");
    out.tags = tags.joinIntoString (", ");
    out.notes = String ("Imported from ") + (pigments ? "Arturia Pigments" : "Arturia (Analog Lab / V Collection)")
              + ". Rebuilt with OZ-mosys engines"
              + (pigments ? String (" from the patch's own parameters.")
                          : String (" from its \"") + (type.isEmpty() ? String ("synth") : type) + "\" sound type, plus its real amp envelope, FX sends and arpeggiator.")
              + (comment.isNotEmpty() ? " Original note: " + comment : String());

    // keep only values that differ from Init (sparse preset)
    for (auto& [id, x] : b.v)
        if (auto* p = apvts.getParameter (id))
        {
            const float def = p->convertFrom0to1 (p->getDefaultValue());
            const float clamped = p->convertFrom0to1 (p->convertTo0to1 (x));
            if (std::abs (clamped - def) > 1.0e-4f) out.values[id] = clamped;
        }
    return true;
}

// ============================================================================ folder walk
namespace
{
static const char archiveSig[] = "serialization::archive";
constexpr int sigLen = 22;

// slice one file into its individual archives (a bank packs several back to back)
void sliceArchives (const File& file, std::vector<MemoryBlock>& out, StringArray& innerNames)
{
    const int64 sz = file.getSize();
    if (sz <= 0 || sz > 64 * 1024 * 1024) return;   // skip the multi-GB wavetable banks
    MemoryBlock whole;
    { FileInputStream in (file); if (! in.openedOk()) return; in.readIntoMemoryBlock (whole); }
    const char* d = static_cast<const char*> (whole.getData());
    const size_t n = whole.getSize();

    // Every "serialization::archive" is an archive boundary, but a preset CONTAINS nested
    // sub-archives (sample objects, keyboard zones). So collect all boundaries first, then keep
    // only the ones that begin a real preset - the ones whose segment carries the preset metadata
    // ("Characteristics,<tags>") - and fold the sub-archives into the preset they belong to.
    std::vector<size_t> marks;
    for (const char* q = d; ; )
    {
        const char* m = std::search (q, d + n, archiveSig, archiveSig + sigLen);
        if (m == d + n) break;
        size_t s = (size_t) (m - d);
        while (s > 0 && (CharacterFunctions::isDigit (d[s - 1]) || d[s - 1] == ' ')) --s;   // back up over the length prefix
        marks.push_back (s);
        q = m + sigLen;
    }

    static const char metaSig[] = "Characteristics,";
    constexpr size_t metaLen = sizeof (metaSig) - 1;
    std::vector<size_t> starts;                            // boundaries that begin a real preset
    for (size_t i = 0; i < marks.size(); ++i)
    {
        const size_t from = marks[i];
        const size_t to = (i + 1 < marks.size()) ? marks[i + 1] : n;
        if (std::search (d + from, d + to, metaSig, metaSig + metaLen) != d + to)
            starts.push_back (from);
    }
    if (starts.empty() && ! marks.empty()) starts.push_back (marks.front());   // single non-bank preset

    for (size_t i = 0; i < starts.size(); ++i)
    {
        const size_t from = starts[i];
        const size_t to = (i + 1 < starts.size()) ? starts[i + 1] : n;   // includes this preset's sub-archives
        if (to <= from) continue;
        MemoryBlock slice (d + from, to - from);
        innerNames.add (Archive (slice).presetName());   // the preset's real display name
        out.push_back (std::move (slice));
    }
}

bool looksLikeArturia (const File& f)
{
    FileInputStream in (f);
    if (! in.openedOk()) return false;
    char buf[256]; const int got = in.read (buf, sizeof (buf));
    return got > sigLen && std::search (buf, buf + got, archiveSig, archiveSig + sigLen) != buf + got;
}

struct Item { String product, sub, name; File file; };

// A short, clean bank label for sequential preset names. Analog Lab .labx buries the real preset
// name deep in its archive and simple heuristics grab internal object strings, so bank presets are
// named "<bank> NNN" instead. The label is the word before "Analog"/"Lab" in the folder name
// (e.g. "...Providence.ANALOG.LAB..." -> "Providence"), falling back to the last meaningful token.
String bankLabel (const String& product)
{
    auto toks = StringArray::fromTokens (product.replaceCharacters (".", " "), " ", "");
    toks.removeEmptyStrings();
    for (int i = 1; i < toks.size(); ++i)
        if (toks[i].equalsIgnoreCase ("analog") || toks[i].equalsIgnoreCase ("lab"))
            return toks[i - 1];
    for (int i = toks.size() - 1; i >= 0; --i)
        if (! toks[i].equalsIgnoreCase ("bank") && ! toks[i].equalsIgnoreCase ("preset") && ! toks[i].equalsIgnoreCase ("presets"))
            return toks[i];
    return product.isNotEmpty() ? product : String ("Preset");
}
} // namespace

int importArturia (const Array<File>& sources, AudioProcessorValueTreeState& apvts,
                   std::function<bool (int, int, const String&)> progress, String& summary)
{
    std::vector<Item> items;
    auto addFromRoot = [&] (const File& root)
    {
        auto files = root.findChildFiles (File::findFiles, true);
        files.sort();
        for (auto& f : files)
        {
            if (f.getFileName().startsWithChar ('.') || f.hasFileExtension ("xml;db3;meta;json;txt;wav;aiff;flac")) continue;
            if (! looksLikeArturia (f)) continue;
            const auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            Item it;
            it.product = rel.containsChar ('/') ? rel.upToFirstOccurrenceOf ("/", false, false) : root.getFileName();
            const String after = rel.fromFirstOccurrenceOf ("/", false, false);
            if (after.containsChar ('/')) it.sub = after.upToLastOccurrenceOf ("/", false, false);
            it.name = f.getFileNameWithoutExtension();
            it.file = f;
            items.push_back (it);
        }
    };
    for (auto& src : sources)
    {
        if (src.isDirectory()) addFromRoot (src);
        else if (src.existsAsFile() && looksLikeArturia (src))
        {
            Item it; it.product = src.getParentDirectory().getFileName(); it.name = src.getFileNameWithoutExtension(); it.file = src;
            items.push_back (it);
        }
    }

    const auto expansions = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OZ-mosys").getChildFile ("Expansions");
    int written = 0, failed = 0;
    std::map<String, int> perPack;
    std::set<String> usedFiles;   // output .ozpreset paths already taken, so same-named presets don't overwrite
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        if (progress && ! progress (i, (int) items.size(), it.name)) break;

        std::vector<MemoryBlock> archives; StringArray innerNames;
        sliceArchives (it.file, archives, innerNames);
        if (archives.empty()) { ++failed; continue; }
        const bool bank = archives.size() > 1;

        const String packName = "Arturia - " + File::createLegalFileName (it.product);
        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + it.product + " patches, rebuilt in OZ-mosys");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }

        for (size_t a = 0; a < archives.size(); ++a)
        {
            // use the preset's real name; fall back to a sequential "<bank> NNN" only if the .labx
            // didn't yield one. A single preset file keeps its own filename.
            String pname;
            if (bank && innerNames[(int) a].isNotEmpty()) pname = innerNames[(int) a];
            else if (bank)                                pname = bankLabel (it.product) + " " + String ((int) a + 1).paddedLeft ('0', 3);
            else                                          pname = it.name;
            Converted cv;
            if (! convertArturiaPreset (archives[a], pname, packName, apvts, cv)) { ++failed; continue; }
            if (it.sub.isNotEmpty()) cv.tags << ", " << it.sub.replaceCharacter ('/', ' ').toLowerCase();
            auto dir = packDir.getChildFile ("Presets");
            if (it.sub.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (it.sub.replaceCharacter ('/', ' ')));
            // unique filename so presets that share a name are not overwritten
            const String base = File::createLegalFileName (pname);
            File outFile = dir.getChildFile (base + ".ozpreset");
            for (int dup = 2; ! usedFiles.insert (outFile.getFullPathName().toLowerCase()).second; ++dup)
                outFile = dir.getChildFile (base + " " + String (dup) + ".ozpreset");
            if (writePreset (cv, outFile)) { ++written; ++perPack[packName]; }
            else ++failed;
        }
    }
    summary = String (written) + " presets imported into " + String ((int) perPack.size()) + " collection" + (perPack.size() == 1 ? "" : "s");
    if (failed > 0) summary << " (" << failed << " couldn't be read)";
    return written;
}
} // namespace ff::import
