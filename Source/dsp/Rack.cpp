#include "Rack.h"
#include <cmath>

namespace ff
{
using juce::MathConstants;

// ===================================================================================== library
const std::vector<RackUnitInfo>& rackUnits()
{
    static const std::vector<RackUnitInfo> u = {
        { "Empty", "", { "", "", "", "" }, { 0, 0, 0, 0 }, "" },
        // Distortion
        { "Screamer", "Distortion", { "Drive", "Tone", "Level", "" }, { 0.5f, 0.5f, 0.6f, 0 }, "Mid-hump overdrive: smooth, singing, pushes amps." },
        { "Big Fuzz", "Distortion", { "Fuzz", "Tone", "Gate", "Level" }, { 0.7f, 0.5f, 0.2f, 0.5f }, "Thick sustaining fuzz with a scooped tone and a noise gate." },
        { "Rat Box", "Distortion", { "Distortion", "Filter", "Level", "" }, { 0.6f, 0.4f, 0.5f, 0 }, "Hard-clipping distortion; Filter darkens as you turn it up." },
        { "Bit Grit", "Distortion", { "Bits", "Rate", "Drive", "" }, { 0.4f, 0.3f, 0.2f, 0 }, "Bit and sample-rate crusher for lo-fi digital grit." },
        // Amps
        { "Clean Amp", "Amps", { "Gain", "Bass", "Mid", "Treble" }, { 0.3f, 0.5f, 0.5f, 0.55f }, "Glassy clean combo with a 3-band tone stack." },
        { "Crunch Amp", "Amps", { "Gain", "Bass", "Mid", "Treble" }, { 0.55f, 0.5f, 0.6f, 0.5f }, "British-style crunch that breaks up as you play harder." },
        { "Lead Amp", "Amps", { "Gain", "Bass", "Mid", "Treble" }, { 0.75f, 0.55f, 0.45f, 0.5f }, "High-gain lead channel with lots of sustain." },
        // Cabinets
        { "Cabinet", "Cabinets", { "Model", "Mic", "Room", "Level" }, { 0.5f, 0.5f, 0.2f, 0.6f }, "Speaker cabinet: Model goes small open-back > 2x12 > 4x12 > tiny radio." },
        { "IR Loader", "Cabinets", { "Low Cut", "High Cut", "Level", "" }, { 0.0f, 1.0f, 0.6f, 0 }, "Load any impulse response WAV (cabinets, rooms, weird spaces)." },
        // Delay & Echo
        { "Tape Echo", "Delay & Echo", { "Time", "Feedback", "Wobble", "Tone" }, { 0.5f, 0.45f, 0.3f, 0.5f }, "Tempo-synced tape delay with saturation and wobble in the loop." },
        { "Multitap", "Delay & Echo", { "Time", "Swing", "Feedback", "Tone" }, { 0.5f, 0.3f, 0.35f, 0.6f }, "Three taps panned left, centre and right." },
        { "Granular Delay", "Delay & Echo", { "Time", "Grain", "Spray", "Pitch" }, { 0.4f, 0.4f, 0.5f, 0.5f }, "Aetherizer-style cloud of grains from the delayed signal; Pitch at centre = none." },
        // Reverb
        { "Shimmer", "Reverb", { "Size", "Shimmer", "Damping", "Width" }, { 0.8f, 0.5f, 0.4f, 1.0f }, "Big reverb with octave-up shine fed back into it." },
        { "Hall", "Reverb", { "Size", "Damping", "Width", "Pre-delay" }, { 0.7f, 0.5f, 1.0f, 0.2f }, "Smooth room to hall reverb with pre-delay." },
        // Modulation
        { "Ensemble", "Modulation", { "Rate", "Depth", "Width", "" }, { 0.3f, 0.5f, 1.0f, 0 }, "Three-voice string-machine chorus." },
        { "Phaser", "Modulation", { "Rate", "Depth", "Feedback", "Centre" }, { 0.3f, 0.6f, 0.5f, 0.5f }, "Classic sweeping phaser." },
        { "Flanger", "Modulation", { "Rate", "Depth", "Feedback", "" }, { 0.2f, 0.6f, 0.6f, 0 }, "Jet flanger with feedback (left of centre = negative)." },
        { "Octaver", "Modulation", { "Sub", "Up", "Dry", "" }, { 0.6f, 0.2f, 0.7f, 0 }, "Analog-style octave down and up." },
        { "Tremolo", "Modulation", { "Rate", "Depth", "Shape", "Stereo" }, { 0.5f, 0.6f, 0.2f, 0.0f }, "Tempo-synced tremolo; Stereo turns it into an auto-pan." },
        // Filters
        { "Auto-Wah", "Filters", { "Sensitivity", "Frequency", "Reso", "Direction" }, { 0.6f, 0.3f, 0.6f, 1.0f }, "Envelope-following wah: play harder to open it." },
        { "EQ", "Filters", { "Low", "Mid", "Mid Freq", "High" }, { 0.5f, 0.5f, 0.5f, 0.5f }, "Three-band EQ (centre = flat)." },
        // Dynamics
        { "Compressor", "Dynamics", { "Threshold", "Ratio", "Attack", "Release" }, { 0.5f, 0.4f, 0.3f, 0.4f }, "Smooth compressor with automatic make-up gain." },
        { "Maximizer", "Dynamics", { "Input", "Ceiling", "Character", "Release" }, { 0.3f, 0.9f, 0.3f, 0.4f }, "Loudness maximizer / limiter; Character adds soft clipping." },
        // Lo-fi / Tape
        { "Tape", "Lo-fi / Tape", { "Hiss", "Wow", "Flutter", "Age" }, { 0.3f, 0.3f, 0.2f, 0.4f }, "Cassette and reel-to-reel: hiss, wow, flutter, saturation and lost highs." },
        { "Vinyl", "Lo-fi / Tape", { "Crackle", "Rumble", "Wear", "Brake" }, { 0.4f, 0.2f, 0.4f, 0.0f }, "Record crackle, pops, rumble and wear; Brake slows it down like stopping a turntable." },
        // Time FX
        { "Half Time", "Time FX", { "Length", "Tape Pitch", "Smooth", "" }, { 0.5f, 0.0f, 0.5f, 0 }, "Plays the music back at half speed, locked to the beat. Tape Pitch also drops it an octave." },
        { "Reverse", "Time FX", { "Length", "Chance", "Fade", "" }, { 0.5f, 1.0f, 0.3f, 0 }, "Plays each beat-synced chunk backwards (Chance = how often)." },
        { "Stutter", "Time FX", { "Slice", "Chance", "Gate", "Pitch Drop" }, { 0.4f, 0.5f, 0.8f, 0.0f }, "Repeats tiny tempo-synced slices: glitch rolls and stutters." },
    };
    return u;
}

float rackDefaultMix (int type)
{
    const juce::String n = juce::isPositiveAndBelow (type, (int) rackUnits().size()) ? rackUnits()[(size_t) type].name : "";
    return (n == "Tape Echo" || n == "Multitap" || n == "Granular Delay" || n == "Shimmer" || n == "Hall") ? 0.35f : 1.0f;
}

juce::StringArray rackUnitNames() { juce::StringArray a; for (auto& u : rackUnits()) a.add (u.name); return a; }
juce::StringArray rackCategories()
{
    juce::StringArray a;
    for (auto& u : rackUnits()) if (juce::String (u.category).isNotEmpty()) a.addIfNotAlreadyThere (u.category);
    return a;
}

// ===================================================================================== helpers
static inline float onePoleCoef (float hz, double sr) { return 1.0f - std::exp (-MathConstants<float>::twoPi * hz / (float) sr); }
static inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// tempo-synced note value: 0..1 -> 1/32 .. 1 bar (in beats)
static inline double syncBeats (float v)
{
    static const double vals[] = { 0.125, 0.1875, 0.25, 0.375, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };
    return vals[juce::jlimit (0, 10, (int) std::round (v * 10.0f))];
}

struct Biquad
{
    juce::dsp::IIR::Filter<float> f[2];
    void set (juce::dsp::IIR::Coefficients<float>::Ptr c) { f[0].coefficients = c; f[1].coefficients = c; }
    void reset() { f[0].reset(); f[1].reset(); }
    inline float run (int ch, float x) { return f[ch].processSample (x); }
};

struct RingBuffer
{
    juce::AudioBuffer<float> buf;
    int pos = 0, size = 1;
    void prepare (int len) { size = juce::jmax (4, len); buf.setSize (2, size); buf.clear(); pos = 0; }
    void clear() { buf.clear(); pos = 0; }
    inline void write (float l, float r) { buf.setSample (0, pos, l); buf.setSample (1, pos, r); pos = (pos + 1) % size; }
    inline float read (int ch, double delaySamples) const   // linear interpolation, delay back from the write head
    {
        double rp = (double) pos - delaySamples;
        while (rp < 0) rp += size;
        const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
        const float t = (float) (rp - std::floor (rp));
        const float* d = buf.getReadPointer (ch);
        return d[i0] + t * (d[i1] - d[i0]);
    }
};

// ===================================================================================== distortion
class Screamer : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { for (auto& v : hp) v = 0; for (auto& v : lp) v = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float gain = std::pow (10.0f, p.a * 2.2f);
        const float hpA = onePoleCoef (720.0f, sr), lpA = onePoleCoef (900.0f + p.b * 6000.0f, sr);
        const float lvl = dbToGain ((p.c - 0.6f) * 30.0f) * 0.7f;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                hp[c] += hpA * (x - hp[c]);
                const float hi = x - hp[c];
                const float y = x * 0.35f + std::tanh (hi * gain + x * 0.3f) - 0.08f * std::tanh (hi * gain) * std::tanh (hi * gain);
                lp[c] += lpA * (y - lp[c]);
                ch[c][i] = lp[c] * lvl;
            }
    }
    double sr = 48000; float hp[2] {}, lp[2] {};
};

