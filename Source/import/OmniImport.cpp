#include "OmniImport.h"
#include "../Params.h"
#include "../perform/Perform.h"

using namespace juce;

namespace ff::import
{
namespace
{
// Omnisphere stores most values as the hex bits of a 0..1 float ("3f800000" = 1.0);
// a few (modes, counts) are plain decimal.
float val (const XmlElement* e, const char* attr, float def = 0.0f)
{
    if (e == nullptr || ! e->hasAttribute (attr)) return def;
    const auto s = e->getStringAttribute (attr).trim();
    if (s.length() == 8 && s.containsOnly ("0123456789abcdefABCDEF"))
    {
        const auto bits = (uint32) s.getHexValue64();
        float f; std::memcpy (&f, &bits, sizeof (f));
        return std::isfinite (f) ? f : def;
    }
    return s.getFloatValue();
}
float val01 (const XmlElement* e, const char* attr, float def = 0.0f) { return jlimit (0.0f, 1.0f, val (e, attr, def)); }
bool on (const XmlElement* e, const char* attr, bool def = false) { return val (e, attr, def ? 1.0f : 0.0f) > 0.5f; }

const XmlElement* child (const XmlElement* e, const char* tag)
{
    return e != nullptr ? e->getChildByName (tag) : nullptr;
}

// Omnisphere envelope / time knobs (0..1) -> seconds (approximate curve)
float omniTime (float v) { return jlimit (0.0f, 30.0f, 30.0f * v * v * v); }

// semitones from the oct / semi knobs (semi: 0.48 = 0, 0.02 per semitone; oct: 0.04 = 0)
int omniSemis (const XmlElement* osc)
{
    const int semi = roundToInt (val01 (osc, "semi", 0.48f) * 50.0f) - 24;
    const int oct = roundToInt (val01 (osc, "oct", 0.04f) * 50.0f) - 2;
    return jlimit (-36, 36, semi + oct * 12);
}

// ---- what kind of sound the soundsource is (from its name + folder) ----
enum class Src { Saw, SuperSaw, Square, Sine, Tri, Organ, Bell, Mallet, Vocal, Water, Wind, Pluck, Keys,
                 Digital, Noise, Pad, Bass, Brass, Strings, Metal, Sitar, Bowl, Drum };

bool any (const String& s, std::initializer_list<const char*> words)
{
    for (auto* w : words) if (s.containsIgnoreCase (w)) return true;
    return false;
}

Src classify (const XmlElement* osc, const String& patchName)
{
    if (osc == nullptr) return Src::Saw;
    const String name = osc->getStringAttribute ("name"), folder = osc->getStringAttribute ("folder");
    if (! osc->hasAttribute ("name") || osc->getIntAttribute ("kind") == 0)
    {
        // DSP waveform mode: the waveform index sits in "type"
        const float t = val01 (osc, "type");
        if (t < 0.01f) return Src::Saw;
        if (t < 0.03f) return Src::Square;
        if (t < 0.05f) return Src::Tri;
        if (t < 0.07f) return Src::Sine;
        return Src::Digital;
    }
    const String s = name + " " + folder;
    if (any (s, { "supersaw", "super saw", "hypersaw", "JP-8000", "JP8000" })) return Src::SuperSaw;
    if (any (s, { "choir", "vox", "vocal", "voice", "angel", "chant", "aah", "ooh", "sing" })) return Src::Vocal;
    if (any (s, { "water", "rain", "ocean", "cave", "drip", "river", "liquid", "stalact" })) return Src::Water;
    if (any (s, { "wind", "breath", "air", "whisper", "blow" })) return Src::Wind;
    if (any (s, { "sitar", "tanpura", "oud", "koto" })) return Src::Sitar;
    if (any (s, { "bowl", "singing", "crystal" })) return Src::Bowl;
    if (any (s, { "bell", "chime", "glock", "celest", "music box", "tine", "glass" })) return Src::Bell;
    if (any (s, { "marimba", "vibra", "mallet", "xylo", "kalimba", "steel drum" })) return Src::Mallet;
    if (any (s, { "rhodes", "wurli", "piano", "e-piano", "epiano", "clav", "keys", "harpsi" })) return Src::Keys;
    if (any (s, { "guitar", "pluck", "harp", "string pluck", "pizz", "banjo", "dulcimer" })) return Src::Pluck;
    if (any (s, { "organ", "b3", "hammond", "farfisa", "harmonium" })) return Src::Organ;
    if (any (s, { "brass", "horn", "trumpet", "trombone", "sax" })) return Src::Brass;
    if (any (s, { "string", "violin", "cello", "viola", "orchestra" })) return Src::Strings;
    if (any (s, { "noise", "hiss", "static", "crackle", "vinyl" })) return Src::Noise;
    if (any (s, { "metal", "tesla", "coil", "anvil", "scrap", "industrial", "psychoacoustic" })) return Src::Metal;
    if (any (s, { "kick", "snare", "drum", "perc", "hat", "tom" })) return Src::Drum;
    if (any (s, { "ppg", "wavetable", "digital", "bit", "fm ", "dx", "d-50", "d50", "wave " })) return Src::Digital;
    if (any (s, { "sine", "sines", "flute", "whistle" })) return Src::Sine;
    if (any (s, { "square", "pulse", "chip", "8-bit", "stylophone" })) return Src::Square;
    if (any (s, { "bass", "sub", "808" })) return Src::Bass;
    if (any (s, { "tri", "soft" })) return Src::Tri;
    if (any (s, { "saw", "juno", "virus", "moog", "jupiter", "jp-8", "monopoly", "prophet", "oberheim", "analog",
                  "synthesizer", "minimoog", "ob-" })) return Src::Saw;
    if (any (s, { "pad", "evolv", "texture", "atmos", "drone", "ambien", "scape", "granul", "motion" })) return Src::Pad;
    if (any (patchName, { "bass", "sub" })) return Src::Bass;
    if (any (patchName, { "pad", "atmo" })) return Src::Pad;
    return Src::Saw;
}

String srcTag (Src s)
{
    switch (s)
    {
        case Src::Saw: return "saw"; case Src::SuperSaw: return "supersaw"; case Src::Square: return "square";
        case Src::Sine: return "sine"; case Src::Tri: return "soft"; case Src::Organ: return "organ";
        case Src::Bell: return "bell"; case Src::Mallet: return "mallet"; case Src::Vocal: return "vocal";
        case Src::Water: return "water"; case Src::Wind: return "breath"; case Src::Pluck: return "pluck";
        case Src::Keys: return "keys"; case Src::Digital: return "digital"; case Src::Noise: return "noise";
        case Src::Pad: return "pad"; case Src::Bass: return "bass"; case Src::Brass: return "brass";
        case Src::Strings: return "strings"; case Src::Metal: return "metallic"; case Src::Sitar: return "world";
        case Src::Bowl: return "bowl"; case Src::Drum: return "percussion";
    }
    return {};
}

// engines a source would like, best first
std::vector<String> enginesFor (Src s)
{
    switch (s)
    {
        case Src::Saw: case Src::Square: case Src::Tri: case Src::Bass: case Src::Brass: return { "an", "wt" };
        case Src::SuperSaw: case Src::Strings: case Src::Sine: case Src::Organ: case Src::Digital: case Src::Pad: return { "wt", "an" };
        case Src::Vocal: case Src::Water: case Src::Wind: return { "gr", "wt" };
        case Src::Bell: case Src::Mallet: case Src::Keys: case Src::Pluck: case Src::Sitar: case Src::Bowl: case Src::Drum: case Src::Metal: return { "res", "fm", "wt" };
        case Src::Noise: return { "an", "wt" };
    }
    return { "wt", "an" };
}

struct Builder
{
    std::map<String, float> v;
    void set (const String& id, float x) { v[id] = x; }
};

void setupEngine (Builder& b, const String& eng, Src s, const XmlElement* osc, float levelScale)
{
    const int semis = omniSemis (osc);
    const int oct = jlimit (-3, 3, (int) std::floor ((semis + 6) / 12.0));
    const int semi = jlimit (-12, 12, semis - oct * 12);
    const float level = jlimit (0.0f, 1.0f, val01 (osc, "level", 0.75f) * levelScale);
    const float pan = (val01 (osc, "pan", 0.5f) - 0.5f) * 2.0f;
    const float fine = (val01 (osc, "tuneFine", 0.5f) - 0.5f) * 100.0f;
    b.set (eng + "_on", 1.0f);
    b.set (eng + "_oct", (float) oct);
    b.set (eng + "_semi", (float) semi);
    b.set (eng + "_level", level);
    if (eng != "gr") b.set (eng + "_pan", pan);
    if (eng == "wt" || eng == "an") b.set (eng + "_fine", fine);

    const int unisonCount = roundToInt (val01 (osc, "ucnt") * 8.0f);
    const float uDetune = val01 (osc, "udpth", 0.1f);
    if (eng == "wt")
    {
        int table = 0; float pos = 0.667f;
        switch (s)
        {
            case Src::Sine: pos = 0.0f; break;
            case Src::Tri: pos = 0.33f; break;
            case Src::Square: pos = 1.0f; break;
            case Src::Organ: table = 5; pos = 0.5f; break;
            case Src::Digital: case Src::Metal: table = 3; pos = 0.4f; break;
            case Src::Vocal: table = 2; pos = 0.4f; break;
            case Src::Pad: case Src::Water: case Src::Wind: table = 1; pos = 0.5f; break;
            case Src::Bell: case Src::Mallet: case Src::Keys: table = 1; pos = 0.25f; break;
            default: break;
        }
        b.set ("wt_table", (float) table);
        b.set ("wt_pos", pos + jlimit (-0.2f, 0.2f, (val01 (osc, "timbre", 0.5f) - 0.5f) * 0.4f));
        int uni = s == Src::SuperSaw ? 7 : s == Src::Strings ? 5 : 1;
        if (unisonCount > 1) uni = jmax (uni, jlimit (2, 7, unisonCount));
        b.set ("wt_unison", (float) uni);
        b.set ("wt_detune", s == Src::SuperSaw ? 0.35f : jlimit (0.05f, 0.6f, uDetune * 2.0f));
    }
    else if (eng == "an")
    {
        float shape = 1.0f;
        if (s == Src::Square) shape = 2.0f;
        else if (s == Src::Tri || s == Src::Sine) shape = 0.0f;
        else if (s == Src::Bass) shape = 0.8f;
        b.set ("an_shape", shape);
        b.set ("an_sub", s == Src::Bass ? 0.5f : 0.0f);
        b.set ("an_noise", s == Src::Noise ? 0.9f : 0.0f);
        b.set ("an_tone", s == Src::Noise ? 0.1f : 1.0f);
        b.set ("an_drift", 0.3f);
        b.set ("an_pw", 0.5f + (val01 (osc, "pwidth") * 0.4f));
    }
    else if (eng == "gr")
    {
        const int src = s == Src::Water ? 3 : s == Src::Wind ? 2 : s == Src::Vocal ? 0 : 1;
        b.set ("gr_source", (float) src);
        b.set ("gr_density", on (osc, "gran") ? 40.0f : 25.0f);
        b.set ("gr_spray", 0.2f);
    }
    else if (eng == "res")
    {
        int type = 0;
        switch (s)
        {
            case Src::Bell: type = 7; break;
            case Src::Mallet: type = 17; break;
            case Src::Keys: type = 18; break;
            case Src::Sitar: type = 1; break;
            case Src::Bowl: type = 3; break;
            case Src::Drum: type = 10; break;
            case Src::Metal: type = 15; break;
            default: type = 0; break;
        }
        b.set ("res_type", (float) type);
    }
    else if (eng == "fm")
    {
        b.set ("fm_index", s == Src::Bell ? 1.2f : 0.7f);
    }
}

int filterTypeFor (const XmlElement* f)
{
    const String n = f->getStringAttribute ("NameStr");
    if (any (n, { "hpf", "highpass", "high pass", "hi pass" })) return 3;
    if (any (n, { "bandpass", "band pass", "bpf" })) return 2;
    if (any (n, { "notch" })) return 4;
    if (any (n, { "24", "ladder", "moog", "fatboy" })) return 0;
    return 1;   // LP 12 - Omnisphere's usual
}

int modSourceFor (const String& s)
{
    if (s == "Wheel" || s == "Mod Wheel") return srcModWheel;
    if (s.startsWith ("LFO1")) return srcLfo1;
    if (s.startsWith ("LFO2")) return srcLfo2;
    if (s.startsWith ("LFO")) return srcLfo2;
    if (s.startsWith ("ModEnv")) return srcEnv3;
    if (s.containsIgnoreCase ("Velocity")) return srcVelocity;
    if (s.containsIgnoreCase ("Aftertouch") || s.containsIgnoreCase ("Pressure")) return srcAftertouch;
    if (s.containsIgnoreCase ("Random")) return srcRandom;
    if (s.containsIgnoreCase ("Key")) return srcKeyTrack;
    return -1;
}

int modDestFor (String t, const String& engA, const String& engB)
{
    const bool layerB = t.startsWith ("B ");
    if (t.startsWith ("A ") || t.startsWith ("B ")) t = t.substring (2);
    const String eng = layerB ? engB : engA;
    if (t == "freq" || t == "envdpth" || t == "Freq") return layerB ? dstCutoff2 : dstCutoff;
    if (t == "res") return layerB ? dstRes2 : dstResonance;
    if (t == "pan") return dstPan;
    if (t == "tune" || t == "tuneFine" || t == "pitch")
    {
        if (eng == "wt") return dstWtPitch; if (eng == "an") return dstAnPitch; if (eng == "gr") return dstGrPitch;
        if (eng == "res") return dstResPitch; if (eng == "fm") return dstFmPitch; return dstPitchAll;
    }
    if (t == "level" || t == "atrm" || t == "amp" || t == "vol")
    {
        if (eng == "wt") return dstWtLevel; if (eng == "an") return dstAnLevel; if (eng == "gr") return dstGrLevel;
        if (eng == "res") return dstResLevel; if (eng == "fm") return dstFmLevel; return dstAmp;
    }
    if (t == "timbre" || t == "mogrify" || t == "WSdpth" || t.startsWith ("Harm"))
    {
        if (eng == "wt") return dstWtPos; if (eng == "an") return dstAnShape; if (eng == "gr") return dstGrPos;
        if (eng == "res") return dstResBright; if (eng == "fm") return dstFmIndex; return dstCutoff;
    }
    if (t == "hrdsnc") return dstFeedback;
    return -1;
}
} // namespace

bool convertOmniPatch (const XmlElement& root, const String& name, const String& bank,
                       AudioProcessorValueTreeState& apvts, Converted& out)
{
    const XmlElement* synth = root.hasTagName ("SYNTHENG") ? &root : nullptr;
    if (synth == nullptr)
        for (auto* e = &root; e != nullptr && synth == nullptr; )
        {
            if (auto* s = e->getChildByName ("SynthEngine")) e = s;
            if (auto* s = e->getChildByName ("SYNTHENG")) { synth = s; break; }
            break;
        }
    if (synth == nullptr) return false;
    const XmlElement* arpEl = root.getChildByName ("SynthEngine") != nullptr ? root.getChildByName ("SynthEngine")->getChildByName ("ARP") : nullptr;

    Builder b;
    out = {};
    out.name = name; out.bank = bank;

    // ---- the two layers ----
    std::vector<const XmlElement*> voices;
    for (auto* v : synth->getChildWithTagNameIterator ("VOICE")) voices.push_back (v);
    if (voices.empty()) return false;
    const bool layerAOff = on (synth, "dsia"), layerBOff = on (synth, "dsib");

    // Init has the wavetable engine on; start from everything off
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);

