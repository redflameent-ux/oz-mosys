#pragma once
#include <juce_dsp/juce_dsp.h>

namespace ff
{
// Master effects: Drive -> Chorus -> Ping-pong Delay -> Reverb
class EffectsChain
{
public:
    struct Settings
    {
        float drive = 0, choMix = 0, choRate = 0.6f, choDepth = 0.4f;
        float dlyMix = 0, dlyTimeMs = 375, dlyFb = 0.35f, dlyTone = 0.6f;
        float revMix = 0.15f, revSize = 0.6f, revDamp = 0.5f;
    };

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        chorus.prepare (spec);
        chorus.setCentreDelay (8.0f);
        chorus.setFeedback (0.15f);
        const int maxDelay = (int) (sr * 1.6) + 4;
        delayBuf.setSize (2, maxDelay);
        delayBuf.clear();
        writePos = 0;
        smoothedDelay.reset (sr, 0.15);
        smoothedDelay.setCurrentAndTargetValue (375.0f * 0.001f * (float) sr);
        reverb.setSampleRate (sr);
        dlyLp[0] = dlyLp[1] = 0.0f;
    }

    void reset()
    {
        chorus.reset();
        delayBuf.clear();
        reverb.reset();
    }

    void process (juce::AudioBuffer<float>& buf, const Settings& s)
    {
        const int n = buf.getNumSamples();
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);

        // ---- Drive (soft saturation) ----
        if (s.drive > 0.001f)
        {
            const float g = 1.0f + s.drive * 12.0f;
            const float makeup = 1.0f / std::tanh (g * 0.5f) * 0.5f;
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh (L[i] * g) * makeup;
                R[i] = std::tanh (R[i] * g) * makeup;
            }
        }

        // ---- Chorus ----
        if (s.choMix > 0.001f)
        {
            chorus.setRate (s.choRate);
            chorus.setDepth (s.choDepth);
            chorus.setMix (s.choMix * 0.5f);
            juce::dsp::AudioBlock<float> block (buf);
            chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // ---- Ping-pong delay ----
        const int dlen = delayBuf.getNumSamples();
        smoothedDelay.setTargetValue (juce::jlimit (1.0f, (float) dlen - 2.0f, s.dlyTimeMs * 0.001f * (float) sampleRate));
        const float toneCoef = 0.05f + 0.9f * s.dlyTone;
        auto* dL = delayBuf.getWritePointer (0);
        auto* dR = delayBuf.getWritePointer (1);
        if (s.dlyMix > 0.001f || delayActive)
        {
            float energy = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float d = smoothedDelay.getNextValue();
                float rp = (float) writePos - d;
                if (rp < 0) rp += (float) dlen;
                const int r0 = (int) rp;
                const int r1 = (r0 + 1) % dlen;
                const float t = rp - (float) r0;
                const float yl = dL[r0] + t * (dL[r1] - dL[r0]);
                const float yr = dR[r0] + t * (dR[r1] - dR[r0]);
                dlyLp[0] += toneCoef * (yl - dlyLp[0]);
                dlyLp[1] += toneCoef * (yr - dlyLp[1]);
                const float mono = 0.5f * (L[i] + R[i]);
                // ping-pong: input enters the left side, feedback crosses over
                dL[writePos] = mono + dlyLp[1] * s.dlyFb;
                dR[writePos] = dlyLp[0] * s.dlyFb;
                writePos = (writePos + 1) % dlen;
                L[i] += yl * s.dlyMix;
                R[i] += yr * s.dlyMix;
                energy += std::abs (yl) + std::abs (yr);
            }
            delayActive = s.dlyMix > 0.001f || energy > 1.0e-4f;
        }

        // ---- Reverb ----
        if (s.revMix > 0.001f)
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.3f + 0.69f * s.revSize;
            p.damping = s.revDamp;
            p.width = 1.0f;
            p.wetLevel = s.revMix * 0.6f;
            p.dryLevel = 1.0f - s.revMix * 0.5f;
            reverb.setParameters (p);
            reverb.processStereo (L, R, n);
        }
    }

private:
    double sampleRate = 48000.0;
    juce::dsp::Chorus<float> chorus;
    juce::AudioBuffer<float> delayBuf;
    int writePos = 0;
    bool delayActive = false;
    float dlyLp[2] {};
    juce::SmoothedValue<float> smoothedDelay;
    juce::Reverb reverb;
};

} // namespace ff
