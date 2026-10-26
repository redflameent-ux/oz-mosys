// =====================================================================================
//  PresetForge - generate an original VIBRA-VII preset library from curated "archetypes".
//
//  Each archetype is a hand-designed hero patch (real OZ parameter values) plus tasteful
//  per-knob variation ranges. For each one we spin up variations, RENDER every variation through
//  OZ and measure it (ff::design), drop the silent/blown-out ones, tag the rest by character, and
//  write them as .ozpreset files into one expansion pack, organised by category.
//
//  The palette (Trance-6): dark horrorcore Memphis / Three 6 Mafia, Nine Inch Nails industrial,
//  and Aphex Twin / Squarepusher IDM-braindance, over a meditative world undertow (trap drums
//  blended with ritual/ambient beds). Built from OZ's Analog, Wavetable, Resonator (bells, bowls,
//  mallets, world perc), Granular (choirs, chant, breath, rain) and FM engines, with a weighting
//  pass that leans hard into the dark/industrial/glitch end.
//
//  Opt-in + local only (cmake -DFOURFOLD_BUILD_TOOLS=ON); never built in CI.
//
//  Usage:  PresetForge "<PackName>" [<total-preset-target>]     (default 60)
// =====================================================================================
#include "../Source/PluginProcessor.h"
#include "../Source/import/OmniImport.h"      // Converted + writePreset
#include "../Source/design/SoundFeatures.h"
#include <iostream>
#include <random>
#include <set>
#include <map>

using namespace juce;

struct Vary
{
    String id; float lo = 0, hi = 0; bool isInt = false; std::vector<int> choices;
    Vary (String i, float l, float h, bool n = false) : id (std::move (i)), lo (l), hi (h), isInt (n) {}
    Vary (String i, std::vector<int> c) : id (std::move (i)), choices (std::move (c)) {}
};

struct Archetype
{
    String name, folder, tags;
    std::map<String, float> base;
    std::vector<Vary> vary;
    float weight = 1.0f;   // relative share of the pack (set by a post-pass on name+tags)
};