    std::set<String> used;
    String engA, engB;
    StringArray tags, srcNames;
    for (int layer = 0; layer < (int) jmin<size_t> (2, voices.size()); ++layer)
    {
        if ((layer == 0 && layerAOff) || (layer == 1 && layerBOff)) continue;
        auto* osc = child (voices[(size_t) layer], "OSC");
        if (osc == nullptr || val01 (osc, "level", 0.75f) < 0.01f) continue;
        const Src s = classify (osc, name);
        String eng;
        for (auto& e : enginesFor (s)) if (! used.count (e)) { eng = e; break; }
        if (eng.isEmpty()) for (auto* e : { "wt", "an", "gr", "res", "fm" }) if (! used.count (e)) { eng = e; break; }
        used.insert (eng);
        (layer == 0 ? engA : engB) = eng;
        setupEngine (b, eng, s, osc, 1.0f);
        tags.addIfNotAlreadyThere (srcTag (s));
        srcNames.add (osc->getStringAttribute ("name", "synth waveform"));
        // glide
        if (on (osc, "portAct")) b.set ("glide", jlimit (0.0f, 2.0f, omniTime (val01 (osc, "port")) * 0.5f));
    }
    if (engA.isEmpty() && engB.isEmpty()) { b.set ("wt_on", 1.0f); engA = "wt"; }

