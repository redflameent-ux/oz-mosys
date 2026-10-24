#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Wavetable.h"
#include "dsp/Granular.h"
#include "dsp/DspCore.h"
#include "dsp/Resonator.h"
#include "dsp/Mseg.h"
#include "dsp/FmEngine.h"

namespace ff
{
// Raw, lock-free pointers to every parameter the voices need.
struct ParamRefs
{
    std::atomic<float> *glide, *bend, *ampVel, *tuneA4, *scale, *scaleRoot;
    std::atomic<float> *resOn, *resType, *resDecay, *resBright, *resDamp, *resStrike, *resBow, *resBuzz, *resRetrig,
                       *resOct, *resSemi, *resLevel, *resPan;
    std::atomic<float> *lfoFade[2];
    std::atomic<float> *wtOn, *wtTable, *wtPos, *wtWarp, *wtWarpAmt, *wtOct, *wtSemi, *wtFine, *wtUnison, *wtDetune, *wtSpread, *wtLevel, *wtPan;
    std::atomic<float> *anOn, *anTone, *anShape, *anPw, *anOct, *anSemi, *anFine, *anSub, *anNoise, *anDrift, *anLevel, *anPan;
    std::atomic<float> *grOn, *grPos, *grScan, *grSize, *grDensity, *grSpray, *grPitchRand, *grStereo, *grReverse, *grOct, *grSemi, *grLevel;
    std::atomic<float> *fmOn, *fmLevel, *fmPan, *fmOct, *fmSemi, *fmIndex;
    std::atomic<float> *fmRatio[6], *fmFine[6], *fmOpLevel[6], *fmWave[6], *fmEnv[6][4], *fmVel[6], *fmMatrix[6][6];
    std::atomic<float> *fltOn, *fltType, *fltCutoff, *fltRes, *fltDrive, *fltEnv, *fltKey;
    std::atomic<float> *env[3][4];
    std::atomic<float> *lfoShape[2], *lfoRate[2], *lfoRetrig[2];
    std::atomic<float> *macro[8];
    std::atomic<float> *modSrc[numModSlots], *modDst[numModSlots], *modAmt[numModSlots];