class BigFuzz : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { for (auto& v : lp) v = 0; for (auto& v : hp) v = 0; env = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float gain = 5.0f + p.a * 120.0f;
        const float lpA = onePoleCoef (500.0f, sr), hpA = onePoleCoef (1500.0f, sr);
        const float lvl = dbToGain ((p.d - 0.5f) * 30.0f) * 0.5f;
        const float gateTh = p.c * p.c * 0.05f;
        for (int i = 0; i < n; ++i)
        {
            env = juce::jmax (std::abs (L[i]) + std::abs (R[i]), env * 0.9995f);
            const float gate = gateTh <= 0 ? 1.0f : juce::jlimit (0.0f, 1.0f, (env - gateTh) / (gateTh + 1.0e-6f));
            float* ch[2] = { &L[i], &R[i] };
            for (int c = 0; c < 2; ++c)
            {
                float x = *ch[c] * gain;
                x = x > 0 ? 1.0f - std::exp (-x) : -0.8f * (1.0f - std::exp (x));      // asymmetric
                x = std::tanh (x * 2.0f);
                lp[c] += lpA * (x - lp[c]);
                hp[c] += hpA * (x - hp[c]);
                const float hiPart = x - hp[c];
                const float y = lp[c] * (1.0f - p.b) + hiPart * p.b;                     // Big-Muff style tone blend
                *ch[c] = y * lvl * gate * 1.6f;
            }
        }
    }
    double sr = 48000; float lp[2] {}, hp[2] {}, env = 0;
};

class RatBox : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { for (auto& v : lp) v = 0; for (auto& v : hp) v = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float gain = 2.0f + p.a * p.a * 400.0f;
        const float hpA = onePoleCoef (60.0f, sr), lpA = onePoleCoef (12000.0f * std::pow (0.04f, p.b), sr);
        const float lvl = dbToGain ((p.c - 0.5f) * 30.0f) * 0.4f;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                hp[c] += hpA * (ch[c][i] - hp[c]);
                float y = juce::jlimit (-1.0f, 1.0f, (ch[c][i] - hp[c]) * gain);
                lp[c] += lpA * (y - lp[c]);
                ch[c][i] = lp[c] * lvl;
            }
    }
    double sr = 48000; float lp[2] {}, hp[2] {};
};

class BitGrit : public RackUnit
{
public:
    void prepare (double, int) override { reset(); }
    void reset() override { count = 0; held[0] = held[1] = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float steps = std::exp2 (16.0f - p.a * 13.0f);
        const int hold = 1 + (int) (p.b * p.b * 48.0f);
        const float drive = 1.0f + p.c * 6.0f;
        for (int i = 0; i < n; ++i)
        {
            if (++count >= hold)
            {
                count = 0;
                held[0] = std::round (std::tanh (L[i] * drive) * steps) / steps;
                held[1] = std::round (std::tanh (R[i] * drive) * steps) / steps;
            }
            L[i] = held[0]; R[i] = held[1];
        }
    }
    int count = 0; float held[2] {};
};

