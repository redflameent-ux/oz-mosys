// =====================================================================================
//  PluginProbe - tools for reading undocumented binary synth presets, for building OZ importers.
//  Opt-in, local only (cmake -DFOURFOLD_BUILD_TOOLS=ON); never built in CI.
//
//  Two approaches live here, because the first one did NOT pan out for DUNE:
//
//  1. The "plugin-oracle" (host the plugin, read params by name). The idea was that a plugin
//     decodes its own format, so loading a preset and reading getParameters() yields a labelled
//     dump. In practice DUNE 3's VST3, hosted via JUCE, IGNORES a VST2 .fxp fed to
//     setStateInformation (a silent no-op) and also ignores host parameter writes - so the dump
//     is just DUNE's default state, identical for every preset. Kept here only as an inspection
//     aid (it may still work for plugins whose VST3 accepts their old chunk); do NOT trust it to
//     reflect a DUNE preset.
//
//  2. Direct .fxp parsing (what actually works for DUNE, and needs NO plugin). A DUNE .fxp is a
//     VST2 FPCh wrapper around a zlib stream; inflated, it holds a `para` block of
//     [id][value] records. The id->name map was recovered from a table inside DUNE3.vst3. This
//     reads each preset's REAL values straight off disk. See parseDuneFxp / kDuneParams.
//
//  Usage:
//      PluginProbe --fxp2oz <PackName> <preset.fxp | folder>   (DUNE 3 -> OZ, no plugin needed)
//      PluginProbe <plugin.vst3|.dll> <preset.fxp | folder>    (oracle JSON dump; see caveat above)
//      PluginProbe <plugin.vst3|.dll> --state <outfile> [<idx> <val>]   (dump plugin state, for RE)
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <map>
#include <cmath>
#include "../Source/import/SpireConvert.h"   // shared Spire .spf parse + OZ mapping

using namespace juce;

// Extract the raw plugin state chunk from a VST2 .fxp "opaque chunk" (FPCh) preset.
// FXP layout (all multi-byte ints are BIG-endian):
//   'CcnK' | byteSize | 'FPCh' | version | fxID | fxVersion | numPrograms | prgName[28] | chunkSize | chunk[]
static MemoryBlock fxpChunk (const File& f)
{
    MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() < 64) return {};
    const auto* d = static_cast<const uint8*> (mb.getData());
    auto be32 = [&] (int off) { return (uint32) ((d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3]); };
    if (std::memcmp (d, "CcnK", 4) != 0 || std::memcmp (d + 8, "FPCh", 4) != 0) return {};   // not an opaque-chunk fxp
    const uint32 chunkSize = be32 (56);
    if ((size_t) 60 + chunkSize > mb.getSize()) return {};
    return MemoryBlock (d + 60, chunkSize);
}

static String jstr (const String& s)   // minimal JSON string literal
{
    String out = "\"";
    for (auto c : s)
    {
        if (c == '"' || c == '\\') out << '\\' << (juce_wchar) c;
        else if (c == '\n' || c == '\r' || c == '\t') out << ' ';
        else out << (juce_wchar) c;
    }
    return out + "\"";
}

// =====================================================================================
//  DUNE 3 (and any hosted synth) -> OZ-mosys converter.
//
//  The oracle gives us every parameter BY NAME, but as a raw normalised 0..1 value: DUNE
//  reports no unit, text or step count to a host, so we cannot read its real semitones/seconds
//  back. We therefore carry across EXACTLY the parameters OZ also stores as 0..1 (filter
//  cutoff/resonance/drive, oscillator levels, pan, env sustain, FX sends) - those are faithful -
//  and APPROXIMATE the two families DUNE hides: pitch (mapped proportionally onto OZ's +/-12
//  semitone range) and envelope times (a soft cubic curve into seconds). The result is a close
//  cousin, not a byte copy. Mod matrix / LFO routings are left out of this first version.
// =====================================================================================
static float clamp01 (float x) { return jlimit (0.0f, 1.0f, x); }

