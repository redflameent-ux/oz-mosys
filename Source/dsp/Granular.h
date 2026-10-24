#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>

namespace ff
{
// Audio the granular engine chews on. Either a built-in texture or a file you load.
class GrainSource : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<GrainSource>;

    juce::AudioBuffer<float> buffer;   // always 2 channels
    double sampleRate = 48000.0;
    float rootMidiNote = 60.0f;        // pitch the source is treated as (C3/C4 = 60)
    juce::String name;

    static Ptr createBuiltIn()
    {
        Ptr s = new GrainSource();
        s->name = "Built-in: Breathing Choir";
        s->sampleRate = 48000.0;
        const int len = 48000 * 6;
        s->buffer.setSize (2, len);
        juce::Random rng (77);
        const double f0 = 261.6256;   // middle C
        const double twoPi = juce::MathConstants<double>::twoPi;

        // An evolving harmonic texture: 24 partials whose loudness drifts slowly,
        // a little breath noise, and slightly different detune per channel.
        std::array<double, 24> rate {}, offs {}, ph {};
        for (int h = 0; h < 24; ++h) { rate[(size_t) h] = 0.05 + rng.nextDouble() * 0.4; offs[(size_t) h] = rng.nextDouble() * twoPi; ph[(size_t) h] = rng.nextDouble(); }
        float noiseLp[2] = { 0, 0 };
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = s->buffer.getWritePointer (ch);
            const double det = ch == 0 ? 0.9995 : 1.0005;
            for (int i = 0; i < len; ++i)
            {
                const double t = (double) i / s->sampleRate;
                double v = 0.0;
                for (int h = 0; h < 24; ++h)
                {
                    const int k = h + 1;
                    const double env = 0.5 + 0.5 * std::sin (t * twoPi * rate[(size_t) h] + offs[(size_t) h] + ch);
                    const double amp = env * env / std::pow ((double) k, 0.9);
                    v += amp * std::sin (twoPi * (f0 * k * det * t + ph[(size_t) h]));
                }
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                noiseLp[ch] += 0.08f * (n - noiseLp[ch]);
                d[i] = (float) (v * 0.22) + noiseLp[ch] * 0.15f;
            }
        }
        // fade the ends so grains at the edges don't click
        const int fade = 2400;
        for (int ch = 0; ch < 2; ++ch)
        {
            s->buffer.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
            s->buffer.applyGainRamp (ch, len - fade, fade, 1.0f, 0.0f);
        }
        s->buffer.applyGain (0.9f / juce::jmax (0.01f, s->buffer.getMagnitude (0, len)));
        return s;
    }

    // Low "throat singing" drone: harmonic series through slowly shifting vowel
    // formants, with a sub-octave growl (kargyraa style).
    static Ptr createMonkChant()
    {
        Ptr s = new GrainSource();
        s->name = "Built-in: Monk Chant";
        s->sampleRate = 48000.0;
        s->rootMidiNote = 36.0f;                 // C1-ish drone
        const int len = 48000 * 8;
        s->buffer.setSize (2, len);
        const double f0 = 65.406, sr = s->sampleRate, twoPi = juce::MathConstants<double>::twoPi;
        juce::Random rng (909);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = s->buffer.getWritePointer (ch);
            double ph = 0.0, subPh = 0.0;
            float lp = 0.0f;
            std::array<double, 41> amps {};
            for (int i = 0; i < len; ++i)
            {
                const double t = i / sr;
                const double vib = 1.0 + 0.003 * std::sin (twoPi * 4.7 * t + ch);
                // vowel morph "oo" -> "aa" -> "oh" over the loop
                if ((i & 255) == 0)
                {
                    const double m = 0.5 + 0.5 * std::sin (twoPi * t / 8.0);
                    const double F1 = 300.0 + 400.0 * m, F2 = 800.0 + 500.0 * m;
                    for (int k = 1; k <= 40; ++k)
                    {
                        const double hz = f0 * k;
                        amps[(size_t) k] = std::exp (-0.5 * std::pow ((hz - F1) / 90.0, 2))
                                         + 0.6 * std::exp (-0.5 * std::pow ((hz - F2) / 120.0, 2)) + 0.15 / k;
                    }
                }
                double v = 0.0;
                for (int k = 1; k <= 40; ++k)
                    v += amps[(size_t) k] * std::sin (twoPi * k * ph + k * 0.7);
                const double growl = 0.35 * std::sin (twoPi * subPh) * (0.6 + 0.4 * std::sin (twoPi * 0.31 * t));
                ph += f0 * vib / sr;   ph -= std::floor (ph);
                subPh += f0 * 0.5 * vib / sr; subPh -= std::floor (subPh);
                lp += 0.05f * ((rng.nextFloat() * 2.0f - 1.0f) - lp);
                d[i] = (float) (v * 0.15 + growl) + lp * 0.05f;
            }
        }
        finish (*s);
        return s;
    }

    // Breath & wind: noise through slowly wandering resonant band-passes.
    static Ptr createBreath()
    {
        Ptr s = new GrainSource();
        s->name = "Built-in: Breath & Wind";
        s->sampleRate = 48000.0;
        const int len = 48000 * 8;
        s->buffer.setSize (2, len);
        juce::Random rng (31337);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = s->buffer.getWritePointer (ch);
            float ic1 = 0, ic2 = 0, ic3 = 0, ic4 = 0;
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) i / 48000.0f;
                const float gust = 0.55f + 0.45f * std::sin (t * 0.9f + ch) * std::sin (t * 0.37f + 1.0f);
                const float fc1 = 400.0f + 900.0f * (0.5f + 0.5f * std::sin (t * 0.5f + ch));
                const float fc2 = 1800.0f + 1500.0f * (0.5f + 0.5f * std::sin (t * 0.23f + 2.0f));
                auto svf = [] (float x, float fc, float q, float& a, float& b)
                {
                    const float g = std::tan (juce::MathConstants<float>::pi * fc / 48000.0f);
                    const float k = 1.0f / q, a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1;
                    const float v3 = x - b, v1 = a1 * a + a2 * v3, v2 = b + a2 * a + g * a2 * v3;
                    a = 2.0f * v1 - a; b = 2.0f * v2 - b;
                    return v1;
                };
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                d[i] = gust * (svf (n, fc1, 4.0f, ic1, ic2) + 0.5f * svf (n, fc2, 6.0f, ic3, ic4));
            }
        }
        finish (*s);
        return s;
    }

    // Rain & water: a soft noise bed with random droplet "plinks".
    static Ptr createRain()
    {
        Ptr s = new GrainSource();
        s->name = "Built-in: Rain & Water";
        s->sampleRate = 48000.0;
        const int len = 48000 * 8;
        s->buffer.setSize (2, len);
        juce::Random rng (2024);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = s->buffer.getWritePointer (ch);
            float lp = 0.0f, hp = 0.0f;
            for (int i = 0; i < len; ++i)
            {
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                lp += 0.3f * (n - lp);
                hp = lp - hp * 0.0f;
                d[i] = hp * 0.12f;
            }
            // droplets: short rising sine chirps
            const int drops = 900;
            for (int k = 0; k < drops; ++k)
            {
                const int start = rng.nextInt (len - 4000);
                const float f = 900.0f + rng.nextFloat() * 3500.0f;
                const float amp = 0.05f + rng.nextFloat() * 0.25f;
                const int dl = 400 + rng.nextInt (1600);
                double ph = 0.0;
                for (int j = 0; j < dl; ++j)
                {
                    const float env = std::exp (-6.0f * (float) j / dl);
                    ph += (f * (1.0f + 0.8f * (float) j / dl)) / 48000.0;
                    d[start + j] += amp * env * (float) std::sin (ph * juce::MathConstants<double>::twoPi);
                }
            }
        }
        finish (*s);
        return s;
    }

    static void finish (GrainSource& s)
    {
        const int len = s.buffer.getNumSamples();
        const int fade = 2400;
        for (int ch = 0; ch < 2; ++ch)
        {
            s.buffer.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
            s.buffer.applyGainRamp (ch, len - fade, fade, 1.0f, 0.0f);
        }
        const float mag = s.buffer.getMagnitude (0, len);
        if (mag > 1.0e-4f) s.buffer.applyGain (0.9f / mag);
    }

    static Ptr fromFile (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples < 64)
            return nullptr;

        Ptr s = new GrainSource();
        s->name = file.getFileName();
        s->sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (s->sampleRate * 90.0));
        s->buffer.setSize (2, len);
        reader->read (&s->buffer, 0, len, 0, true, reader->numChannels > 1);
        if (reader->numChannels == 1)
            s->buffer.copyFrom (1, 0, s->buffer, 0, 0, len);
        const float mag = s->buffer.getMagnitude (0, len);
        if (mag > 0.0001f) s->buffer.applyGain (0.9f / mag);
        return s;
    }
};

