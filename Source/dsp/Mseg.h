#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <vector>
#include <array>

// =====================================================================================
//  Breakpoint envelopes (MSEG 1 and MSEG 2), in the spirit of Absynth's envelopes:
//  up to 68 points with curves, an optional sustain point and an optional loop.
//  Times are in seconds (or beats when synced to tempo). Output 0..1.
// =====================================================================================
namespace ff
{
struct MsegPoint { double time = 0.0; float level = 0.0f; float curve = 0.0f; };

struct MsegShape
{
    static constexpr int maxPoints = 68;
    std::vector<MsegPoint> points;   // sorted by time, first at 0
    int sustain = -1;                // point index the envelope holds at while the key is down
    int loopStart = -1;              // loop from here back... (to sustain while held, or to the end)
    bool loop = false;
    bool sync = false;               // times are beats instead of seconds
    float rate = 1.0f;               // speed multiplier
    float lfoDepth = 0.0f;           // LFO overlay (wobble on top of the shape), 0..1
    float lfoRate = 4.0f;            // Hz, or cycles per beat when synced
    float shRate = 0.0f;             // sample & hold steps per second (per beat when synced), 0 = off
    juce::String name;               // shown in the envelope list

    // ---- Performer / stepper mode (Massive-style): a loop of steps, each a little drawn curve ----
    static constexpr int maxSteps = 32;
    static constexpr int numStepShapes = 10;
    bool stepper = false;
    int numSteps = 16;
    float stepBeats = 0.25f;         // length of one step (beats when synced, else seconds)
    std::array<int, maxSteps> stepShape = [] { std::array<int, maxSteps> a {}; for (int i = 0; i < maxSteps; ++i) a[(size_t) i] = i % 4 == 0 ? 5 : (i % 2 ? 8 : 4); return a; }();
    std::array<float, maxSteps> stepLevel = [] { std::array<float, maxSteps> a {}; a.fill (1.0f); return a; }();

    static const juce::StringArray& stepShapeNames()
    {
        static const juce::StringArray n { "Hold", "Ramp up", "Ramp down", "Triangle", "Decay", "Pluck", "Gate", "Bump", "Off", "S-curve" };
        return n;
    }
    // one step's curve, u = 0..1 through the step
    static float stepCurve (int shape, float u)
    {
        u = juce::jlimit (0.0f, 1.0f, u);
        switch (shape)
        {
            case 0: return 1.0f;
            case 1: return u;
            case 2: return 1.0f - u;
            case 3: return u < 0.5f ? u * 2.0f : 2.0f - u * 2.0f;
            case 4: return std::pow (1.0f - u, 3.0f);
            case 5: return u < 0.06f ? u / 0.06f : std::pow (1.0f - (u - 0.06f) / 0.94f, 4.0f);
            case 6: return u < 0.5f ? 1.0f : 0.0f;
            case 7: return std::sin (u * juce::MathConstants<float>::pi);
            case 8: return 0.0f;
            default: return u * u * (3.0f - 2.0f * u);
        }
    }
    double stepLoopLength() const { return juce::jmax (1.0e-3, (double) numSteps * stepBeats); }
    float stepValueAt (double t) const
    {
        const double len = stepLoopLength();
        t = std::fmod (juce::jmax (0.0, t), len);
        const int i = juce::jlimit (0, numSteps - 1, (int) (t / stepBeats));
        const float u = (float) ((t - i * stepBeats) / stepBeats);
        return stepCurve (stepShape[(size_t) i], u) * stepLevel[(size_t) i];
    }

    double length() const { return stepper ? stepLoopLength() : (points.empty() ? 0.0 : points.back().time); }

    static float shape (float t, float curve)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (std::abs (curve) < 1.0e-3f) return t;
        // curve > 0: moves fast then settles; curve < 0: starts slow then rushes
        return curve > 0 ? 1.0f - std::pow (1.0f - t, 1.0f + 4.0f * curve) : std::pow (t, 1.0f - 4.0f * curve);
    }

