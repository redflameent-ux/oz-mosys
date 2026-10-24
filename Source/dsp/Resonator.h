#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace ff
{
// ---------------------------------------------------------------------------
// Resonator engine: physical-modelling voices.
//   * Plucked String / Sitar - Karplus-Strong string (+ jawari buzz and
//     sympathetic strings for the sitar)
//   * Everything else - a bank of tuned "modes" (ringing filters) whose
//     frequency ratios come from how the real object vibrates.
// Exciters: Strike (pluck / mallet / hand) and Bow (continuous rub or breath).
// ---------------------------------------------------------------------------
inline const juce::StringArray& resonatorTypeNames()
{
    static const juce::StringArray names { "Plucked String", "Sitar", "Tibetan Bowl", "Crystal Bowl", "Gong",
                                           "Handpan", "Tongue Drum", "Tingsha / Bell", "Music Box", "Tabla",
                                           "Hand Drum", "Clay Pot", "808 Cowbell", "Scrap Metal", "Anvil", "Metal Pipe",
                                           "Vibraphone", "Marimba", "E-Piano" };
    return names;
}

struct Mode { float ratio, gain, decay; };

inline const std::vector<Mode>& modesFor (int type)
{
    static const std::vector<Mode> table[] = {
        {}, {},   // strings use the waveguide
        // Tibetan bowl (metal): strong inharmonic overtones
        { { 1.0f, 1.0f, 1.0f }, { 2.77f, 0.6f, 0.7f }, { 5.15f, 0.35f, 0.5f }, { 8.1f, 0.2f, 0.35f }, { 11.7f, 0.12f, 0.25f }, { 15.8f, 0.06f, 0.2f } },
        // Crystal bowl: almost a pure sine
        { { 1.0f, 1.0f, 1.0f }, { 2.76f, 0.1f, 0.6f }, { 5.4f, 0.04f, 0.4f } },
        // Gong: dense cloud of partials
        { { 1.0f, 1.0f, 1.0f }, { 1.47f, 0.7f, 0.9f }, { 1.94f, 0.6f, 0.85f }, { 2.36f, 0.5f, 0.8f }, { 2.82f, 0.45f, 0.75f },
          { 3.4f, 0.4f, 0.7f }, { 3.95f, 0.3f, 0.6f }, { 4.6f, 0.25f, 0.55f }, { 5.3f, 0.2f, 0.5f }, { 6.1f, 0.15f, 0.45f },
          { 7.0f, 0.1f, 0.4f }, { 8.2f, 0.08f, 0.35f } },
        // Handpan: tuned fundamental, octave and compound fifth
        { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.5f, 0.8f }, { 3.0f, 0.3f, 0.6f }, { 2.42f, 0.08f, 0.4f }, { 4.0f, 0.1f, 0.4f }, { 5.1f, 0.05f, 0.3f } },
        // Steel tongue drum: soft, mostly fundamental
        { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.15f, 0.6f }, { 6.27f, 0.18f, 0.3f }, { 17.5f, 0.04f, 0.15f } },
        // Tingsha / temple bell: bright and piercing
        { { 1.0f, 1.0f, 1.0f }, { 2.32f, 0.7f, 0.8f }, { 4.25f, 0.5f, 0.6f }, { 6.63f, 0.35f, 0.5f }, { 9.38f, 0.25f, 0.4f }, { 12.4f, 0.15f, 0.3f } },
        // Music box tine
        { { 1.0f, 1.0f, 1.0f }, { 6.27f, 0.25f, 0.25f }, { 17.55f, 0.08f, 0.1f } },
        // Tabla (dayan): the black spot makes it nearly harmonic
        { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.8f, 0.8f }, { 3.0f, 0.6f, 0.6f }, { 4.0f, 0.4f, 0.5f }, { 5.0f, 0.25f, 0.4f }, { 2.7f, 0.1f, 0.2f } },
        // Hand drum (djembe / darbuka): circular membrane
        { { 1.0f, 1.0f, 1.0f }, { 1.59f, 0.8f, 0.8f }, { 2.14f, 0.6f, 0.7f }, { 2.30f, 0.5f, 0.6f }, { 2.65f, 0.45f, 0.55f },
          { 2.92f, 0.4f, 0.5f }, { 3.16f, 0.3f, 0.45f }, { 3.50f, 0.25f, 0.4f }, { 4.06f, 0.15f, 0.3f } },
        // Clay pot (udu / ghatam): hollow "bloop"
        { { 1.0f, 1.0f, 1.0f }, { 2.3f, 0.25f, 0.3f }, { 3.9f, 0.12f, 0.2f } },
        // 808 cowbell: two square waves (ratio 1 : 1.48) and their odd harmonics
        { { 1.0f, 1.0f, 1.0f }, { 1.48f, 0.9f, 1.0f }, { 3.0f, 0.33f, 0.8f }, { 4.44f, 0.3f, 0.8f }, { 5.0f, 0.2f, 0.7f }, { 7.4f, 0.18f, 0.7f } },
        // Scrap metal sheet: dense, clashing plate modes
        { { 1.0f, 1.0f, 1.0f }, { 1.62f, 0.9f, 0.9f }, { 2.24f, 0.8f, 0.85f }, { 2.82f, 0.75f, 0.8f }, { 3.48f, 0.7f, 0.75f },
          { 4.12f, 0.6f, 0.7f }, { 4.71f, 0.55f, 0.65f }, { 5.39f, 0.5f, 0.6f }, { 6.02f, 0.45f, 0.55f }, { 6.76f, 0.4f, 0.5f },
          { 7.53f, 0.35f, 0.45f }, { 8.39f, 0.3f, 0.4f } },
        // Anvil: hard steel block, bright short ring
        { { 1.0f, 1.0f, 1.0f }, { 2.76f, 0.9f, 0.8f }, { 5.40f, 0.8f, 0.6f }, { 8.93f, 0.6f, 0.45f }, { 13.34f, 0.4f, 0.35f } },
        // Metal pipe: long clang
        { { 1.0f, 1.0f, 1.0f }, { 2.76f, 0.7f, 0.9f }, { 5.40f, 0.5f, 0.8f }, { 8.93f, 0.35f, 0.7f }, { 13.34f, 0.2f, 0.6f } },
        // Vibraphone: aluminium bar tuned 1 : 4 : 10
        { { 1.0f, 1.0f, 1.0f }, { 4.0f, 0.35f, 0.5f }, { 10.0f, 0.12f, 0.3f } },
        // Marimba: wooden bar tuned 1 : 4 : 9.2, short
        { { 1.0f, 1.0f, 1.0f }, { 3.93f, 0.4f, 0.4f }, { 9.2f, 0.12f, 0.2f } },
        // E-Piano (tine + pickup): mostly harmonic with a bell-like tine partial
        { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.45f, 0.6f }, { 3.0f, 0.18f, 0.4f }, { 4.0f, 0.08f, 0.35f }, { 7.1f, 0.12f, 0.15f } },
    };
    return table[juce::jlimit (0, 18, type)];
}