// ===================================================================================== amps
class Amp : public RackUnit
{
public:
    explicit Amp (int m) : model (m) {}
    void prepare (double s, int) override { sr = s; reset(); last[0] = last[1] = last[2] = -1; }
    void reset() override { for (auto& b : stack) b.reset(); for (auto& v : hp) v = 0; for (auto& v : lp) v = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        if (p.b != last[0] || p.c != last[1] || p.d != last[2])
        {
            last[0] = p.b; last[1] = p.c; last[2] = p.d;
            using C = juce::dsp::IIR::Coefficients<float>;
            stack[0].set (C::makeLowShelf (sr, 120.0, 0.7, dbToGain ((p.b - 0.5f) * 24.0f)));
            stack[1].set (C::makePeakFilter (sr, model == 2 ? 650.0 : 800.0, 0.8, dbToGain ((p.c - 0.5f) * 24.0f)));
            stack[2].set (C::makeHighShelf (sr, 3200.0, 0.7, dbToGain ((p.d - 0.5f) * 24.0f)));
        }
        static const float gainRange[3] = { 6.0f, 30.0f, 140.0f };
        const float gain = 1.0f + p.a * p.a * gainRange[model];
        const float hpA = onePoleCoef (model == 0 ? 80.0f : 140.0f, sr);
        const float lpA = onePoleCoef (model == 0 ? 9000.0f : 6500.0f, sr);
        const float out = 0.6f / std::sqrt (juce::jmax (1.0f, gain * 0.2f));
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i] * gain;
                x = std::tanh (x + 0.1f * x * x);                        // first triode (a little even harmonic)
                hp[c] += hpA * (x - hp[c]);
                x = x - hp[c];                                            // coupling cap
                if (model > 0) x = std::tanh (x * (1.0f + p.a * 3.0f));   // second stage
                x = stack[2].run (c, stack[1].run (c, stack[0].run (c, x)));
                lp[c] += lpA * (x - lp[c]);
                ch[c][i] = std::tanh (lp[c] * 1.2f) * out * 1.4f;         // power amp
            }
    }
    int model; double sr = 48000; float last[3] {}; Biquad stack[3]; float hp[2] {}, lp[2] {};
};

// ===================================================================================== cabinets
class Cabinet : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; room.prepare ((int) (s * 0.05)); reset(); lastA = lastB = -1; }
    void reset() override { for (auto& b : f) b.reset(); room.clear(); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        if (p.a != lastA || p.b != lastB)
        {
            lastA = p.a; lastB = p.b;
            using C = juce::dsp::IIR::Coefficients<float>;
            const int m = juce::jlimit (0, 3, (int) (p.a * 3.999f));
            static const float hpF[4] = { 90, 75, 65, 300 }, bumpF[4] = { 120, 110, 95, 900 }, lpF[4] = { 5500, 5000, 4200, 3000 };
            static const float bumpG[4] = { 3, 4, 6, 6 }, notchF[4] = { 1800, 1500, 1200, 2200 };
            const float mic = 0.6f + p.b * 0.8f;   // dark .. bright
            f[0].set (C::makeHighPass (sr, hpF[m], 0.7));
            f[1].set (C::makePeakFilter (sr, bumpF[m], 1.2, dbToGain (bumpG[m])));
            f[2].set (C::makePeakFilter (sr, notchF[m], 1.5, dbToGain (-4.0f)));
            f[3].set (C::makeLowPass (sr, juce::jmin (16000.0f, lpF[m] * mic), 0.8));
            f[4].set (C::makeLowPass (sr, juce::jmin (16000.0f, lpF[m] * mic * 1.3f), 0.6));
        }
        const float lvl = dbToGain ((p.d - 0.6f) * 30.0f) * 1.4f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            for (int k = 0; k < 5; ++k) { l = f[k].run (0, l); r = f[k].run (1, r); }
            room.write (l, r);
            const float er = p.c * 0.5f;
            l += er * (room.read (1, sr * 0.011) * 0.6f + room.read (0, sr * 0.023) * 0.4f);
            r += er * (room.read (0, sr * 0.013) * 0.6f + room.read (1, sr * 0.029) * 0.4f);
            L[i] = l * lvl; R[i] = r * lvl;
        }
    }
    double sr = 48000; Biquad f[5]; RingBuffer room; float lastA = -1, lastB = -1;
};

class IrLoader : public RackUnit
{
public:
    void prepare (double s, int maxBlock) override
    {
        sr = s;
        conv.prepare ({ s, (juce::uint32) juce::jmax (1, maxBlock), 2 });
        tmp.setSize (2, juce::jmax (1, maxBlock));
        if (file.existsAsFile()) loadFile (file);
        lastA = lastB = -1;
    }
    void reset() override { conv.reset(); for (auto& b : f) b.reset(); }
    void loadFile (const juce::File& fl) override
    {
        file = fl;
        if (fl.existsAsFile())
        {
            conv.loadImpulseResponse (fl, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, 0,
                                      juce::dsp::Convolution::Normalise::yes);
            loaded = true;
        }
    }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        if (p.a != lastA || p.b != lastB)
        {
            lastA = p.a; lastB = p.b;
            using C = juce::dsp::IIR::Coefficients<float>;
            f[0].set (C::makeHighPass (sr, 20.0 * std::pow (25.0, p.a), 0.7));
            f[1].set (C::makeLowPass (sr, 1000.0 * std::pow (18.0, p.b), 0.7));
        }
        const float lvl = dbToGain ((p.c - 0.6f) * 30.0f);
        if (loaded && n <= tmp.getNumSamples())
        {
            tmp.copyFrom (0, 0, L, n); tmp.copyFrom (1, 0, R, n);
            juce::dsp::AudioBlock<float> block (tmp.getArrayOfWritePointers(), 2, (size_t) n);
            conv.process (juce::dsp::ProcessContextReplacing<float> (block));
            std::copy (tmp.getReadPointer (0), tmp.getReadPointer (0) + n, L);
            std::copy (tmp.getReadPointer (1), tmp.getReadPointer (1) + n, R);
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = f[1].run (0, f[0].run (0, L[i])) * lvl;
            R[i] = f[1].run (1, f[0].run (1, R[i])) * lvl;
        }
    }
    double sr = 48000; juce::dsp::Convolution conv; juce::AudioBuffer<float> tmp; juce::File file; bool loaded = false;
    Biquad f[2]; float lastA = -1, lastB = -1;
};

// ===================================================================================== delays
class TapeEcho : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 4.5)); reset(); }
    void reset() override { buf.clear(); lp[0] = lp[1] = 0; ph = 0; smoothD = -1; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        const double target = juce::jmin ((double) buf.size - 8, syncBeats (p.a) * 60.0 / t.bpm * sr);
        if (smoothD < 0) smoothD = target;
        const float fb = p.b * 1.02f;
        const float lpA = onePoleCoef (1500.0f + p.d * 9000.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            smoothD += (target - smoothD) * 0.0005;
            ph += 0.8 / sr; if (ph >= 1) ph -= 1;
            const double d = smoothD * (1.0 + p.c * 0.004 * std::sin (ph * MathConstants<double>::twoPi));
            const float yl = buf.read (0, d), yr = buf.read (1, d);
            lp[0] += lpA * (yl - lp[0]); lp[1] += lpA * (yr - lp[1]);
            buf.write (std::tanh (L[i] + lp[1] * fb), std::tanh (R[i] + lp[0] * fb));   // slight ping-pong
            L[i] = lp[0]; R[i] = lp[1];
        }
    }
    double sr = 48000, ph = 0, smoothD = -1; RingBuffer buf; float lp[2] {};
};

