#pragma once
// =====================================================================================
//  Reveal Sound Spire (.spf) -> VIBRA-VII, shared by the in-plugin importer (SpireImport.cpp)
//  and the PluginProbe --spf2oz CLI. No plugin needed: a .spf is a fixed 3133-byte file - a text
//  header (name @0, category @8, notes @17) then a float param block at offset 281 + (id-1)*4,
//  1-based, normalised 0..1. The parameter name->id map is taken from the table inside Spire.vst3
//  (see docs/PLUGIN_ORACLE.md). We read each preset's real values and build a close OZ cousin.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <cmath>
#include <algorithm>

namespace ff::import::spire
{
// Spire parameter name -> id (from the DLL's "name:id" table). Only the ids the OZ mapping needs.
struct SpireParam { const char* name; int id; };
inline const SpireParam* params (int& count)
{
    static const SpireParam p[] = {
        // all 4 oscillators (a Spire preset may use any of them, so we read all and pick the loudest)
        { "Osc1_Amp", 1 }, { "Osc1_Pitch", 5 }, { "Osc1_Detune", 10 }, { "Osc1_Density", 11 }, { "Osc1_Wide", 12 }, { "Osc1_Pan", 13 },
        { "Osc2_Amp", 2 }, { "Osc2_Pitch", 15 }, { "Osc2_Detune", 20 }, { "Osc2_Density", 21 }, { "Osc2_Wide", 22 }, { "Osc2_Pan", 23 },
        { "Osc3_Amp", 3 }, { "Osc3_Pitch", 25 }, { "Osc3_Detune", 30 }, { "Osc3_Density", 31 }, { "Osc3_Wide", 32 }, { "Osc3_Pan", 33 },
        { "Osc4_Amp", 4 }, { "Osc4_Pitch", 35 }, { "Osc4_Detune", 40 }, { "Osc4_Density", 41 }, { "Osc4_Wide", 42 }, { "Osc4_Pan", 43 },
        { "Cutoff_1", 45 }, { "Reso_1", 46 }, { "Cutoff_2", 47 }, { "Reso_2", 48 }, { "Filt_Balance", 49 },
        { "Env1_Att", 50 }, { "Env1_Dec", 51 }, { "Env1_Sus", 52 }, { "Env1_Rel", 55 },
        { "Env2_Att", 58 }, { "Env2_Dec", 59 }, { "Env2_Sus", 60 }, { "Env2_Rel", 63 },
        { "LFO1_Rate", 82 },
        { "Shp_Drive", 110 }, { "Shp_DryWet", 115 },
        { "Chr_Depth", 125 }, { "Chr_DryWet", 129 },
        { "Del_FBack", 132 }, { "Del_DryWet", 137 },
        { "Rev_Decay", 139 }, { "Rev_Damp", 140 }, { "Rev_DryWet", 143 },
        { "Glide", 221 },
    };
    count = (int) (sizeof (p) / sizeof (p[0]));
    return p;
}

// Read a .spf into name -> value (normalised 0..1). false if it is not a 3133-byte Spire preset.
inline bool parseSpf (const juce::File& f, std::map<juce::String, float>& out)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return false;
    const size_t sz = mb.getSize();
    if (sz < 3100 || sz > 3200) return false;   // a Spire preset record (seen across versions: 3121 / 3133 / 3165)
    const auto* b = static_cast<const juce::uint8*> (mb.getData());
    auto f32 = [&] (size_t o) { float v; std::memcpy (&v, b + o, 4); return v; };
    // The 777-float param block is anchored to the end of the file; only the text header length
    // varies by version. Param id 1 sits at (size - 2852) - i.e. 281 for a 3133-byte preset.
    const size_t paramBase = sz - 2852;
    int n = 0; const auto* P = params (n);
    for (int i = 0; i < n; ++i)
    {
        const size_t o = paramBase + (size_t) (P[i].id - 1) * 4;
        if (o + 4 > sz) continue;
        const float v = f32 (o);
        if (std::isfinite (v)) out[P[i].name] = v;
    }
    return ! out.empty();
}