// ---------------------------------------------------------------------------------------
static std::vector<Archetype> archetypes()
{
    std::vector<Archetype> a;

    // A struck/plucked/ringing resonator voice (covers most bells, mallets and world percussion).
    auto res = [] (const char* name, const char* folder, const char* tags, int type,
                   float decay, float bright, float ampD, float ampR, float rev,
                   std::vector<Vary> extra = {}) -> Archetype
    {
        Archetype x; x.name = name; x.folder = folder; x.tags = tags;
        x.base = { { "res_on", 1 }, { "res_type", (float) type }, { "res_decay", decay }, { "res_bright", bright },
                   { "res_damp", 0.5f }, { "res_strike", 0.6f }, { "res_level", 0.82f },
                   { "amp_a", 0.001f }, { "amp_d", ampD }, { "amp_s", 0.0f }, { "amp_r", ampR } };
        if (rev > 0) { x.base["rev_mix"] = rev; x.base["rev_size"] = 0.7f; }
        x.vary = { { "res_decay", decay * 0.6f, decay * 1.5f }, { "res_bright", jmax (0.08f, bright - 0.25f), jmin (1.0f, bright + 0.25f) },
                   { "res_strike", 0.3f, 0.85f }, { "res_damp", 0.3f, 0.7f } };
        for (auto& v : extra) x.vary.push_back (v);
        return x;
    };

    // ---- Bass ----
    a.push_back ({ "808 Glide Boom", "Bass", "808, bass, sub, trap, crunk",
        { { "an_on", 1 }, { "an_shape", 0.0f }, { "an_oct", -2 }, { "an_sub", 0.25f }, { "an_tone", 1.0f }, { "an_level", 0.88f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.38f }, { "flt_res", 0.12f }, { "flt_drive", 0.2f },
          { "amp_a", 0.002f }, { "amp_d", 0.6f }, { "amp_s", 0.85f }, { "amp_r", 0.5f },
          { "fx_drive", 0.35f }, { "voice_mode", 1 }, { "glide", 0.08f }, { "master", 0.72f } },
        { { "an_oct", { -2, -1 } }, { "flt_cutoff", 0.25f, 0.55f }, { "fx_drive", 0.1f, 0.65f }, { "drive_type", { 0, 1, 2 } },
          { "amp_r", 0.3f, 1.3f }, { "amp_d", 0.3f, 0.9f }, { "glide", 0.03f, 0.22f } } });
    a.push_back ({ "Reese Bass", "Bass", "bass, reese, detuned, dark, horror",
        { { "wt_on", 1 }, { "wt_table", 0 }, { "wt_pos", 0.2f }, { "wt_oct", -1 }, { "wt_unison", 4 }, { "wt_detune", 0.5f }, { "wt_spread", 0.4f }, { "wt_level", 0.82f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.4f }, { "flt_res", 0.3f },
          { "amp_a", 0.004f }, { "amp_d", 0.5f }, { "amp_s", 0.9f }, { "amp_r", 0.3f }, { "voice_mode", 1 }, { "glide", 0.05f }, { "fx_drive", 0.25f } },
        { { "wt_detune", 0.35f, 0.7f }, { "flt_cutoff", 0.28f, 0.55f }, { "flt_res", 0.15f, 0.45f }, { "fx_drive", 0.1f, 0.45f }, { "wt_oct", { -2, -1 } } } });
    a.push_back ({ "G-Funk Bass", "Bass", "bass, g-funk, round, mono, funk",
        { { "an_on", 1 }, { "an_shape", 0.6f }, { "an_oct", -1 }, { "an_sub", 0.3f }, { "an_level", 0.85f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.45f }, { "flt_res", 0.15f },
          { "amp_a", 0.003f }, { "amp_d", 0.4f }, { "amp_s", 0.8f }, { "amp_r", 0.25f }, { "voice_mode", 1 }, { "glide", 0.06f } },
        { { "an_shape", 0.2f, 1.1f }, { "flt_cutoff", 0.35f, 0.6f }, { "an_sub", 0.15f, 0.5f }, { "glide", 0.03f, 0.15f } } });
    a.push_back ({ "Growl Bass", "Bass", "bass, growl, aggressive, fm, distorted",
        { { "fm_on", 1 }, { "fm_oct", -1 }, { "fm_index", 1.4f }, { "fm_level", 0.82f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.5f }, { "flt_res", 0.3f }, { "flt_drive", 0.3f },
          { "amp_a", 0.003f }, { "amp_d", 0.4f }, { "amp_s", 0.85f }, { "amp_r", 0.25f }, { "voice_mode", 1 }, { "fx_drive", 0.35f },
          { "lfo1_shape", 4 }, { "lfo1_rate", 7.0f }, { "mod1_src", 3 }, { "mod1_dst", 16 }, { "mod1_amt", 0.3f } },
        { { "fm_index", 0.8f, 2.0f }, { "flt_cutoff", 0.35f, 0.7f }, { "mod1_amt", 0.1f, 0.5f }, { "lfo1_rate", 3.0f, 10.0f }, { "fx_drive", 0.15f, 0.6f } } });
    a.push_back ({ "Industrial Bass", "Bass", "bass, industrial, nin, distorted, mechanical, aggressive, dark",
        { { "fm_on", 1 }, { "fm_oct", -1 }, { "fm_index", 2.0f }, { "fm_level", 0.8f },
          { "an_on", 1 }, { "an_shape", 2.0f }, { "an_oct", -1 }, { "an_level", 0.35f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.42f }, { "flt_res", 0.35f }, { "flt_drive", 0.5f },
          { "amp_a", 0.002f }, { "amp_d", 0.4f }, { "amp_s", 0.8f }, { "amp_r", 0.2f }, { "voice_mode", 1 },
          { "fx_drive", 0.6f }, { "drive_type", 2 }, { "crush_bits", 10 } },
        { { "fm_index", 1.2f, 3.0f }, { "flt_cutoff", 0.3f, 0.6f }, { "fx_drive", 0.4f, 0.85f }, { "crush_bits", { 6, 8, 10, 12 } },
          { "flt_res", 0.2f, 0.5f }, { "drive_type", { 1, 2 } } } });

    // ---- Leads ----
    a.push_back ({ "Crunk Siren Lead", "Leads", "lead, crunk, siren, aggressive",
        { { "an_on", 1 }, { "an_shape", 1.7f }, { "an_pw", 0.5f }, { "an_level", 0.82f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.72f }, { "flt_res", 0.25f }, { "flt_drive", 0.18f },
          { "amp_a", 0.004f }, { "amp_d", 0.4f }, { "amp_s", 0.9f }, { "amp_r", 0.25f },
          { "lfo1_shape", 1 }, { "lfo1_rate", 5.5f }, { "mod1_src", 3 }, { "mod1_dst", 21 }, { "mod1_amt", 0.08f },
          { "voice_mode", 1 }, { "glide", 0.04f }, { "fx_drive", 0.2f }, { "dly_mix", 0.12f } },
        { { "flt_cutoff", 0.55f, 0.9f }, { "lfo1_rate", 3.0f, 9.0f }, { "mod1_amt", 0.04f, 0.18f }, { "an_oct", { 0, 1 } },
          { "fx_drive", 0.1f, 0.4f }, { "flt_res", 0.1f, 0.4f }, { "an_shape", 1.3f, 2.0f } } });
    a.push_back ({ "G-Funk Lead", "Leads", "lead, g-funk, portamento, whiny",
        { { "an_on", 1 }, { "an_shape", 1.2f }, { "an_oct", 1 }, { "an_level", 0.8f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.78f }, { "flt_res", 0.28f },
          { "amp_a", 0.02f }, { "amp_d", 0.3f }, { "amp_s", 0.9f }, { "amp_r", 0.3f },
          { "lfo2_shape", 0 }, { "lfo2_rate", 5.5f }, { "lfo2_fade", 0.4f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.025f },
          { "voice_mode", 1 }, { "glide", 0.11f }, { "rev_mix", 0.2f }, { "dly_mix", 0.1f } },
        { { "an_oct", { 1, 2 } }, { "flt_cutoff", 0.6f, 0.9f }, { "glide", 0.05f, 0.2f }, { "mod1_amt", 0.01f, 0.05f },
          { "flt_res", 0.15f, 0.4f }, { "lfo2_rate", 4.0f, 7.0f } } });
    a.push_back ({ "Memphis Horror Lead", "Leads", "lead, memphis, horror, dark, detuned",
        { { "an_on", 1 }, { "an_shape", 1.6f }, { "an_oct", 0 }, { "an_fine", -12.0f }, { "an_level", 0.78f },
          { "wt_on", 1 }, { "wt_table", 5 }, { "wt_oct", 0 }, { "wt_level", 0.4f }, { "wt_detune", 0.3f }, { "wt_unison", 2 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f }, { "flt_res", 0.3f },
          { "amp_a", 0.01f }, { "amp_d", 0.4f }, { "amp_s", 0.85f }, { "amp_r", 0.4f }, { "voice_mode", 1 }, { "glide", 0.05f },
          { "rev_mix", 0.3f }, { "tape_age", 0.25f } },
        { { "flt_cutoff", 0.4f, 0.72f }, { "an_fine", -25.0f, -4.0f }, { "flt_res", 0.15f, 0.45f }, { "rev_mix", 0.15f, 0.45f }, { "wt_detune", 0.15f, 0.5f } } });
    a.push_back ({ "Supersaw Lead", "Leads", "lead, supersaw, wide, anthemic, trap",
        { { "wt_on", 1 }, { "wt_table", 0 }, { "wt_pos", 0.33f }, { "wt_unison", 7 }, { "wt_detune", 0.35f }, { "wt_spread", 0.8f }, { "wt_level", 0.82f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.82f }, { "flt_res", 0.15f },
          { "amp_a", 0.01f }, { "amp_d", 0.4f }, { "amp_s", 0.9f }, { "amp_r", 0.4f }, { "rev_mix", 0.25f }, { "dly_mix", 0.15f } },
        { { "wt_detune", 0.2f, 0.55f }, { "flt_cutoff", 0.65f, 0.95f }, { "wt_oct", { 0, 1 } }, { "rev_mix", 0.1f, 0.4f }, { "wt_pos", 0.1f, 0.6f } } });
    a.push_back ({ "Square Chip Lead", "Leads", "lead, square, chip, crunk, bright",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_pw", 0.5f }, { "an_oct", 1 }, { "an_level", 0.8f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.85f }, { "flt_res", 0.15f },
          { "amp_a", 0.002f }, { "amp_d", 0.2f }, { "amp_s", 0.85f }, { "amp_r", 0.15f }, { "voice_mode", 1 }, { "dly_mix", 0.18f } },
        { { "an_pw", 0.2f, 0.7f }, { "flt_cutoff", 0.7f, 0.95f }, { "an_oct", { 0, 1, 2 } }, { "dly_mix", 0.05f, 0.3f } } });
    a.push_back ({ "Trap Bell Lead", "Leads", "lead, bell, trap, pluck, bright",
        { { "res_on", 1 }, { "res_type", 16 }, { "res_decay", 1.2f }, { "res_bright", 0.75f }, { "res_level", 0.8f },
          { "an_on", 1 }, { "an_shape", 0.2f }, { "an_level", 0.3f }, { "an_oct", 0 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.8f },
          { "amp_a", 0.001f }, { "amp_d", 1.0f }, { "amp_s", 0.2f }, { "amp_r", 0.6f }, { "rev_mix", 0.3f }, { "dly_mix", 0.2f } },
        { { "res_type", { 16, 17, 7, 8 } }, { "res_bright", 0.55f, 0.95f }, { "rev_mix", 0.15f, 0.45f }, { "dly_mix", 0.05f, 0.35f }, { "res_decay", 0.7f, 2.0f } } });
    a.push_back ({ "Acid Squelch", "Leads", "lead, acid, 303, idm, squelch, aggressive, dark",
        { { "an_on", 1 }, { "an_shape", 1.9f }, { "an_level", 0.8f }, { "an_oct", 0 },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.5f }, { "flt_res", 0.6f }, { "flt_env", 0.6f }, { "flt_drive", 0.35f },
          { "amp_a", 0.002f }, { "amp_d", 0.25f }, { "amp_s", 0.6f }, { "amp_r", 0.15f }, { "e2_a", 0.001f }, { "e2_d", 0.25f },
          { "voice_mode", 1 }, { "glide", 0.06f }, { "fx_drive", 0.3f }, { "dly_mix", 0.15f } },
        { { "flt_cutoff", 0.3f, 0.7f }, { "flt_res", 0.45f, 0.82f }, { "flt_env", 0.4f, 0.8f }, { "e2_d", 0.1f, 0.5f },
          { "glide", 0.02f, 0.14f }, { "an_oct", { 0, 1 } }, { "fx_drive", 0.15f, 0.5f } } });
    a.push_back ({ "Factory Noise Lead", "Leads", "lead, industrial, noise, nin, distorted, mechanical, dark",
        { { "wt_on", 1 }, { "wt_table", 5 }, { "wt_pos", 0.6f }, { "wt_unison", 2 }, { "wt_detune", 0.3f }, { "wt_level", 0.7f },
          { "noise_level", 0.35f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f }, { "flt_res", 0.3f }, { "flt_drive", 0.4f },
          { "amp_a", 0.004f }, { "amp_d", 0.3f }, { "amp_s", 0.85f }, { "amp_r", 0.25f }, { "voice_mode", 1 },
          { "fx_drive", 0.5f }, { "drive_type", 2 }, { "crush_bits", 10 }, { "lfo1_shape", 4 }, { "lfo1_rate", 8.0f },
          { "mod1_src", 3 }, { "mod1_dst", 16 }, { "mod1_amt", 0.25f } },
        { { "flt_cutoff", 0.4f, 0.75f }, { "fx_drive", 0.3f, 0.75f }, { "noise_level", 0.15f, 0.5f }, { "crush_bits", { 7, 9, 11, 13 } },
          { "lfo1_rate", 4.0f, 12.0f }, { "mod1_amt", 0.1f, 0.4f } } });

    // ---- Keys ----
    a.push_back ({ "Memphis Piano", "Keys", "keys, piano, memphis, detuned, dark, lo-fi",
        { { "an_on", 1 }, { "an_shape", 0.9f }, { "an_level", 0.55f }, { "an_fine", -8.0f },
          { "res_on", 1 }, { "res_type", 18 }, { "res_decay", 2.0f }, { "res_bright", 0.45f }, { "res_level", 0.7f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f },
          { "amp_a", 0.002f }, { "amp_d", 1.5f }, { "amp_s", 0.3f }, { "amp_r", 0.6f }, { "tape_age", 0.3f }, { "crush_bits", 12 }, { "rev_mix", 0.25f } },
        { { "an_fine", -18.0f, -2.0f }, { "flt_cutoff", 0.4f, 0.7f }, { "tape_age", 0.1f, 0.5f }, { "res_bright", 0.3f, 0.6f }, { "rev_mix", 0.1f, 0.4f } } });
    a.push_back (res ("Funk Rhodes", "Keys", "keys, rhodes, e-piano, funk, warm", 18, 2.2f, 0.5f, 1.8f, 0.6f, 0.25f,
        { { "res_oct", { 0, 1 } } }));
    a.push_back ({ "Clav", "Keys", "keys, clav, funk, plucky",
        { { "an_on", 1 }, { "an_shape", 1.9f }, { "an_pw", 0.3f }, { "an_level", 0.75f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.7f }, { "flt_res", 0.3f }, { "flt_env", 0.4f },
          { "amp_a", 0.001f }, { "amp_d", 0.25f }, { "amp_s", 0.4f }, { "amp_r", 0.15f }, { "e2_a", 0.001f }, { "e2_d", 0.2f } },
        { { "an_pw", 0.15f, 0.5f }, { "flt_cutoff", 0.5f, 0.85f }, { "flt_env", 0.2f, 0.7f }, { "flt_res", 0.15f, 0.45f } } });

    // ---- Bells & Plucks (resonator) ----
    a.push_back ({ "Memphis Music-Box", "Bells & Plucks", "bells, music box, horror, memphis, dark",
        { { "res_on", 1 }, { "res_type", 8 }, { "res_decay", 3.0f }, { "res_bright", 0.5f }, { "res_damp", 0.5f },
          { "res_strike", 0.6f }, { "res_level", 0.82f },
          { "amp_a", 0.001f }, { "amp_d", 2.0f }, { "amp_s", 0.0f }, { "amp_r", 1.0f },
          { "rev_mix", 0.42f }, { "rev_size", 0.72f }, { "tape_age", 0.3f }, { "tape_hiss", 0.15f }, { "crush_bits", 12 } },
        { { "res_type", { 8, 7, 3, 18 } }, { "res_decay", 1.5f, 5.0f }, { "res_bright", 0.3f, 0.8f },
          { "rev_mix", 0.25f, 0.55f }, { "res_oct", { -1, 0 } }, { "tape_age", 0.1f, 0.5f } } });
    a.push_back (res ("Trap Bells", "Bells & Plucks", "bells, trap, vibraphone, bright", 16, 1.4f, 0.75f, 1.2f, 0.8f, 0.3f, { { "dly_mix", 0.1f, 0.3f } }));
    a.push_back (res ("Crystal Bell", "Bells & Plucks", "bells, crystal, ritual, bright", 3, 4.0f, 0.7f, 3.0f, 1.2f, 0.45f, { { "res_oct", { 0, 1 } } }));
    a.push_back (res ("Tingsha Chime", "Bells & Plucks", "bells, tingsha, chime, ritual", 7, 3.5f, 0.8f, 2.5f, 1.0f, 0.4f));
    a.push_back (res ("Glass Pluck", "Bells & Plucks", "pluck, glass, short, bright", 0, 0.8f, 0.7f, 0.6f, 0.3f, 0.2f, { { "res_type", { 0, 16 } } }));
    a.push_back ({ "Braindance Bell", "Bells & Plucks", "bells, idm, braindance, fm, detuned, glitch, aphex",
        { { "fm_on", 1 }, { "fm_index", 1.6f }, { "fm_level", 0.72f }, { "fm_oct", 0 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.7f },
          { "amp_a", 0.001f }, { "amp_d", 0.9f }, { "amp_s", 0.15f }, { "amp_r", 0.5f },
          { "crush_bits", 11 }, { "dly_mix", 0.22f }, { "rev_mix", 0.3f } },
        { { "fm_index", 0.8f, 2.6f }, { "crush_bits", { 7, 9, 11, 13 } }, { "dly_mix", 0.1f, 0.4f }, { "rev_mix", 0.15f, 0.45f },
          { "fm_oct", { 0, 1 } }, { "amp_d", 0.5f, 1.4f } } });

    // ---- Pads ----
    a.push_back ({ "Dark Horror Pad", "Pads", "pad, dark, horror, evolving, memphis",
        { { "wt_on", 1 }, { "wt_table", 5 }, { "wt_pos", 0.3f }, { "wt_unison", 4 }, { "wt_detune", 0.3f }, { "wt_spread", 0.7f }, { "wt_level", 0.7f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.45f }, { "flt_res", 0.2f }, { "flt_env", 0.3f },
          { "amp_a", 0.8f }, { "amp_d", 1.5f }, { "amp_s", 0.8f }, { "amp_r", 2.0f }, { "e2_a", 1.5f }, { "e2_d", 2.0f },
          { "lfo1_shape", 5 }, { "lfo1_rate", 0.3f }, { "mod1_src", 3 }, { "mod1_dst", 16 }, { "mod1_amt", 0.2f },
          { "rev_mix", 0.45f }, { "rev_size", 0.8f } },
        { { "flt_cutoff", 0.3f, 0.6f }, { "wt_detune", 0.15f, 0.5f }, { "amp_a", 0.4f, 1.5f }, { "mod1_amt", 0.1f, 0.35f },
          { "rev_mix", 0.3f, 0.6f }, { "wt_pos", 0.1f, 0.7f } } });
    a.push_back ({ "Warm Funk Pad", "Pads", "pad, warm, funk, lush",
        { { "an_on", 1 }, { "an_shape", 1.0f }, { "an_level", 0.65f }, { "an_drift", 0.3f },
          { "wt_on", 1 }, { "wt_table", 1 }, { "wt_level", 0.4f }, { "wt_detune", 0.2f }, { "wt_unison", 3 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f }, { "flt_res", 0.15f },
          { "amp_a", 0.5f }, { "amp_d", 1.0f }, { "amp_s", 0.85f }, { "amp_r", 1.5f }, { "cho_mix", 0.3f }, { "rev_mix", 0.35f } },
        { { "flt_cutoff", 0.45f, 0.8f }, { "amp_a", 0.2f, 1.0f }, { "cho_mix", 0.1f, 0.45f }, { "rev_mix", 0.2f, 0.5f } } });
    a.push_back ({ "Ambient Drone", "Pads", "pad, drone, ambient, dark, cinematic",
        { { "gr_on", 1 }, { "gr_source", 3 }, { "gr_level", 0.5f }, { "gr_stereo", 0.8f }, { "gr_size", 300.0f }, { "gr_density", 15.0f },
          { "wt_on", 1 }, { "wt_table", 5 }, { "wt_oct", -1 }, { "wt_level", 0.5f }, { "wt_detune", 0.25f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.4f },
          { "amp_a", 1.5f }, { "amp_d", 2.0f }, { "amp_s", 0.85f }, { "amp_r", 3.0f }, { "rev_mix", 0.55f }, { "rev_size", 0.85f } },
        { { "flt_cutoff", 0.25f, 0.55f }, { "gr_level", 0.3f, 0.7f }, { "amp_a", 0.8f, 2.5f }, { "rev_mix", 0.4f, 0.65f }, { "wt_oct", { -2, -1 } } } });

    // ---- Choirs & Vox (granular) ----
    a.push_back ({ "Bone Choir Stack", "Choirs & Vox", "choir, chant, vocal, pad, bone",
        { { "gr_on", 1 }, { "gr_source", 0 }, { "gr_level", 0.8f }, { "gr_stereo", 0.8f }, { "gr_pos", 0.3f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.7f },
          { "amp_a", 0.5f }, { "amp_d", 1.0f }, { "amp_s", 0.8f }, { "amp_r", 1.5f },
          { "rev_mix", 0.5f }, { "rev_size", 0.75f }, { "cho_mix", 0.3f }, { "master", 0.72f } },
        { { "gr_source", { 0, 1 } }, { "amp_a", 0.2f, 1.2f }, { "rev_mix", 0.35f, 0.6f }, { "cho_mix", 0.1f, 0.45f },
          { "flt_cutoff", 0.5f, 0.85f }, { "gr_pos", 0.1f, 0.6f } } });
    a.push_back ({ "Monk Chant", "Choirs & Vox", "chant, monk, ritual, dark, vocal",
        { { "gr_on", 1 }, { "gr_source", 1 }, { "gr_level", 0.8f }, { "gr_stereo", 0.6f }, { "gr_oct", -1 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f },
          { "amp_a", 0.4f }, { "amp_d", 1.0f }, { "amp_s", 0.85f }, { "amp_r", 1.8f }, { "rev_mix", 0.5f }, { "rev_size", 0.85f } },
        { { "flt_cutoff", 0.4f, 0.7f }, { "amp_a", 0.2f, 0.9f }, { "rev_mix", 0.35f, 0.62f }, { "gr_oct", { -1, 0 } } } });
    a.push_back ({ "Rain Texture", "Choirs & Vox", "texture, rain, water, ambient, world",
        { { "gr_on", 1 }, { "gr_source", 3 }, { "gr_level", 0.7f }, { "gr_stereo", 0.9f }, { "gr_spray", 0.4f }, { "gr_density", 40.0f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f },
          { "amp_a", 0.6f }, { "amp_d", 1.0f }, { "amp_s", 0.8f }, { "amp_r", 2.0f }, { "rev_mix", 0.45f } },
        { { "gr_spray", 0.2f, 0.7f }, { "gr_density", 20.0f, 70.0f }, { "flt_cutoff", 0.45f, 0.8f }, { "rev_mix", 0.3f, 0.6f } } });
    a.push_back ({ "Sacred Chant Pad", "Choirs & Vox", "choir, chant, sacred, ritual, temple, dark, pad",
        { { "gr_on", 1 }, { "gr_source", 1 }, { "gr_level", 0.75f }, { "gr_stereo", 0.8f }, { "gr_oct", -1 },
          { "wt_on", 1 }, { "wt_table", 5 }, { "wt_level", 0.3f }, { "wt_oct", -1 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.5f },
          { "amp_a", 0.8f }, { "amp_d", 1.5f }, { "amp_s", 0.85f }, { "amp_r", 2.5f }, { "rev_mix", 0.55f }, { "rev_size", 0.88f } },
        { { "flt_cutoff", 0.35f, 0.65f }, { "amp_a", 0.4f, 1.5f }, { "rev_mix", 0.4f, 0.65f }, { "gr_oct", { -1, 0 } } } });

    // ---- Brass & Organs ----
    a.push_back ({ "Crunk Brass Stab", "Brass & Organs", "brass, stab, crunk, aggressive",
        { { "an_on", 1 }, { "an_shape", 1.4f }, { "an_level", 0.78f },
          { "wt_on", 1 }, { "wt_table", 1 }, { "wt_level", 0.5f }, { "wt_unison", 2 }, { "wt_detune", 0.15f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f }, { "flt_res", 0.2f }, { "flt_env", 0.5f },
          { "amp_a", 0.01f }, { "amp_d", 0.3f }, { "amp_s", 0.7f }, { "amp_r", 0.3f }, { "e2_a", 0.01f }, { "e2_d", 0.25f }, { "fx_drive", 0.2f } },
        { { "flt_cutoff", 0.45f, 0.75f }, { "flt_env", 0.3f, 0.7f }, { "amp_a", 0.005f, 0.06f }, { "fx_drive", 0.1f, 0.4f } } });
    a.push_back ({ "Gospel Organ", "Brass & Organs", "organ, gospel, funk, warm",
        { { "an_on", 1 }, { "an_shape", 0.0f }, { "an_level", 0.5f },
          { "fm_on", 1 }, { "fm_index", 0.8f }, { "fm_level", 0.55f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.75f },
          { "amp_a", 0.01f }, { "amp_d", 0.3f }, { "amp_s", 0.95f }, { "amp_r", 0.2f },
          { "lfo2_shape", 0 }, { "lfo2_rate", 6.0f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.02f }, { "rev_mix", 0.2f } },
        { { "fm_index", 0.4f, 1.3f }, { "flt_cutoff", 0.6f, 0.9f }, { "mod1_amt", 0.0f, 0.04f }, { "cho_mix", 0.0f, 0.3f } } });
    a.push_back ({ "Horror Organ", "Brass & Organs", "organ, horror, church, dark, eerie",
        { { "wt_on", 1 }, { "wt_table", 5 }, { "wt_pos", 0.5f }, { "wt_level", 0.7f }, { "wt_unison", 2 }, { "wt_detune", 0.2f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f },
          { "amp_a", 0.02f }, { "amp_d", 0.4f }, { "amp_s", 0.9f }, { "amp_r", 0.6f }, { "rev_mix", 0.5f }, { "rev_size", 0.85f } },
        { { "flt_cutoff", 0.4f, 0.7f }, { "wt_detune", 0.1f, 0.4f }, { "rev_mix", 0.35f, 0.6f }, { "wt_oct", { -1, 0 } } } });

    // ---- World Winds / Strings ----
    a.push_back ({ "Shakuhachi Breath", "World Winds", "flute, world, breath, shakuhachi, ethnic",
        { { "gr_on", 1 }, { "gr_source", 2 }, { "gr_level", 0.5f }, { "gr_stereo", 0.6f }, { "gr_size", 150.0f }, { "gr_density", 30.0f },
          { "an_on", 1 }, { "an_shape", 0.1f }, { "an_level", 0.4f }, { "an_noise", 0.1f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f },
          { "amp_a", 0.08f }, { "amp_d", 0.4f }, { "amp_s", 0.85f }, { "amp_r", 0.4f },
          { "lfo2_shape", 0 }, { "lfo2_rate", 5.0f }, { "lfo2_fade", 0.3f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.02f }, { "rev_mix", 0.3f } },
        { { "gr_level", 0.3f, 0.7f }, { "an_level", 0.25f, 0.6f }, { "flt_cutoff", 0.45f, 0.75f }, { "amp_a", 0.03f, 0.2f },
          { "mod1_amt", 0.01f, 0.04f }, { "gr_density", 15.0f, 50.0f } } });
    a.push_back ({ "Pan Flute", "World Winds", "flute, pan, world, soft, airy",
        { { "wt_on", 1 }, { "wt_table", 2 }, { "wt_pos", 0.5f }, { "wt_level", 0.6f },
          { "an_on", 1 }, { "an_shape", 0.05f }, { "an_level", 0.3f }, { "an_noise", 0.15f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.65f },
          { "amp_a", 0.06f }, { "amp_d", 0.3f }, { "amp_s", 0.8f }, { "amp_r", 0.35f },
          { "lfo2_shape", 0 }, { "lfo2_rate", 5.5f }, { "lfo2_fade", 0.35f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.025f }, { "rev_mix", 0.3f } },
        { { "wt_pos", 0.3f, 0.7f }, { "an_noise", 0.05f, 0.25f }, { "flt_cutoff", 0.5f, 0.8f }, { "amp_a", 0.03f, 0.12f } } });
    a.push_back (res ("Sitar", "World Strings", "sitar, world, plucked, drone", 1, 2.5f, 0.6f, 2.0f, 0.9f, 0.3f, { { "res_buzz", 0.2f, 0.7f } }));
    a.push_back (res ("Koto", "World Strings", "koto, world, plucked, string", 0, 1.8f, 0.6f, 1.5f, 0.8f, 0.25f));
    a.push_back (res ("Plucked String", "World Strings", "string, plucked, world, folk", 0, 1.2f, 0.55f, 1.0f, 0.6f, 0.2f));

    // ---- World Mallets & Percussion (resonator) ----
    a.push_back (res ("Kalimba", "World Mallets", "kalimba, mbira, world, plucked, bright", 0, 1.0f, 0.72f, 0.8f, 0.5f, 0.25f, { { "res_type", { 0, 6 } } }));
    a.push_back (res ("Marimba", "World Mallets", "marimba, mallet, world, warm", 17, 1.0f, 0.55f, 0.8f, 0.45f, 0.2f));
    a.push_back (res ("Vibraphone", "World Mallets", "vibraphone, mallet, jazz, bright", 16, 1.8f, 0.65f, 1.5f, 0.8f, 0.3f));
    a.push_back (res ("Handpan", "World Mallets", "handpan, hang, world, meditative", 5, 2.0f, 0.5f, 1.6f, 0.9f, 0.35f));
    a.push_back (res ("Tongue Drum", "World Mallets", "tongue drum, world, mellow", 6, 1.6f, 0.45f, 1.3f, 0.7f, 0.3f));
    a.push_back (res ("Tibetan Bowl", "World Mallets", "bowl, tibetan, singing, ritual, drone", 2, 5.0f, 0.4f, 4.0f, 1.5f, 0.4f, { { "res_bow", 0.3f, 0.8f } }));
    a.push_back (res ("Gong", "World Mallets", "gong, metal, ritual, dark, impact", 4, 4.5f, 0.45f, 3.5f, 1.5f, 0.45f));
    a.push_back (res ("Tabla", "World Mallets", "tabla, hand drum, world, percussion", 9, 0.6f, 0.55f, 0.5f, 0.25f, 0.1f));
    a.push_back (res ("Hand Drum", "World Mallets", "hand drum, world, percussion, warm", 10, 0.7f, 0.45f, 0.6f, 0.3f, 0.12f));
    a.push_back (res ("Clay Pot", "World Mallets", "clay pot, udu, world, percussion", 11, 0.8f, 0.4f, 0.6f, 0.3f, 0.12f));
    a.push_back (res ("Frame Drum", "World Mallets", "frame drum, fusion, world, percussion, trap", 10, 0.5f, 0.5f, 0.4f, 0.2f, 0.1f, { { "res_type", { 9, 10, 11 } }, { "fx_drive", 0.1f, 0.4f } }));
    a.push_back (res ("Fusion Conga", "World Mallets", "conga, fusion, world, percussion, hand, trap", 11, 0.5f, 0.5f, 0.4f, 0.2f, 0.08f));

    // ---- Textures & FX ----
    a.push_back (res ("Scrap Metal Hit", "Textures & FX", "metal, scrap, horror, industrial, hit", 13, 1.5f, 0.6f, 1.2f, 0.8f, 0.35f, { { "res_type", { 13, 14, 15 } } }));
    a.push_back (res ("Machine Perc", "Textures & FX", "perc, industrial, metal, nin, mechanical, hit", 14, 0.7f, 0.6f, 0.5f, 0.3f, 0.15f,
        { { "res_type", { 13, 14, 15 } }, { "crush_bits", { 6, 8, 10 } }, { "fx_drive", 0.2f, 0.6f } }));
    a.push_back ({ "Glitch Stab", "Textures & FX", "fx, glitch, idm, bitcrush, stab, aphex, squarepusher",
        { { "fm_on", 1 }, { "fm_index", 2.4f }, { "fm_level", 0.75f },
          { "an_on", 1 }, { "an_shape", 2.0f }, { "an_level", 0.3f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.65f }, { "flt_res", 0.3f },
          { "amp_a", 0.001f }, { "amp_d", 0.18f }, { "amp_s", 0.2f }, { "amp_r", 0.12f }, { "e2_a", 0.001f }, { "e2_d", 0.15f },
          { "crush_bits", 8 }, { "fx_drive", 0.4f }, { "dly_mix", 0.2f } },
        { { "fm_index", 1.2f, 3.2f }, { "crush_bits", { 5, 6, 8, 10 } }, { "flt_cutoff", 0.45f, 0.85f }, { "dly_mix", 0.05f, 0.35f },
          { "fx_drive", 0.2f, 0.6f } } });
    a.push_back ({ "Horror Riser", "Textures & FX", "riser, horror, fx, tension, uplifter",
        { { "wt_on", 1 }, { "wt_table", 3 }, { "wt_pos", 0.4f }, { "wt_unison", 3 }, { "wt_detune", 0.4f }, { "wt_level", 0.7f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.3f },
          { "amp_a", 0.3f }, { "amp_d", 1.0f }, { "amp_s", 0.9f }, { "amp_r", 0.5f },
          { "lfo1_shape", 2 }, { "lfo1_rate", 0.25f }, { "mod1_src", 3 }, { "mod1_dst", 16 }, { "mod1_amt", 0.6f },
          { "mod2_src", 3 }, { "mod2_dst", 21 }, { "mod2_amt", 0.2f }, { "rev_mix", 0.5f }, { "noise_level", 0.2f } },
        { { "mod1_amt", 0.4f, 0.8f }, { "lfo1_rate", 0.12f, 0.4f }, { "wt_detune", 0.2f, 0.6f }, { "noise_level", 0.1f, 0.4f } } });
    a.push_back ({ "Impact Boom", "Textures & FX", "impact, boom, hit, cinematic, trailer",
        { { "an_on", 1 }, { "an_shape", 0.0f }, { "an_oct", -2 }, { "an_level", 0.85f },
          { "res_on", 1 }, { "res_type", 4 }, { "res_decay", 2.0f }, { "res_bright", 0.4f }, { "res_level", 0.6f }, { "res_oct", -1 },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.4f },
          { "amp_a", 0.001f }, { "amp_d", 1.5f }, { "amp_s", 0.0f }, { "amp_r", 1.5f }, { "fx_drive", 0.3f }, { "rev_mix", 0.4f }, { "rev_size", 0.85f } },
        { { "res_decay", 1.0f, 3.5f }, { "flt_cutoff", 0.25f, 0.55f }, { "fx_drive", 0.1f, 0.5f }, { "rev_mix", 0.25f, 0.55f } } });

    // ---- Arps & Sequences ----
    a.push_back ({ "Trap Bell Arp", "Arps & Sequences", "arp, bell, trap, sequence, bright",
        { { "res_on", 1 }, { "res_type", 16 }, { "res_decay", 0.8f }, { "res_bright", 0.75f }, { "res_level", 0.8f },
          { "amp_a", 0.001f }, { "amp_d", 0.5f }, { "amp_s", 0.1f }, { "amp_r", 0.3f },
          { "arp_on", 1 }, { "arp_rate", 3 }, { "arp_mode", 0 }, { "arp_oct", 2 }, { "arp_gate", 0.6f }, { "rev_mix", 0.3f }, { "dly_mix", 0.2f } },
        { { "res_type", { 16, 17, 8, 7 } }, { "arp_rate", { 2, 3, 4 } }, { "arp_mode", { 0, 1, 2, 6 } }, { "arp_oct", { 1, 2, 3 } }, { "dly_mix", 0.05f, 0.35f } } });
    a.push_back ({ "Memphis Dark Sequence", "Arps & Sequences", "arp, sequence, memphis, dark, horror",
        { { "an_on", 1 }, { "an_shape", 1.6f }, { "an_level", 0.72f }, { "an_oct", 0 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.5f }, { "flt_res", 0.3f }, { "flt_env", 0.4f },
          { "amp_a", 0.001f }, { "amp_d", 0.3f }, { "amp_s", 0.4f }, { "amp_r", 0.2f }, { "e2_a", 0.001f }, { "e2_d", 0.2f },
          { "arp_on", 1 }, { "arp_rate", 4 }, { "arp_mode", 1 }, { "arp_oct", 1 }, { "arp_gate", 0.5f }, { "tape_age", 0.25f }, { "rev_mix", 0.3f } },
        { { "flt_cutoff", 0.35f, 0.65f }, { "arp_rate", { 3, 4, 5 } }, { "arp_mode", { 0, 1, 4, 6 } }, { "flt_env", 0.2f, 0.6f }, { "arp_gate", 0.35f, 0.8f } } });
    a.push_back ({ "World Kalimba Arp", "Arps & Sequences", "arp, kalimba, world, sequence",
        { { "res_on", 1 }, { "res_type", 0 }, { "res_decay", 0.9f }, { "res_bright", 0.7f }, { "res_level", 0.8f },
          { "amp_a", 0.001f }, { "amp_d", 0.6f }, { "amp_s", 0.1f }, { "amp_r", 0.3f },
          { "arp_on", 1 }, { "arp_rate", 3 }, { "arp_mode", 6 }, { "arp_oct", 2 }, { "arp_gate", 0.55f }, { "rev_mix", 0.3f } },
        { { "res_type", { 0, 6, 17 } }, { "arp_rate", { 2, 3, 4 } }, { "arp_mode", { 0, 2, 6 } }, { "arp_oct", { 1, 2 } }, { "rev_mix", 0.15f, 0.45f } } });
    a.push_back ({ "Drill Sequence", "Arps & Sequences", "arp, drill, idm, squarepusher, fast, glitch, dark",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_level", 0.72f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.55f }, { "flt_res", 0.4f }, { "flt_env", 0.45f },
          { "amp_a", 0.001f }, { "amp_d", 0.18f }, { "amp_s", 0.25f }, { "amp_r", 0.12f }, { "e2_a", 0.001f }, { "e2_d", 0.15f },
          { "arp_on", 1 }, { "arp_rate", 5 }, { "arp_mode", 6 }, { "arp_oct", 2 }, { "arp_gate", 0.4f },
          { "crush_bits", 10 }, { "fx_drive", 0.35f }, { "dly_mix", 0.18f } },
        { { "flt_cutoff", 0.4f, 0.7f }, { "flt_res", 0.25f, 0.6f }, { "arp_rate", { 4, 5, 6 } }, { "arp_mode", { 1, 4, 6 } },
          { "arp_oct", { 1, 2, 3 } }, { "crush_bits", { 7, 9, 11 } }, { "arp_gate", 0.3f, 0.6f } } });
    a.push_back ({ "Modular Bleep", "Arps & Sequences", "arp, idm, braindance, bleep, bright, glitch, aphex",
        { { "fm_on", 1 }, { "fm_index", 1.2f }, { "fm_level", 0.7f },
          { "an_on", 1 }, { "an_shape", 2.0f }, { "an_level", 0.3f }, { "an_oct", 1 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.75f }, { "flt_res", 0.25f },
          { "amp_a", 0.001f }, { "amp_d", 0.2f }, { "amp_s", 0.1f }, { "amp_r", 0.15f },
          { "arp_on", 1 }, { "arp_rate", 4 }, { "arp_mode", 6 }, { "arp_oct", 2 }, { "arp_gate", 0.5f },
          { "crush_bits", 12 }, { "dly_mix", 0.25f }, { "rev_mix", 0.2f } },
        { { "fm_index", 0.6f, 2.0f }, { "flt_cutoff", 0.6f, 0.9f }, { "arp_rate", { 3, 4, 5 } }, { "arp_mode", { 0, 2, 6 } },
          { "arp_oct", { 1, 2, 3 } }, { "dly_mix", 0.1f, 0.4f }, { "crush_bits", { 10, 12, 14 } } } });

    // ---- Reggae & Dub (dark cinematic) ----
    a.push_back ({ "Dub Sub Bass", "Reggae & Dub", "bass, reggae, dub, sub, round, one-drop, dark",
        { { "an_on", 1 }, { "an_shape", 0.0f }, { "an_oct", -2 }, { "an_sub", 0.35f }, { "an_tone", 0.8f }, { "an_level", 0.9f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.3f }, { "flt_res", 0.1f },
          { "amp_a", 0.01f }, { "amp_d", 0.5f }, { "amp_s", 0.8f }, { "amp_r", 0.3f }, { "voice_mode", 1 }, { "glide", 0.07f }, { "master", 0.72f } },
        { { "flt_cutoff", 0.22f, 0.4f }, { "an_sub", 0.2f, 0.5f }, { "glide", 0.03f, 0.14f }, { "amp_d", 0.3f, 0.8f } } });
    a.push_back ({ "Skank Organ", "Reggae & Dub", "organ, reggae, skank, stab, offbeat, bright",
        { { "an_on", 1 }, { "an_shape", 1.6f }, { "an_level", 0.6f },
          { "wt_on", 1 }, { "wt_table", 1 }, { "wt_level", 0.4f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.7f }, { "flt_res", 0.2f },
          { "amp_a", 0.002f }, { "amp_d", 0.18f }, { "amp_s", 0.0f }, { "amp_r", 0.12f }, { "dly_mix", 0.18f }, { "rev_mix", 0.2f } },
        { { "flt_cutoff", 0.55f, 0.85f }, { "amp_d", 0.1f, 0.3f }, { "dly_mix", 0.05f, 0.35f }, { "rev_mix", 0.1f, 0.35f } } });
    a.push_back ({ "Dub Chord Stab", "Reggae & Dub", "chord, dub, stab, delay, spring, cinematic, dark",
        { { "wt_on", 1 }, { "wt_table", 1 }, { "wt_unison", 2 }, { "wt_detune", 0.15f }, { "wt_level", 0.7f },
          { "an_on", 1 }, { "an_shape", 1.0f }, { "an_level", 0.4f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f }, { "flt_res", 0.2f },
          { "amp_a", 0.003f }, { "amp_d", 0.3f }, { "amp_s", 0.0f }, { "amp_r", 0.2f }, { "dly_mix", 0.35f }, { "rev_mix", 0.4f }, { "rev_size", 0.8f } },
        { { "flt_cutoff", 0.4f, 0.7f }, { "dly_mix", 0.2f, 0.5f }, { "rev_mix", 0.25f, 0.55f }, { "amp_d", 0.2f, 0.5f } } });
    a.push_back ({ "Melodica Lead", "Reggae & Dub", "lead, melodica, reggae, reedy, breath",
        { { "an_on", 1 }, { "an_shape", 1.3f }, { "an_pw", 0.4f }, { "an_level", 0.7f }, { "an_noise", 0.08f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.7f },
          { "amp_a", 0.03f }, { "amp_d", 0.3f }, { "amp_s", 0.85f }, { "amp_r", 0.3f }, { "voice_mode", 1 },
          { "lfo2_shape", 0 }, { "lfo2_rate", 5.5f }, { "lfo2_fade", 0.4f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.02f }, { "dly_mix", 0.2f }, { "rev_mix", 0.25f } },
        { { "flt_cutoff", 0.55f, 0.85f }, { "an_pw", 0.3f, 0.5f }, { "an_noise", 0.03f, 0.15f }, { "dly_mix", 0.1f, 0.35f } } });

    // ---- Jazz & Noir (acid jazz / jazz noir) ----
    a.push_back (res ("Jazz Rhodes", "Jazz & Noir", "keys, rhodes, e-piano, jazz, acid jazz, warm", 18, 2.4f, 0.55f, 2.0f, 0.7f, 0.25f,
        { { "res_oct", { 0, 1 } }, { "cho_mix", 0.1f, 0.4f } }));
    a.push_back (res ("Noir Vibes", "Jazz & Noir", "vibraphone, jazz, noir, mallet, dark, smoky", 16, 2.2f, 0.55f, 1.8f, 0.9f, 0.4f,
        { { "rev_mix", 0.3f, 0.6f } }));
    a.push_back ({ "Noir Trumpet", "Jazz & Noir", "lead, trumpet, jazz, noir, muted, breath, smoky",
        { { "fm_on", 1 }, { "fm_index", 1.1f }, { "fm_level", 0.7f },
          { "an_on", 1 }, { "an_shape", 1.0f }, { "an_level", 0.2f }, { "an_noise", 0.1f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f },
          { "amp_a", 0.04f }, { "amp_d", 0.3f }, { "amp_s", 0.85f }, { "amp_r", 0.3f }, { "voice_mode", 1 },
          { "lfo2_shape", 0 }, { "lfo2_rate", 5.0f }, { "lfo2_fade", 0.5f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.02f }, { "rev_mix", 0.35f } },
        { { "fm_index", 0.6f, 1.8f }, { "flt_cutoff", 0.45f, 0.75f }, { "an_noise", 0.04f, 0.18f }, { "amp_a", 0.02f, 0.08f }, { "rev_mix", 0.2f, 0.5f } } });
    a.push_back ({ "Upright Bass", "Jazz & Noir", "bass, upright, jazz, noir, acoustic, round",
        { { "an_on", 1 }, { "an_shape", 0.3f }, { "an_oct", -1 }, { "an_level", 0.8f },
          { "res_on", 1 }, { "res_type", 0 }, { "res_decay", 0.6f }, { "res_bright", 0.4f }, { "res_level", 0.4f }, { "res_oct", -1 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.4f },
          { "amp_a", 0.004f }, { "amp_d", 0.5f }, { "amp_s", 0.4f }, { "amp_r", 0.3f }, { "voice_mode", 1 }, { "glide", 0.04f } },
        { { "flt_cutoff", 0.3f, 0.55f }, { "res_decay", 0.4f, 0.9f }, { "amp_d", 0.3f, 0.7f }, { "glide", 0.02f, 0.1f } } });

    // ---- Psych & Fuzz (60s psych / krautrock / zamrock) ----
    a.push_back ({ "Psych Organ", "Psych & Fuzz", "organ, psych, 60s, farfisa, vox, bright, vibrato",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_level", 0.55f },
          { "wt_on", 1 }, { "wt_table", 1 }, { "wt_level", 0.45f }, { "wt_oct", 1 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.78f },
          { "amp_a", 0.01f }, { "amp_d", 0.3f }, { "amp_s", 0.95f }, { "amp_r", 0.2f },
          { "lfo2_shape", 0 }, { "lfo2_rate", 6.5f }, { "mod1_src", 4 }, { "mod1_dst", 21 }, { "mod1_amt", 0.03f }, { "rev_mix", 0.25f } },
        { { "flt_cutoff", 0.6f, 0.9f }, { "lfo2_rate", 4.5f, 8.0f }, { "mod1_amt", 0.01f, 0.05f }, { "cho_mix", 0.0f, 0.3f } } });
    a.push_back ({ "Fuzz Lead", "Psych & Fuzz", "lead, fuzz, psych, zamrock, distorted, aggressive",
        { { "an_on", 1 }, { "an_shape", 1.0f }, { "an_level", 0.8f }, { "an_oct", 0 },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.65f }, { "flt_res", 0.25f }, { "flt_drive", 0.5f },
          { "amp_a", 0.01f }, { "amp_d", 0.3f }, { "amp_s", 0.9f }, { "amp_r", 0.3f }, { "voice_mode", 1 },
          { "fx_drive", 0.6f }, { "drive_type", 1 }, { "rev_mix", 0.2f } },
        { { "flt_cutoff", 0.5f, 0.85f }, { "fx_drive", 0.4f, 0.8f }, { "drive_type", { 0, 1, 2 } }, { "flt_res", 0.15f, 0.4f }, { "an_oct", { 0, 1 } } } });
    a.push_back ({ "Mellotron Choir", "Psych & Fuzz", "pad, mellotron, psych, 60s, tape, choir, lo-fi",
        { { "gr_on", 1 }, { "gr_source", 0 }, { "gr_level", 0.65f }, { "gr_stereo", 0.6f },
          { "wt_on", 1 }, { "wt_table", 2 }, { "wt_level", 0.3f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.55f },
          { "amp_a", 0.3f }, { "amp_d", 0.8f }, { "amp_s", 0.8f }, { "amp_r", 1.2f }, { "tape_age", 0.4f }, { "tape_hiss", 0.2f }, { "rev_mix", 0.35f } },
        { { "flt_cutoff", 0.4f, 0.7f }, { "tape_age", 0.2f, 0.55f }, { "amp_a", 0.15f, 0.6f }, { "rev_mix", 0.2f, 0.5f } } });
    a.push_back ({ "Krautrock Pulse", "Psych & Fuzz", "sequence, krautrock, motorik, analog, hypnotic, evolving",
        { { "an_on", 1 }, { "an_shape", 1.7f }, { "an_level", 0.72f },
          { "flt_on", 1 }, { "flt_type", 0 }, { "flt_cutoff", 0.5f }, { "flt_res", 0.3f },
          { "amp_a", 0.002f }, { "amp_d", 0.2f }, { "amp_s", 0.3f }, { "amp_r", 0.12f },
          { "arp_on", 1 }, { "arp_rate", 4 }, { "arp_mode", 0 }, { "arp_oct", 1 }, { "arp_gate", 0.5f },
          { "lfo1_shape", 0 }, { "lfo1_rate", 0.2f }, { "mod1_src", 3 }, { "mod1_dst", 16 }, { "mod1_amt", 0.3f }, { "dly_mix", 0.2f } },
        { { "flt_cutoff", 0.35f, 0.65f }, { "mod1_amt", 0.15f, 0.45f }, { "lfo1_rate", 0.1f, 0.4f }, { "arp_rate", { 3, 4, 5 } }, { "dly_mix", 0.1f, 0.35f } } });

    // ---- Chip & Lo-Fi (8-bit) ----
    a.push_back ({ "Chip Bass", "Chip & Lo-Fi", "bass, chip, 8-bit, lo-fi, square, nes",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_pw", 0.5f }, { "an_oct", -1 }, { "an_level", 0.8f },
          { "amp_a", 0.001f }, { "amp_d", 0.3f }, { "amp_s", 0.7f }, { "amp_r", 0.1f }, { "voice_mode", 1 }, { "crush_bits", 8 } },
        { { "an_pw", 0.25f, 0.5f }, { "crush_bits", { 6, 8, 10 } }, { "amp_d", 0.15f, 0.45f }, { "an_oct", { -2, -1 } } } });
    a.push_back ({ "8-Bit Arp", "Chip & Lo-Fi", "arp, chip, 8-bit, lo-fi, square, bright, sequence",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_pw", 0.5f }, { "an_oct", 1 }, { "an_level", 0.75f },
          { "amp_a", 0.001f }, { "amp_d", 0.15f }, { "amp_s", 0.2f }, { "amp_r", 0.08f },
          { "arp_on", 1 }, { "arp_rate", 4 }, { "arp_mode", 0 }, { "arp_oct", 2 }, { "arp_gate", 0.5f }, { "crush_bits", 8 }, { "dly_mix", 0.18f } },
        { { "an_pw", 0.25f, 0.5f }, { "arp_rate", { 3, 4, 5 } }, { "arp_mode", { 0, 1, 2, 6 } }, { "arp_oct", { 1, 2, 3 } }, { "crush_bits", { 6, 8, 10 } }, { "dly_mix", 0.05f, 0.3f } } });
    a.push_back ({ "Lo-Fi Pulse Lead", "Chip & Lo-Fi", "lead, lo-fi, chip, pulse, tape, mellow",
        { { "an_on", 1 }, { "an_shape", 2.0f }, { "an_pw", 0.35f }, { "an_level", 0.72f },
          { "flt_on", 1 }, { "flt_type", 1 }, { "flt_cutoff", 0.6f },
          { "amp_a", 0.01f }, { "amp_d", 0.3f }, { "amp_s", 0.8f }, { "amp_r", 0.2f }, { "voice_mode", 1 }, { "crush_bits", 10 }, { "tape_age", 0.3f }, { "dly_mix", 0.18f } },
        { { "an_pw", 0.2f, 0.45f }, { "flt_cutoff", 0.45f, 0.75f }, { "crush_bits", { 8, 10, 12 } }, { "tape_age", 0.15f, 0.45f } } });

    return a;
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    if (auto os = std::unique_ptr<OutputStream> (f.createOutputStream().release()))
        if (auto* w = wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0))
        {
            os.release();
            std::unique_ptr<AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
}