    float valueAt (double t) const
    {
        if (stepper) return stepValueAt (t);
        if (points.empty()) return 0.0f;
        if (t <= points.front().time) return points.front().level;
        for (size_t i = 0; i + 1 < points.size(); ++i)
            if (t <= points[i + 1].time)
            {
                const double len = points[i + 1].time - points[i].time;
                const float u = len > 1.0e-9 ? (float) ((t - points[i].time) / len) : 1.0f;
                return points[i].level + (points[i + 1].level - points[i].level) * shape (u, points[i].curve);
            }
        return points.back().level;
    }

    void sanitise()
    {
        std::sort (points.begin(), points.end(), [] (auto& a, auto& b) { return a.time < b.time; });
        if (points.size() > (size_t) maxPoints) points.resize ((size_t) maxPoints);
        if (points.empty()) points = { { 0.0, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } };
        points.front().time = 0.0;
        if (points.size() < 2) points.push_back ({ 1.0, points.front().level, 0.0f });
        if (sustain >= (int) points.size()) sustain = -1;
        if (loopStart >= (int) points.size() - 1) loopStart = -1;
        if (sustain >= 0 && loopStart >= sustain) loopStart = -1;
    }

    juce::String toString() const
    {
        juce::String s;
        s << sustain << ' ' << loopStart << ' ' << (loop ? 1 : 0) << ' ' << (sync ? 1 : 0) << ' ' << juce::String (rate, 3)
          << ' ' << juce::String (lfoDepth, 3) << ' ' << juce::String (lfoRate, 3) << ' ' << juce::String (shRate, 3) << '|';
        for (auto& p : points) s << juce::String (p.time, 4) << ' ' << juce::String (p.level, 4) << ' ' << juce::String (p.curve, 3) << ';';
        if (stepper || numSteps != 16)
        {
            s << '#' << (stepper ? 1 : 0) << ' ' << numSteps << ' ' << juce::String (stepBeats, 4) << ' ';
            for (int i = 0; i < maxSteps; ++i) s << stepShape[(size_t) i] << ',' << juce::String (stepLevel[(size_t) i], 3) << ';';
        }
        return s;
    }

    static MsegShape fromString (const juce::String& text)
    {
        MsegShape m;
        auto head = juce::StringArray::fromTokens (text.upToFirstOccurrenceOf ("|", false, false), " ", "");
        if (head.size() >= 5)
        {
            m.sustain = head[0].getIntValue(); m.loopStart = head[1].getIntValue();
            m.loop = head[2].getIntValue() != 0; m.sync = head[3].getIntValue() != 0;
            m.rate = juce::jlimit (0.05f, 20.0f, head[4].getFloatValue());
        }
        if (head.size() >= 8)
        {
            m.lfoDepth = juce::jlimit (0.0f, 1.0f, head[5].getFloatValue());
            m.lfoRate = juce::jlimit (0.01f, 40.0f, head[6].getFloatValue());
            m.shRate = juce::jlimit (0.0f, 64.0f, head[7].getFloatValue());
        }
        if (text.containsChar ('#'))
        {
            const auto st = text.fromFirstOccurrenceOf ("#", false, false);
            auto h = juce::StringArray::fromTokens (st.upToFirstOccurrenceOf (";", false, false), " ", "");
            if (h.size() >= 3)
            {
                m.stepper = h[0].getIntValue() != 0;
                m.numSteps = juce::jlimit (2, maxSteps, h[1].getIntValue());
                m.stepBeats = juce::jlimit (0.03125f, 4.0f, h[2].getFloatValue());
                // first step pair sits after the three header numbers
                auto rest = st.fromFirstOccurrenceOf (h[2], false, false).trim();
                int i = 0;
                for (auto& tok : juce::StringArray::fromTokens (rest, ";", ""))
                {
                    if (i >= maxSteps) break;
                    if (tok.trim().isEmpty()) continue;
                    m.stepShape[(size_t) i] = juce::jlimit (0, numStepShapes - 1, tok.upToFirstOccurrenceOf (",", false, false).trim().getIntValue());
                    m.stepLevel[(size_t) i] = juce::jlimit (0.0f, 1.0f, tok.fromFirstOccurrenceOf (",", false, false).getFloatValue());
                    ++i;
                }
            }
        }
        for (auto& tok : juce::StringArray::fromTokens (text.fromFirstOccurrenceOf ("|", false, false).upToFirstOccurrenceOf ("#", false, false), ";", ""))
        {
            auto p = juce::StringArray::fromTokens (tok, " ", "");
            if (p.size() >= 2) m.points.push_back ({ p[0].getDoubleValue(), juce::jlimit (0.0f, 1.0f, p[1].getFloatValue()), p.size() > 2 ? p[2].getFloatValue() : 0.0f });
        }
        m.sanitise();
        return m;
    }