static void convertDune (const std::map<String, float>& P, std::map<String, float>& v)
{
    auto g = [&] (const char* n, float d = 0.0f) -> float
    {
        auto it = P.find (n);
        return it == P.end() ? d : it->second;
    };
    // normalised 0..1 time knob -> seconds (soft cubic, matches the feel of the Hive importer)
    auto envTime = [] (float n, float maxS) { n = clamp01 (n); return 0.001f + maxS * n * n * n; };
    // DUNE tuning knobs are centred at 0.5; map proportionally onto OZ's range (approximate).
    auto semi = [&] (const char* n) { return (float) jlimit (-12, 12, roundToInt ((g (n, 0.5f) - 0.5f) * 24.0f)); };
    auto fine = [&] (const char* n) { return jlimit (-100.0f, 100.0f, (g (n, 0.5f) - 0.5f) * 200.0f); };

    // ---- oscillators: Osc 1 -> wavetable engine, Osc 2 -> analog engine ----
    // DUNE routes an osc via "Osc N Amt" and/or "Mixer: Osc N"; take whichever is louder so a
    // preset that uses Amt (mixer at 0) still imports with sound. "Voices Amount" is a raw count,
    // not normalised. "Osc 1 Semi" is skipped (its DUNE id collides and reads garbage).
    const int   voices = jmax (1, roundToInt (g ("Voices Amount", 1.0f)));
    const float o1 = jmax (g ("Mixer: Osc 1"), g ("Osc 1 Amt"));
    if (o1 > 0.001f)
    {
        v["wt_on"] = 1.0f;
        v["wt_level"]  = clamp01 (o1);
        v["wt_pan"]    = jlimit (-1.0f, 1.0f, g ("Mixer: Osc 1 Pan", 0.5f) * 2.0f - 1.0f);
        v["wt_pos"]    = clamp01 (g ("Osc 1 WT Position", 0.0f));
        v["wt_unison"] = (float) jlimit (1, 7, voices);
        v["wt_detune"] = clamp01 (g ("Unison Detune", 0.25f));
        v["wt_spread"] = clamp01 (g ("Unison Spread", 0.7f));
        v["wt_fine"]   = fine ("Osc 1 Fine");
    }
    else v["wt_on"] = 0.0f;

    const float o2 = jmax (g ("Mixer: Osc 2"), g ("Osc 2 Amt"));
    if (o2 > 0.001f)
    {
        v["an_on"] = 1.0f;
        v["an_level"] = clamp01 (o2);
        v["an_pan"]   = jlimit (-1.0f, 1.0f, g ("Mixer: Osc 2 Pan", 0.5f) * 2.0f - 1.0f);
        v["an_semi"]  = semi ("Osc 2 Semi");
        v["an_fine"]  = fine ("Osc 2 Fine");
    }
    if (o1 <= 0.001f && o2 <= 0.001f) { v["wt_on"] = 1.0f; v["wt_level"] = 0.75f; }  // never import silent

    // ---- filter 1 (cutoff/res/drive are 0..1 in both; carried faithfully) ----
    v["flt_on"]     = 1.0f;
    v["flt_cutoff"] = clamp01 (g ("Filter 1 Cutoff", 0.85f));
    v["flt_res"]    = clamp01 (g ("Filter 1 Resonance", 0.0f));
    v["flt_drive"]  = clamp01 (g ("Filter 1 Drive", 0.0f));
    v["flt_env"]    = jlimit (-1.0f, 1.0f, g ("Filter 1 Env Amount", 0.0f));
    v["flt_key"]    = clamp01 (g ("Filter 1 Key-Trk", 0.0f));

    // ---- filter 2 only if it looks used (not sitting wide open and clean) ----
    const float f2c = g ("Filter 2 Cutoff", 1.0f), f2r = g ("Filter 2 Resonance"), f2d = g ("Filter 2 Drive");
    if (f2c < 0.98f || f2r > 0.01f || f2d > 0.01f)
    {
        v["flt2_on"]     = 1.0f;
        v["flt2_cutoff"] = clamp01 (f2c);
        v["flt2_res"]    = clamp01 (f2r);
        v["flt2_drive"]  = clamp01 (f2d);
        v["flt2_env"]    = jlimit (-1.0f, 1.0f, g ("Filter 2 Env Amount", 0.0f));
        v["flt_route"]   = 1.0f;   // parallel
    }

    // ---- envelopes: Amp -> amp, Filter env -> Env 2 (OZ's filter-envelope source) ----
    v["amp_a"] = envTime (g ("Amp Env Attack"),  8.0f);
    v["amp_d"] = jmax (0.001f, envTime (g ("Amp Env Decay",   0.3f), 10.0f));
    v["amp_s"] = clamp01 (g ("Amp Env Sustain", 0.85f));
    v["amp_r"] = jmax (0.005f, envTime (g ("Amp Env Release", 0.3f), 12.0f));
    v["e2_a"]  = envTime (g ("Filter Env Attack"), 8.0f);
    v["e2_d"]  = jmax (0.001f, envTime (g ("Filter Env Decay",   0.3f), 10.0f));
    v["e2_s"]  = clamp01 (g ("Filter Env Sustain", 0.0f));
    v["e2_r"]  = jmax (0.005f, envTime (g ("Filter Env Release", 0.3f), 12.0f));

    // ---- effects: carry the send/amount when the unit is active ----
    if (g ("FX-1 Reverb Active") > 0.5f)
    {
        v["rev_mix"]  = jlimit (0.0f, 0.6f, g ("FX-1 Reverb Dry/Wet", 0.3f));
        v["rev_size"] = clamp01 (g ("FX-1 Reverb Time", 0.6f));
        v["rev_damp"] = clamp01 (g ("FX-1 Reverb HF Damp", 0.5f));
    }
    if (g ("FX-1 Delay 1 Active") > 0.5f)
    {
        v["dly_mix"] = jlimit (0.0f, 0.5f, g ("FX-1 Delay 1 Dry/Wet", 0.3f));
        v["dly_fb"]  = jlimit (0.0f, 0.95f, g ("FX-1 Delay 1 Feedback", 0.35f));
    }
    if (g ("FX-1 Chorus Active") > 0.5f)
    {
        v["cho_mix"]   = jlimit (0.0f, 0.5f, g ("FX-1 Chorus Dry/Wet", 0.3f));
        v["cho_depth"] = clamp01 (g ("FX-1 Chorus Depth", 0.4f));
    }
    if (g ("FX-1 Distortion Active") > 0.5f)
        v["fx_drive"] = jlimit (0.0f, 0.8f, g ("FX-1 Distort Drive", 0.0f));

    // ---- arpeggiator (on/off; DUNE's rate encoding isn't exposed, so leave OZ's default rate) ----
    if (g ("Arp 1") > 0.5f) v["arp_on"] = 1.0f;
}

