#pragma once
#include <juce_dsp/juce_dsp.h>

namespace ff
{
// Master effects: Drive -> Bit-crush -> Tape -> Ring mod -> Chorus -> Ping-pong Delay -> Reverb
class EffectsChain
{
public:
    struct Settings
    {
        float drive = 0, choMix = 0, choRate = 0.6f, choDepth = 0.4f;
        int driveType = 0;
        float crushBits = 24, crushRate = 1;
        float tapeWow = 0, tapeHiss = 0, tapeAge = 0;
        float ringFreq = 300, ringMix = 0;
        float eqBass = 0, eqTreble = 0;   // dB
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
        for (int c = 0; c < 2; ++c) { lowShelf[c].reset(); highShelf[c].reset(); }
        lastBass = lastTreble = 999.0f;
        tapeBuf.setSize (2, (int) (sr * 0.05) + 4);
        tapeBuf.clear();
        tapePos = 0;
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

        // ---- Drive ----
        if (s.drive > 0.001f)
        {
            const float g = 1.0f + s.drive * 12.0f;
            const float makeup = 1.0f / std::tanh (g * 0.5f) * 0.5f;
            const float sr = (float) sampleRate;
            const float preA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 700.0f / sr);
            const float postA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (s.driveType == 1 ? 4500.0f : 7000.0f) / sr);
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                auto& st = drv[c];
                for (int i = 0; i < n; ++i)
                {
                    float x = ch[c][i];
                    float y;
                    if (s.driveType == 0)            // Warm: smooth tape-like tanh
                        y = std::tanh (x * g) * makeup;
                    else if (s.driveType == 1)       // Rat: mid push + hard clip + dark post filter
                    {
                        st.pre += preA * (x - st.pre);
                        const float mid = x + 1.5f * (x - st.pre);
                        y = juce::jlimit (-0.5f, 0.5f, mid * g * 1.5f);
                        st.post += postA * (y - st.post);
                        y = st.post;
                    }
                    else                             // Fuzz: asymmetric, gated, spitty
                    {
                        const float v = x * g * 3.0f;
                        y = v > 0 ? juce::jmin (v, 0.6f) : juce::jmax (v * 1.3f, -0.35f);
                        st.post += postA * (y - st.post);
                        y = st.post;
                    }
                    // DC blocker (asymmetric clipping adds DC)
                    const float o = y - st.dcX + 0.995f * st.dcY;
                    st.dcX = y; st.dcY = o;
                    ch[c][i] = o;
                }
            }
        }

        // ---- Tone: bass and treble shelves ----
        if (std::abs (s.eqBass) > 0.05f || std::abs (s.eqTreble) > 0.05f)
        {
            if (std::abs (s.eqBass - lastBass) > 0.01f || std::abs (s.eqTreble - lastTreble) > 0.01f)
            {
                lastBass = s.eqBass; lastTreble = s.eqTreble;
                auto lo = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, 180.0, 0.7, juce::Decibels::decibelsToGain (s.eqBass));
                auto hi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 4500.0, 0.7, juce::Decibels::decibelsToGain (s.eqTreble));
                for (int c = 0; c < 2; ++c) { lowShelf[c].coefficients = lo; highShelf[c].coefficients = hi; }
            }
            float* ch2[2] = { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i) ch2[c][i] = highShelf[c].processSample (lowShelf[c].processSample (ch2[c][i]));
        }

        // ---- Bit-crush / sample-rate reduction ----
        if (s.crushBits < 23.5f || s.crushRate > 1.5f)
        {
            const float steps = std::exp2 (s.crushBits - 1.0f);
            const int hold = juce::jmax (1, (int) std::round (s.crushRate));
            for (int i = 0; i < n; ++i)
            {
                if (++crushCount >= hold)
                {
                    crushCount = 0;
                    crushHeld[0] = std::round (L[i] * steps) / steps;
                    crushHeld[1] = std::round (R[i] * steps) / steps;
                }
                L[i] = crushHeld[0]; R[i] = crushHeld[1];
            }
        }

        // ---- Tape: wow & flutter, age (saturation + darkening), hiss ----
        if (s.tapeWow > 0.001f || s.tapeAge > 0.001f || s.tapeHiss > 0.001f)
        {
            const float sr = (float) sampleRate;
            const int tl = tapeBuf.getNumSamples();
            auto* tL = tapeBuf.getWritePointer (0);
            auto* tR = tapeBuf.getWritePointer (1);
            const float lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (18000.0f - 15000.0f * s.tapeAge) / sr);
            const float satG = 1.0f + s.tapeAge * 3.0f;
            const float satM = 1.0f / std::tanh (satG);
            for (int i = 0; i < n; ++i)
            {
                wowPhase += 0.55f / sr;          if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                flutterPhase += 6.3f / sr;       if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
                flutterNoise += 0.0005f * ((hissRng.nextFloat() * 2.0f - 1.0f) - flutterNoise);
                const float mod = std::sin (wowPhase * juce::MathConstants<float>::twoPi) * 1.0f
                                + std::sin (flutterPhase * juce::MathConstants<float>::twoPi) * 0.15f + flutterNoise * 8.0f;
                const float dly = (0.006f + 0.0025f * s.tapeWow * mod) * sr + 4.0f;
                tL[tapePos] = L[i]; tR[tapePos] = R[i];
                float rp = (float) tapePos - (s.tapeWow > 0.001f ? dly : 0.0f);
                while (rp < 0) rp += (float) tl;
                const int r0 = (int) rp, r1 = (r0 + 1) % tl;
                const float t = rp - (float) r0;
                float l = tL[r0] + t * (tL[r1] - tL[r0]);
                float r = tR[r0] + t * (tR[r1] - tR[r0]);
                tapePos = (tapePos + 1) % tl;
                if (s.tapeAge > 0.001f)
                {
                    l = std::tanh (l * satG) * satM; r = std::tanh (r * satG) * satM;
                    tapeLp[0] += lpA * (l - tapeLp[0]); tapeLp[1] += lpA * (r - tapeLp[1]);
                    l = tapeLp[0]; r = tapeLp[1];
                }
                if (s.tapeHiss > 0.001f)
                {
                    hissLp[0] += 0.5f * ((hissRng.nextFloat() * 2.0f - 1.0f) - hissLp[0]);
                    hissLp[1] += 0.5f * ((hissRng.nextFloat() * 2.0f - 1.0f) - hissLp[1]);
                    l += hissLp[0] * s.tapeHiss * 0.03f; r += hissLp[1] * s.tapeHiss * 0.03f;
                }
                L[i] = l; R[i] = r;
            }
        }

        // ---- Ring modulator ----
        if (s.ringMix > 0.001f)
        {
            const float inc = s.ringFreq / (float) sampleRate;
            for (int i = 0; i < n; ++i)
            {
                const float m = std::sin (ringPhase * juce::MathConstants<float>::twoPi);
                ringPhase += inc; if (ringPhase >= 1.0f) ringPhase -= 1.0f;
                L[i] += (L[i] * m - L[i]) * s.ringMix;
                R[i] += (R[i] * m - R[i]) * s.ringMix;
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

    struct DriveState { float pre = 0, post = 0, dcX = 0, dcY = 0; };
    DriveState drv[2];
    int crushCount = 0;
    float crushHeld[2] {};
    juce::AudioBuffer<float> tapeBuf;
    int tapePos = 0;
    float wowPhase = 0, flutterPhase = 0, flutterNoise = 0, tapeLp[2] {}, hissLp[2] {};
    juce::Random hissRng { 99 };
    float ringPhase = 0;
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float lastBass = 999.0f, lastTreble = 999.0f;
};

} // namespace ff