class Multitap : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 6.5)); reset(); }
    void reset() override { buf.clear(); lp = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        const double base = juce::jmin ((double) buf.size / 3.2, syncBeats (p.a) * 60.0 / t.bpm * sr);
        const double t1 = base, t2 = base * (2.0 + p.b * 0.5), t3 = base * (3.0 + p.b);
        const float fb = p.c * 0.85f;
        const float lpA = onePoleCoef (1200.0f + p.d * 12000.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = buf.read (0, t1), b = buf.read (0, t2), c = buf.read (0, t3);
            lp += lpA * (c - lp);
            buf.write (0.5f * (L[i] + R[i]) + lp * fb, 0.0f);
            L[i] = a * 0.9f + b * 0.45f;
            R[i] = b * 0.45f + c * 0.9f;
        }
    }
    double sr = 48000; RingBuffer buf; float lp = 0;
};

class GranularDelay : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 3.0)); reset(); }
    void reset() override { buf.clear(); for (auto& g : grains) g.active = false; untilNext = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        const double delay = juce::jmin ((double) buf.size * 0.8, syncBeats (p.a) * 60.0 / t.bpm * sr);
        const double grainLen = sr * (0.02 + p.b * p.b * 0.3);
        const double ratio = std::exp2 (std::round ((p.d - 0.5f) * 24.0f) / 12.0);
        const double spawnEvery = grainLen / (1.5 + p.c * 6.0);
        for (int i = 0; i < n; ++i)
        {
            buf.write (L[i] + fbL * 0.35f, R[i] + fbR * 0.35f);
            if (--untilNext <= 0)
            {
                untilNext = (int) (spawnEvery * (0.6 + rng.nextDouble() * 0.8));
                for (auto& g : grains)
                    if (! g.active)
                    {
                        g.active = true; g.age = 0; g.len = grainLen;
                        g.start = delay + (rng.nextDouble() * 2.0 - 1.0) * p.c * sr * 0.25 + grainLen * juce::jmax (0.0, ratio - 1.0);
                        g.pan = (float) (rng.nextDouble() * 2.0 - 1.0) * p.c;
                        break;
                    }
            }
            float l = 0, r = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const double u = g.age / g.len;
                const float w = (float) (0.5 - 0.5 * std::cos (u * MathConstants<double>::twoPi));
                const double d = juce::jlimit (2.0, (double) buf.size - 4.0, g.start - g.age * (ratio - 1.0));
                const float s = 0.5f * (buf.read (0, d) + buf.read (1, d)) * w;
                l += s * (1.0f - g.pan) * 0.5f; r += s * (1.0f + g.pan) * 0.5f;
                g.age += 1.0;
                if (g.age >= g.len) g.active = false;
            }
            fbL = l; fbR = r;
            L[i] = l * 1.3f; R[i] = r * 1.3f;
        }
    }
    struct Grain { bool active = false; double age = 0, len = 1, start = 0; float pan = 0; };
    double sr = 48000; RingBuffer buf; std::array<Grain, 24> grains; int untilNext = 0; juce::Random rng { 7 }; float fbL = 0, fbR = 0;
};

// ===================================================================================== reverbs
class Hall : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; verb.setSampleRate (s); pre.prepare ((int) (s * 0.3)); reset(); }
    void reset() override { verb.reset(); pre.clear(); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.65f * p.a; rp.damping = p.b; rp.width = p.c; rp.wetLevel = 0.33f; rp.dryLevel = 0.0f;
        verb.setParameters (rp);
        const double d = p.d * 0.25 * sr;
        for (int i = 0; i < n; ++i) { pre.write (L[i], R[i]); L[i] = pre.read (0, d + 1); R[i] = pre.read (1, d + 1); }
        verb.processStereo (L, R, n);
    }
    double sr = 48000; juce::Reverb verb; RingBuffer pre;
};

class Shimmer : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; verb.setSampleRate (s); shift.prepare ((int) (s * 0.2)); reset(); }
    void reset() override { verb.reset(); shift.clear(); ph = 0; std::fill (std::begin (prevL), std::end (prevL), 0.0f); std::fill (std::begin (prevR), std::end (prevR), 0.0f); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.5f + 0.45f * p.a; rp.damping = p.c; rp.width = p.d; rp.wetLevel = 0.33f; rp.dryLevel = 0.0f;
        verb.setParameters (rp);
        const double win = sr * 0.08;
        for (int i = 0; i < n; ++i)
        {
            // octave-up pitch shifter on the reverb tail (two crossfaded read heads)
            shift.write (prevL[i], prevR[i]);   // last block's reverb output (32 samples behind)
            ph += 1.0 / win; if (ph >= 1) ph -= 1;
            const double d1 = (1.0 - ph) * win + 2, d2 = (1.0 - std::fmod (ph + 0.5, 1.0)) * win + 2;
            const float w1 = (float) std::sin (ph * MathConstants<double>::pi), w2 = (float) std::sin (std::fmod (ph + 0.5, 1.0) * MathConstants<double>::pi);
            const float sl = shift.read (0, d1) * w1 + shift.read (0, d2) * w2;
            const float sr2 = shift.read (1, d1) * w1 + shift.read (1, d2) * w2;
            L[i] = L[i] + std::tanh (sl * p.b * 0.35f);
            R[i] = R[i] + std::tanh (sr2 * p.b * 0.35f);
        }
        verb.processStereo (L, R, n);
        for (int i = 0; i < n; ++i) { prevL[i] = std::tanh (L[i]) * 0.8f; prevR[i] = std::tanh (R[i]) * 0.8f; L[i] *= 0.7f; R[i] *= 0.7f; }
    }
    double sr = 48000, ph = 0; juce::Reverb verb; RingBuffer shift; float prevL[32] {}, prevR[32] {};
};

// ===================================================================================== modulation
class Ensemble : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 0.05)); reset(); }
    void reset() override { buf.clear(); for (auto& v : ph) v = 0; ph[1] = 0.33; ph[2] = 0.66; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const double rate = 0.15 + p.a * 2.0, depth = sr * 0.004 * p.b;
        for (int i = 0; i < n; ++i)
        {
            buf.write (L[i], R[i]);
            float o[3];
            for (int v = 0; v < 3; ++v)
            {
                ph[v] += rate * (1.0 + v * 0.13) / sr; if (ph[v] >= 1) ph[v] -= 1;
                const double fast = std::sin ((ph[v] * 7.0) * MathConstants<double>::twoPi) * 0.15;
                o[v] = 0.5f * (buf.read (0, sr * 0.007 + depth * (1.0 + std::sin (ph[v] * MathConstants<double>::twoPi) + fast))
                             + buf.read (1, sr * 0.007 + depth * (1.0 + std::sin (ph[v] * MathConstants<double>::twoPi) + fast)));
            }
            const float w = p.c;
            L[i] = (o[0] + o[1] * (1.0f - w * 0.5f) + o[2] * (1.0f - w)) * 0.45f;
            R[i] = (o[2] + o[1] * (1.0f - w * 0.5f) + o[0] * (1.0f - w)) * 0.45f;
        }
    }
    double sr = 48000, ph[3] {}; RingBuffer buf;
};