    // a few useful starting shapes
    static MsegShape swell()
    {
        MsegShape m;
        m.points = { { 0.0, 0.0f, -0.4f }, { 1.2, 1.0f, 0.3f }, { 2.0, 0.7f, 0.0f }, { 3.5, 0.0f, 0.0f } };
        m.sustain = 2;
        return m;
    }
    static MsegShape rhythm()
    {
        MsegShape m;
        m.sync = true; m.loop = true; m.loopStart = 0;
        m.points = { { 0.0, 1.0f, 0.5f }, { 0.25, 0.1f, 0.0f }, { 0.5, 0.8f, 0.5f }, { 0.75, 0.0f, 0.0f },
                     { 1.0, 0.6f, 0.5f }, { 1.25, 0.2f, 0.0f }, { 1.5, 1.0f, 0.6f }, { 2.0, 1.0f, 0.0f } };
        m.points.back().level = 1.0f;
        return m;
    }
};

using MsegPtr = std::shared_ptr<const MsegShape>;

// One voice's position in an MSEG.
struct MsegPlayer
{
    double t = 0.0, lfoPhase = 0.0, shClock = 1.0e9;
    float held = 0.0f;
    bool released = false;

    void start() { t = 0.0; released = false; lfoPhase = 0.0; shClock = 1.0e9; }
    void release() { released = true; }

    float smoothed = 0.0f;

    float advance (const MsegShape& m, double seconds, double bpm)
    {
        if (m.stepper)
        {
            t += seconds * m.rate * (m.sync ? bpm / 60.0 : 1.0);
            t = std::fmod (t, m.stepLoopLength());
            const float target = m.stepValueAt (t);
            const float k = (float) juce::jmin (1.0, seconds / 0.003);   // ~3 ms slew so steps never click
            smoothed += (target - smoothed) * k;
            return smoothed;
        }
        const double end = m.length();
        const double step = seconds * m.rate * (m.sync ? bpm / 60.0 : 1.0);
        t += step;
        if (m.sustain >= 0 && ! released)
        {
            const double st = m.points[(size_t) m.sustain].time;
            if (t >= st)
            {
                if (m.loop && m.loopStart >= 0 && m.loopStart < m.sustain)
                {
                    const double ls = m.points[(size_t) m.loopStart].time;
                    const double span = juce::jmax (1.0e-3, st - ls);
                    t = ls + std::fmod (t - ls, span);
                }
                else t = st;
            }
        }
        else if (m.loop && m.sustain < 0)
        {
            const double ls = m.loopStart >= 0 ? m.points[(size_t) m.loopStart].time : 0.0;
            const double span = juce::jmax (1.0e-3, end - ls);
            if (t >= end) t = ls + std::fmod (t - ls, span);
        }
        if (t > end) t = end;
        float v = m.valueAt (t);
        const double unit = m.sync ? bpm / 60.0 : 1.0;
        if (m.lfoDepth > 0.0001f)
        {
            lfoPhase += seconds * m.lfoRate * unit;
            lfoPhase -= std::floor (lfoPhase);
            v = juce::jlimit (0.0f, 1.0f, v + m.lfoDepth * 0.5f * std::sin ((float) lfoPhase * juce::MathConstants<float>::twoPi));
        }
        if (m.shRate > 0.0001f)
        {
            shClock += seconds * m.shRate * unit;
            if (shClock >= 1.0) { shClock = 0.0; held = v; }
            v = held;
        }
        return v;
    }
};
} // namespace ff
