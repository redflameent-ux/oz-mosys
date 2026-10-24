#include "Wavetable.h"

namespace ff
{
using Spectrum = std::vector<std::complex<float>>;
static constexpr int numBins = WavetableBank::frameSize / 2;   // harmonics 1..1023 usable

WavetableBank::Ptr WavetableBank::fromSpectra (const std::vector<Spectrum>& spectra, const juce::String& tableName)
{
    Ptr bank = new WavetableBank();
    bank->name = tableName;
    bank->numFrames = juce::jlimit (1, maxFrames, (int) spectra.size());
    bank->data.assign ((size_t) bank->numFrames * numLevels * frameSize, 0.0f);

    juce::dsp::FFT fft (11);
    std::vector<float> work ((size_t) frameSize * 2);

    for (int f = 0; f < bank->numFrames; ++f)
    {
        const auto& spec = spectra[(size_t) f];
        int maxHarm = 1024;
        for (int level = 0; level < numLevels; ++level, maxHarm >>= 1)
        {
            std::fill (work.begin(), work.end(), 0.0f);
            const int limit = juce::jmin (maxHarm, numBins - 1, (int) spec.size() - 1);
            for (int k = 1; k <= limit; ++k)
            {
                work[(size_t) (2 * k)]     = spec[(size_t) k].real();
                work[(size_t) (2 * k + 1)] = spec[(size_t) k].imag();
            }
            fft.performRealOnlyInverseTransform (work.data());
            float* dest = bank->data.data() + ((size_t) f * numLevels + (size_t) level) * frameSize;
            std::copy (work.begin(), work.begin() + frameSize, dest);
        }
    }

    // Normalise each frame so sweeping the position keeps a steady loudness.
    for (int f = 0; f < bank->numFrames; ++f)
    {
        float peak = 0.0f;
        const float* p0 = bank->frame (f, 0);
        for (int i = 0; i < frameSize; ++i) peak = juce::jmax (peak, std::abs (p0[i]));
        if (peak < 1.0e-9f) continue;
        const float g = 1.0f / peak;
        float* d = bank->data.data() + (size_t) f * numLevels * frameSize;
        for (int i = 0; i < numLevels * frameSize; ++i) d[i] *= g;
    }
    return bank;
}

WavetableBank::Ptr WavetableBank::fromTimeFrames (const std::vector<std::vector<float>>& frames, const juce::String& tableName)
{
    juce::dsp::FFT fft (11);
    std::vector<float> work ((size_t) frameSize * 2);
    std::vector<Spectrum> spectra;

    for (auto& fr : frames)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        // resample the incoming frame to exactly frameSize samples
        const int n = (int) fr.size();
        for (int i = 0; i < frameSize; ++i)
        {
            const double src = (double) i * n / frameSize;
            const int i0 = (int) src;
            const int i1 = (i0 + 1) % juce::jmax (1, n);
            const float t = (float) (src - i0);
            work[(size_t) i] = n > 0 ? fr[(size_t) i0] + t * (fr[(size_t) i1] - fr[(size_t) i0]) : 0.0f;
        }
        fft.performRealOnlyForwardTransform (work.data(), true);
        Spectrum s ((size_t) numBins);
        for (int k = 1; k < numBins; ++k)
            s[(size_t) k] = { work[(size_t) (2 * k)], work[(size_t) (2 * k + 1)] };
        spectra.push_back (std::move (s));
    }
    return fromSpectra (spectra, tableName);
}

WavetableBank::Ptr WavetableBank::fromAudioFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 16)
        return nullptr;

    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) frameSize * 256);
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);

    // mix to mono
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] += buf.getSample (ch, i) / (float) buf.getNumChannels();

    std::vector<std::vector<float>> frames;
    if (len < frameSize)
    {
        frames.push_back (mono);                       // single cycle, any length
    }
    else
    {
        const int total = len / frameSize;             // Serum-style 2048-sample frames
        const int use = juce::jmin (total, maxFrames);
        for (int f = 0; f < use; ++f)
        {
            const int srcFrame = (use == total) ? f : (int) std::round ((double) f * (total - 1) / juce::jmax (1, use - 1));
            frames.emplace_back (mono.begin() + srcFrame * frameSize, mono.begin() + (srcFrame + 1) * frameSize);
        }
    }
    return fromTimeFrames (frames, file.getFileNameWithoutExtension());
}

// ---------------------------------------------------------------------------
// Built-in tables, designed from scratch as harmonic spectra.
// ---------------------------------------------------------------------------
static Spectrum sineSeries (const std::function<float (int)>& amp, bool spreadPhases = false)
{
    // spreadPhases uses Schroeder phases: same timbre, far less peaky waveform (louder, smoother)
    Spectrum s ((size_t) numBins);
    for (int k = 1; k < numBins; ++k)
    {
        const float a = amp (k);
        if (spreadPhases) s[(size_t) k] = std::polar (a, juce::MathConstants<float>::pi * (float) (k * k) / 64.0f);
        else              s[(size_t) k] = { 0.0f, -a };
    }
    return s;
}

