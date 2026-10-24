#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ff
{
// ---------------------------------------------------------------------------
// ADSR with linear attack and exponential decay / release.
// ---------------------------------------------------------------------------
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) { sampleRate = sr; }
    void set (float a, float d, float s, float r)
    {
        attackInc = a <= 0.0005f ? 1.0f : 1.0f / (float) (a * sampleRate);
        decayCoef = coefFor (d);
        sustain = s;
        releaseCoef = coefFor (r);
    }
    void noteOn()  { stage = Stage::Attack; }
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset()   { stage = Stage::Idle; value = 0.0f; }
    bool isActive() const { return stage != Stage::Idle; }
    float getValue() const { return value; }

    inline float next() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                value += attackInc;
                if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                value = sustain + (value - sustain) * decayCoef;
                if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain: value = sustain; break;
            case Stage::Release:
                value *= releaseCoef;
                if (value < 1.0e-4f) { value = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle: break;
        }
        return value;
    }

private:
    float coefFor (float seconds) const
    {
        // reach ~ -80 dB in the given time
        const double n = juce::jmax (1.0, (double) seconds * sampleRate);
        return (float) std::exp (std::log (1.0e-4) / n);
    }
    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    float value = 0.0f, attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;
};

// ---------------------------------------------------------------------------
// LFO (control rate). Output is -1..1.
// ---------------------------------------------------------------------------
class Lfo
{
public:
    void reset (float startPhase, juce::Random& r)
    {
        phase = startPhase; held = r.nextFloat() * 2.0f - 1.0f;
        prev = held; target = r.nextFloat() * 2.0f - 1.0f;
    }

    float advance (int shape, float rateHz, float seconds, juce::Random& r)
    {
        phase += rateHz * seconds;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = r.nextFloat() * 2.0f - 1.0f;
            prev = target; target = r.nextFloat() * 2.0f - 1.0f;
        }
        const float p = phase;
        switch (shape)
        {
            case 0:  return std::sin (p * juce::MathConstants<float>::twoPi);
            case 1:  return p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            case 2:  return 1.0f - 2.0f * p;
            case 3:  return p < 0.5f ? 1.0f : -1.0f;
            case 4:  return held;
            default:
            {
                const float s = p * p * (3.0f - 2.0f * p);
                return prev + (target - prev) * s;
            }
        }
    }
    float phase = 0.0f;

private:
    float held = 0.0f, prev = 0.0f, target = 0.0f;
};

// ---------------------------------------------------------------------------
// Stereo multimode filter: zero-delay-feedback ladder + state-variable modes.
// ---------------------------------------------------------------------------
class VoiceFilter
{
public:
    void setSampleRate (double sr) { sampleRate = (float) sr; }
    void reset()
    {
        for (auto& ch : st) { ch = {}; }
    }

    // type: 0 ladder LP24, 1 LP12, 2 BP, 3 HP, 4 Notch
    void setParams (int newType, float cutoffHz, float res01, float drive01)
    {
        type = newType;
        const float fc = juce::jlimit (15.0f, sampleRate * 0.45f, cutoffHz);
        g = std::tan (juce::MathConstants<float>::pi * fc / sampleRate);
        res = juce::jlimit (0.0f, 1.0f, res01);
        drive = 1.0f + drive01 * 6.0f;
        // SVF damping
        k = 2.0f - 1.96f * res;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        // ladder
        G = g / (1.0f + g);
        ladderK = res * 4.1f;
    }

    inline float process (float x, int ch) noexcept
    {
        auto& s = st[(size_t) ch];
        if (drive > 1.001f) x = std::tanh (x * drive) / std::sqrt (drive);

        if (type == 0)
        {
            // TPT 4-pole ladder with tanh on the feedback path
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
            const float S = (G3 * s.z[0] + G2 * s.z[1] + G * s.z[2] + s.z[3]) / (1.0f + g);
            float u = (x - ladderK * S) / (1.0f + ladderK * G4);
            u = std::tanh (u);
            float in = u;
            for (int i = 0; i < 4; ++i)
            {
                const float v = (in - s.z[i]) * G;
                const float y = v + s.z[i];
                s.z[i] = y + v;
                in = y;
            }
            return in * (1.0f + res * 0.6f);   // gentle level compensation for resonance
        }

        // Cytomic SVF
        const float v3 = x - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        switch (type)
        {
            case 1:  return v2;
            case 2:  return v1 * k;
            case 3:  return x - k * v1 - v2;
            default: return x - k * v1;
        }
    }

private:
    struct State { float z[4] {}; float ic1 = 0, ic2 = 0; };
    std::array<State, 2> st {};
    float sampleRate = 44100.0f;
    int type = 0;
    float g = 0.1f, res = 0.0f, drive = 1.0f, k = 2.0f, a1 = 0, a2 = 0, a3 = 0, G = 0.1f, ladderK = 0.0f;
};

// ---------------------------------------------------------------------------
// Band-limited analog-style oscillator (PolyBLEP) with shape morph.
// shape 0 = triangle, 1 = saw, 2 = pulse (crossfades in between)
// ---------------------------------------------------------------------------
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt;  return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

class AnalogOsc
{
public:
    void reset (float startPhase)
    {
        phase = startPhase; subPhase = startPhase * 0.5f;
        triState = phase < 0.5f ? (-1.0f + 4.0f * phase) : (3.0f - 4.0f * phase);
    }

    inline float process (float inc, float shape, float pw, float& subOut) noexcept
    {
        inc = juce::jlimit (1.0e-6f, 0.45f, inc);
        // saw
        float saw = 2.0f * phase - 1.0f - polyBlep (phase, inc);
        // pulse
        float t2 = phase + (1.0f - pw); if (t2 >= 1.0f) t2 -= 1.0f;
        float pulse = (phase < pw ? 1.0f : -1.0f) + polyBlep (phase, inc) - polyBlep (t2, inc);
        // triangle via leaky integration of a 50% square
        float t3 = phase + 0.5f; if (t3 >= 1.0f) t3 -= 1.0f;
        float sq = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep (phase, inc) - polyBlep (t3, inc);
        triState = inc * 4.0f * sq + (1.0f - 0.0005f) * triState;
        float tri = juce::jlimit (-1.2f, 1.2f, triState);

        float out;
        if (shape <= 1.0f) out = tri + (saw - tri) * shape;
        else               out = saw + (pulse - saw) * (shape - 1.0f);

        subOut = std::sin (subPhase * juce::MathConstants<float>::twoPi);
        subPhase += inc * 0.5f; if (subPhase >= 1.0f) subPhase -= 1.0f;
        phase += inc;           if (phase >= 1.0f)    phase -= 1.0f;
        return out;
    }

private:
    float phase = 0.0f, triState = 0.0f, subPhase = 0.0f;
};

} // namespace ff
