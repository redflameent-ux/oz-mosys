#pragma once
// =====================================================================================
//  Module slots (Absynth-style): a small processor in each engine's channel.
//  A and B mean different things per type (shown on the PATCH page).
// =====================================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include "DspCore.h"
#include "../Params.h"

namespace ff
{
// Hilbert transformer (two allpass chains, 90 degrees apart) for the frequency shifter
class Hilbert
{
public:
    void reset() { for (auto& s : st) s = {}; d1 = 0; }
    inline void process (float x, float& re, float& im) noexcept
    {
        static constexpr float c1[4] = { 0.6923878f * 0.6923878f, 0.9360654322959f * 0.9360654322959f,
                                         0.9882295226860f * 0.9882295226860f, 0.9987488452737f * 0.9987488452737f };
        static constexpr float c2[4] = { 0.4021921162426f * 0.4021921162426f, 0.8561710882420f * 0.8561710882420f,
                                         0.9722909545651f * 0.9722909545651f, 0.9952884791278f * 0.9952884791278f };
        float a = x, b = x;
        for (int i = 0; i < 4; ++i) a = ap (st[(size_t) i], c1[i], a);
        for (int i = 0; i < 4; ++i) b = ap (st[(size_t) i + 4], c2[i], b);
        re = d1; d1 = a;   // one-sample delay on the first path
        im = b;
    }
private:
    struct S { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static inline float ap (S& s, float c, float in) noexcept
    {
        const float out = c * (in + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = in; s.y2 = s.y1; s.y1 = out;
        return out;
    }
    std::array<S, 8> st {};
    float d1 = 0;
};

class ModuleSlot
{
public:
    enum Type { Off, LadderLP, LP12, HP12, BandPass, Notch, Comb, FreqShift, RingMod, Waveshaper, Decimator };

    void prepare (double sr)
    {
        sampleRate = sr;
        filter.setSampleRate (sr);
        const int len = juce::nextPowerOfTwo ((int) (sr / 15.0) + 4);
        for (auto& d : delay) d.assign ((size_t) len, 0.0f);
        mask = len - 1;
    }
    void reset()
    {
        filter.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        writePos = 0; oscPhase = 0.0; for (auto& h : hil) h.reset();
        holdCount = 0; held[0] = held[1] = 0.0f; lastType = -1;
    }

