#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

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
    srcSlide,
    numModSources
};

inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray names { "None", "Env 2", "Env 3", "LFO 1", "LFO 2", "Velocity",
                                           "Mod Wheel", "Aftertouch", "Key Track", "Random",
                                           "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Slide (MPE)" };
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
        "Res Pitch", "Res Brightness", "Res Decay", "Res Bow", "Res Level", "Res Buzz" };
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
    static const juce::StringArray names { "Off", "Sync", "Bend +", "Bend -", "Mirror", "Asym" };
    return names;
}

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names { "Ladder LP 24", "LP 12", "Band Pass", "High Pass", "Notch" };
    return names;
}

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

    // ---- Filter ----
    addB ("flt_on", "Filter On", true);
    addC ("flt_type", "Filter Type", filterTypeNames(), 0);
    addF ("flt_cutoff", "Filter Cutoff", { 0.0f, 1.0f }, 0.85f);
    addF ("flt_res", "Filter Resonance", { 0.0f, 1.0f }, 0.15f);
    addF ("flt_drive", "Filter Drive", { 0.0f, 1.0f }, 0.0f);
    addF ("flt_env", "Filter Env Amount", { -1.0f, 1.0f }, 0.0f);
    addF ("flt_key", "Filter Key Track", { 0.0f, 1.0f }, 0.0f);

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
    for (int i = 1; i <= 4; ++i)
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

    return layout;
}

// Cutoff knob (0..1) to Hz: 20 Hz .. 20 kHz on an exponential curve.
inline float cutoffToHz (float n) { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, n)); }

} // namespace ff