    void bind (juce::AudioProcessorValueTreeState& s)
    {
        auto g = [&] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
        glide = g ("glide"); bend = g ("bend"); ampVel = g ("amp_vel");
        tuneA4 = g ("tune_a4"); scale = g ("scale"); scaleRoot = g ("scale_root");
        resOn = g ("res_on"); resType = g ("res_type"); resDecay = g ("res_decay"); resBright = g ("res_bright");
        resDamp = g ("res_damp"); resStrike = g ("res_strike"); resBow = g ("res_bow"); resBuzz = g ("res_buzz");
        resRetrig = g ("res_retrig"); resOct = g ("res_oct"); resSemi = g ("res_semi"); resLevel = g ("res_level"); resPan = g ("res_pan");
        wtOn = g ("wt_on"); wtTable = g ("wt_table"); wtPos = g ("wt_pos"); wtWarp = g ("wt_warp"); wtWarpAmt = g ("wt_warpamt");
        wtOct = g ("wt_oct"); wtSemi = g ("wt_semi"); wtFine = g ("wt_fine"); wtUnison = g ("wt_unison"); wtDetune = g ("wt_detune");
        wtSpread = g ("wt_spread"); wtLevel = g ("wt_level"); wtPan = g ("wt_pan");
        anOn = g ("an_on"); anTone = g ("an_tone"); anShape = g ("an_shape"); anPw = g ("an_pw"); anOct = g ("an_oct"); anSemi = g ("an_semi"); anFine = g ("an_fine");
        anSub = g ("an_sub"); anNoise = g ("an_noise"); anDrift = g ("an_drift"); anLevel = g ("an_level"); anPan = g ("an_pan");
        grOn = g ("gr_on"); grPos = g ("gr_pos"); grScan = g ("gr_scan"); grSize = g ("gr_size"); grDensity = g ("gr_density");
        grSpray = g ("gr_spray"); grPitchRand = g ("gr_pitchrand"); grStereo = g ("gr_stereo"); grReverse = g ("gr_reverse");
        grOct = g ("gr_oct"); grSemi = g ("gr_semi"); grLevel = g ("gr_level");
        fmOn = g ("fm_on"); fmLevel = g ("fm_level"); fmPan = g ("fm_pan"); fmOct = g ("fm_oct"); fmSemi = g ("fm_semi"); fmIndex = g ("fm_index");
        for (int i = 0; i < 6; ++i)
        {
            const juce::String pre = "fm" + juce::String (i + 1);
            fmRatio[i] = g (pre + "_ratio"); fmFine[i] = g (pre + "_fine"); fmOpLevel[i] = g (pre + "_level"); fmWave[i] = g (pre + "_wave");
            fmEnv[i][0] = g (pre + "_a"); fmEnv[i][1] = g (pre + "_d"); fmEnv[i][2] = g (pre + "_s"); fmEnv[i][3] = g (pre + "_r");
            fmVel[i] = g (pre + "_vel");
            for (int j = 0; j < 6; ++j) fmMatrix[i][j] = g ("fm_m" + juce::String (i + 1) + juce::String (j + 1));
        }
        fltOn = g ("flt_on"); fltType = g ("flt_type"); fltCutoff = g ("flt_cutoff"); fltRes = g ("flt_res"); fltDrive = g ("flt_drive");
        fltEnv = g ("flt_env"); fltKey = g ("flt_key");
        const char* envPre[3] = { "amp", "e2", "e3" };
        const char* envSuf[4] = { "_a", "_d", "_s", "_r" };
        for (int e = 0; e < 3; ++e)
            for (int k = 0; k < 4; ++k)
                env[e][k] = g (juce::String (envPre[e]) + envSuf[k]);
        for (int i = 0; i < 2; ++i)
        {
            const juce::String pre = "lfo" + juce::String (i + 1);
            lfoShape[i] = g (pre + "_shape"); lfoRate[i] = g (pre + "_rate"); lfoRetrig[i] = g (pre + "_retrig");
            lfoFade[i] = g (pre + "_fade");
        }
        for (int i = 0; i < 8; ++i) macro[i] = g ("macro" + juce::String (i + 1));
        for (int i = 0; i < numModSlots; ++i) { modSrc[i] = g (slotSrc (i)); modDst[i] = g (slotDst (i)); modAmt[i] = g (slotAmt (i)); }
    }
};

// State shared by all voices, refreshed by the processor at the start of each block.
struct SharedState
{
    ParamRefs p;
    WavetableBank* table = nullptr;
    GrainSource* grains = nullptr;
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f;   // pitchBend -1..1
    float lastNote = -1.0f;
    const MsegShape* mseg[4] { nullptr, nullptr, nullptr, nullptr };
    double bpm = 120.0;
    std::atomic<float>* modView = nullptr;   // live mod-source values for the GUI strip (last voice wins)
    // free-running LFO phases for when "retrigger" is off
    float freeLfoPhase[2] { 0.0f, 0.0f };
};

// MPE voice: every note has its own pitch bend, pressure and slide (timbre),
// so the ROLL page, Logic/Ableton/Bitwig and MPE keyboards can bend single notes.
class SynthVoice : public juce::MPESynthesiserVoice
{
public:
    explicit SynthVoice (SharedState& s, int seed) : shared (s), rng (seed) {}

    void noteStarted() override;
    void noteStopped (bool allowTailOff) override;
    void notePitchbendChanged() override {}
    void notePressureChanged() override {}
    void noteTimbreChanged() override {}
    void noteKeyStateChanged() override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    void setCurrentSampleRate (double sr) override;

    // For the GUI: latest modulated wavetable position of this voice (-1 when idle)
    std::atomic<float> displayWtPos { -1.0f };
    std::atomic<float> displayGrPos { -1.0f };

private:
    void renderChunk (float* outL, float* outR, int n);
    float warpPhase (float p, int mode, float amt) const noexcept;

    SharedState& shared;
    juce::Random rng;

    Envelope ampEnv, env2, env3;
    Lfo lfo[2];
    VoiceFilter filter;
    AnalogOsc analog;
    GrainCloud cloud;
    Resonator resonator;
    FmVoice fm;
    FmVoice::Settings fmSettings() const;
    MsegPlayer msegPlayer[4];
    float noteTime = 0.0f;
    float tuneOffset = 0.0f;   // A4 tuning + scale tuning, in semitones

    static constexpr int maxUnison = 7;
    float wtPhase[maxUnison] {};
    float note = 60.0f, targetNote = 60.0f;
    float velocity = 1.0f, randomValue = 0.0f, bendNow = 0.0f;
    bool  bendReset = true;
    float driftValue = 0.0f, driftTarget = 0.0f;
    float scanOffset = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace ff
