#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Rack.h"
#include "perform/Perform.h"

// ============================================================================
//  OZ-mosys - parameter definitions
//  Four engines in one synth: Wavetable, Analog, Granular + a deep mod system.
// ============================================================================

namespace ff
{
// ---- modulation sources ----------------------------------------------------
enum ModSource
{
    srcNone = 0, srcEnv2, srcEnv3, srcLfo1, srcLfo2, srcVelocity, srcModWheel,
    srcAftertouch, srcKeyTrack, srcRandom, srcMacro1, srcMacro2, srcMacro3, srcMacro4,
    srcSlide, srcMseg1, srcMseg2, srcMseg3, srcMseg4, srcMacro5, srcMacro6, srcMacro7, srcMacro8,
    numModSources
};

inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray names { "None", "Env 2", "Env 3", "LFO 1", "LFO 2", "Velocity",
                                           "Mod Wheel", "Aftertouch", "Key Track", "Random",
                                           "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Slide (MPE)",
                                           "MSEG 1", "MSEG 2", "MSEG 3", "MSEG 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8" };
    return names;
}

// ---- modulation destinations ----------------------------------------------
enum ModDest
{
    dstNone = 0,
    dstWtPos, dstWtWarp, dstWtPitch, dstWtDetune, dstWtLevel,
    dstAnShape, dstAnPw, dstAnPitch, dstAnLevel,
    dstGrPos, dstGrSize, dstGrDensity, dstGrSpray, dstGrPitch, dstGrLevel,
    dstCutoff, dstResonance, dstFltDrive,
    dstAmp, dstPan, dstPitchAll, dstLfo1Rate, dstLfo2Rate,
    dstResPitch, dstResBright, dstResDecay, dstResBow, dstResLevel, dstResBuzz,
    dstFmPitch, dstFmIndex, dstFmLevel,
    dstCutoff2, dstRes2, dstFeedback, dstMoPitch, dstNoiseLevel,
    numModDests
};

inline const juce::StringArray& modDestNames()
{
    static const juce::StringArray names { "None",
        "WT Position", "WT Warp", "WT Pitch", "WT Detune", "WT Level",
        "Analog Shape", "Analog PW", "Analog Pitch", "Analog Level",
        "Grain Position", "Grain Size", "Grain Density", "Grain Spray", "Grain Pitch", "Grain Level",
        "Filter Cutoff", "Filter Resonance", "Filter Drive",
        "Volume", "Pan", "Pitch (All)", "LFO 1 Rate", "LFO 2 Rate",
        "Res Pitch", "Res Brightness", "Res Decay", "Res Bow", "Res Level", "Res Buzz",
        "FM Pitch", "FM Depth", "FM Level",
        "Filter 2 Cutoff", "Filter 2 Resonance", "Feedback", "Mod Osc Pitch", "Noise Level" };
    return names;
}

constexpr int numModSlots = 10;

inline const juce::StringArray& wavetableNames()
{
    static const juce::StringArray names { "Basic Shapes", "Harmonic Sweep", "Vocal Formants",
                                           "Digital Grit", "Pulse Width", "Dark Organ", "User Table" };
    return names;
}
constexpr int numBuiltInTables = 6;   // the last entry is the user-imported slot

inline const juce::StringArray& warpNames()
{
    static const juce::StringArray names { "Off", "Sync", "Bend +", "Bend -", "Mirror", "Asym", "Formant", "Steps", "Fold" };
    return names;
}

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names { "Ladder LP 24", "LP 12", "Band Pass", "High Pass", "Notch" };
    return names;
}

// Absynth-style module slots (2 per engine)
inline const juce::StringArray& slotTypeNames()
{
    static const juce::StringArray n { "Off", "Ladder LP", "LP 12", "HP 12", "Band Pass", "Notch", "Comb", "Freq Shift",
                                       "Ring Mod", "Waveshaper", "Decimator" };
    return n;
}
inline const char* const* enginePrefixes() { static const char* p[] = { "wt", "an", "gr", "res", "fm" }; return p; }
constexpr int numEngines = 5;
constexpr int slotsPerEngine = 2;

inline const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw", "Square", "Sample & Hold", "Smooth Random" };
    return names;
}