static String characterTags (const ff::design::SoundFeatures& f)
{
    StringArray t;
    t.add (f.brightness < 0.33f ? "dark" : f.brightness > 0.66f ? "bright" : "mid");
    if (f.aggression > 0.6f) t.add ("aggressive");
    else if (f.aggression < 0.25f) t.add ("soft");
    if (f.movement > 0.5f) t.add ("evolving");
    return t.joinIntoString (", ");
}

// Merge the archetype's hand tags with the measured character tags, dropping duplicates.
static String mergeTags (const String& base, const String& extra)
{
    StringArray out;
    for (auto& s : StringArray::fromTokens (base + ", " + extra, ",", ""))
    {
        const String w = s.trim().toLowerCase();
        if (w.isNotEmpty() && ! out.contains (w)) out.add (w);
    }
    return out.joinIntoString (", ");
}

// Evocative name roots (dark / ritual / regal / street), paired with the instrument so a browse
// reads "Crypt Pad", "Venom Lead", "Obsidian 808" - a name tells you the vibe, the note the detail.
static const char* const kMoods[] = {
    "Crypt", "Specter", "Midnight", "Venom", "Obsidian", "Hollow", "Ritual", "Nocturne", "Ash", "Ember",
    "Frost", "Void", "Shadow", "Grim", "Omen", "Relic", "Phantom", "Dusk", "Sable", "Onyx", "Wraith",
    "Grave", "Thorn", "Cinder", "Mourn", "Hex", "Rune", "Fang", "Mire", "Gloom", "Pale", "Hush", "Velvet",
    "Smoke", "Opal", "Cobalt", "Oracle", "Serpent", "Mystic", "Astral", "Lunar", "Cobra", "Haze", "Pharaoh",
    "Temple", "Incense", "Saffron", "Monsoon", "Nomad", "Bazaar", "Dune", "Oasis", "Ivory", "Sultan",
    "Chrome", "Gilded", "Reverie", "Seance", "Tundra", "Cathedral", "Molten", "Abyss", "Requiem", "Sorrow",
    "Rust", "Static", "Fracture", "Circuit", "Rivet", "Furnace", "Tremor", "Decay", "Scour", "Grind",
    "Lesion", "Glitch", "Dredge", "Pylon", "Reactor", "Vise", "Carrion", "Sulfur", "Tether", "Corrode" };
