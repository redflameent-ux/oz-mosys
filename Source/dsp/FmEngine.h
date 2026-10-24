#pragma once
// =====================================================================================
//  FM engine (in the spirit of FM8): six operators, any operator can modulate any other
//  (and itself) through a 6x6 matrix, each with its own ratio, waveform and envelope.
//  Modulation uses the previous sample of every operator, so every routing is possible.
// =====================================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include "DspCore.h"

namespace ff
{
constexpr int numFmOps = 6;

inline const juce::StringArray& fmWaveNames()
{
    static const juce::StringArray n { "Sine", "Half Sine", "Abs Sine", "Triangle", "Saw", "Square", "Pulse 25", "Sine x2" };
    return n;
}

// single-cycle tables (few harmonics, so FM stays clean)
class FmWaves
{
public:
    static constexpr int size = 2048;
    static const FmWaves& get() { static FmWaves w; return w; }
    inline float read (int wave, float phase01) const noexcept
    {
        const float idx = phase01 * (float) size;
        const int i0 = ((int) idx) & (size - 1);
        const float t = idx - std::floor (idx);
        const float* tb = tables[(size_t) juce::jlimit (0, numWaves - 1, wave)].data();
        return tb[i0] + t * (tb[(i0 + 1) & (size - 1)] - tb[i0]);
    }
    static constexpr int numWaves = 8;
private:
    FmWaves()
    {
        const double tp = juce::MathConstants<double>::twoPi;
        for (int w = 0; w < numWaves; ++w)
        {
            auto& tb = tables[(size_t) w];
            for (int i = 0; i < size; ++i)
            {
                const double p = (double) i / size;
                double v = 0;
                switch (w)
                {
                    case 0: v = std::sin (tp * p); break;
                    case 1: v = p < 0.5 ? std::sin (tp * p) * 2.0 - 0.6366 : -0.6366; break;          // half sine, DC removed
                    case 2: v = std::abs (std::sin (tp * p)) * 2.0 - 1.2732; break;                   // abs sine
                    case 3: for (int k = 1; k <= 15; k += 2) v += (((k - 1) / 2) % 2 ? -1.0 : 1.0) * std::sin (tp * k * p) / (k * k); v *= 0.81; break;
                    case 4: for (int k = 1; k <= 16; ++k) v += std::sin (tp * k * p) / k; v *= 0.55; break;
                    case 5: for (int k = 1; k <= 15; k += 2) v += std::sin (tp * k * p) / k; v *= 1.0; break;
                    case 6: for (int k = 1; k <= 16; ++k) v += std::sin (juce::MathConstants<double>::pi * k * 0.25) * std::cos (tp * k * p) / k; v *= 0.9; break;
                    default: v = std::sin (tp * p) * 0.6 + std::sin (2.0 * tp * p) * 0.6; break;
                }
                tb[(size_t) i] = (float) v;
            }
            // normalise peak to 1
            float pk = 0.0001f;
            for (auto x : tb) pk = juce::jmax (pk, std::abs (x));
            for (auto& x : tb) x /= pk;
        }
    }
    std::array<std::array<float, size>, numWaves> tables {};
};

class FmVoice
{
public:
    struct OpSettings { float ratio = 1.0f, fineCents = 0.0f, level = 0.0f, velSens = 0.5f; int wave = 0; float a = 0.005f, d = 0.5f, s = 0.8f, r = 0.4f; };
    struct Settings
    {
        std::array<OpSettings, numFmOps> op {};
        float matrix[numFmOps][numFmOps] {};   // matrix[from][to], 0..1 (diagonal = feedback)
        float index = 1.0f;                    // overall modulation depth multiplier
    };

    void prepare (double sr) { sampleRate = sr; for (auto& e : env) e.setSampleRate (sr); }

    void noteOn (const Settings& s, float velocity, juce::Random& rng)
    {
        vel = velocity;
        for (int i = 0; i < numFmOps; ++i)
        {
            phase[(size_t) i] = 0.0f;
            prev[(size_t) i] = prev2[(size_t) i] = 0.0f;
            env[(size_t) i].reset();
            const auto& o = s.op[(size_t) i];
            env[(size_t) i].set (o.a, o.d, o.s, o.r);
            env[(size_t) i].noteOn();
        }
        juce::ignoreUnused (rng);
    }
    void noteOff() { for (auto& e : env) e.noteOff(); }