class PhaserUnit : public RackUnit
{
public:
    void prepare (double s, int maxBlock) override { phaser.prepare ({ s, (juce::uint32) maxBlock, 2 }); tmp.setSize (2, maxBlock); }
    void reset() override { phaser.reset(); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        phaser.setRate (0.05f + p.a * p.a * 8.0f); phaser.setDepth (p.b); phaser.setFeedback (p.c * 1.8f - 0.9f);
        phaser.setCentreFrequency (200.0f * std::pow (15.0f, p.d)); phaser.setMix (1.0f);
        if (n > tmp.getNumSamples()) return;
        tmp.copyFrom (0, 0, L, n); tmp.copyFrom (1, 0, R, n);
        juce::dsp::AudioBlock<float> block (tmp.getArrayOfWritePointers(), 2, (size_t) n);
        phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
        std::copy (tmp.getReadPointer (0), tmp.getReadPointer (0) + n, L);
        std::copy (tmp.getReadPointer (1), tmp.getReadPointer (1) + n, R);
    }
    juce::dsp::Phaser<float> phaser; juce::AudioBuffer<float> tmp;
};

class Flanger : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 0.03)); reset(); }
    void reset() override { buf.clear(); ph = 0; yl = yr = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const double rate = 0.03 + p.a * p.a * 4.0;
        const float fb = (p.c * 2.0f - 1.0f) * 0.92f;
        for (int i = 0; i < n; ++i)
        {
            ph += rate / sr; if (ph >= 1) ph -= 1;
            const double m = 0.5 + 0.5 * std::sin (ph * MathConstants<double>::twoPi);
            const double mr = 0.5 + 0.5 * std::sin ((ph + 0.25) * MathConstants<double>::twoPi);
            buf.write (L[i] + yl * fb, R[i] + yr * fb);
            yl = buf.read (0, sr * (0.0005 + 0.006 * p.b * m) + 1);
            yr = buf.read (1, sr * (0.0005 + 0.006 * p.b * mr) + 1);
            L[i] = 0.6f * (L[i] + yl); R[i] = 0.6f * (R[i] + yr);
        }
    }
    double sr = 48000, ph = 0; RingBuffer buf; float yl = 0, yr = 0;
};

class Octaver : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { lp = lp2 = subLp = 0; flip = 1; prev = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float a = onePoleCoef (900.0f, sr), a2 = onePoleCoef (350.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (L[i] + R[i]);
            lp += a * (x - lp);
            lp2 += a * (lp - lp2);
            if ((prev <= 0 && lp2 > 0)) flip = -flip;              // flip-flop on each upward zero crossing = octave down
            prev = lp2;
            subLp += a2 * (std::abs (lp2) * flip * 2.0f - subLp);
            const float up = std::abs (x) * 1.5f - 0.3f * std::abs (lp);   // full-wave rectifier = octave up
            const float wet = subLp * p.a * 1.6f + up * p.b;
            L[i] = L[i] * p.c + wet; R[i] = R[i] * p.c + wet;
        }
    }
    double sr = 48000; float lp = 0, lp2 = 0, subLp = 0, flip = 1, prev = 0;
};

class Tremolo : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        const double beats = syncBeats (0.3f + (1.0f - p.a) * 0.5f);   // 1/16 .. 1/2
        const double phase0 = std::fmod (t.ppq / beats, 1.0);
        const double inc = t.bpm / 60.0 / sr / beats;
        for (int i = 0; i < n; ++i)
        {
            const double ph = phase0 + inc * i;
            auto lfo = [&] (double x)
            {
                const float s = (float) std::sin (x * MathConstants<double>::twoPi);
                const float sq = s > 0 ? 1.0f : -1.0f;
                return s + (sq - s) * p.c;                      // sine .. square
            };
            const float m1 = lfo (ph), m2 = lfo (ph + 0.5 * p.d);
            L[i] *= 1.0f - p.b * (0.5f + 0.5f * m1);
            R[i] *= 1.0f - p.b * (0.5f + 0.5f * (p.d > 0.01f ? m2 : m1));
        }
    }
    double sr = 48000;
};

// ===================================================================================== filters / dynamics
class AutoWah : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { env = 0; for (auto& v : ic1) v = 0; for (auto& v : ic2) v = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float att = onePoleCoef (60.0f, sr), rel = onePoleCoef (6.0f, sr);
        const float k = 2.0f - 1.9f * p.c;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::abs (L[i]) + std::abs (R[i]);
            env += (lvl > env ? att : rel) * (lvl - env);
            float e = juce::jlimit (0.0f, 1.0f, env * (1.0f + p.a * 20.0f));
            if (p.d < 0.5f) e = 1.0f - e;
            const float fc = 200.0f * std::pow (20.0f, juce::jlimit (0.0f, 1.0f, p.b + e * 0.7f));
            const float g = std::tan (MathConstants<float>::pi * juce::jmin (fc, (float) sr * 0.45f) / (float) sr);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            float* ch[2] = { &L[i], &R[i] };
            for (int c = 0; c < 2; ++c)
            {
                const float v3 = *ch[c] - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2 * v1 - ic1[c]; ic2[c] = 2 * v2 - ic2[c];
                *ch[c] = v1 * k * 4.0f;
            }
        }
    }
    double sr = 48000; float env = 0, ic1[2] {}, ic2[2] {};
};

class EqUnit : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; last = -1; reset(); }
    void reset() override { for (auto& b : f) b.reset(); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float key = p.a + p.b * 3 + p.c * 7 + p.d * 13;
        if (key != last)
        {
            last = key;
            using C = juce::dsp::IIR::Coefficients<float>;
            f[0].set (C::makeLowShelf (sr, 120.0, 0.7, dbToGain ((p.a - 0.5f) * 30.0f)));
            f[1].set (C::makePeakFilter (sr, 200.0 * std::pow (25.0, p.c), 1.0, dbToGain ((p.b - 0.5f) * 30.0f)));
            f[2].set (C::makeHighShelf (sr, 6000.0, 0.7, dbToGain ((p.d - 0.5f) * 30.0f)));
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = f[2].run (0, f[1].run (0, f[0].run (0, L[i])));
            R[i] = f[2].run (1, f[1].run (1, f[0].run (1, R[i])));
        }
    }
    double sr = 48000; Biquad f[3]; float last = -1;
};

class Compressor : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { env = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float th = -40.0f + p.a * 40.0f, ratio = 1.0f + p.b * p.b * 19.0f;
        const float att = 1.0f - std::exp (-1.0f / ((0.0005f + p.c * p.c * 0.08f) * (float) sr));
        const float rel = 1.0f - std::exp (-1.0f / ((0.02f + p.d * 0.8f) * (float) sr));
        const float makeup = dbToGain (-th * (1.0f - 1.0f / ratio) * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            const float x2 = 0.5f * (L[i] * L[i] + R[i] * R[i]);
            env += (x2 > env ? att : rel) * (x2 - env);
            const float db = 10.0f * std::log10 (env + 1.0e-12f);
            const float over = db - th;
            const float gr = over > 0 ? -over * (1.0f - 1.0f / ratio) : 0.0f;
            const float g = dbToGain (gr) * makeup;
            L[i] *= g; R[i] *= g;
        }
    }
    double sr = 48000; float env = 0;
};