    // process L/R in place. noteHz lets comb and ring mod follow the keyboard.
    void process (int type, float a, float b, float mix, float noteHz, float* L, float* R, int n) noexcept
    {
        if (type == Off || mix <= 0.0001f || delay[0].empty()) return;
        if (type != lastType) { filter.reset(); lastType = type; }
        float dryL[32], dryR[32];
        std::copy (L, L + n, dryL); std::copy (R, R + n, dryR);
        const float sr = (float) sampleRate;
        switch (type)
        {
            case LadderLP: case LP12: case HP12: case BandPass: case Notch:
            {
                static constexpr int map[] = { 0, 0, 1, 3, 2, 4 };
                filter.setParams (map[type], cutoffToHz (a), b, 0.0f);
                for (int i = 0; i < n; ++i) { L[i] = filter.process (L[i], 0); R[i] = filter.process (R[i], 1); }
                break;
            }
            case Comb:
            {
                // tuned to the note, A = -24..+24 semitones, B = feedback (left half negative = hollow/odd)
                const float hz = juce::jlimit (16.0f, sr * 0.4f, noteHz * std::exp2 ((a * 48.0f - 24.0f) / 12.0f));
                const float d = juce::jlimit (2.0f, (float) mask - 2.0f, sr / hz);
                const float fb = (b * 2.0f - 1.0f) * 0.97f;
                const float norm = std::sqrt (1.0f - std::abs (fb)) * 0.9f;
                for (int i = 0; i < n; ++i)
                {
                    float rp = (float) writePos - d;
                    if (rp < 0) rp += (float) (mask + 1);
                    const int i0 = (int) rp; const float t = rp - (float) i0;
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        auto& dl = delay[(size_t) ch];
                        const float y0 = dl[(size_t) (i0 & mask)], y1 = dl[(size_t) ((i0 + 1) & mask)];
                        const float del = y0 + (y1 - y0) * t;
                        float* io = ch == 0 ? L : R;
                        const float y = io[i] + fb * del;
                        dl[(size_t) writePos] = std::tanh (y);
                        io[i] = y * norm;
                    }
                    writePos = (writePos + 1) & mask;
                }
                break;
            }
            case FreqShift:
            {
                // A = shift (centre = none, left down, right up, up to 2 kHz), B = stereo spread (right goes the other way)
                const float x = a * 2.0f - 1.0f;
                const float hz = (x < 0 ? -1.0f : 1.0f) * (std::exp2 (std::abs (x) * 11.0f) - 1.0f);
                const double incL = hz / sampleRate, incR = hz * (1.0f - 2.0f * b) / sampleRate;
                for (int i = 0; i < n; ++i)
                {
                    float reL, imL, reR, imR;
                    hil[0].process (L[i], reL, imL);
                    hil[1].process (R[i], reR, imR);
                    const double pL = oscPhase * juce::MathConstants<double>::twoPi;
                    const double pR = oscPhase2 * juce::MathConstants<double>::twoPi;
                    L[i] = (float) (reL * std::cos (pL) - imL * std::sin (pL));
                    R[i] = (float) (reR * std::cos (pR) - imR * std::sin (pR));
                    oscPhase += incL; oscPhase -= std::floor (oscPhase);
                    oscPhase2 += incR; oscPhase2 -= std::floor (oscPhase2);
                }
                break;
            }
            case RingMod:
            {
                // A = carrier pitch relative to the note (-24..+24 st), B = add a fixed low wobble (0..30 Hz)
                const double hz = noteHz * std::exp2 ((a * 48.0f - 24.0f) / 12.0f) + b * 30.0;
                const double inc = hz / sampleRate;
                for (int i = 0; i < n; ++i)
                {
                    const float m = (float) std::sin (oscPhase * juce::MathConstants<double>::twoPi);
                    L[i] *= m; R[i] *= m;
                    oscPhase += inc; oscPhase -= std::floor (oscPhase);
                }
                break;
            }
            case Waveshaper:
            {
                // A = drive, B = soft (tanh) .. folding
                const float drive = 1.0f + a * 24.0f;
                const float comp = 1.0f / std::sqrt (drive);
                for (int i = 0; i < n; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float* io = ch == 0 ? L : R;
                        const float x = io[i] * drive;
                        io[i] = ((1.0f - b) * std::tanh (x) + b * std::sin (x * 1.5708f)) * (0.5f + 0.5f * comp);
                    }
                break;
            }
            case Decimator:
            {
                // A = sample-rate reduction, B = bit reduction
                const int hold = 1 + (int) (a * a * 40.0f);
                const float steps = std::exp2 (16.0f - b * 13.0f);
                for (int i = 0; i < n; ++i)
                {
                    if (--holdCount <= 0)
                    {
                        holdCount = hold;
                        held[0] = std::round (L[i] * steps) / steps;
                        held[1] = std::round (R[i] * steps) / steps;
                    }
                    L[i] = held[0]; R[i] = held[1];
                }
                break;
            }
            default: break;
        }
        if (mix < 0.9999f)
            for (int i = 0; i < n; ++i) { L[i] = dryL[i] + (L[i] - dryL[i]) * mix; R[i] = dryR[i] + (R[i] - dryR[i]) * mix; }
    }

    // what A and B do for each type (for the PATCH page labels)
    static juce::String labelA (int t)
    {
        switch (t) { case LadderLP: case LP12: case HP12: case BandPass: case Notch: return "Cutoff";
                     case Comb: case RingMod: return "Pitch"; case FreqShift: return "Shift"; case Waveshaper: return "Drive";
                     case Decimator: return "Rate"; default: return "A"; }
    }
    static juce::String labelB (int t)
    {
        switch (t) { case LadderLP: case LP12: case HP12: case BandPass: case Notch: return "Reso";
                     case Comb: return "Feedback"; case FreqShift: return "Spread"; case RingMod: return "Wobble";
                     case Waveshaper: return "Fold"; case Decimator: return "Bits"; default: return "B"; }
    }

private:
    double sampleRate = 48000.0;
    VoiceFilter filter;
    std::array<std::vector<float>, 2> delay;
    int mask = 0, writePos = 0, lastType = -1;
    double oscPhase = 0.0, oscPhase2 = 0.0;
    std::array<Hilbert, 2> hil;
    int holdCount = 0;
    float held[2] {};
};

// Coloured noise source
class NoiseGen
{
public:
    inline float next (int colour, juce::Random& r) noexcept
    {
        const float w = r.nextFloat() * 2.0f - 1.0f;
        switch (colour)
        {
            case 1:   // pink (Paul Kellet's economy filter)
                b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
                return (b0 + b1 + b2 + w * 0.1848f) * 0.22f;
            case 2:   // brown
                br = juce::jlimit (-1.0f, 1.0f, br * 0.995f + w * 0.06f);
                return br * 2.2f;
            case 3:   // digital: stepped, crushed
                if (--hold <= 0) { hold = 2 + r.nextInt (12); dig = std::round (w * 4.0f) / 4.0f; }
                return dig * 0.8f;
            case 4:   // crackle: sparse clicks over a little hiss
                return (r.nextFloat() < 0.0007f ? (w > 0 ? 1.0f : -1.0f) * (0.4f + 0.6f * r.nextFloat()) : 0.0f) + w * 0.02f;
            default: return w * 0.7f;
        }
    }
private:
    float b0 = 0, b1 = 0, b2 = 0, br = 0, dig = 0;
    int hold = 0;
};
} // namespace ff