    // ---- filters: layer A -> Filter 1, layer B -> Filter 2 (parallel) ----
    auto* main = voices[layerAOff && voices.size() > 1 ? 1 : 0];
    auto filterFrom = [&] (const XmlElement* voice, const String& pre)
    {
        auto* f = child (voice, "FILTER");
        const bool act = f != nullptr && on (f, "act");
        b.set (pre + "_on", act ? 1.0f : 0.0f);
        if (! act) return false;
        b.set (pre + "_type", (float) filterTypeFor (f));
        b.set (pre + "_cutoff", val01 (f, "freq", 1.0f));
        b.set (pre + "_res", jlimit (0.0f, 0.95f, val01 (f, "res") * 0.9f));
        float env = val01 (f, "envdpth") * (on (f, "envdpthinv") ? -1.0f : 1.0f);
        // the env only moves the cutoff when the filter envelope actually goes somewhere
        b.set (pre + "_env", jlimit (-1.0f, 1.0f, env * 0.8f));
        if (pre == "flt") b.set ("flt_key", jlimit (0.0f, 1.0f, (val01 (f, "key", 0.5f) - 0.5f) * 2.0f));
        return true;
    };
    const bool f1 = filterFrom (main, "flt");
    if (voices.size() > 1 && ! layerBOff && ! layerAOff && engB.isNotEmpty())
    {
        if (filterFrom (voices[1], "flt2")) b.set ("flt_route", 1.0f);
    }
    else b.set ("flt2_on", 0.0f);
    if (! f1) { b.set ("flt_on", 1.0f); b.set ("flt_type", 1.0f); b.set ("flt_cutoff", 1.0f); b.set ("flt_env", 0.0f); }