static Spectrum lerp (const Spectrum& a, const Spectrum& b, float t)
{
    Spectrum r (a.size());
    for (size_t i = 0; i < a.size(); ++i) r[i] = a[i] * (1.0f - t) + b[i] * t;
    return r;
}

static float gauss (float x, float c, float w) { const float d = (x - c) / w; return std::exp (-0.5f * d * d); }

WavetableBank::Ptr WavetableBank::createBuiltIn (int index)
{
    constexpr int frames = 16;
    std::vector<Spectrum> spectra;
    const float pi = juce::MathConstants<float>::pi;

    switch (index)
    {
        default:
        case 0: // Basic Shapes: sine -> triangle -> saw -> square
        {
            const Spectrum shapes[4] = {
                sineSeries ([] (int k) { return k == 1 ? 1.0f : 0.0f; }),
                sineSeries ([] (int k) { return (k % 2) ? ((((k - 1) / 2) % 2) ? -1.0f : 1.0f) / (float) (k * k) : 0.0f; }),
                sineSeries ([] (int k) { return 1.0f / (float) k; }),
                sineSeries ([] (int k) { return (k % 2) ? 1.0f / (float) k : 0.0f; })
            };
            for (int f = 0; f < frames; ++f)
            {
                const float t = (float) f / (frames - 1) * 3.0f;
                const int seg = juce::jmin (2, (int) t);
                spectra.push_back (lerp (shapes[seg], shapes[seg + 1], t - (float) seg));
            }
            return fromSpectra (spectra, "Basic Shapes");
        }
        case 1: // Harmonic Sweep: a resonant bump travelling up the harmonics
        {
            for (int f = 0; f < frames; ++f)
            {
                const float centre = 1.0f + (float) f * 3.5f;
                const float width = 1.5f + (float) f * 0.6f;
                spectra.push_back (sineSeries ([=] (int k)
                {
                    return 0.35f / (float) k + gauss ((float) k, centre, width) * (k == 1 ? 1.0f : 0.8f);
                }, true));
            }
            return fromSpectra (spectra, "Harmonic Sweep");
        }
        case 2: // Vocal formants: A - E - I - O - U
        {
            const float formants[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 },
                                           { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float f0 = 110.0f;
            for (int f = 0; f < frames; ++f)
            {
                const float t = (float) f / (frames - 1) * 4.0f;
                const int seg = juce::jmin (3, (int) t);
                const float u = t - (float) seg;
                float F[3];
                for (int j = 0; j < 3; ++j) F[j] = formants[seg][j] * (1.0f - u) + formants[seg + 1][j] * u;
                spectra.push_back (sineSeries ([=] (int k)
                {
                    const float hz = (float) k * f0;
                    float a = gauss (hz, F[0], 90.0f) + 0.7f * gauss (hz, F[1], 120.0f) + 0.35f * gauss (hz, F[2], 160.0f);
                    return (a + 0.02f) / std::sqrt ((float) k);
                }, true));
            }
            return fromSpectra (spectra, "Vocal Formants");
        }
        case 3: // Digital Grit: sparse, quantised random harmonics getting denser
        {
            juce::Random rng (4242);
            for (int f = 0; f < frames; ++f)
            {
                const float density = 0.1f + 0.8f * (float) f / (frames - 1);
                Spectrum s ((size_t) numBins);
                for (int k = 1; k < 256; ++k)
                {
                    if (k == 1 || rng.nextFloat() < density * (1.0f - (float) k / 300.0f))
                    {
                        const float amp = std::round (rng.nextFloat() * 4.0f) / 4.0f / std::pow ((float) k, 0.6f);
                        const float ph = rng.nextFloat() * 2.0f * pi;
                        s[(size_t) k] = std::polar (k == 1 ? 1.0f : amp, ph);
                    }
                }
                spectra.push_back (s);
            }
            return fromSpectra (spectra, "Digital Grit");
        }
        case 4: // Pulse width: 50% down to 3%
        {
            for (int f = 0; f < frames; ++f)
            {
                const float w = 0.5f - 0.47f * (float) f / (frames - 1);
                Spectrum s ((size_t) numBins);
                for (int k = 1; k < numBins; ++k)
                    s[(size_t) k] = { std::sin (pi * (float) k * w) / (float) k, 0.0f };
                spectra.push_back (s);
            }
            return fromSpectra (spectra, "Pulse Width");
        }
        case 5: // Dark Organ: drawbar-style registrations getting brighter
        {
            const int bars[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            for (int f = 0; f < frames; ++f)
            {
                const float bright = (float) f / (frames - 1);
                Spectrum s ((size_t) numBins);
                for (int b = 0; b < 9; ++b)
                {
                    const float pull = juce::jlimit (0.0f, 1.0f, bright * 9.0f - (float) b + 1.5f);
                    s[(size_t) bars[b]] += std::complex<float> (0.0f, -pull * (b == 0 ? 1.0f : 0.6f));
                }
                spectra.push_back (s);
            }
            return fromSpectra (spectra, "Dark Organ");
        }
    }
}

} // namespace ff