inline bool typeBeats (int type) { return type == 2 || type == 3 || type == 4 || type == 7 || type == 15; }   // metal: slow "wah-wah" beating

class Resonator
{
public:
    struct Settings
    {
        int type = 0;
        float decay = 2.0f;      // seconds (T60 of the lowest mode)
        float bright = 0.6f;     // 0..1
        float damp = 0.5f;       // how much faster high modes die
        float strike = 0.6f;     // exciter hardness
        float bow = 0.0f;        // continuous excitation (rub / bow / breath)
        float buzz = 0.0f;       // sitar jawari / extra grit
        float retrigHz = 0.0f;   // auto re-pluck rate (0 = off)
    };

    void prepare (double newSr)
    {
        sr = (float) newSr;
        delay.assign ((size_t) (sr / 15.0f) + 8, 0.0f);
    }

    void noteOn (juce::Random& rng)
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        writePos = 0; lpState = 0.0f; dcX = dcY = 0.0f;
        for (auto& m : res) m = {};
        for (auto& s : symp) s = {};
        retrigCount = 0.0f;
        triggerStrike (rng);
    }

    void triggerStrike (juce::Random& rng)
    {
        burstLen = 0;     // set on first process call when we know the frequency
        burstPos = 0;
        burstPending = true;
        burstSeed = rng.nextInt();
    }

    // Renders n samples (mono) into out (added).
    void process (const Settings& s, float freq, float* out, int n, juce::Random& rng) noexcept
    {
        freq = juce::jlimit (8.0f, sr * 0.45f, freq);
        if (s.retrigHz > 0.01f)
        {
            retrigCount += (float) n * s.retrigHz / sr;
            if (retrigCount >= 1.0f) { retrigCount -= std::floor (retrigCount); triggerStrike (rng); }
        }

        if (burstPending)
        {
            // short burst; harder strikes = shorter + brighter
            const float ms = 0.15f + (1.0f - s.strike) * 6.0f;
            burstLen = juce::jmax (2, (int) (ms * 0.001f * sr));
            if (s.type <= 1) burstLen = juce::jmax (2, (int) (sr / freq));   // classic pluck: fill one period
            stringBurst = s.type <= 1;
            burstPos = 0;
            burstPending = false;
            burstRng.setSeed (burstSeed);
        }
        const float exLp = 0.05f + 0.93f * s.strike;   // exciter brightness

        if (type != s.type) { type = s.type; for (auto& m : res) m = {}; }

        outGain = s.type <= 1 ? 1.2f : 0.12f;   // matched to the other engines
        if (s.type <= 1) processString (s, freq, out, n, rng, exLp);
        else             processModal  (s, freq, out, n, rng, exLp);
    }

