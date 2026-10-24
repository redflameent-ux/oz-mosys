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

} // namespace ff