// One voice's cloud of grains.
class GrainCloud
{
public:
    static constexpr int maxGrains = 48;

    struct Settings
    {
        float position = 0.3f, sizeMs = 100.0f, density = 20.0f, spray = 0.1f;
        float pitchRandSemis = 0.0f, stereo = 0.5f, reverseChance = 0.0f;
        double rate = 1.0;   // playback speed for the note (already includes source/host rate)
    };

    void prepare (double sr) { hostRate = sr; }
    void reset (juce::Random& r)
    {
        for (auto& g : grains) g.active = false;
        countdown = r.nextInt (64);
    }

    void render (const GrainSource& src, const Settings& s, float* L, float* R, int n, juce::Random& rng) noexcept
    {
        const int len = src.buffer.getNumSamples();
        if (len < 2) return;
        const float* dL = src.buffer.getReadPointer (0);
        const float* dR = src.buffer.getReadPointer (1);

        const float sizeSamples = juce::jmax (32.0f, s.sizeMs * 0.001f * (float) hostRate);
        const float interval = (float) hostRate / juce::jmax (0.5f, s.density);
        const float overlap = juce::jmax (1.0f, s.density * s.sizeMs * 0.001f);
        const float norm = 1.0f / std::sqrt (overlap);

        for (int i = 0; i < n; ++i)
        {
            if (--countdown <= 0)
            {
                spawn (s, len, sizeSamples, rng);
                countdown = (int) juce::jmax (1.0f, interval * (0.75f + 0.5f * rng.nextFloat()));
            }

            float outL = 0.0f, outR = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float w = window (g.age * g.invLen);
                double p = g.pos;
                while (p < 0.0) p += len;
                while (p >= len) p -= len;
                const int i0 = (int) p;
                const int i1 = (i0 + 1) % len;
                const float t = (float) (p - i0);
                const float sl = dL[i0] + t * (dL[i1] - dL[i0]);
                const float sr = dR[i0] + t * (dR[i1] - dR[i0]);
                outL += (sl * g.gainL) * w;
                outR += (sr * g.gainR) * w;
                g.pos += g.step;
                g.age += 1.0f;
                if (g.age >= g.length) g.active = false;
            }
            L[i] += outL * norm;
            R[i] += outR * norm;
        }
    }

