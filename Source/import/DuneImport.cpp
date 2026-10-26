#include "DuneImport.h"
#include <set>
#include <map>
#include <cmath>
#include <cstring>

using namespace juce;

namespace ff::import
{
namespace
{
// ---------------------------------------------------------------------------------------
//  DUNE's internal parameter id -> name, recovered from the name table inside DUNE3.vst3.
//  Only the ids used by the OZ mapping are listed, each validated to read sane values across
//  several banks. ("Osc 1 Semi" id 1 is omitted on purpose - its id collides and reads garbage.)
// ---------------------------------------------------------------------------------------
struct DuneParam { const char* name; int id; };
const DuneParam kDuneParams[] = {
    { "Mixer: Osc 1", 471 }, { "Osc 1 Amt", 382 }, { "Mixer: Osc 1 Pan", 437 },
    { "Mixer: Osc 2", 472 }, { "Osc 2 Amt", 383 }, { "Mixer: Osc 2 Pan", 440 },
    { "Osc 1 WT Position", 4 }, { "Osc 1 Fine", 2 }, { "Osc 2 Semi", 6 }, { "Osc 2 Fine", 7 },
    { "Voices Amount", 77 }, { "Unison Detune", 76 }, { "Unison Spread", 75 },
    { "Filter 1 Cutoff", 28 }, { "Filter 1 Resonance", 29 }, { "Filter 1 Drive", 31 },
    { "Filter 1 Env Amount", 30 }, { "Filter 1 Key-Trk", 26 }, { "Filter 2 Cutoff", 429 },
    { "Amp Env Attack", 18 }, { "Amp Env Decay", 19 }, { "Amp Env Sustain", 20 }, { "Amp Env Release", 21 },
    { "Filter Env Attack", 22 }, { "Filter Env Decay", 23 }, { "Filter Env Sustain", 24 }, { "Filter Env Release", 25 },
    { "FX-1 Reverb Active", 121 }, { "FX-1 Reverb Dry/Wet", 94 }, { "FX-1 Reverb Time", 93 }, { "FX-1 Reverb HF Damp", 92 },
    { "FX-1 Delay 1 Active", 120 }, { "FX-1 Delay 1 Dry/Wet", 88 }, { "FX-1 Delay 1 Feedback", 87 },
    { "FX-1 Chorus Active", 372 }, { "FX-1 Chorus Dry/Wet", 359 }, { "FX-1 Chorus Depth", 357 },
    { "FX-1 Distortion Active", 116 }, { "FX-1 Distort Drive", 99 }, { "Arp 1", 115 },
};

float clamp01 (float x) { return jlimit (0.0f, 1.0f, x); }

// A DUNE .fxp: 'CcnK' | size | 'FPCh' | ver | 'DNE3' | ... | zlib stream. Inflate and read `para`.
bool parseFxp (const File& f, std::map<String, float>& P)
{
    MemoryBlock raw;
    if (! f.loadFileAsData (raw) || raw.getSize() < 128) return false;
    const auto* d = static_cast<const uint8*> (raw.getData());
    if (std::memcmp (d, "CcnK", 4) != 0 || std::memcmp (d + 8, "FPCh", 4) != 0 || std::memcmp (d + 16, "DNE3", 4) != 0)
        return false;                                    // not a DUNE 3 preset (DUNE 2 is "DUE2")

    int z = -1;                                          // locate the zlib header after the FPCh header
    for (int i = 60; i < 200 && (size_t) (i + 1) < raw.getSize(); ++i)
        if (d[i] == 0x78 && d[i + 1] == 0x9c) { z = i; break; }
    if (z < 0) return false;

    MemoryInputStream mis (d + z, raw.getSize() - (size_t) z, false);
    GZIPDecompressorInputStream gz (&mis, false, GZIPDecompressorInputStream::zlibFormat);
    MemoryBlock mb;
    gz.readIntoMemoryBlock (mb);
    if (mb.getSize() < 348) return false;
    const auto* b = static_cast<const uint8*> (mb.getData());
    auto u32 = [&] (size_t o) { return (uint32) (b[o] | (b[o+1] << 8) | (b[o+2] << 16) | ((uint32) b[o+3] << 24)); };
    auto f32 = [&] (size_t o) { float v; std::memcpy (&v, b + o, 4); return v; };

    // Find the 'arap' (para) block; its offset moves in wavetable-heavy presets, so search for it.
    //   'arap' | int32 (8) | int32 count | int32 baseId | count * { uint32 id ; float value }
    size_t ap = 0; bool found = false;
    for (size_t i = 0; i + 24 < mb.getSize(); ++i)
        if (b[i] == 'a' && b[i+1] == 'r' && b[i+2] == 'a' && b[i+3] == 'p') { ap = i; found = true; break; }
    if (! found) return false;
    const uint32 count = u32 (ap + 8);
    const size_t base = ap + 20;
    if (count == 0 || count > 100000 || base + (size_t) count * 8 > mb.getSize()) return false;

    std::map<int, float> byId;                           // id -> value, preferring voice 0
    for (uint32 k = 0; k < count; ++k)
    {
        const uint32 rid = u32 (base + (size_t) k * 8);
        const int    lo  = (int) (rid & 0xffff);
        const uint32 hi  = rid >> 16;
        const float  val = f32 (base + (size_t) k * 8 + 4);
        if (hi == 0 || byId.find (lo) == byId.end()) byId[lo] = val;
    }
    for (auto& dp : kDuneParams)
    {
        auto it = byId.find (dp.id);
        if (it == byId.end()) continue;
        if (std::abs (it->second) > 100.0f) continue;    // guard against any stray garbage read
        P[dp.name] = it->second;
    }
    return ! P.empty();
}

struct Builder { std::map<String, float> v; void set (const String& id, float x) { v[id] = x; } };

bool convertDune (const File& fxp, const String& name, const String& bank,
                  AudioProcessorValueTreeState& apvts, Converted& out)
{
    std::map<String, float> P;
    if (! parseFxp (fxp, P)) return false;

    auto g = [&] (const char* n, float d = 0.0f) -> float { auto it = P.find (n); return it == P.end() ? d : it->second; };
    auto envTime = [] (float n, float maxS) { n = clamp01 (n); return 0.001f + maxS * n * n * n; };
    auto fine = [&] (const char* n) { return jlimit (-100.0f, 100.0f, (g (n, 0.5f) - 0.5f) * 200.0f); };
    auto semi = [&] (const char* n) { return (float) jlimit (-12, 12, roundToInt ((g (n, 0.5f) - 0.5f) * 24.0f)); };

    Builder b;
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);