    // ---- envelopes ----
    if (auto* ae = child (main, "AENVPARAMS"))
    {
        b.set ("amp_a", omniTime (val01 (ae, "attk")));
        b.set ("amp_d", jmax (0.001f, omniTime (val01 (ae, "decy", 0.3f))));
        b.set ("amp_s", val01 (ae, "sust", 1.0f));
        b.set ("amp_r", jmax (0.005f, omniTime (val01 (ae, "rels", 0.2f))));
        b.set ("amp_vel", val01 (ae, "velsens", 0.5f));
    }
    if (auto* fe = child (main, "FENVPARAMS"))
    {
        b.set ("e2_a", omniTime (val01 (fe, "attk")));
        b.set ("e2_d", jmax (0.001f, omniTime (val01 (fe, "decy", 0.3f))));
        b.set ("e2_s", val01 (fe, "sust", 1.0f));
        b.set ("e2_r", jmax (0.005f, omniTime (val01 (fe, "rels", 0.2f))));
    }
    if (auto* me = synth->getChildByName ("MODENVPARAMS"))
    {
        b.set ("e3_a", omniTime (val01 (me, "attk")));
        b.set ("e3_d", jmax (0.001f, omniTime (val01 (me, "decy", 0.3f))));
        b.set ("e3_s", val01 (me, "sust", 1.0f));
        b.set ("e3_r", jmax (0.005f, omniTime (val01 (me, "rels", 0.2f))));
    }