class Maximizer : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; gain = 1; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float in = dbToGain (p.a * 18.0f), ceiling = dbToGain ((p.b - 1.0f) * 6.0f);
        const float rel = 1.0f - std::exp (-1.0f / ((0.01f + p.d * 0.4f) * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * in, r = R[i] * in;
            if (p.c > 0.001f)
            {
                const float d = 1.0f + p.c * 2.0f;
                l = l + (std::tanh (l * d) / d * 1.5f - l) * p.c;
                r = r + (std::tanh (r * d) / d * 1.5f - r) * p.c;
            }
            const float pk = juce::jmax (std::abs (l), std::abs (r));
            const float want = pk * gain > ceiling ? ceiling / juce::jmax (pk, 1.0e-6f) : 1.0f;
            gain = want < gain ? want : gain + rel * (want - gain);
            L[i] = juce::jlimit (-ceiling, ceiling, l * gain);
            R[i] = juce::jlimit (-ceiling, ceiling, r * gain);
        }
    }
    double sr = 48000; float gain = 1;
};

// ===================================================================================== lo-fi
class TapeUnit : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; buf.prepare ((int) (s * 0.06)); reset(); }
    void reset() override { buf.clear(); wow = flut = 0; lp[0] = lp[1] = 0; hiss[0] = hiss[1] = 0; drift = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        const float lpA = onePoleCoef (18000.0f - 15000.0f * p.d, sr);
        const float sat = 1.0f + p.d * 3.0f, satM = 1.0f / std::tanh (sat);
        for (int i = 0; i < n; ++i)
        {
            wow += 0.5 / sr; if (wow >= 1) wow -= 1;
            flut += 7.1 / sr; if (flut >= 1) flut -= 1;
            drift += 0.0003f * ((rng.nextFloat() * 2.0f - 1.0f) - drift);
            const double m = std::sin (wow * MathConstants<double>::twoPi) * p.b + std::sin (flut * MathConstants<double>::twoPi) * p.c * 0.25 + drift * 6.0f * p.b;
            const double d = sr * (0.008 + 0.003 * m) + 4;
            buf.write (L[i], R[i]);
            float l = buf.read (0, d), r = buf.read (1, d);
            l = std::tanh (l * sat) * satM; r = std::tanh (r * sat) * satM;
            lp[0] += lpA * (l - lp[0]); lp[1] += lpA * (r - lp[1]);
            hiss[0] += 0.45f * ((rng.nextFloat() * 2.0f - 1.0f) - hiss[0]);
            hiss[1] += 0.45f * ((rng.nextFloat() * 2.0f - 1.0f) - hiss[1]);
            L[i] = lp[0] + hiss[0] * p.a * p.a * 0.06f;
            R[i] = lp[1] + hiss[1] * p.a * p.a * 0.06f;
        }
    }
    double sr = 48000, wow = 0, flut = 0; RingBuffer buf; float lp[2] {}, hiss[2] {}, drift = 0; juce::Random rng { 3 };
};

class Vinyl : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; brakeBuf.prepare ((int) (s * 3.0)); reset(); lastWear = -1; }
    void reset() override { brakeBuf.clear(); lag = 0; rumblePh = 0; pop = 0; for (auto& b : f) b.reset(); }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime&) override
    {
        if (p.c != lastWear)
        {
            lastWear = p.c;
            using C = juce::dsp::IIR::Coefficients<float>;
            f[0].set (C::makeHighPass (sr, 30.0 + p.c * 250.0, 0.7));
            f[1].set (C::makeLowPass (sr, 16000.0 * std::pow (0.25, p.c), 0.7));
        }
        const float speed = 1.0f - p.d;   // Brake: 1 = normal speed, 0 = stopped
        for (int i = 0; i < n; ++i)
        {
            brakeBuf.write (L[i], R[i]);
            float l = L[i], r = R[i];
            if (p.d > 0.001f)
            {
                lag += 1.0 - speed;                                    // read head falls behind as it slows
                if (lag > brakeBuf.size - 8) lag = brakeBuf.size - 8;
                l = brakeBuf.read (0, lag + 1) * (0.3f + 0.7f * speed);
                r = brakeBuf.read (1, lag + 1) * (0.3f + 0.7f * speed);
            }
            else if (lag > 0) lag = lag < 1.0 ? 0.0 : lag * 0.995;         // ease back to normal when released
            l = f[1].run (0, f[0].run (0, l)); r = f[1].run (1, f[0].run (1, r));
            const float mono = 0.5f * (l + r);
            l = l + (mono - l) * p.c * 0.6f; r = r + (mono - r) * p.c * 0.6f;   // worn records lose stereo
            // crackle and pops
            float cr = 0.0f;
            if (rng.nextFloat() < p.a * p.a * 0.004f) pop = (rng.nextFloat() * 2.0f - 1.0f) * (0.2f + 0.8f * rng.nextFloat());
            cr += pop; pop *= 0.6f;
            if (rng.nextFloat() < p.a * 0.0004f) pop = rng.nextBool() ? 0.9f : -0.9f;
            cr += (rng.nextFloat() * 2.0f - 1.0f) * p.a * 0.004f;
            rumblePh += 33.3 / 60.0 / sr; if (rumblePh >= 1) rumblePh -= 1;
            const float rumble = (float) std::sin (rumblePh * MathConstants<double>::twoPi * 30.0) * p.b * 0.02f
                               + (float) std::sin (rumblePh * MathConstants<double>::twoPi) * p.b * 0.01f;
            L[i] = l + (cr + rumble) * 0.6f;
            R[i] = r + (cr * 0.8f + rumble) * 0.6f;
        }
    }
    double sr = 48000, lag = 0, rumblePh = 0; RingBuffer brakeBuf; Biquad f[2]; float lastWear = -1, pop = 0; juce::Random rng { 11 };
};