    // ---- oscillators: Osc 1 -> wavetable, Osc 2 -> analog (take the louder of mixer / amt) ----
    const int   voices = jmax (1, roundToInt (g ("Voices Amount", 1.0f)));   // a raw count, not normalised
    const float o1 = jmax (g ("Mixer: Osc 1"), g ("Osc 1 Amt"));
    if (o1 > 0.001f)
    {
        b.set ("wt_on", 1.0f);
        b.set ("wt_level",  clamp01 (o1));
        b.set ("wt_pan",    jlimit (-1.0f, 1.0f, g ("Mixer: Osc 1 Pan", 0.5f) * 2.0f - 1.0f));
        b.set ("wt_pos",    clamp01 (g ("Osc 1 WT Position", 0.0f)));
        b.set ("wt_unison", (float) jlimit (1, 7, voices));
        b.set ("wt_detune", clamp01 (g ("Unison Detune", 0.25f)));
        b.set ("wt_spread", clamp01 (g ("Unison Spread", 0.7f)));
        b.set ("wt_fine",   fine ("Osc 1 Fine"));
    }
    const float o2 = jmax (g ("Mixer: Osc 2"), g ("Osc 2 Amt"));
    if (o2 > 0.001f)
    {
        b.set ("an_on", 1.0f);
        b.set ("an_level", clamp01 (o2));
        b.set ("an_pan",   jlimit (-1.0f, 1.0f, g ("Mixer: Osc 2 Pan", 0.5f) * 2.0f - 1.0f));
        b.set ("an_semi",  semi ("Osc 2 Semi"));
        b.set ("an_fine",  fine ("Osc 2 Fine"));
    }
    if (o1 <= 0.001f && o2 <= 0.001f) { b.set ("wt_on", 1.0f); b.set ("wt_level", 0.75f); }   // never silent

    // ---- filters ----
    b.set ("flt_on", 1.0f);
    b.set ("flt_cutoff", clamp01 (g ("Filter 1 Cutoff", 0.85f)));
    b.set ("flt_res",    clamp01 (g ("Filter 1 Resonance", 0.0f)));
    b.set ("flt_drive",  clamp01 (g ("Filter 1 Drive", 0.0f)));
    b.set ("flt_env",    jlimit (-1.0f, 1.0f, g ("Filter 1 Env Amount", 0.0f)));
    b.set ("flt_key",    clamp01 (g ("Filter 1 Key-Trk", 0.0f)));
    const float f2c = g ("Filter 2 Cutoff", 1.0f);
    if (f2c < 0.98f) { b.set ("flt2_on", 1.0f); b.set ("flt2_cutoff", clamp01 (f2c)); b.set ("flt_route", 1.0f); }