    // ---- voice ----
    if (on (synth, "legm") || on (synth, "useLeg")) b.set ("voice_mode", 1.0f);
    {
        const int bend = jlimit (0, 24, roundToInt (val01 (synth, "pbup", 0.04f) * 50.0f));
        b.set ("bend", (float) jmax (1, bend));
    }

    // ---- LFOs (first two) ----
    std::vector<const XmlElement*> lfos;
    if (auto* ls = synth->getChildByName ("LFO_SET")) for (auto* l : ls->getChildWithTagNameIterator ("LFO")) lfos.push_back (l);
    for (int i = 0; i < (int) jmin<size_t> (2, lfos.size()); ++i)
    {
        const String pre = "lfo" + String (i + 1);
        const float r = val01 (lfos[(size_t) i], "rate", 0.2f);
        b.set (pre + "_rate", jlimit (0.01f, 30.0f, 0.05f * std::pow (600.0f, r)));
        const int t = roundToInt (val (lfos[(size_t) i], "type"));
        static const int shapeMap[] = { 0, 1, 2, 3, 4, 5 };
        b.set (pre + "_shape", (float) shapeMap[jlimit (0, 5, t)]);
    }

    // ---- mod matrix ----
    int slot = 0;
    if (auto* mm = synth->getChildByName ("MOD_MATRIX"))
        for (int i = 0; i < 32 && slot < numModSlots; ++i)
        {
            const String s = mm->getStringAttribute ("source" + String (i)), t = mm->getStringAttribute ("target" + String (i));
            if (s.isEmpty() || s == "off" || t.isEmpty() || t == "off") continue;
            if (on (mm, ("mute" + String (i)).toRawUTF8())) continue;
            const int src = modSourceFor (s), dst = modDestFor (t, engA, engB.isNotEmpty() ? engB : engA);
            if (src < 0 || dst < 0) continue;
            const float lo = val (mm, ("lo" + String (i)).toRawUTF8()), hi = val (mm, ("hi" + String (i)).toRawUTF8(), 0.5f);
            float amt = jlimit (-1.0f, 1.0f, (hi - lo));
            if (dst == dstWtPitch || dst == dstAnPitch || dst == dstGrPitch || dst == dstResPitch || dst == dstFmPitch || dst == dstPitchAll) amt *= 0.25f;
            if (std::abs (amt) < 0.01f) continue;
            b.set (slotSrc (slot), (float) src);
            b.set (slotDst (slot), (float) dst);
            b.set (slotAmt (slot), amt);
            ++slot;
        }
    // mod wheel always does something useful, like most Omnisphere patches
    if (slot < numModSlots && slot == 0)
    {
        b.set (slotSrc (slot), (float) srcModWheel);
        b.set (slotDst (slot), (float) dstCutoff);
        b.set (slotAmt (slot), 0.3f);
        ++slot;
    }