// OZ-mosys parameter defaults (real values), for the sparse write: a value equal to its default
// is left out, so loadPresetFromFile fills it in. Mirrors Source/Params.h createLayout().
static const std::map<String, float>& ozDefaults()
{
    static const std::map<String, float> d {
        { "wt_on", 1 }, { "wt_level", 0.75f }, { "wt_pan", 0 }, { "wt_pos", 0.66f },
        { "wt_unison", 1 }, { "wt_detune", 0.25f }, { "wt_spread", 0.7f }, { "wt_semi", 0 }, { "wt_fine", 0 },
        { "an_on", 0 }, { "an_level", 0.7f }, { "an_pan", 0 }, { "an_semi", 0 }, { "an_fine", 0 },
        { "flt_on", 1 }, { "flt_cutoff", 0.85f }, { "flt_res", 0.15f }, { "flt_drive", 0 }, { "flt_env", 0 }, { "flt_key", 0 },
        { "flt2_on", 0 }, { "flt2_cutoff", 0.3f }, { "flt2_res", 0.2f }, { "flt2_drive", 0 }, { "flt2_env", 0 }, { "flt_route", 0 },
        { "amp_a", 0.005f }, { "amp_d", 0.3f }, { "amp_s", 0.85f }, { "amp_r", 0.35f },
        { "e2_a", 0.01f }, { "e2_d", 0.5f }, { "e2_s", 0 }, { "e2_r", 0.5f },
        { "rev_mix", 0.15f }, { "rev_size", 0.6f }, { "rev_damp", 0.5f },
        { "dly_mix", 0 }, { "dly_fb", 0.35f }, { "cho_mix", 0 }, { "cho_depth", 0.4f },
        { "fx_drive", 0 }, { "arp_on", 0 }
    };
    return d;
}

static bool writeOzPreset (const File& file, const String& name, const String& bank, const String& notes,
                           const std::map<String, float>& values)
{
    XmlElement root ("FourfoldState");
    root.setAttribute ("presetName", name);
    root.setAttribute ("bank", bank);
    root.setAttribute ("author", "Imported (DUNE 3)");
    root.setAttribute ("tags", "dune, imported");
    root.setAttribute ("notes", notes);
    root.setAttribute ("sparse", 1);
    const auto& def = ozDefaults();
    for (auto& [id, val] : values)
    {
        auto it = def.find (id);                                   // skip anything already at OZ's default
        if (it != def.end() && std::abs (val - it->second) < 1.0e-4f) continue;
        auto* p = root.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", val);
    }
    root.createNewChildElement ("Msegs");
    file.getParentDirectory().createDirectory();
    return root.writeTo (file);
}

