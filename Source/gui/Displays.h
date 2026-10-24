#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"
#include "../dsp/Resonator.h"

namespace ff::gui
{
// Serum-style stacked view of the wavetable with the current frame highlighted.
class WavetableDisplay : public juce::Component
{
public:
    explicit WavetableDisplay (FourfoldProcessor& p) : proc (p) {}

    void update()
    {
        auto t = proc.getDisplayTable();
        float pos = proc.getLiveWtPos();
        if (pos < 0.0f) pos = proc.apvts.getRawParameterValue ("wt_pos")->load();
        const int warp = (int) proc.apvts.getRawParameterValue ("wt_warp")->load();
        const float warpAmt = proc.apvts.getRawParameterValue ("wt_warpamt")->load();
        if (t.get() != table.get() || std::abs (pos - position) > 0.002f || warp != warpMode || std::abs (warpAmt - warpAmount) > 0.002f)
        {
            table = t; position = pos; warpMode = warp; warpAmount = warpAmt;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 6.0f);
        if (table == nullptr) return;

        const int frames = table->getNumFrames();
        const int shown = juce::jmin (frames, 16);
        auto area = r.reduced (10.0f, 8.0f);
        const float depthX = area.getWidth() * 0.18f, depthY = area.getHeight() * 0.35f;
        const float w = area.getWidth() - depthX, h = area.getHeight() - depthY;

        auto drawFrame = [&] (float pos01, float offsetT, juce::Colour c, float thick)
        {
            juce::Path p;
            const float ox = area.getX() + depthX * (1.0f - offsetT);
            const float oy = area.getY() + depthY * offsetT;
            const int pts = 160;
            for (int i = 0; i <= pts; ++i)
            {
                const float ph = (float) i / pts;
                const float v = table->read (pos01, ph, 3);
                const float x = ox + ph * w;
                const float y = oy + h * 0.5f - v * h * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thick));
        };

        for (int i = 0; i < shown; ++i)
        {
            const float t = shown > 1 ? (float) i / (shown - 1) : 0.0f;
            drawFrame (t, t, col::wavetable.withAlpha (0.12f), 1.0f);
        }
        drawFrame (position, position, col::wavetable, 2.0f);

        g.setColour (col::dim);
        g.setFont (font (10.0f));
        g.drawText (table->name + "  |  frame " + juce::String (1 + (int) std::round (position * (frames - 1))) + "/" + juce::String (frames),
                    r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::topLeft);
    }

private:
    FourfoldProcessor& proc;
    WavetableBank::Ptr table;
    float position = -1.0f, warpAmount = 0.0f;
    int warpMode = 0;
};

// Waveform of the granular source, with the play position and spray zone.
class GrainDisplay : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit GrainDisplay (FourfoldProcessor& p) : proc (p) {}
    std::function<void (const juce::File&)> onFileDropped;

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (onFileDropped && ! files.isEmpty()) onFileDropped (juce::File (files[0]));
    }

    void update()
    {
        auto s = proc.getDisplayGrains();
        float pos = proc.getLiveGrPos();
        if (pos < 0.0f) pos = proc.apvts.getRawParameterValue ("gr_pos")->load();
        const float spray = proc.apvts.getRawParameterValue ("gr_spray")->load();
        if (s.get() != source.get())
        {
            source = s;
            rebuildPeaks();
            repaint();
        }
        if (std::abs (pos - position) > 0.001f || std::abs (spray - sprayAmt) > 0.002f)
        {
            position = pos; sprayAmt = spray;
            repaint();
        }
    }

    void resized() override { rebuildPeaks(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 6.0f);
        auto area = r.reduced (6.0f, 14.0f);

        // spray zone
        const float cx = area.getX() + position * area.getWidth();
        const float sw = sprayAmt * area.getWidth() * 0.5f;
        g.setColour (col::granular.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (cx - sw, area.getY(), sw * 2.0f, area.getHeight()).getIntersection (area));

        // waveform
        g.setColour (col::granular.withAlpha (0.65f));
        const float mid = area.getCentreY();
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float x = area.getX() + (float) i;
            g.drawVerticalLine ((int) x, mid - peaks[i] * area.getHeight() * 0.5f, mid + peaks[i] * area.getHeight() * 0.5f);
        }

        g.setColour (juce::Colours::white);
        g.drawVerticalLine ((int) cx, area.getY() - 4.0f, area.getBottom() + 4.0f);

        g.setColour (col::dim);
        g.setFont (font (10.0f));
        g.drawText (source != nullptr ? source->name : juce::String(), r.reduced (8.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);
        g.drawText ("drop audio here", r.reduced (8.0f, 2.0f).toNearestInt(), juce::Justification::bottomRight);
    }

private:
    void rebuildPeaks()
    {
        peaks.clear();
        if (source == nullptr) return;
        const int w = juce::jmax (1, getWidth() - 12);
        const auto& b = source->buffer;
        const int len = b.getNumSamples();
        peaks.resize ((size_t) w);
        for (int x = 0; x < w; ++x)
        {
            const int s0 = (int) ((juce::int64) x * len / w);
            const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * len / w));
            peaks[(size_t) x] = juce::jmin (1.0f, b.getMagnitude (0, s0, s1 - s0));
        }
    }

    FourfoldProcessor& proc;
    GrainSource::Ptr source;
    std::vector<float> peaks;
    float position = 0.0f, sprayAmt = 0.0f;
};