    // ---- effects (per-layer racks, common rack and aux sends) ----
    float rev = 0.0f, revSize = 0.6f, dly = 0.0f, cho = 0.0f, drive = 0.0f, tape = 0.0f, phase = 0.0f;
    bool auxRev = false;
    auto scanRack = [&] (const XmlElement* rack, bool aux)
    {
        if (rack == nullptr) return;
        for (auto* m : rack->getChildWithTagNameIterator ("EFFMODULE"))
        {
            if (! on (m, "Active")) continue;
            const String type = m->getStringAttribute ("Type");
            if (any (type, { "verb", "innerspace", "reverb", "hall", "plate" }))
            {
                rev = jmax (rev, aux ? 0.3f : 0.22f);
                if (any (type, { "innerspace", "pro-verb", "hall" })) revSize = 0.8f;
                if (aux) auxRev = true;
            }
            else if (any (type, { "delay", "echo", "retroplex", "plex" })) dly = jmax (dly, aux ? 0.22f : 0.18f);
            else if (any (type, { "chorus", "ensemble", "flang", "unison" })) cho = jmax (cho, 0.35f);
            else if (any (type, { "phaser" })) phase = 0.4f;
            else if (any (type, { "distort", "amp", "fuzz", "overdrive", "crusher", "smasher" })) drive = jmax (drive, 0.3f);
            else if (any (type, { "tape", "tube", "slammer", "saturat" })) tape = jmax (tape, 0.25f);
        }
    };
    for (auto* v : voices) scanRack (child (v, "EFFRACK"), false);
    for (auto* r : synth->getChildWithTagNameIterator ("EFFRACK")) scanRack (r, false);
    for (auto* r : synth->getChildWithTagNameIterator ("AUXEFFRACK")) scanRack (r, true);
    b.set ("rev_mix", rev);
    b.set ("rev_size", revSize);
    b.set ("dly_mix", dly);
    b.set ("cho_mix", jmax (cho, phase * 0.6f));
    if (drive > 0) b.set ("fx_drive", drive);
    if (tape > 0) b.set ("tape_age", tape);
    if (rev > 0) tags.addIfNotAlreadyThere (auxRev ? "spacious" : "reverb");