constexpr int kNumMoods = (int) (sizeof (kMoods) / sizeof (kMoods[0]));

static String instrumentNoun (const String& archName)
{
    const String w = archName.fromLastOccurrenceOf (" ", false, false);
    return w.isEmpty() ? archName : w;
}

static String describe (const String& archName, const ff::design::SoundFeatures& f)
{
    StringArray c;
    c.add (f.brightness < 0.33f ? "dark" : f.brightness > 0.66f ? "bright" : "warm");
    if (f.aggression > 0.6f) c.add ("gritty");
    else if (f.aggression < 0.25f) c.add ("smooth");
    if (f.movement > 0.5f) c.add ("evolving");
    return archName + " - " + c.joinIntoString (", ") + ".";
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::cerr << "usage: PresetForge \"<PackName>\" [<total-preset-target>]\n"; return 1; }
    const String packName = CharPointer_UTF8 (argv[1]);
    const int target = argc > 2 ? jmax (6, std::atoi (argv[2])) : 60;
    const double sr = 48000.0;

    auto archs = archetypes();

    // Weighting pass: lean hard into the Trance-6 palette - dark horrorcore Memphis + NIN
    // industrial + Aphex/Squarepusher IDM - while keeping the meditative/world side alive and
    // trimming the clean funk/gospel/anthemic material back to accents.
    auto has = [] (const String& t, std::initializer_list<const char*> ws)
    { for (auto* w : ws) if (t.contains (w)) return true; return false; };
    double sumW = 0.0;
    for (auto& x : archs)
    {
        const String t = (x.name + " " + x.tags).toLowerCase();
        float w = 1.0f;
        // dark/industrial/IDM core - weighted hardest
        if (has (t, { "dark", "horror", "memphis", "grim", "eerie", "crypt" }))                w = jmax (w, 2.4f);
        if (has (t, { "industrial", "nin", "scrap", "metal", "mechanical", "machine" }))       w = jmax (w, 2.2f);
        if (has (t, { "idm", "glitch", "braindance", "acid", "drill", "aphex", "squarepusher", "bitcrush" })) w = jmax (w, 2.2f);
        if (has (t, { "reese", "growl", "808", "distorted", "aggressive", "crunk" }))          w = jmax (w, 1.7f);
        // the broadened Trance-6 built-in palette - well represented, below the dark core
        if (has (t, { "reggae", "dub", "skank", "melodica" }))                                 w = jmax (w, 1.6f);
        if (has (t, { "psych", "fuzz", "krautrock", "zamrock", "mellotron", "motorik" }))      w = jmax (w, 1.5f);
        if (has (t, { "chip", "8-bit", "lo-fi", "bleep" }))                                    w = jmax (w, 1.5f);
        if (has (t, { "jazz", "noir", "upright", "vibes", "trumpet" }))                        w = jmax (w, 1.4f);
        if (has (t, { "sacred", "temple", "ritual", "chant", "monk", "drone", "meditative", "bowl", "handpan", "tibetan" })) w = jmax (w, 1.3f);
        if (has (t, { "fusion", "tabla", "conga", "frame drum", "hand drum", "clay" }))        w = jmax (w, 1.3f);
        // clean/anthemic outliers pulled back to accents (Vol-2-leaning material)
        if (has (t, { "gospel", "supersaw", "anthemic", "pan flute", "warm funk" }))           w *= 0.5f;
        x.weight = w;
        sumW += w;
    }

    const auto packDir = File::getSpecialLocation (File::userDocumentsDirectory)
                           .getChildFile ("VIBRA-VII").getChildFile ("Expansions").getChildFile (File::createLegalFileName (packName));
    packDir.createDirectory();
    { XmlElement px ("Pack");
      px.setAttribute ("name", packName);
      px.setAttribute ("tagline", "Dark horrorcore Memphis + NIN industrial + Aphex/Squarepusher IDM, with a meditative world undertow - original VIBRA-VII sounds");
      px.setAttribute ("author", "TRANCE-6");
      px.setAttribute ("style", "Urban");
      px.writeTo (packDir.getChildFile ("pack.xml")); }
    const auto sampleDir = packDir.getChildFile ("_samples");

    FourfoldProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, 512);
    proc.prepareToPlay (sr, 512);
    auto& params = proc.getParameters();

    std::mt19937 rng (0xC0A5EED);
    auto pick = [&] (const Vary& v) -> float
    {
        if (! v.choices.empty()) return (float) v.choices[(size_t) (rng() % v.choices.size())];
        std::uniform_real_distribution<float> d (v.lo, v.hi);
        const float x = d (rng);
        return v.isInt ? (float) roundToInt (x) : x;
    };

    int written = 0, dropped = 0, archIdx = 0;
    std::set<String> usedNames;
    for (auto& arch : archs)
    {
        auto dir = packDir.getChildFile ("Presets").getChildFile (File::createLegalFileName (arch.folder));
        const String noun = instrumentNoun (arch.name);
        const int per = jmax (1, roundToInt (target * arch.weight / sumW));
        int kept = 0;
        for (int i = 0; i < per; ++i)
        {
            std::map<String, float> vals = arch.base;
            for (auto& v : arch.vary) vals[v.id] = pick (v);

            for (auto* p : params) if (p != nullptr) p->setValueNotifyingHost (p->getDefaultValue());
            for (auto& [id, x] : vals)
                if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (x));
            proc.syncRack();

            auto audio = ff::design::renderAudition (proc, sr, 3.0);
            const float peak = audio.getMagnitude (0, audio.getNumSamples());
            if (! std::isfinite (peak) || peak < 0.01f || peak > 1.8f) { ++dropped; continue; }
            const auto feat = ff::design::analyse (audio, sr);

            ff::import::Converted cv;
            // evocative "<mood> <instrument>" name, unique across the pack
            String nm = String (kMoods[(archIdx * 7 + kept) % kNumMoods]) + " " + noun;
            { const String b = nm; int d = 2; while (! usedNames.insert (nm.toLowerCase()).second) nm = b + " " + String (d++); }
            cv.name = nm;
            cv.bank = packName;
            cv.author = "TRANCE-6";
            cv.tags = mergeTags (arch.tags, characterTags (feat));
            cv.notes = describe (arch.name, feat);
            for (auto& [id, x] : vals)
                if (auto* p = proc.apvts.getParameter (id))
                {
                    const float def = p->convertFrom0to1 (p->getDefaultValue());
                    const float clamped = p->convertFrom0to1 (p->convertTo0to1 (x));
                    if (std::abs (clamped - def) > 1.0e-4f) cv.values[id] = clamped;
                }
            if (ff::import::writePreset (cv, dir.getChildFile (File::createLegalFileName (cv.name) + ".ozpreset")))
            {
                ++written; ++kept;
                if (kept == 1) { sampleDir.createDirectory(); writeWav (sampleDir.getChildFile (File::createLegalFileName (arch.name) + ".wav"), audio, sr); }
            }
        }
        std::cerr << arch.name << ": " << kept << "\n";
        ++archIdx;
    }
    std::cerr << "\n" << written << " presets (" << archs.size() << " archetypes, weighted) into "
              << packDir.getFullPathName() << " (" << dropped << " dropped)\n";
    return 0;
}