inline const juce::StringArray& grainSourceNames()
{
    static const juce::StringArray names { "Breathing Choir", "Monk Chant", "Breath & Wind", "Rain & Water", "Your Sample" };
    return names;
}
constexpr int grainSourceUser = 4;

inline const juce::StringArray& driveTypeNames()
{
    static const juce::StringArray names { "Warm", "Rat", "Fuzz" };
    return names;
}

// ---- tunings ---------------------------------------------------------------
inline const juce::StringArray& scaleNames()
{
    static const juce::StringArray names { "Equal (12-TET)", "Just Intonation", "Raga Bhairav", "Maqam Rast",
                                           "Maqam Bayati", "Maqam Hijaz", "Pythagorean" };
    return names;
}

inline const juce::StringArray& noteNames()
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

// Cents offset from equal temperament for a note `degree` semitones above the root.
inline float scaleOffsetCents (int scale, int degree)
{
    static const float table[7][12] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        // 5-limit just: 1 16/15 9/8 6/5 5/4 4/3 45/32 3/2 8/5 5/3 16/9 15/8
        { 0, 11.73f, 3.91f, 15.64f, -13.69f, -1.96f, -9.78f, 1.96f, 13.69f, -15.64f, -3.91f, -11.73f },
        // Bhairav: very flat komal Re and komal Dha, just Ga / Ni
        { 0, -9.78f, 3.91f, 15.64f, -13.69f, -1.96f, -9.78f, 1.96f, -7.82f, -15.64f, -3.91f, -11.73f },
        // Rast (on the root): 3rd and 7th a quarter-tone flat
        { 0, 0, 0, 0, -50.0f, 0, 0, 0, 0, 0, 0, -50.0f },
        // Bayati (on the root): 2nd a quarter-tone flat
        { 0, 0, -50.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        // Hijaz: raised flat-2 and lowered major-3 (wide augmented second)
        { 0, 10.0f, 0, 0, -15.0f, 0, 0, 0, 10.0f, 0, 0, 0 },
        // Pythagorean (pure fifths)
        { 0, -9.78f, 3.91f, -5.87f, 7.82f, -1.96f, 11.73f, 1.96f, -7.82f, 5.87f, -3.91f, 9.78f },
    };
    return table[juce::jlimit (0, 6, scale)][((degree % 12) + 12) % 12];
}

// ---- helpers ---------------------------------------------------------------
inline juce::String slotSrc (int i) { return "mod" + juce::String (i + 1) + "_src"; }
inline juce::String slotDst (int i) { return "mod" + juce::String (i + 1) + "_dst"; }
inline juce::String slotAmt (int i) { return "mod" + juce::String (i + 1) + "_amt"; }