    // adds the carrier mix into out[], baseHz = note frequency
    void process (const Settings& s, float baseHz, float* out, int n) noexcept
    {
        const auto& waves = FmWaves::get();
        float inc[numFmOps], outLevel[numFmOps], velGain[numFmOps];
        int wave[numFmOps];
        constexpr float maxIndexCycles = 1.6f;   // 0..1 in the matrix = up to ~10 radians of modulation
        float m[numFmOps][numFmOps];
        for (int i = 0; i < numFmOps; ++i)
        {
            const auto& o = s.op[(size_t) i];
            env[(size_t) i].set (o.a, o.d, o.s, o.r);
            inc[i] = juce::jlimit (0.0f, 0.49f, (float) (baseHz * o.ratio * std::exp2 (o.fineCents / 1200.0f) / sampleRate));
            outLevel[i] = o.level;
            velGain[i] = 1.0f - o.velSens * (1.0f - vel);
            wave[i] = o.wave;
            for (int j = 0; j < numFmOps; ++j)
            {
                const float a = s.matrix[i][j];
                m[i][j] = a * a * maxIndexCycles * s.index;   // squared curve: fine control at low amounts
            }
        }

        for (int k = 0; k < n; ++k)
        {
            float cur[numFmOps];
            for (int j = 0; j < numFmOps; ++j)
            {
                float pm = 0.0f;
                for (int i = 0; i < numFmOps; ++i)
                {
                    // self-feedback averages two samples (DX-style) to stay smooth
                    const float src = i == j ? 0.5f * (prev[(size_t) i] + prev2[(size_t) i]) : prev[(size_t) i];
                    pm += m[i][j] * src;
                }
                float ph = phase[(size_t) j] + pm;
                ph -= std::floor (ph);
                const float e = env[(size_t) j].next();
                cur[j] = waves.read (wave[j], ph) * e * velGain[j];
                phase[(size_t) j] += inc[j];
                if (phase[(size_t) j] >= 1.0f) phase[(size_t) j] -= 1.0f;
            }
            float mix = 0.0f;
            for (int j = 0; j < numFmOps; ++j)
            {
                prev2[(size_t) j] = prev[(size_t) j];
                prev[(size_t) j] = cur[j];
                mix += cur[j] * outLevel[j];
            }
            out[k] += mix;
        }
    }

    bool isActive() const { for (auto& e : env) if (e.isActive()) return true; return false; }

private:
    double sampleRate = 48000.0;
    float vel = 1.0f;
    std::array<float, numFmOps> phase {}, prev {}, prev2 {};
    std::array<Envelope, numFmOps> env;
};

// Starting-point routings (FM8 / DX style). Each entry: list of {from, to, amount}, plus which operators are heard.
struct FmAlgorithm { const char* name; std::vector<std::array<float, 3>> links; std::array<float, numFmOps> out; };
inline const std::vector<FmAlgorithm>& fmAlgorithms()
{
    static const std::vector<FmAlgorithm> a = {
        { "Single pair (B > A)",         { { 1, 0, 0.5f } },                                            { 1, 0, 0, 0, 0, 0 } },
        { "Stack of 3 (C > B > A)",      { { 2, 1, 0.5f }, { 1, 0, 0.5f } },                            { 1, 0, 0, 0, 0, 0 } },
        { "Stack of 6",                  { { 5, 4, 0.4f }, { 4, 3, 0.4f }, { 3, 2, 0.4f }, { 2, 1, 0.4f }, { 1, 0, 0.5f } }, { 1, 0, 0, 0, 0, 0 } },
        { "Two pairs (E-piano)",         { { 1, 0, 0.45f }, { 3, 2, 0.6f } },                           { 1, 0, 0.6f, 0, 0, 0 } },
        { "Three pairs",                 { { 1, 0, 0.5f }, { 3, 2, 0.5f }, { 5, 4, 0.5f } },            { 1, 0, 0.8f, 0, 0.6f, 0 } },
        { "One modulator, 3 carriers",   { { 3, 0, 0.45f }, { 3, 1, 0.45f }, { 3, 2, 0.45f } },         { 0.8f, 0.8f, 0.8f, 0, 0, 0 } },
        { "Three into one (brass)",      { { 1, 0, 0.4f }, { 2, 0, 0.35f }, { 3, 0, 0.3f } },           { 1, 0, 0, 0, 0, 0 } },
        { "Feedback bass",               { { 1, 1, 0.45f }, { 1, 0, 0.55f } },                          { 1, 0, 0, 0, 0, 0 } },
        { "Organ (6 carriers)",          {},                                                             { 0.8f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f } },
        { "Bell tree",                   { { 5, 4, 0.5f }, { 4, 0, 0.4f }, { 3, 2, 0.6f }, { 2, 0, 0.3f }, { 1, 0, 0.4f } }, { 1, 0, 0, 0, 0, 0 } },
    };
    return a;
}
} // namespace ff
