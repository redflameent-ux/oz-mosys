#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <functional>
#include <algorithm>
#include <vector>

namespace ff
{
// A wavetable: a stack of single-cycle frames, each stored at several
// band-limited "mip" levels so high notes don't alias.
class WavetableBank : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<WavetableBank>;

    static constexpr int frameSize = 2048;
    static constexpr int frameMask = frameSize - 1;
    static constexpr int numLevels = 11;   // 1024, 512 ... 1 harmonics
    static constexpr int maxFrames = 64;

    juce::String name;

    int getNumFrames() const noexcept { return numFrames; }

    // Spectra: one vector per frame, index k = harmonic k (0 = DC, ignored).
    static Ptr fromSpectra (const std::vector<std::vector<std::complex<float>>>& spectra, const juce::String& name);
    static Ptr fromTimeFrames (const std::vector<std::vector<float>>& frames, const juce::String& name);
    static Ptr fromAudioFile (const juce::File& file);
    static Ptr createBuiltIn (int index);

    // Pick the mip level that keeps all harmonics below Nyquist.
    static int levelForIncrement (float phaseInc) noexcept
    {
        // phaseInc = f / sr  ->  max harmonics allowed = 0.5 / phaseInc
        const float allowed = 0.5f / juce::jmax (1.0e-7f, phaseInc);
        int level = 0;
        int harm = 1024;
        while (level < numLevels - 1 && (float) harm > allowed) { harm >>= 1; ++level; }
        return level;
    }

    // Read with linear interpolation inside a frame and across frames.
    inline float read (float position01, float phase01, int level) const noexcept
    {
        const float fpos = juce::jlimit (0.0f, 1.0f, position01) * (float) (numFrames - 1);
        const int f0 = (int) fpos;
        const int f1 = juce::jmin (f0 + 1, numFrames - 1);
        const float ft = fpos - (float) f0;

        const float idx = phase01 * (float) frameSize;
        const int i0 = ((int) idx) & frameMask;
        const int i1 = (i0 + 1) & frameMask;
        const float t = idx - std::floor (idx);

        const float* a = frame (f0, level);
        const float* b = frame (f1, level);
        const float va = a[i0] + t * (a[i1] - a[i0]);
        const float vb = b[i0] + t * (b[i1] - b[i0]);
        return va + ft * (vb - va);
    }

    const float* frame (int f, int level) const noexcept
    {
        return data.data() + ((size_t) f * numLevels + (size_t) level) * frameSize;
    }

private:
    int numFrames = 0;
    std::vector<float> data;
};

// One sample of a frame with the warp modes applied (used by the display; the voice has fast loops).
inline float warpedRead (const WavetableBank& t, float pos, float p, int mode, float amt, int level) noexcept
{
    if (amt <= 0.0001f || mode == 0) return t.read (pos, p, level);
    auto frac = [] (float x) { return x - std::floor (x); };
    switch (mode)
    {
        case 1: return t.read (pos, frac (p * (1.0f + amt * 7.0f)), level);
        case 2: return t.read (pos, std::pow (p, 1.0f / (1.0f + amt * 3.0f)), level);
        case 3: return t.read (pos, std::pow (p, 1.0f + amt * 3.0f), level);
        case 4: { const float m = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; return t.read (pos, p + (m - p) * amt, level); }
        case 5: { const float w = 0.5f - 0.48f * amt; return t.read (pos, p < w ? 0.5f * p / w : 0.5f + 0.5f * (p - w) / (1.0f - w), level); }
        case 6: return t.read (pos, frac (p * (1.0f + amt * 7.0f)), level) * ((1.0f - amt) + amt * 6.0f * p * (1.0f - p));
        case 7: { const float steps = 64.0f - amt * 60.0f; return t.read (pos, std::floor (p * steps) / steps, level); }
        case 8: { const float x = t.read (pos, p, level) * (1.0f + amt * 4.0f) + 1.0f; const float m = x - 4.0f * std::floor (x * 0.25f); return 1.0f - std::abs (m - 2.0f); }
        default: return t.read (pos, p, level);
    }
}

} // namespace ff