// =====================================================================================
//  Direct DUNE 3 .fxp reader (no plugin needed).
//
//  A DUNE .fxp is a VST2 FPCh wrapper around a small header + a zlib stream. Inflated, it holds
//  a `para` block: int32 count, then `count` records of [uint32 rawid][float value], where
//  rawid low-16 = DUNE's internal parameter id and high-16 = voice/sub index. We read the real
//  per-preset values this way - unlike hosting DUNE, whose VST3 ignores the .fxp entirely.
//
//  The id->name map was recovered from a parameter-name table inside DUNE3.vst3 (entries of
//  [int32 id][int32 flag][char name[36]], in host-enumeration order). Only the ids used for the
//  OZ mapping are listed here, each validated to read sane values across several presets.
//  ("Osc 1 Semi" id 1 is intentionally omitted - its id collides and reads garbage.)
// =====================================================================================
struct DuneParam { const char* name; int id; };
static const DuneParam kDuneParams[] = {
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

static MemoryBlock inflateFxp (const File& f)
{
    MemoryBlock raw;
    if (! f.loadFileAsData (raw) || raw.getSize() < 128) return {};
    const auto* d = static_cast<const uint8*> (raw.getData());
    // DUNE-only guard: a VST2 FPCh whose fxID is 'DNE3'. Skips any other synth's .fxp safely.
    if (std::memcmp (d, "CcnK", 4) != 0 || std::memcmp (d + 8, "FPCh", 4) != 0 || std::memcmp (d + 16, "DNE3", 4) != 0) return {};
    int z = -1;                                           // find the zlib header (0x78 0x9c) after the FPCh header
    for (int i = 60; i < 200 && (size_t) (i + 1) < raw.getSize(); ++i)
        if (d[i] == 0x78 && d[i + 1] == 0x9c) { z = i; break; }
    if (z < 0) return {};
    MemoryInputStream mis (d + z, raw.getSize() - (size_t) z, false);
    GZIPDecompressorInputStream gz (&mis, false, GZIPDecompressorInputStream::zlibFormat);
    MemoryBlock out;
    gz.readIntoMemoryBlock (out);
    return out;
}

// parse a DUNE .fxp into name -> value (real per-preset, normalised 0..1; "Voices Amount" is a count)
static bool parseDuneFxp (const File& f, std::map<String, float>& P)
{
    const auto mb = inflateFxp (f);
    if (mb.getSize() < 348) return false;
    const auto* b = static_cast<const uint8*> (mb.getData());
    auto u32 = [&] (size_t o) { return (uint32) (b[o] | (b[o+1] << 8) | (b[o+2] << 16) | ((uint32) b[o+3] << 24)); };
    auto f32 = [&] (size_t o) { float v; std::memcpy (&v, b + o, 4); return v; };

    // Locate the 'arap' (para) block. Its offset varies per preset - wavetable-heavy patches push
    // it past the fixed position - so search rather than assume. Header after the tag:
    //   'arap' | int32 (8) | int32 count | int32 baseId | records[count] of [uint32 id][float].
    size_t ap = 0; bool found = false;
    for (size_t i = 0; i + 24 < mb.getSize(); ++i)
        if (b[i] == 'a' && b[i+1] == 'r' && b[i+2] == 'a' && b[i+3] == 'p') { ap = i; found = true; break; }
    if (! found) return false;
    const uint32 count = u32 (ap + 8);
    const size_t base = ap + 20;
    if (count == 0 || count > 100000 || base + (size_t) count * 8 > mb.getSize()) return false;

    std::map<int, float> byId;                            // id -> value (prefer voice 0)
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
        const float v = it->second;
        if (std::abs (v) > 100.0f) continue;              // guard against any stray garbage read
        P[dp.name] = v;
    }
    return ! P.empty();
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: PluginProbe --fxp2oz <PackName> <preset.fxp | folder>   (DUNE 3 -> OZ, no plugin needed)\n"
                  << "   or: PluginProbe <plugin.vst3|.dll|.component> <preset.fxp | folder>   (oracle JSON dump)\n";
        return 1;
    }

    // ---------- direct DUNE .fxp -> OZ (no plugin hosted) ----------
    if (std::strcmp (argv[1], "--fxp2oz") == 0)
    {
        if (argc < 4) { std::cerr << "usage: --fxp2oz <PackName> <preset.fxp | folder>\n"; return 1; }
        const String pack = CharPointer_UTF8 (argv[2]);
        const String inPath = CharPointer_UTF8 (argv[3]);
        const File in (inPath);
        Array<File> presets;
        if (in.isDirectory()) presets = in.findChildFiles (File::findFiles, true, "*.fxp");
        else                  presets.add (in);

        const auto packDir = File::getSpecialLocation (File::userDocumentsDirectory)
                               .getChildFile ("OZ-mosys").getChildFile ("Expansions")
                               .getChildFile (File::createLegalFileName (pack));
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", pack);
            px.setAttribute ("tagline", "Imported DUNE 3 presets, rebuilt as OZ-mosys cousins");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }

        int written = 0, skipped = 0;
        for (auto& pf : presets)
        {
            std::map<String, float> P;
            if (! parseDuneFxp (pf, P)) { ++skipped; continue; }
            std::map<String, float> v;
            convertDune (P, v);

            String sub = in.isDirectory() ? pf.getParentDirectory().getRelativePathFrom (in).replaceCharacter ('\\', '/') : String();
            if (sub == ".") sub = {};
            auto dir = packDir.getChildFile ("Presets");
            if (sub.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (sub.replaceCharacter ('/', ' ')));

            const String name = pf.getFileNameWithoutExtension();
            const String notes = "Imported from Synapse DUNE 3 by reading the preset file directly (no plugin). "
                                 "A close cousin: filters, oscillator levels, pan, envelopes and effect sends are "
                                 "carried from the preset's real values; tuning is approximated.";
            if (writeOzPreset (dir.getChildFile (File::createLegalFileName (name) + ".ozpreset"), name, pack, notes, v))
                ++written;
            else ++skipped;
        }
        std::cerr << "wrote " << written << " presets into " << packDir.getFullPathName()
                  << (skipped ? (" (" + String (skipped) + " skipped)") : String()) << "\n";
        return 0;
    }

    // ---------- direct Reveal Sound Spire .spf -> OZ (no plugin hosted) ----------
    if (std::strcmp (argv[1], "--spf2oz") == 0)
    {
        if (argc < 4) { std::cerr << "usage: --spf2oz <PackName> <preset.spf | folder>\n"; return 1; }
        const String pack = CharPointer_UTF8 (argv[2]);
        const String inPath = CharPointer_UTF8 (argv[3]);   // copy-init: avoids the most-vexing-parse
        const File in (inPath);
        Array<File> presets;
        if (in.isDirectory()) presets = in.findChildFiles (File::findFiles, true, "*.spf");
        else                  presets.add (in);

        const auto packDir = File::getSpecialLocation (File::userDocumentsDirectory)
                               .getChildFile ("OZ-mosys").getChildFile ("Expansions")
                               .getChildFile (File::createLegalFileName (pack));
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", pack);
            px.setAttribute ("tagline", "Imported Spire presets, rebuilt as OZ-mosys cousins");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }

        int written = 0, skipped = 0;
        for (auto& pf : presets)
        {
            std::map<String, float> P;
            if (! ff::import::spire::parseSpf (pf, P)) { ++skipped; continue; }
            std::map<String, float> v;
            ff::import::spire::toOz (P, v);

            String sub = in.isDirectory() ? pf.getParentDirectory().getRelativePathFrom (in).replaceCharacter ('\\', '/') : String();
            if (sub == ".") sub = {};
            auto dir = packDir.getChildFile ("Presets");
            if (sub.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (sub.replaceCharacter ('/', ' ')));

            const String name = pf.getFileNameWithoutExtension();
            const String notes = "Imported from Reveal Sound Spire by reading the preset file directly. A close "
                                 "cousin: oscillators, filters, envelopes, pan and effect sends from the preset's "
                                 "real values; tuning and times approximate.";
            if (writeOzPreset (dir.getChildFile (File::createLegalFileName (name) + ".ozpreset"), name, pack, notes, v))
                ++written;
            else ++skipped;
        }
        std::cerr << "wrote " << written << " presets into " << packDir.getFullPathName()
                  << (skipped ? (" (" + String (skipped) + " skipped)") : String()) << "\n";
        return 0;
    }

    ScopedJuceInitialiser_GUI juceInit;   // plugin scanning/hosting needs the message manager

    AudioPluginFormatManager fm;
    fm.addDefaultFormats();

    const String pluginPath = CharPointer_UTF8 (argv[1]);   // copy-init: avoids the most-vexing-parse
    const File pluginFile (pluginPath);
    OwnedArray<PluginDescription> types;
    for (auto* fmt : fm.getFormats())
        fmt->findAllTypesForFile (types, pluginFile.getFullPathName());
    if (types.isEmpty())
    {
        std::cerr << "no plugin found in " << argv[1] << "\n";
        return 2;
    }

    String err;
    auto inst = fm.createPluginInstance (*types[0], 44100.0, 512, err);
    if (inst == nullptr)
    {
        std::cerr << "could not load plugin: " << err << "\n";
        return 3;
    }
    inst->prepareToPlay (44100.0, 512);

    // ---------- RE/calibration: dump DUNE's own serialized state, optionally after nudging one param ----------
    // usage: PluginProbe <plugin> --state <outfile> [<paramIndex> <value>]
    if (std::strcmp (argv[2], "--state") == 0)
    {
        if (argc < 4) { std::cerr << "usage: --state <outfile> [<paramIndex> <value>]\n"; return 1; }
        if (argc >= 6)
        {
            const int idx = std::atoi (argv[4]);
            const float val = (float) std::atof (argv[5]);
            auto& ps = inst->getParameters();
            if (idx >= 0 && idx < ps.size() && ps[idx] != nullptr)
            {
                ps[idx]->setValueNotifyingHost (val);
                MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
        }
        MemoryBlock st;
        inst->getStateInformation (st);
        const String outPath = CharPointer_UTF8 (argv[3]);
        File (outPath).replaceWithData (st.getData(), (size_t) st.getSize());
        std::cerr << "state bytes: " << (int) st.getSize() << "\n";
        inst->releaseResources();
        return 0;
    }

    Array<File> presets;
    const String arg2Path = CharPointer_UTF8 (argv[2]);     // copy-init: avoids the most-vexing-parse
    const File arg2 (arg2Path);
    if (arg2.isDirectory()) presets = arg2.findChildFiles (File::findFiles, true, "*.fxp");
    else                    presets.add (arg2);

    // ---------- default mode: JSON dump ----------
    std::cout << "[\n";
    bool firstPreset = true;
    for (auto& pf : presets)
    {
        MemoryBlock raw;
        if (fxpChunk (pf).getSize() == 0 || ! pf.loadFileAsData (raw)) continue;   // not an opaque-chunk .fxp

        inst->setStateInformation (raw.getData(), (int) raw.getSize());            // feed the whole .fxp
        MessageManager::getInstance()->runDispatchLoopUntil (40);   // let VST3 push values to the host

        if (! firstPreset) std::cout << ",\n";
        firstPreset = false;

        std::cout << "  {" << jstr ("preset") << ":" << jstr (pf.getFileNameWithoutExtension()) << ","
                  << jstr ("params") << ":{";
        bool firstParam = true;
        auto& params = inst->getParameters();
        for (int pi = 0; pi < params.size(); ++pi)
        {
            auto* p = params[pi];
            if (p == nullptr) continue;
            if (! firstParam) std::cout << ",";
            firstParam = false;
            // pid = the plugin's own parameter ID (VST3 ParamID). DUNE's .fxp indexes its value
            //   array by this ID, so pid is the key that maps a chunk value back to this name.
            // v = normalised 0..1; t/u = display text / unit (empty for DUNE); n/d = steps / discrete.
            String pid;
            if (auto* hp = inst->getHostedParameter (pi)) pid = hp->getParameterID();
            std::cout << jstr (p->getName (64)) << ":{"
                      << jstr ("pid") << ":" << jstr (pid) << ","
                      << jstr ("v") << ":" << String (p->getValue(), 6) << ","
                      << jstr ("t") << ":" << jstr (p->getText (p->getValue(), 0)) << ","
                      << jstr ("u") << ":" << jstr (p->getLabel()) << ","
                      << jstr ("n") << ":" << p->getNumSteps() << ","
                      << jstr ("d") << ":" << (p->isDiscrete() ? 1 : 0) << "}";
        }
        std::cout << "}}";
    }
    std::cout << "\n]\n";

    inst->releaseResources();
    return 0;
}