    // ---- envelopes: Amp -> amp, Filter env -> Env 2 ----
    b.set ("amp_a", envTime (g ("Amp Env Attack"), 8.0f));
    b.set ("amp_d", jmax (0.001f, envTime (g ("Amp Env Decay",   0.3f), 10.0f)));
    b.set ("amp_s", clamp01 (g ("Amp Env Sustain", 0.85f)));
    b.set ("amp_r", jmax (0.005f, envTime (g ("Amp Env Release", 0.3f), 12.0f)));
    b.set ("e2_a",  envTime (g ("Filter Env Attack"), 8.0f));
    b.set ("e2_d",  jmax (0.001f, envTime (g ("Filter Env Decay",   0.3f), 10.0f)));
    b.set ("e2_s",  clamp01 (g ("Filter Env Sustain", 0.0f)));
    b.set ("e2_r",  jmax (0.005f, envTime (g ("Filter Env Release", 0.3f), 12.0f)));

    // ---- effects ----
    if (g ("FX-1 Reverb Active") > 0.5f)
    {
        b.set ("rev_mix",  jlimit (0.0f, 0.6f, g ("FX-1 Reverb Dry/Wet", 0.3f)));
        b.set ("rev_size", clamp01 (g ("FX-1 Reverb Time", 0.6f)));
        b.set ("rev_damp", clamp01 (g ("FX-1 Reverb HF Damp", 0.5f)));
    }
    if (g ("FX-1 Delay 1 Active") > 0.5f)
    {
        b.set ("dly_mix", jlimit (0.0f, 0.5f, g ("FX-1 Delay 1 Dry/Wet", 0.3f)));
        b.set ("dly_fb",  jlimit (0.0f, 0.95f, g ("FX-1 Delay 1 Feedback", 0.35f)));
    }
    if (g ("FX-1 Chorus Active") > 0.5f)
    {
        b.set ("cho_mix",   jlimit (0.0f, 0.5f, g ("FX-1 Chorus Dry/Wet", 0.3f)));
        b.set ("cho_depth", clamp01 (g ("FX-1 Chorus Depth", 0.4f)));
    }
    if (g ("FX-1 Distortion Active") > 0.5f) b.set ("fx_drive", jlimit (0.0f, 0.8f, g ("FX-1 Distort Drive", 0.0f)));
    if (g ("Arp 1") > 0.5f) b.set ("arp_on", 1.0f);

    // ---- write up: clamp to each parameter's range and keep only what differs from the default ----
    out = {};
    out.name = name;
    out.bank = bank;
    out.author = "Imported (DUNE 3)";
    out.tags = "dune, imported";
    out.notes = "Imported from Synapse DUNE 3 by reading the preset file directly. A close cousin: "
                "filters, oscillator levels, pan, envelopes and effect sends come from the preset's "
                "real values; tuning is approximate.";
    for (auto& [id, x] : b.v)
        if (auto* pr = apvts.getParameter (id))
        {
            const float def = pr->convertFrom0to1 (pr->getDefaultValue());
            const float clamped = pr->convertFrom0to1 (pr->convertTo0to1 (x));
            if (std::abs (clamped - def) > 1.0e-4f) out.values[id] = clamped;
        }
    return true;
}

struct Item { String product, sub, name; File file; };
} // namespace

int importDune (const Array<File>& sources, AudioProcessorValueTreeState& apvts,
                std::function<bool (int, int, const String&)> progress, String& summary)
{
    std::vector<Item> items;
    auto addRoot = [&] (const File& root)
    {
        auto files = root.findChildFiles (File::findFiles, true, "*.fxp");
        files.sort();
        for (auto& f : files)
        {
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
        if (src.isDirectory()) addRoot (src);
        else if (src.hasFileExtension ("fxp"))
        { Item it; it.product = src.getParentDirectory().getFileName(); it.name = src.getFileNameWithoutExtension(); it.file = src; items.push_back (it); }
    }

    const auto expansions = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OZ-mosys").getChildFile ("Expansions");
    int written = 0, failed = 0;
    std::map<String, int> perPack;
    std::set<String> seen;
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        if (progress && ! progress (i, (int) items.size(), it.name)) break;

        const String packName = "DUNE 3 - " + File::createLegalFileName (it.product);
        if (! seen.insert (packName + "/" + it.sub + "/" + it.name).second) continue;

        Converted cv;
        if (! convertDune (it.file, it.name, packName, apvts, cv)) { ++failed; continue; }   // non-DUNE / unreadable
        if (it.sub.isNotEmpty()) cv.tags << ", " << it.sub.replaceCharacter ('/', ' ').toLowerCase();

        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + it.product + " DUNE 3 patches, rebuilt in OZ-mosys");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }
        auto dir = packDir.getChildFile ("Presets");
        if (it.sub.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (it.sub.replaceCharacter ('/', ' ')));
        if (writePreset (cv, dir.getChildFile (File::createLegalFileName (cv.name) + ".ozpreset"))) { ++written; ++perPack[packName]; }
        else ++failed;
    }
    summary = String (written) + " presets imported into " + String ((int) perPack.size()) + " collection" + (perPack.size() == 1 ? "" : "s");
    if (failed > 0) summary << " (" << failed << " skipped)";
    return written;
}
} // namespace ff::import