// Build OZ real parameter values from a parsed Spire preset (unsparsed; caller drops defaults).
inline void toOz (const std::map<juce::String, float>& P, std::map<juce::String, float>& v)
{
    using namespace juce;
    auto g = [&] (const char* n, float d = 0.0f) -> float { auto it = P.find (n); return it == P.end() ? d : it->second; };
    auto c01 = [] (float x) { return jlimit (0.0f, 1.0f, x); };
    auto envTime = [] (float n, float maxS) { n = jlimit (0.0f, 1.0f, n); return 0.001f + maxS * n * n * n; };

    for (auto* e : { "wt", "an", "gr", "res", "fm" }) v[String (e) + "_on"] = 0.0f;

    // ---- oscillators: a Spire preset may use any of its 4 oscillators, so pick the two loudest
    //      and map them to OZ's wavetable + analog engines (osc level / pitch / unison / pan). ----
    struct O { float amp, pitch, detune, density, wide, pan; };
    O osc[4];
    for (int i = 0; i < 4; ++i)
    {
        const String pre = "Osc" + String (i + 1) + "_";
        osc[i] = { g ((pre + "Amp").toRawUTF8()), g ((pre + "Pitch").toRawUTF8(), 0.5f),
                   g ((pre + "Detune").toRawUTF8(), 0.25f), g ((pre + "Density").toRawUTF8()),
                   g ((pre + "Wide").toRawUTF8(), 0.7f), g ((pre + "Pan").toRawUTF8(), 0.5f) };
    }
    int order[4] = { 0, 1, 2, 3 };                                    // sort oscillator indices by amp, loudest first
    std::sort (std::begin (order), std::end (order), [&] (int a, int b) { return osc[a].amp > osc[b].amp; });
    auto applyOsc = [&] (const O& o, const char* pre, bool withUnison)
    {
        v[String (pre) + "_on"] = 1.0f;
        v[String (pre) + "_level"] = c01 (o.amp);
        v[String (pre) + "_pan"] = jlimit (-1.0f, 1.0f, o.pan * 2.0f - 1.0f);
        v[String (pre) + "_semi"] = (float) jlimit (-12, 12, roundToInt ((o.pitch - 0.5f) * 48.0f));
        if (withUnison)
        {
            v["wt_unison"] = (float) jlimit (1, 7, 1 + roundToInt (o.density * 8.0f));
            v["wt_detune"] = c01 (o.detune);
            v["wt_spread"] = c01 (o.wide);
        }
    };
    if (osc[order[0]].amp > 0.004f) applyOsc (osc[order[0]], "wt", true);
    else { v["wt_on"] = 1.0f; v["wt_level"] = 0.75f; }                // never import silent
    if (osc[order[1]].amp > 0.004f) applyOsc (osc[order[1]], "an", false);

    // ---- filters (Spire cutoff/reso are 0..1) ----
    v["flt_on"]     = 1.0f;
    v["flt_cutoff"] = c01 (g ("Cutoff_1", 0.85f));
    v["flt_res"]    = c01 (g ("Reso_1", 0.0f));
    const float c2 = g ("Cutoff_2", 1.0f);
    if (c2 < 0.98f) { v["flt2_on"] = 1.0f; v["flt2_cutoff"] = c01 (c2); v["flt2_res"] = c01 (g ("Reso_2")); v["flt_route"] = 1.0f; }

    // ---- envelopes: Env1 -> amp, Env2 -> Env 2 (filter) ----
    v["amp_a"] = envTime (g ("Env1_Att"), 8.0f);
    v["amp_d"] = jmax (0.001f, envTime (g ("Env1_Dec", 0.3f), 10.0f));
    v["amp_s"] = c01 (g ("Env1_Sus", 0.85f));
    v["amp_r"] = jmax (0.005f, envTime (g ("Env1_Rel", 0.3f), 12.0f));
    v["e2_a"]  = envTime (g ("Env2_Att"), 8.0f);
    v["e2_d"]  = jmax (0.001f, envTime (g ("Env2_Dec", 0.3f), 10.0f));
    v["e2_s"]  = c01 (g ("Env2_Sus", 0.0f));
    v["e2_r"]  = jmax (0.005f, envTime (g ("Env2_Rel", 0.3f), 12.0f));

    // ---- effects (Spire dry/wet are 0..1) ----
    if (g ("Rev_DryWet") > 0.004f)
    {
        v["rev_mix"]  = jlimit (0.0f, 0.6f, g ("Rev_DryWet"));
        v["rev_size"] = c01 (g ("Rev_Decay", 0.6f));
        v["rev_damp"] = c01 (g ("Rev_Damp", 0.5f));
    }
    if (g ("Del_DryWet") > 0.004f) { v["dly_mix"] = jlimit (0.0f, 0.5f, g ("Del_DryWet")); v["dly_fb"] = jlimit (0.0f, 0.95f, g ("Del_FBack", 0.35f)); }
    if (g ("Chr_DryWet") > 0.004f) { v["cho_mix"] = jlimit (0.0f, 0.5f, g ("Chr_DryWet")); v["cho_depth"] = c01 (g ("Chr_Depth", 0.4f)); }
    if (g ("Shp_DryWet") > 0.004f) v["fx_drive"] = jlimit (0.0f, 0.8f, g ("Shp_Drive"));

    // ---- glide ----
    if (g ("Glide") > 0.004f) v["glide"] = jlimit (0.0f, 2.0f, g ("Glide") * 2.0f);
}
} // namespace ff::import::spire