// ===================================================================================== time FX
// Half Time: records a beat-synced window and plays it back at half speed.
class HalfTime : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; rec.setSize (2, (int) (s * 8.5)); reset(); }
    void reset() override { rec.clear(); writeIdx = 0; windowStart = -1; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        static const double lens[] = { 1.0, 2.0, 4.0, 8.0 };
        const double winBeats = lens[juce::jlimit (0, 3, (int) (p.a * 3.999f))];
        const int winLen = juce::jmin (rec.getNumSamples() / 2 - 1, (int) (winBeats * 60.0 / t.bpm * sr));
        const bool tapePitch = p.b > 0.5f;
        const double grain = sr * (0.03 + p.c * 0.12);
        for (int i = 0; i < n; ++i)
        {
            const double ppq = t.ppq + i * t.bpm / 60.0 / sr;
            const long win = (long) std::floor (ppq / winBeats);
            if (win != windowStart) { windowStart = win; writeIdx = 0; }   // new window: start over
            const int w = juce::jmin (writeIdx, rec.getNumSamples() - 1);
            rec.setSample (0, w, L[i]); rec.setSample (1, w, R[i]);
            const double prog = (double) writeIdx;               // samples into this window
            ++writeIdx;
            if (prog >= winLen) continue;
            float l, r;
            if (tapePitch)
            {
                // read at half rate: pitch drops an octave, like slowing tape
                const double rp = prog * 0.5;
                const int i0 = (int) rp; const float fr = (float) (rp - i0);
                l = rec.getSample (0, i0) + fr * (rec.getSample (0, i0 + 1) - rec.getSample (0, i0));
                r = rec.getSample (1, i0) + fr * (rec.getSample (1, i0 + 1) - rec.getSample (1, i0));
            }
            else
            {
                // granular: position moves at half speed, grains play at normal speed (pitch kept)
                const double ph = std::fmod (prog, grain) / grain;
                auto tap = [&] (double phase, int ch)
                {
                    // each grain started where the half-speed position was, then plays at normal speed
                    const double src = juce::jmin (prog, 0.5 * prog + 0.5 * phase * grain);
                    const int i0 = (int) src; const float fr = (float) (src - i0);
                    return rec.getSample (ch, i0) + fr * (rec.getSample (ch, juce::jmin (i0 + 1, w)) - rec.getSample (ch, i0));
                };
                const double ph2 = std::fmod (ph + 0.5, 1.0);
                const float w1 = (float) std::sin (ph * MathConstants<double>::pi), w2 = (float) std::sin (ph2 * MathConstants<double>::pi);
                l = tap (ph, 0) * w1 + tap (ph2, 0) * w2;
                r = tap (ph, 1) * w1 + tap (ph2, 1) * w2;
            }
            L[i] = l; R[i] = r;
        }
    }
    double sr = 48000; juce::AudioBuffer<float> rec; int writeIdx = 0; long windowStart = -1;
};

// Reverse: each beat-synced chunk is played backwards one chunk later.
class Reverse : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; for (auto& b : bufs) b.setSize (2, (int) (s * 4.5)); reset(); }
    void reset() override { for (auto& b : bufs) b.clear(); idx = 0; cur = 0; lastLen = 0; win = -1; playThis = true; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        const double beats = syncBeats (0.2f + p.a * 0.8f);    // 1/16 .. 1 bar
        const int maxLen = bufs[0].getNumSamples() - 1;
        for (int i = 0; i < n; ++i)
        {
            const double ppq = t.ppq + i * t.bpm / 60.0 / sr;
            const long w = (long) std::floor (ppq / beats);
            if (w != win)
            {
                win = w; lastLen = idx; idx = 0; cur = 1 - cur;
                playThis = rng.nextFloat() <= p.b;
            }
            auto& recBuf = bufs[(size_t) cur];
            auto& playBuf = bufs[(size_t) (1 - cur)];
            if (idx < maxLen) { recBuf.setSample (0, idx, L[i]); recBuf.setSample (1, idx, R[i]); }
            if (playThis && lastLen > 0 && idx < lastLen)
            {
                const int rp = lastLen - 1 - idx;
                const float fadeLen = juce::jmax (1.0f, p.c * 0.25f * (float) lastLen);
                const float fade = juce::jmin (1.0f, juce::jmin ((float) idx, (float) (lastLen - idx)) / fadeLen);
                L[i] = playBuf.getSample (0, rp) * fade; R[i] = playBuf.getSample (1, rp) * fade;
            }
            ++idx;
        }
    }
    double sr = 48000; std::array<juce::AudioBuffer<float>, 2> bufs; int idx = 0, cur = 0, lastLen = 0; long win = -1; bool playThis = true; juce::Random rng { 5 };
};

// Stutter: on beat boundaries (by chance) it grabs a tiny slice and repeats it for the rest of the beat.
class Stutter : public RackUnit
{
public:
    void prepare (double s, int) override { sr = s; slice.setSize (2, (int) (s * 1.1)); reset(); }
    void reset() override { slice.clear(); active = false; beat = -1; recIdx = 0; playIdx = 0; repeat = 0; }
    void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) override
    {
        static const double sliceBeats[] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0 };   // 1/64 .. 1/4 note
        const double sb = sliceBeats[juce::jlimit (0, 4, (int) ((1.0f - p.a) * 4.999f))];
        const int sliceLen = juce::jlimit (16, slice.getNumSamples() - 1, (int) (sb * 60.0 / t.bpm * sr));
        for (int i = 0; i < n; ++i)
        {
            const double ppq = t.ppq + i * t.bpm / 60.0 / sr;
            const long b = (long) std::floor (ppq * 2.0);              // decisions every half beat (1/8 note)
            if (b != beat)
            {
                beat = b;
                active = rng.nextFloat() < p.b;
                recIdx = 0; playIdx = 0; repeat = 0;
            }
            if (! active) continue;
            if (recIdx < sliceLen)
            {
                slice.setSample (0, recIdx, L[i]); slice.setSample (1, recIdx, R[i]);
                ++recIdx;
                continue;                                               // first pass plays through
            }
            const float gateLen = juce::jmax (0.05f, p.c) * (float) sliceLen;
            const float rate = std::exp2 (-p.d * 0.25f * (float) repeat);   // each repeat a little lower
            const double rp = playIdx * rate;
            if (rp >= sliceLen) { playIdx = 0; ++repeat; }
            const int i0 = juce::jmin ((int) (playIdx * rate), sliceLen - 1);
            const float env = (float) playIdx < gateLen ? juce::jmin (1.0f, juce::jmin ((float) playIdx, gateLen - (float) playIdx) / 32.0f) : 0.0f;
            L[i] = slice.getSample (0, i0) * env; R[i] = slice.getSample (1, i0) * env;
            ++playIdx;
        }
    }
    double sr = 48000; juce::AudioBuffer<float> slice; bool active = false; long beat = -1; int recIdx = 0, playIdx = 0, repeat = 0; juce::Random rng { 9 };
};