    // ---- arpeggiator ----
    if (arpEl != nullptr && on (arpEl, "ArpOnOff"))
    {
        b.set ("arp_on", 1.0f);
        const int clock = roundToInt (val (arpEl, "ArpClock", 4.0f));
        const int rate = clock <= 2 ? 0 : clock == 3 ? 1 : clock == 4 ? 3 : 5;
        b.set ("arp_rate", (float) rate);
        const int mode = roundToInt (val (arpEl, "ArpMode"));
        static const int modeMap[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
        b.set ("arp_mode", (float) modeMap[jlimit (0, 7, mode)]);
        b.set ("arp_oct", (float) jlimit (1, 4, 1 + roundToInt (val (arpEl, "ArpOct"))));
        b.set ("arp_swing", jlimit (0.0f, 0.75f, val01 (arpEl, "ArpSwing")));
        b.set ("arp_gate", jlimit (0.05f, 1.0f, val01 (arpEl, "ArpLen", 0.6f)));
        perf::ArpPattern pat;
        int n = 0;
        if (auto* seq = arpEl->getChildByName ("ARPSEQ2") != nullptr ? arpEl->getChildByName ("ARPSEQ2") : arpEl->getChildByName ("ARPSEQ"))
            for (auto* st : seq->getChildWithTagNameIterator ("SLICESEQSTEP"))
            {
                if (n >= perf::ArpPattern::maxSteps) break;
                auto& s = pat.steps[(size_t) n++];
                s.on = st->getIntAttribute ("END") > st->getIntAttribute ("BEGIN");
                s.velocity = jlimit (0.05f, 1.0f, (float) st->getIntAttribute ("VEL", 100) / 127.0f);
                s.accent = st->getIntAttribute ("VEL", 100) >= 120;
            }
        if (n > 0)
        {
            b.set ("arp_steps", (float) jlimit (1, 32, n));
            out.arpPattern = pat.toString();
        }
        tags.addIfNotAlreadyThere ("arp");
    }

    // ---- tags from the name / folder words Omnisphere banks tend to use ----
    const String lower = (name + " " + bank).toLowerCase();
    for (auto [word, tag] : { std::pair<const char*, const char*> { "pad", "pad" }, { "bass", "bass" }, { "lead", "lead" }, { " led ", "lead" },
                              { "arp", "arp" }, { "pluck", "pluck" }, { "keys", "keys" }, { "key ", "keys" }, { "bell", "bell" },
                              { "seq", "sequence" }, { "stab", "stab" }, { "fx", "fx" }, { "atmo", "atmosphere" }, { "texture", "texture" },
                              { "drone", "drone" }, { "string", "strings" }, { "brass", "brass" }, { "choir", "vocal" }, { "flute", "flute" },
                              { "guitar", "guitar" }, { "organ", "organ" }, { "dark", "dark" } })
        if (lower.contains (word)) tags.addIfNotAlreadyThere (tag);
    out.tags = "omnisphere, " + tags.joinIntoString (", ");
    out.notes = "Imported from Omnisphere. Original soundsources: " + srcNames.joinIntoString (" + ")
              + ". Rebuilt with OZ-mosys engines (" + (engA + (engB.isNotEmpty() ? " + " + engB : String())) + ").";

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

bool writePreset (const Converted& c, const File& file)
{
    XmlElement root ("FourfoldState");
    root.setAttribute ("presetName", c.name);
    root.setAttribute ("bank", c.bank);
    root.setAttribute ("author", "Imported (Omnisphere)");
    root.setAttribute ("tags", c.tags);
    root.setAttribute ("notes", c.notes);
    root.setAttribute ("sparse", 1);
    if (c.arpPattern.isNotEmpty()) root.setAttribute ("arpPattern", c.arpPattern);
    for (auto& [id, v] : c.values)
    {
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", v);
    }
    root.createNewChildElement ("Msegs");
    file.getParentDirectory().createDirectory();
    return root.writeTo (file);
}

int importOmnisphereFolder (const File& rootDir, AudioProcessorValueTreeState& apvts,
                            std::function<bool (int, int, const String&)> progress, String& summary)
{
    auto files = rootDir.findChildFiles (File::findFiles, true, "*.prt_omn");
    files.sort();
    const auto expansions = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OZ-mosys").getChildFile ("Expansions");
    int written = 0, failed = 0;
    std::map<String, int> perBank;
    for (int i = 0; i < files.size(); ++i)
    {
        const auto& f = files.getReference (i);
        // bank = first folder under the chosen one (or the chosen folder itself)
        auto rel = f.getRelativePathFrom (rootDir).replaceCharacter ('\\', '/');
        String bank = rel.containsChar ('/') ? rel.upToFirstOccurrenceOf ("/", false, false) : rootDir.getFileName();
        String sub;
        {
            const String afterBank = rel.containsChar ('/') ? rel.fromFirstOccurrenceOf ("/", false, false) : String();
            if (afterBank.containsChar ('/')) sub = afterBank.upToLastOccurrenceOf ("/", false, false);
        }
        const String packName = "Omni - " + File::createLegalFileName (bank);
        if (progress && ! progress (i, files.size(), f.getFileNameWithoutExtension())) break;

        auto xml = XmlDocument::parse (f);
        Converted c;
        if (xml == nullptr || ! convertOmniPatch (*xml, f.getFileNameWithoutExtension(), packName, apvts, c)) { ++failed; continue; }
        if (sub.isNotEmpty()) c.tags << ", " << sub.replaceCharacter ('/', ' ').toLowerCase();
        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + bank + " Omnisphere patches, rebuilt in OZ-mosys");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }
        auto out = packDir.getChildFile ("Presets").getChildFile (File::createLegalFileName (sub.replaceCharacter ('/', ' ')))
                          .getChildFile (File::createLegalFileName (c.name) + ".ozpreset");
        if (sub.isEmpty()) out = packDir.getChildFile ("Presets").getChildFile (File::createLegalFileName (c.name) + ".ozpreset");
        if (writePreset (c, out)) { ++written; ++perBank[packName]; } else ++failed;
    }
    summary = String (written) + " presets imported into " + String ((int) perBank.size()) + " collection" + (perBank.size() == 1 ? "" : "s");
    if (failed > 0) summary << " (" << failed << " couldn't be read)";
    return written;
}
} // namespace ff::import