private:
    struct Grain { bool active = false; double pos = 0, step = 1; float age = 0, length = 1, invLen = 1, gainL = 1, gainR = 1; };

    static float window (float x) noexcept
    {
        // Hann window
        const float s = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, x));
        return s * s;
    }

    void spawn (const Settings& s, int len, float sizeSamples, juce::Random& rng)
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            float p = s.position + (rng.nextFloat() - 0.5f) * s.spray;
            p -= std::floor (p);
            g.pos = (double) p * (len - 1);
            const float semis = s.pitchRandSemis * (rng.nextFloat() * 2.0f - 1.0f);
            g.step = s.rate * std::pow (2.0, semis / 12.0);
            if (rng.nextFloat() < s.reverseChance) g.step = -g.step;
            g.length = sizeSamples;
            g.invLen = 1.0f / sizeSamples;
            g.age = 0.0f;
            const float pan = 0.5f + (rng.nextFloat() - 0.5f) * s.stereo;
            g.gainL = std::cos (pan * juce::MathConstants<float>::halfPi) * 1.414f;
            g.gainR = std::sin (pan * juce::MathConstants<float>::halfPi) * 1.414f;
            g.active = true;
            return;
        }
    }

    std::array<Grain, maxGrains> grains {};
    double hostRate = 48000.0;
    int countdown = 0;
};

} // namespace ff
