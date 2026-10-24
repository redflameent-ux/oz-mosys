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
