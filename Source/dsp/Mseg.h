#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <vector>

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

    double length() const { return points.empty() ? 0.0 : points.back().time; }

    static float shape (float t, float curve)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (std::abs (curve) < 1.0e-3f) return t;
        // curve > 0: moves fast then settles; curve < 0: starts slow then rushes
        return curve > 0 ? 1.0f - std::pow (1.0f - t, 1.0f + 4.0f * curve) : std::pow (t, 1.0f - 4.0f * curve);
    }

    float valueAt (double t) const
    {
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
        for (auto& tok : juce::StringArray::fromTokens (text.fromFirstOccurrenceOf ("|", false, false), ";", ""))
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

    float advance (const MsegShape& m, double seconds, double bpm)
    {
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
