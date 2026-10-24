#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Wavetable.h"
#include "dsp/Granular.h"
#include "dsp/DspCore.h"

namespace ff
{
// Raw, lock-free pointers to every parameter the voices need.
struct ParamRefs
{
    std::atomic<float> *glide, *bend, *ampVel;
    std::atomic<float> *wtOn, *wtTable, *wtPos, *wtWarp, *wtWarpAmt, *wtOct, *wtSemi, *wtFine, *wtUnison, *wtDetune, *wtSpread, *wtLevel, *wtPan;
    std::atomic<float> *anOn, *anShape, *anPw, *anOct, *anSemi, *anFine, *anSub, *anNoise, *anDrift, *anLevel, *anPan;
    std::atomic<float> *grOn, *grPos, *grScan, *grSize, *grDensity, *grSpray, *grPitchRand, *grStereo, *grReverse, *grOct, *grSemi, *grLevel;
    std::atomic<float> *fltOn, *fltType, *fltCutoff, *fltRes, *fltDrive, *fltEnv, *fltKey;
    std::atomic<float> *env[3][4];
    std::atomic<float> *lfoShape[2], *lfoRate[2], *lfoRetrig[2];
    std::atomic<float> *macro[4];
    std::atomic<float> *modSrc[numModSlots], *modDst[numModSlots], *modAmt[numModSlots];

    void bind (juce::AudioProcessorValueTreeState& s)
    {
        auto g = [&] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
        glide = g ("glide"); bend = g ("bend"); ampVel = g ("amp_vel");
        wtOn = g ("wt_on"); wtTable = g ("wt_table"); wtPos = g ("wt_pos"); wtWarp = g ("wt_warp"); wtWarpAmt = g ("wt_warpamt");
        wtOct = g ("wt_oct"); wtSemi = g ("wt_semi"); wtFine = g ("wt_fine"); wtUnison = g ("wt_unison"); wtDetune = g ("wt_detune");
        wtSpread = g ("wt_spread"); wtLevel = g ("wt_level"); wtPan = g ("wt_pan");
        anOn = g ("an_on"); anShape = g ("an_shape"); anPw = g ("an_pw"); anOct = g ("an_oct"); anSemi = g ("an_semi"); anFine = g ("an_fine");
        anSub = g ("an_sub"); anNoise = g ("an_noise"); anDrift = g ("an_drift"); anLevel = g ("an_level"); anPan = g ("an_pan");
        grOn = g ("gr_on"); grPos = g ("gr_pos"); grScan = g ("gr_scan"); grSize = g ("gr_size"); grDensity = g ("gr_density");
        grSpray = g ("gr_spray"); grPitchRand = g ("gr_pitchrand"); grStereo = g ("gr_stereo"); grReverse = g ("gr_reverse");
        grOct = g ("gr_oct"); grSemi = g ("gr_semi"); grLevel = g ("gr_level");
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
        }
        for (int i = 0; i < 4; ++i) macro[i] = g ("macro" + juce::String (i + 1));
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
    // free-running LFO phases for when "retrigger" is off
    float freeLfoPhase[2] { 0.0f, 0.0f };
};

class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (SharedState& s, int seed) : shared (s), rng (seed) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int v) override;
    void controllerMoved (int cc, int v) override;
    void channelPressureChanged (int v) override { shared.aftertouch = (float) v / 127.0f; }
    void aftertouchChanged (int v) override { aftertouch = (float) v / 127.0f; }
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate (double sr) override;

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

    static constexpr int maxUnison = 7;
    float wtPhase[maxUnison] {};
    float note = 60.0f, targetNote = 60.0f;
    float velocity = 1.0f, aftertouch = 0.0f, randomValue = 0.0f;
    float driftValue = 0.0f, driftTarget = 0.0f;
    float scanOffset = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace ff