private:
    inline float exciterSample (const Settings& s, juce::Random& rng, float exLp, float& bowOut) noexcept
    {
        float x = 0.0f;
        if (burstPos < burstLen)
        {
            const float w = stringBurst ? 1.0f : std::sin (juce::MathConstants<float>::pi * (float) burstPos / (float) burstLen);
            x = (burstRng.nextFloat() * 2.0f - 1.0f) * w + (burstPos == 0 ? 1.0f : 0.0f) * s.strike;
            ++burstPos;
        }
        exState += exLp * (x - exState);
        bowOut = 0.0f;
        if (s.bow > 0.0001f)
        {
            bowState += (0.02f + 0.3f * s.bright) * ((rng.nextFloat() * 2.0f - 1.0f) - bowState);
            bowOut = bowState * s.bow;
        }
        return exState;
    }

    void processString (const Settings& s, float freq, float* out, int n, juce::Random& rng, float exLp) noexcept
    {
        // loop filter: one-pole low-pass; its delay is compensated so it stays in tune
        const float a = 0.25f + 0.74f * s.bright;
        const float period = sr / freq - (1.0f - a) / a;
        const float loopGain = std::pow (10.0f, -3.0f / (juce::jmax (0.02f, s.decay) * freq));
        const int len = (int) delay.size();
        const float buzz = s.type == 1 ? juce::jmax (0.35f, s.buzz) : s.buzz;

        // sitar sympathetic strings (tuned to octave, fifth and fourth)
        const bool sitar = s.type == 1;
        if (sitar)
        {
            const float ratios[3] = { 2.0f, 1.5f, 1.3348f };
            for (int i = 0; i < 3; ++i) setResonator (symp[(size_t) i], freq * ratios[i], s.decay * 1.6f);
        }

        for (int i = 0; i < n; ++i)
        {
            float bow = 0.0f;
            const float ex = exciterSample (s, rng, exLp, bow);

            float rp = (float) writePos - period;
            while (rp < 0) rp += (float) len;
            const int r0 = (int) rp;
            const int r1 = (r0 + 1) % len;
            const float t = rp - (float) r0;
            const float d = delay[(size_t) r0] + t * (delay[(size_t) r1] - delay[(size_t) r0]);

            lpState += a * (d - lpState);
            float y = lpState * loopGain;
            // jawari: the curved bridge keeps re-brightening the string as it decays.
            // (never adds energy: |f(y)| <= |y|, so the string still dies away)
            if (buzz > 0.001f) y += buzz * (std::tanh (y * 4.0f) * 0.25f - y);
            y = juce::jlimit (-2.0f, 2.0f, y + ex + bow * 0.08f);
            delay[(size_t) writePos] = y;
            writePos = (writePos + 1) % len;

            float o = y;
            if (buzz > 0.001f) o += buzz * 0.5f * (std::tanh (y * 3.0f) / 3.0f - y * 0.2f);
            if (sitar)
                for (auto& sm : symp) o += tick (sm, y * 0.02f) * 0.6f;

            // DC blocker
            dcY = o - dcX + 0.995f * dcY; dcX = o;
            out[i] += dcY * outGain;
        }
    }

    void processModal (const Settings& s, float freq, float* out, int n, juce::Random& rng, float exLp) noexcept
    {
        const auto& modes = modesFor (s.type);
        const bool beats = typeBeats (s.type);
        int count = 0;
        for (size_t m = 0; m < modes.size() && count < maxModes - 1; ++m)
        {
            const auto& md = modes[m];
            const float f = freq * md.ratio;
            if (f > sr * 0.45f) break;
            const float decay = juce::jmax (0.01f, s.decay * std::pow (md.decay, 0.3f + 2.0f * s.damp));
            const float tilt = std::pow (md.ratio, -(1.0f - s.bright) * 1.5f);
            setResonator (res[(size_t) count], f, decay);
            res[(size_t) count].gain = md.gain * tilt;
            ++count;
            if (beats)
            {
                setResonator (res[(size_t) count], f * 1.0021f, decay);
                res[(size_t) count].gain = md.gain * tilt * 0.7f;
                ++count;
            }
        }

        for (int i = 0; i < n; ++i)
        {
            float bow = 0.0f;
            const float ex = exciterSample (s, rng, exLp, bow);
            float y = 0.0f;
            for (int m = 0; m < count; ++m)
            {
                auto& r = res[(size_t) m];
                const float in = (ex + bow * r.bowScale) * r.gain;
                y += tick (r, in);
            }
            if (s.buzz > 0.001f) y += s.buzz * (std::tanh (y * 4.0f) * 0.25f - y * 0.2f);
            out[i] += y * outGain;
        }
    }

    struct Res { float a1 = 0, a2 = 0, b = 0, y1 = 0, y2 = 0, gain = 1, bowScale = 0; };

    void setResonator (Res& r, float f, float t60) const noexcept
    {
        const float w = juce::MathConstants<float>::twoPi * f / sr;
        const float rad = std::exp (-6.9078f / (t60 * sr));
        r.a1 = 2.0f * rad * std::cos (w);
        r.a2 = -rad * rad;
        r.b = std::sin (w);              // unit-amplitude impulse response
        r.bowScale = (1.0f - rad) * 60.0f / juce::jmax (0.05f, r.b);   // keeps bowed level sane at any Q
    }

    static inline float tick (Res& r, float x) noexcept
    {
        const float y = r.b * x + r.a1 * r.y1 + r.a2 * r.y2;
        r.y2 = r.y1; r.y1 = y;
        return y;
    }

    static constexpr int maxModes = 26;
    std::array<Res, maxModes> res {};
    std::array<Res, 3> symp {};
    std::vector<float> delay;
    int writePos = 0, type = -1;
    float sr = 48000.0f, lpState = 0.0f, dcX = 0.0f, dcY = 0.0f;
    float exState = 0.0f, bowState = 0.0f, retrigCount = 0.0f;
    int burstLen = 0, burstPos = 0, burstSeed = 0;
    bool burstPending = false, stringBurst = false;
public:
    float outGain = 0.1f;   // calibrated per type below
private:
    juce::Random burstRng;
};

} // namespace ff