// Shows the analog engine's current wave shape.
class AnalogDisplay : public juce::Component
{
public:
    explicit AnalogDisplay (FourfoldProcessor& p) : proc (p) {}

    void update()
    {
        const float s = proc.apvts.getRawParameterValue ("an_shape")->load();
        const float w = proc.apvts.getRawParameterValue ("an_pw")->load();
        const float sub = proc.apvts.getRawParameterValue ("an_sub")->load();
        if (std::abs (s - shape) > 0.002f || std::abs (w - pw) > 0.002f || std::abs (sub - subLvl) > 0.002f)
        {
            shape = s; pw = w; subLvl = sub;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 6.0f);
        auto area = r.reduced (12.0f, 14.0f);
        juce::Path p;
        const int pts = 300;
        for (int i = 0; i <= pts; ++i)
        {
            const float ph = std::fmod ((float) i / pts * 2.0f, 1.0f);   // two cycles
            const float tri = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            const float saw = 2.0f * ph - 1.0f;
            const float pulse = ph < pw ? 1.0f : -1.0f;
            float v = shape <= 1.0f ? tri + (saw - tri) * shape : saw + (pulse - saw) * (shape - 1.0f);
            v += subLvl * std::sin ((float) i / pts * juce::MathConstants<float>::twoPi);
            v /= (1.0f + subLvl);
            const float x = area.getX() + (float) i / pts * area.getWidth();
            const float y = area.getCentreY() - v * area.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::analog.withAlpha (0.15f));
        g.strokePath (p, juce::PathStrokeType (6.0f));
        g.setColour (col::analog);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }

private:
    FourfoldProcessor& proc;
    float shape = -1.0f, pw = 0.5f, subLvl = 0.0f;
};

// Shows the Resonator's partials (or a vibrating string for the string models).
class ResonatorDisplay : public juce::Component
{
public:
    explicit ResonatorDisplay (FourfoldProcessor& p) : proc (p) {}

    void update()
    {
        auto& a = proc.apvts;
        const int t = (int) a.getRawParameterValue ("res_type")->load();
        const float b = a.getRawParameterValue ("res_bright")->load();
        const float d = a.getRawParameterValue ("res_damp")->load();
        const float bz = a.getRawParameterValue ("res_buzz")->load();
        if (t != type || std::abs (b - bright) > 0.003f || std::abs (d - damp) > 0.003f || std::abs (bz - buzz) > 0.003f)
        {
            type = t; bright = b; damp = d; buzz = bz;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 6.0f);
        auto area = r.reduced (12.0f, 16.0f);

        if (type <= 1)
        {
            // a plucked string: a few harmonics frozen mid-vibration
            juce::Path p;
            const int pts = 200;
            for (int i = 0; i <= pts; ++i)
            {
                const float x = (float) i / pts;
                float y = 0.0f;
                for (int k = 1; k <= 8; ++k)
                    y += std::sin (juce::MathConstants<float>::pi * k * x) * std::pow ((float) k, -(2.2f - bright * 1.6f)) * (k % 2 ? 1.0f : 0.6f);
                if (buzz > 0.01f || type == 1) y += (type == 1 ? 0.25f : buzz * 0.3f) * std::sin (x * 60.0f) * std::sin (juce::MathConstants<float>::pi * x);
                const float px = area.getX() + x * area.getWidth();
                const float py = area.getCentreY() - y * area.getHeight() * 0.38f;
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            g.setColour (col::resonator.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (6.0f));
            g.setColour (col::resonator);
            g.strokePath (p, juce::PathStrokeType (1.8f));
        }
        else
        {
            // partials: position = frequency ratio, height = loudness, width = how long it rings
            const auto& modes = modesFor (type);
            float maxRatio = 1.0f;
            for (auto& m : modes) maxRatio = juce::jmax (maxRatio, m.ratio);
            maxRatio *= 1.1f;
            g.setColour (col::border);
            g.drawHorizontalLine ((int) area.getBottom(), area.getX(), area.getRight());
            for (auto& m : modes)
            {
                const float x = area.getX() + std::log2 (m.ratio) / std::log2 (maxRatio) * area.getWidth() * 0.95f + 4.0f;
                const float tilt = std::pow (m.ratio, -(1.0f - bright) * 1.5f);
                const float h = juce::jlimit (0.03f, 1.0f, m.gain * tilt) * area.getHeight();
                const float life = std::pow (m.decay, 0.3f + 2.0f * damp);
                g.setColour (col::resonator.withAlpha (0.25f + 0.75f * life));
                g.fillRoundedRectangle (x - 2.0f, area.getBottom() - h, 4.0f, h, 2.0f);
            }
        }

        g.setColour (col::dim);
        g.setFont (font (10.0f));
        g.drawText (resonatorTypeNames()[type], r.reduced (8.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);
    }

private:
    FourfoldProcessor& proc;
    int type = -1;
    float bright = 0, damp = 0, buzz = 0;
};

} // namespace ff::gui