// ===================================================================================== factory
std::unique_ptr<RackUnit> createRackUnit (int type)
{
    std::unique_ptr<RackUnit> u;
    switch (type)
    {
        case 1: u = std::make_unique<Screamer>(); break;
        case 2: u = std::make_unique<BigFuzz>(); break;
        case 3: u = std::make_unique<RatBox>(); break;
        case 4: u = std::make_unique<BitGrit>(); break;
        case 5: u = std::make_unique<Amp> (0); break;
        case 6: u = std::make_unique<Amp> (1); break;
        case 7: u = std::make_unique<Amp> (2); break;
        case 8: u = std::make_unique<Cabinet>(); break;
        case 9: u = std::make_unique<IrLoader>(); break;
        case 10: u = std::make_unique<TapeEcho>(); break;
        case 11: u = std::make_unique<Multitap>(); break;
        case 12: u = std::make_unique<GranularDelay>(); break;
        case 13: u = std::make_unique<Shimmer>(); break;
        case 14: u = std::make_unique<Hall>(); break;
        case 15: u = std::make_unique<Ensemble>(); break;
        case 16: u = std::make_unique<PhaserUnit>(); break;
        case 17: u = std::make_unique<Flanger>(); break;
        case 18: u = std::make_unique<Octaver>(); break;
        case 19: u = std::make_unique<Tremolo>(); break;
        case 20: u = std::make_unique<AutoWah>(); break;
        case 21: u = std::make_unique<EqUnit>(); break;
        case 22: u = std::make_unique<Compressor>(); break;
        case 23: u = std::make_unique<Maximizer>(); break;
        case 24: u = std::make_unique<TapeUnit>(); break;
        case 25: u = std::make_unique<Vinyl>(); break;
        case 26: u = std::make_unique<HalfTime>(); break;
        case 27: u = std::make_unique<Reverse>(); break;
        case 28: u = std::make_unique<Stutter>(); break;
        default: return nullptr;
    }
    u->type = type;
    return u;
}

// ===================================================================================== rack
void FxRack::bind (juce::AudioProcessorValueTreeState& s)
{
    for (int i = 0; i < numRackSlots; ++i)
    {
        const juce::String pre = "rk" + juce::String (i + 1);
        auto& r = refs[(size_t) i];
        r.type = s.getRawParameterValue (pre + "_type"); r.on = s.getRawParameterValue (pre + "_on");
        r.a = s.getRawParameterValue (pre + "_a"); r.b = s.getRawParameterValue (pre + "_b");
        r.c = s.getRawParameterValue (pre + "_c"); r.d = s.getRawParameterValue (pre + "_d");
        r.mix = s.getRawParameterValue (pre + "_mix");
    }
    split = s.getRawParameterValue ("rk_split");
    xover = s.getRawParameterValue ("rk_xover");
}

void FxRack::prepare (double sr, int mb)
{
    sampleRate = sr; maxBlock = juce::jmax (32, mb);
    low.setSize (2, maxBlock); high.setSize (2, maxBlock); dry.setSize (2, maxBlock);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    lpX.prepare (spec); hpX.prepare (spec);
    lpX.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hpX.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    builtType.fill (-1);    // rebuild everything for the new rate
    sync();
}

void FxRack::sync()
{
    bool changed = false;
    std::array<std::shared_ptr<RackUnit>, numRackSlots> next = pending;
    for (int i = 0; i < numRackSlots; ++i)
    {
        const int t = refs[(size_t) i].type != nullptr ? (int) refs[(size_t) i].type->load() : 0;
        if (t == builtType[(size_t) i]) continue;
        builtType[(size_t) i] = t;
        std::shared_ptr<RackUnit> u (createRackUnit (t));
        if (u != nullptr)
        {
            if (irFiles[(size_t) i].existsAsFile()) u->loadFile (irFiles[(size_t) i]);
            u->prepare (sampleRate, maxBlock);
        }
        if (next[(size_t) i] != nullptr) keepAlive.push_back ({ next[(size_t) i], juce::Time::getMillisecondCounter() });
        next[(size_t) i] = u;
        changed = true;
    }
    if (changed)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = next;
    }
    // free old units once the audio thread has certainly let go
    const auto now = juce::Time::getMillisecondCounter();
    while (! keepAlive.empty() && now - keepAlive.front().second > 2000 && keepAlive.front().first.use_count() == 1)
        keepAlive.pop_front();
}

void FxRack::setIrFile (int slot, const juce::File& f)
{
    if (! juce::isPositiveAndBelow (slot, numRackSlots)) return;
    irFiles[(size_t) slot] = f;
    if (auto u = pending[(size_t) slot]) u->loadFile (f);   // Convolution loads on its own background thread
}

void FxRack::process (juce::AudioBuffer<float>& buf, const RackTime& t)
{
    if (lock.tryEnter()) { active = pending; lock.exit(); }
    const int n = buf.getNumSamples();
    if (buf.getNumChannels() < 2) return;
    bool any = false;
    for (int i = 0; i < numRackSlots; ++i) any = any || (active[(size_t) i] != nullptr && refs[(size_t) i].on->load() > 0.5f);
    if (! any) return;

    auto runSlot = [&] (int i, float* L, float* R, int len)
    {
        auto& u = active[(size_t) i];
        const auto& r = refs[(size_t) i];
        if (u == nullptr || r.on->load() < 0.5f) return;
        const float mix = r.mix->load();
        if (mix <= 0.0001f) return;
        float dl[32], drr[32];
        for (int s0 = 0; s0 < len; s0 += 32)
        {
            const int m = juce::jmin (32, len - s0);
            std::copy (L + s0, L + s0 + m, dl); std::copy (R + s0, R + s0 + m, drr);
            RackTime tt = t;
            tt.ppq += (double) s0 * t.bpm / 60.0 / sampleRate;
            u->process (L + s0, R + s0, m, { r.a->load(), r.b->load(), r.c->load(), r.d->load() }, tt);
            if (mix < 0.9999f)
                for (int k = 0; k < m; ++k) { L[s0 + k] = dl[k] + (L[s0 + k] - dl[k]) * mix; R[s0 + k] = drr[k] + (R[s0 + k] - drr[k]) * mix; }
        }
    };

    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    if (split->load() > 0.5f && n <= maxBlock)
    {
        const float x = xover->load();
        if (std::abs (x - lastX) > 0.5f) { lpX.setCutoffFrequency (x); hpX.setCutoffFrequency (x); lastX = x; }
        for (int ch = 0; ch < 2; ++ch) { low.copyFrom (ch, 0, buf, ch, 0, n); high.copyFrom (ch, 0, buf, ch, 0, n); }
        juce::dsp::AudioBlock<float> lb (low.getArrayOfWritePointers(), 2, (size_t) n), hb (high.getArrayOfWritePointers(), 2, (size_t) n);
        lpX.process (juce::dsp::ProcessContextReplacing<float> (lb));
        hpX.process (juce::dsp::ProcessContextReplacing<float> (hb));
        for (int i = 0; i < 4; ++i) runSlot (i, low.getWritePointer (0), low.getWritePointer (1), n);
        for (int i = 4; i < 8; ++i) runSlot (i, high.getWritePointer (0), high.getWritePointer (1), n);
        for (int ch = 0; ch < 2; ++ch) { buf.copyFrom (ch, 0, low, ch, 0, n); buf.addFrom (ch, 0, high, ch, 0, n); }
    }
    else
        for (int i = 0; i < numRackSlots; ++i) runSlot (i, L, R, n);

    // safety: never let a unit blow up the output
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i) if (! std::isfinite (d[i])) d[i] = 0.0f;
    }
}
} // namespace ff