inline juce::NormalisableRange<float> timeRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (lo + (hi - lo) * 0.08f);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int v = 1;
    auto pid = [&] (const String& id) { return ParameterID { id, v }; };

    auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const String& label = {})
    {
        layout.add (std::make_unique<AudioParameterFloat> (pid (id), name, r, def, AudioParameterFloatAttributes().withLabel (label)));
    };
    auto addI = [&] (const String& id, const String& name, int lo, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (pid (id), name, lo, hi, def));
    };
    auto addC = [&] (const String& id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (pid (id), name, choices, def));
    };
    auto addB = [&] (const String& id, const String& name, bool def)
    {
        // stored as an Off/On choice so hosts always restore it exactly
        layout.add (std::make_unique<AudioParameterChoice> (pid (id), name, StringArray { "Off", "On" }, def ? 1 : 0));
    };

    // ---- Master ----
    addF ("master", "Master Volume", { 0.0f, 1.5f }, 0.7f);
    addF ("glide", "Glide", timeRange (0.0f, 2.0f), 0.0f, "s");
    addI ("bend", "Pitch Bend Range", 0, 24, 2);
    addF ("amp_vel", "Velocity Sensitivity", { 0.0f, 1.0f }, 0.5f);
    addC ("voice_mode", "Voice Mode", StringArray { "Poly", "Mono" }, 0);
    addC ("glide_curve", "Glide Curve", StringArray { "Smooth", "Linear", "Glissando" }, 0);
    addC ("vel_curve", "Velocity Curve", StringArray { "Linear", "Soft touch", "Hard touch", "Fixed" }, 0);
    addF ("tune_a4", "Tuning (A4)", { 400.0f, 480.0f, 0.01f }, 440.0f, "Hz");
    addC ("scale", "Scale Tuning", scaleNames(), 0);
    addC ("scale_root", "Scale Root", noteNames(), 0);

    // ---- Wavetable engine ----
    addB ("wt_on", "WT On", true);
    addC ("wt_table", "WT Table", wavetableNames(), 0);
    addF ("wt_pos", "WT Position", { 0.0f, 1.0f }, 0.66f);
    addC ("wt_warp", "WT Warp Mode", warpNames(), 0);
    addF ("wt_warpamt", "WT Warp Amount", { 0.0f, 1.0f }, 0.0f);
    addI ("wt_oct", "WT Octave", -3, 3, 0);
    addI ("wt_semi", "WT Semitone", -12, 12, 0);
    addF ("wt_fine", "WT Fine", { -100.0f, 100.0f }, 0.0f, "ct");
    addI ("wt_unison", "WT Unison", 1, 7, 1);
    addF ("wt_detune", "WT Detune", { 0.0f, 1.0f }, 0.25f);
    addF ("wt_spread", "WT Stereo Spread", { 0.0f, 1.0f }, 0.7f);
    addF ("wt_level", "WT Level", { 0.0f, 1.0f }, 0.75f);
    addF ("wt_pan", "WT Pan", { -1.0f, 1.0f }, 0.0f);

    // ---- Analog engine ----
    addB ("an_on", "Analog On", false);
    addF ("an_shape", "Analog Shape", { 0.0f, 2.0f }, 1.0f);
    addF ("an_pw", "Analog Pulse Width", { 0.05f, 0.95f }, 0.5f);
    addI ("an_oct", "Analog Octave", -3, 3, 0);
    addI ("an_semi", "Analog Semitone", -12, 12, 0);
    addF ("an_fine", "Analog Fine", { -100.0f, 100.0f }, 0.0f, "ct");
    addF ("an_tone", "Analog Osc Level", { 0.0f, 1.0f }, 1.0f);
    addF ("an_sub", "Analog Sub", { 0.0f, 1.0f }, 0.0f);
    addF ("an_noise", "Analog Noise", { 0.0f, 1.0f }, 0.0f);
    addF ("an_drift", "Analog Drift", { 0.0f, 1.0f }, 0.3f);
    addF ("an_level", "Analog Level", { 0.0f, 1.0f }, 0.7f);
    addF ("an_pan", "Analog Pan", { -1.0f, 1.0f }, 0.0f);

    // ---- Granular engine ----
    addB ("gr_on", "Granular On", false);
    addF ("gr_pos", "Grain Position", { 0.0f, 1.0f }, 0.3f);
    addF ("gr_scan", "Grain Scan Speed", { -1.0f, 1.0f }, 0.0f);
    addF ("gr_size", "Grain Size", timeRange (10.0f, 600.0f), 120.0f, "ms");
    addF ("gr_density", "Grain Density", timeRange (1.0f, 120.0f), 20.0f, "/s");
    addF ("gr_spray", "Grain Spray", { 0.0f, 1.0f }, 0.1f);
    addF ("gr_pitchrand", "Grain Pitch Random", { 0.0f, 12.0f }, 0.0f, "st");
    addF ("gr_stereo", "Grain Stereo Width", { 0.0f, 1.0f }, 0.6f);
    addF ("gr_reverse", "Grain Reverse Chance", { 0.0f, 1.0f }, 0.0f);
    addI ("gr_oct", "Grain Octave", -3, 3, 0);
    addI ("gr_semi", "Grain Semitone", -12, 12, 0);
    addF ("gr_level", "Grain Level", { 0.0f, 1.0f }, 0.7f);
    addC ("gr_source", "Grain Source", grainSourceNames(), 0);

    // ---- Resonator engine (physical modelling) ----
    addB ("res_on", "Resonator On", false);
    addC ("res_type", "Resonator Type", StringArray { "Plucked String", "Sitar", "Tibetan Bowl", "Crystal Bowl", "Gong",
                                                       "Handpan", "Tongue Drum", "Tingsha / Bell", "Music Box", "Tabla",
                                                       "Hand Drum", "Clay Pot", "808 Cowbell", "Scrap Metal", "Anvil", "Metal Pipe",
                                                       "Vibraphone", "Marimba", "E-Piano" }, 0);
    addF ("res_decay", "Res Decay", timeRange (0.02f, 20.0f), 2.0f, "s");
    addF ("res_bright", "Res Brightness", { 0.0f, 1.0f }, 0.6f);
    addF ("res_damp", "Res Damping", { 0.0f, 1.0f }, 0.5f);
    addF ("res_strike", "Res Strike Hardness", { 0.0f, 1.0f }, 0.6f);
    addF ("res_bow", "Res Bow / Rub", { 0.0f, 1.0f }, 0.0f);
    addF ("res_buzz", "Res Buzz", { 0.0f, 1.0f }, 0.0f);
    {
        NormalisableRange<float> rr (0.0f, 16.0f);
        rr.setSkewForCentre (3.0f);
        addF ("res_retrig", "Res Re-pluck Rate", rr, 0.0f, "Hz");
    }
    addI ("res_oct", "Res Octave", -3, 3, 0);
    addI ("res_semi", "Res Semitone", -12, 12, 0);
    addF ("res_level", "Res Level", { 0.0f, 1.0f }, 0.75f);
    addF ("res_pan", "Res Pan", { -1.0f, 1.0f }, 0.0f);

    // ---- FM engine (6 operators + matrix) ----
    addB ("fm_on", "FM On", false);
    addF ("fm_level", "FM Level", { 0.0f, 1.0f }, 0.7f);
    addF ("fm_pan", "FM Pan", { -1.0f, 1.0f }, 0.0f);
    addI ("fm_oct", "FM Octave", -3, 3, 0);
    addI ("fm_semi", "FM Semitone", -12, 12, 0);
    addF ("fm_index", "FM Depth", { 0.0f, 2.0f }, 1.0f);
    {
        const float defRatio[6] = { 1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 7.0f };
        const char* L = "ABCDEF";
        for (int i = 0; i < 6; ++i)
        {
            const String pre = "fm" + String (i + 1), nm = String ("FM Op ") + L[i];
            NormalisableRange<float> rr (0.125f, 32.0f, 0.001f);
            rr.setSkewForCentre (2.0f);
            addF (pre + "_ratio", nm + " Ratio", rr, defRatio[i]);
            addF (pre + "_fine", nm + " Fine", { -100.0f, 100.0f }, 0.0f, "ct");
            addF (pre + "_level", nm + " Output", { 0.0f, 1.0f }, i == 0 ? 1.0f : 0.0f);
            addC (pre + "_wave", nm + " Wave", StringArray { "Sine", "Half Sine", "Abs Sine", "Triangle", "Saw", "Square", "Pulse 25", "Sine x2" }, 0);
            addF (pre + "_a", nm + " Attack", timeRange (0.0f, 10.0f), 0.002f, "s");
            addF (pre + "_d", nm + " Decay", timeRange (0.001f, 10.0f), i == 0 ? 1.5f : 0.8f, "s");
            addF (pre + "_s", nm + " Sustain", { 0.0f, 1.0f }, i == 0 ? 0.7f : 0.4f);
            addF (pre + "_r", nm + " Release", timeRange (0.001f, 15.0f), 0.4f, "s");
            addF (pre + "_vel", nm + " Velocity", { 0.0f, 1.0f }, 0.5f);
        }
        for (int from = 0; from < 6; ++from)
            for (int to = 0; to < 6; ++to)
                addF ("fm_m" + String (from + 1) + String (to + 1), String ("FM ") + L[from] + " > " + L[to], { 0.0f, 1.0f },
                      (from == 1 && to == 0) ? 0.35f : 0.0f);
    }

    // ---- Module slots: 2 per engine (Absynth patch page) ----
    for (int e = 0; e < numEngines; ++e)
        for (int sl = 1; sl <= slotsPerEngine; ++sl)
        {
            const String pre = String (enginePrefixes()[e]) + "_s" + String (sl);
            const String nm = String (enginePrefixes()[e]).toUpperCase() + " Slot " + String (sl);
            addC (pre + "_type", nm + " Type", slotTypeNames(), 0);
            addF (pre + "_a", nm + " A", { 0.0f, 1.0f }, 0.5f);
            addF (pre + "_b", nm + " B", { 0.0f, 1.0f }, 0.3f);
            addF (pre + "_mix", nm + " Mix", { 0.0f, 1.0f }, 1.0f);
        }

    // ---- Modulation oscillator, noise, feedback (Massive) ----
    addF ("mo_pitch", "Mod Osc Pitch", { -24.0f, 24.0f, 0.01f }, 0.0f, "st");
    addF ("mo_ring", "Mod Osc Ring Mod", { 0.0f, 1.0f }, 0.0f);
    addC ("mo_ringdst", "Mod Osc Ring Target", StringArray { "All engines", "Wavetable", "Analog", "Granular", "Resonator", "FM" }, 0);
    addF ("mo_phase", "Mod Osc Phase Mod", { 0.0f, 1.0f }, 0.0f);
    addF ("mo_pos", "Mod Osc Position Mod", { 0.0f, 1.0f }, 0.0f);
    addF ("mo_ffm", "Mod Osc Filter FM", { 0.0f, 1.0f }, 0.0f);
    addF ("noise_level", "Noise Level", { 0.0f, 1.0f }, 0.0f);
    addC ("noise_color", "Noise Colour", StringArray { "White", "Pink", "Brown", "Digital", "Crackle" }, 0);
    addF ("fb_amt", "Feedback", { 0.0f, 1.0f }, 0.0f);

    // ---- Filter ----
    addB ("flt_on", "Filter On", true);
    addC ("flt_type", "Filter Type", filterTypeNames(), 0);
    addF ("flt_cutoff", "Filter Cutoff", { 0.0f, 1.0f }, 0.85f);
    addF ("flt_res", "Filter Resonance", { 0.0f, 1.0f }, 0.15f);
    addF ("flt_drive", "Filter Drive", { 0.0f, 1.0f }, 0.0f);
    addF ("flt_env", "Filter Env Amount", { -1.0f, 1.0f }, 0.0f);
    addF ("flt_key", "Filter Key Track", { 0.0f, 1.0f }, 0.0f);
    addB ("flt2_on", "Filter 2 On", false);
    addC ("flt2_type", "Filter 2 Type", filterTypeNames(), 3);
    addF ("flt2_cutoff", "Filter 2 Cutoff", { 0.0f, 1.0f }, 0.3f);
    addF ("flt2_res", "Filter 2 Resonance", { 0.0f, 1.0f }, 0.2f);
    addF ("flt2_drive", "Filter 2 Drive", { 0.0f, 1.0f }, 0.0f);
    addF ("flt2_env", "Filter 2 Env Amount", { -1.0f, 1.0f }, 0.0f);
    addC ("flt_route", "Filter Routing", StringArray { "Series", "Parallel" }, 0);
    addF ("flt_balance", "Filter Balance", { 0.0f, 1.0f }, 0.5f);

    // ---- Envelopes ----
    auto addEnv = [&] (const String& pre, const String& name, float a, float d, float s, float r)
    {
        addF (pre + "_a", name + " Attack", timeRange (0.0f, 10.0f), a, "s");
        addF (pre + "_d", name + " Decay", timeRange (0.001f, 10.0f), d, "s");
        addF (pre + "_s", name + " Sustain", { 0.0f, 1.0f }, s);
        addF (pre + "_r", name + " Release", timeRange (0.001f, 15.0f), r, "s");
    };
    addEnv ("amp", "Amp", 0.005f, 0.3f, 0.85f, 0.35f);
    addEnv ("e2", "Env 2", 0.01f, 0.5f, 0.0f, 0.5f);
    addEnv ("e3", "Env 3", 1.0f, 1.0f, 0.5f, 1.5f);

    // ---- LFOs ----
    for (int i = 1; i <= 2; ++i)
    {
        String pre = "lfo" + String (i);
        addC (pre + "_shape", "LFO " + String (i) + " Shape", lfoShapeNames(), 0);
        NormalisableRange<float> rr (0.01f, 30.0f);
        rr.setSkewForCentre (2.0f);
        addF (pre + "_rate", "LFO " + String (i) + " Rate", rr, i == 1 ? 0.5f : 5.0f, "Hz");
        addB (pre + "_retrig", "LFO " + String (i) + " Retrigger", true);
        addF (pre + "_fade", "LFO " + String (i) + " Fade In", timeRange (0.0f, 5.0f), 0.0f, "s");
    }

    // ---- Macros ----
    for (int i = 1; i <= 8; ++i)
        addF ("macro" + String (i), "Macro " + String (i), { 0.0f, 1.0f }, 0.0f);

    // ---- Mod matrix ----
    for (int i = 0; i < numModSlots; ++i)
    {
        addC (slotSrc (i), "Mod " + String (i + 1) + " Source", modSourceNames(), 0);
        addC (slotDst (i), "Mod " + String (i + 1) + " Destination", modDestNames(), 0);
        addF (slotAmt (i), "Mod " + String (i + 1) + " Amount", { -1.0f, 1.0f }, 0.0f);
    }

    // ---- Effects ----
    addF ("fx_drive", "FX Drive", { 0.0f, 1.0f }, 0.0f);
    addC ("drive_type", "Drive Type", driveTypeNames(), 0);
    addF ("crush_bits", "Bit Depth", { 3.0f, 24.0f, 1.0f }, 24.0f, "bit");
    {
        NormalisableRange<float> cr (1.0f, 32.0f, 1.0f);
        cr.setSkewForCentre (6.0f);
        addF ("crush_rate", "Sample-Rate Reduce", cr, 1.0f, "x");
    }
    addF ("tape_wow", "Tape Wow & Flutter", { 0.0f, 1.0f }, 0.0f);
    addF ("tape_hiss", "Tape Hiss", { 0.0f, 1.0f }, 0.0f);
    addF ("tape_age", "Tape Age", { 0.0f, 1.0f }, 0.0f);
    {
        NormalisableRange<float> rf (20.0f, 3000.0f);
        rf.setSkewForCentre (300.0f);
        addF ("ring_freq", "Ring Mod Freq", rf, 300.0f, "Hz");
    }
    addF ("ring_mix", "Ring Mod Mix", { 0.0f, 1.0f }, 0.0f);
    addF ("eq_bass", "Bass", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addF ("eq_treble", "Treble", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addF ("cho_mix", "Chorus Mix", { 0.0f, 1.0f }, 0.0f);
    addF ("cho_rate", "Chorus Rate", { 0.05f, 5.0f }, 0.6f, "Hz");
    addF ("cho_depth", "Chorus Depth", { 0.0f, 1.0f }, 0.4f);
    addF ("dly_mix", "Delay Mix", { 0.0f, 1.0f }, 0.0f);
    addF ("dly_time", "Delay Time", timeRange (10.0f, 1500.0f), 375.0f, "ms");
    addF ("dly_fb", "Delay Feedback", { 0.0f, 0.95f }, 0.35f);
    addF ("dly_tone", "Delay Tone", { 0.0f, 1.0f }, 0.6f);
    addF ("rev_mix", "Reverb Mix", { 0.0f, 1.0f }, 0.15f);
    addF ("rev_size", "Reverb Size", { 0.0f, 1.0f }, 0.6f);
    addF ("rev_damp", "Reverb Damping", { 0.0f, 1.0f }, 0.5f);

    // ---- FX rack (8 slots; 1-4 low / 5-8 high when split) ----
    for (int i = 1; i <= numRackSlots; ++i)
    {
        const String pre = "rk" + String (i), nm = "Rack " + String (i);
        addC (pre + "_type", nm + " Unit", rackUnitNames(), 0);
        addB (pre + "_on", nm + " On", true);
        addF (pre + "_a", nm + " A", { 0.0f, 1.0f }, 0.5f);
        addF (pre + "_b", nm + " B", { 0.0f, 1.0f }, 0.5f);
        addF (pre + "_c", nm + " C", { 0.0f, 1.0f }, 0.5f);
        addF (pre + "_d", nm + " D", { 0.0f, 1.0f }, 0.5f);
        addF (pre + "_mix", nm + " Mix", { 0.0f, 1.0f }, 1.0f);
    }
    addB ("rk_split", "Rack Split", false);
    {
        NormalisableRange<float> xr (60.0f, 4000.0f, 1.0f); xr.setSkewForCentre (300.0f);
        addF ("rk_xover", "Rack Split Frequency", xr, 250.0f, "Hz");
    }

    // ---- Chords tool + arpeggiator (the PLAY page) ----
    addB ("ch_on", "Chords On", false);
    addC ("ch_follow", "Chords Mode", StringArray { "In key", "Fixed shape" }, 0);
    addC ("ch_key", "Chords Key", noteNames(), 0);
    addC ("ch_scale", "Chords Scale", perf::chordScaleNames(), 1);
    addC ("ch_type", "Chord Type", perf::chordTypeNames(), 0);
    addC ("ch_voicing", "Chord Voicing", StringArray { "Close", "Open", "Drop 2", "Bass + chord" }, 0);
    addI ("ch_octave", "Chords Octave", -2, 2, 0);
    addF ("ch_strum", "Chords Strum", { 0.0f, 1.0f }, 0.0f);
    addF ("ch_humanize", "Chords Humanize", { 0.0f, 1.0f }, 0.0f);
    addB ("arp_on", "Arp On", false);
    addB ("arp_latch", "Arp Latch", false);
    addC ("arp_rate", "Arp Rate", perf::arpRateNames(), 3);
    addC ("arp_mode", "Arp Mode", perf::arpModeNames(), 0);
    addI ("arp_oct", "Arp Octaves", 1, 4, 1);
    addF ("arp_gate", "Arp Gate", { 0.05f, 1.0f }, 0.6f);
    addF ("arp_swing", "Arp Swing", { 0.0f, 0.75f }, 0.0f);
    addI ("arp_steps", "Arp Steps", 1, 32, 16);

    // ---- Sampler (the SAMPLER page) ----
    addC ("smp_play", "Sampler Play", StringArray { "Off", "Layered with synth", "Sampler only" }, 0);
    addB ("smp_fx", "Sampler Through FX", true);
    addF ("smp_level", "Sampler Level", { -36.0f, 6.0f, 0.1f }, 0.0f, "dB");
    addI ("smp_tune", "Sampler Tune", -24, 24, 0);
    addF ("smp_fine", "Sampler Fine", { -100.0f, 100.0f }, 0.0f, "ct");
    addF ("smp_pan", "Sampler Pan", { -1.0f, 1.0f }, 0.0f);
    {
        NormalisableRange<float> ar (0.0f, 4000.0f, 1.0f); ar.setSkewForCentre (300.0f);
        addF ("smp_attack", "Sampler Attack", ar, 0.0f, "ms");
        NormalisableRange<float> dr (0.0f, 10000.0f, 1.0f); dr.setSkewForCentre (800.0f);
        addF ("smp_decay", "Sampler Decay", dr, 500.0f, "ms");
    }
    addF ("smp_sustain", "Sampler Sustain", { 0.0f, 1.0f }, 1.0f);
    addF ("smp_release", "Sampler Release", timeRange (0.02f, 20.0f), 1.0f, "s");
    addB ("smp_letring", "Sampler Let Ring", true);
    addF ("smp_vel", "Sampler Touch", { 0.0f, 1.0f }, 0.7f);
    addB ("smp_choke", "Sampler Choke", false);

    // ---- Morph pad + Orb (the MORPH page) ----
    addB ("morph_on", "Morph On", false);
    addF ("morph_x", "Morph X", { 0.0f, 1.0f }, 0.0f);
    addF ("morph_y", "Morph Y", { 0.0f, 1.0f }, 1.0f);
    addC ("orb_mode", "Orb Motion", StringArray { "Still", "Dice", "Inertia", "Attractor", "Recorded path" }, 0);
    addF ("orb_speed", "Orb Speed", { 0.0f, 1.0f }, 0.35f);
    addF ("orb_depth", "Orb Depth", { 0.0f, 1.0f }, 0.8f);

    return layout;
}

// Cutoff knob (0..1) to Hz: 20 Hz .. 20 kHz on an exponential curve.
inline float cutoffToHz (float n) { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, n)); }

} // namespace ff
