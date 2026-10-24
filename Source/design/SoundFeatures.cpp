#include "SoundFeatures.h"
#include "../PluginProcessor.h"

namespace ff::design
{
SoundFeatures analyse (const juce::AudioBuffer<float>& audio, double sr)
{
    SoundFeatures f;
    const int order = 11, size = 1 << order, hop = size / 2;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> work ((size_t) size * 2);
    const int len = audio.getNumSamples();
    const int analyseUntil = juce::jmin (len, (int) (sr * 2.2));   // while the notes are held

    std::vector<float> centroids, levels;
    double hfSum = 0, allSum = 0, flatSum = 0;
    int frames = 0;
    for (int start = 0; start + size <= analyseUntil; start += hop)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        for (int i = 0; i < size; ++i)
            work[(size_t) i] = 0.5f * (audio.getSample (0, start + i) + audio.getSample (juce::jmin (1, audio.getNumChannels() - 1), start + i));
        window.multiplyWithWindowingTable (work.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (work.data());

        double num = 0, den = 0, hf = 0, logSum = 0;
        int bins = 0;
        for (int k = 2; k < size / 2; ++k)
        {
            const double hz = (double) k * sr / size;
            const double mag = work[(size_t) k];
            const double pw = mag * mag;
            num += hz * pw; den += pw;
            if (hz > 2500.0) hf += pw;
            if (hz < 12000.0) { logSum += std::log (mag + 1.0e-9); ++bins; }
        }
        double amean = 0;
        for (int k = 2; k < size / 2; ++k) if ((double) k * sr / size < 12000.0) amean += work[(size_t) k];
        amean /= juce::jmax (1, bins);
        const double geo = std::exp (logSum / juce::jmax (1, bins));
        if (den > 1.0e-10)
        {
            centroids.push_back ((float) (num / den));
            hfSum += hf; allSum += den;
            flatSum += amean > 1.0e-12 ? geo / amean : 0.0;
            ++frames;
        }
        levels.push_back (audio.getRMSLevel (0, start, size));
    }
    if (frames == 0) return f;

    // brightness: median spectral centroid on a log scale (100 Hz .. 8 kHz -> 0 .. 1)
    auto sorted = centroids;
    std::sort (sorted.begin(), sorted.end());
    const float med = sorted[sorted.size() / 2];
    f.brightness = juce::jlimit (0.0f, 1.0f, std::log2 (juce::jmax (100.0f, med) / 100.0f) / std::log2 (80.0f));

    // attack: time to reach 80 % of the loudest moment
    float peak = 0;
    for (auto l : levels) peak = juce::jmax (peak, l);
    int attackFrame = 0;
    for (int i = 0; i < (int) levels.size(); ++i) if (levels[(size_t) i] >= peak * 0.8f) { attackFrame = i; break; }
    const float attackSec = (float) (attackFrame * hop / sr);
    const float sharp = 1.0f - juce::jlimit (0.0f, 1.0f, attackSec / 0.8f);

    const float hfRatio = (float) (hfSum / juce::jmax (1.0e-12, allSum));
    const float noisy = (float) (flatSum / frames);
    f.aggression = 0.5f * juce::jlimit (0.0f, 1.0f, hfRatio * 3.0f) + 0.3f * sharp + 0.2f * juce::jlimit (0.0f, 1.0f, noisy * 2.0f);

    // movement: how much the centroid and level wander (after the attack)
    auto variation = [] (const std::vector<float>& v, size_t from)
    {
        if (v.size() <= from + 2) return 0.0f;
        double mean = 0, sq = 0; size_t n = 0;
        for (size_t i = from; i < v.size(); ++i) { mean += v[i]; ++n; }
        mean /= (double) n;
        for (size_t i = from; i < v.size(); ++i) sq += (v[i] - mean) * (v[i] - mean);
        return mean > 1.0e-9 ? (float) (std::sqrt (sq / (double) n) / mean) : 0.0f;
    };
    const size_t settle = (size_t) juce::jmin ((int) centroids.size() - 1, attackFrame + 2);
    f.movement = juce::jlimit (0.0f, 1.0f, variation (centroids, settle) * 2.0f + variation (levels, settle) * 1.0f);
    return f;
}

juce::AudioBuffer<float> renderAudition (FourfoldProcessor& source, double sr, double seconds)
{
    // a private copy of the sound, so the real plugin keeps playing undisturbed
    FourfoldProcessor proc;
    juce::MemoryBlock state;
    source.getStateInformation (state);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    proc.roll.setPattern ({});

    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    const int total = (int) (sr * seconds);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    const int notes[] = { 48, 55, 60 };
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        const int n = juce::jmin (block, total - pos);
        if (pos == 0) for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
        const int off = (int) (sr * 2.0);
        if (pos <= off && pos + n > off) for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOff (1, nn), off - pos);
        buf.setSize (2, n, false, false, true);
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}
} // namespace ff::design
