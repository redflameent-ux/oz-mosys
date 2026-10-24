#pragma once
// MacroBar: eight macro knobs under every page (rename by double-clicking the name,
// drag the name onto any knob to modulate it) with a Controls / Keys switch.
// ModStrip: Pigments-style row of modulation sources with live traces.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ff::gui
{
// ---------------------------------------------------------------------------- macro name tag
class MacroTag : public juce::Label
{
public:
    MacroTag (int src) : source (src)
    {
        setEditable (false, true, false);
        setJustificationType (juce::Justification::centred);
        setFont (ff::gui::font (11.5f, true));
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Label::backgroundColourId, col::mod.withAlpha (0.12f));
        setColour (juce::Label::outlineColourId, col::mod.withAlpha (0.45f));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Double-click to rename.  Drag onto any knob to make this macro move it.");
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4 || isBeingEdited()) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) c->startDragging ("modsrc:" + juce::String (source), this);
    }
    const int source;
};

class MacroBar : public juce::Component, private juce::Timer
{
public:
    MacroBar (FourfoldProcessor& p, juce::Component& keyboardToSwap) : proc (p), keyboard (keyboardToSwap)
    {
        for (int i = 0; i < 8; ++i)
        {
            const int src = i < 4 ? srcMacro1 + i : srcMacro5 + i - 4;
            auto* k = knobs.add (new Knob (proc.apvts, "macro" + juce::String (i + 1), {}, col::mod));
            addAndMakeVisible (k);
            auto* t = tags.add (new MacroTag (src));
            t->onTextChange = [this, i, t]
            {
                auto txt = t->getText().trim().substring (0, 18);
                if (txt.isEmpty()) txt = "Macro " + juce::String (i + 1);
                proc.macroNames.set (i, txt);
                t->setText (txt, juce::dontSendNotification);
            };
            addAndMakeVisible (t);
        }
        for (auto* b : { &controlsBtn, &keysBtn })
        {
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, col::mod.withAlpha (0.3f));
            addAndMakeVisible (b);
        }
        controlsBtn.onClick = [this] { setKeys (false); };
        keysBtn.onClick = [this] { setKeys (true); };
        setKeys (false);
        refreshNames();
        startTimerHz (4);
    }

    void setKeys (bool k)
    {
        showKeys = k;
        keyboard.setVisible (k);
        for (auto* c : knobs) c->setVisible (! k);
        for (auto* c : tags) c->setVisible (! k);
        controlsBtn.setToggleState (! k, juce::dontSendNotification);
        keysBtn.setToggleState (k, juce::dontSendNotification);
        if (k) keyboard.toFront (false);
        repaint();
    }

    void refreshNames()
    {
        for (int i = 0; i < 8; ++i)
            if (! tags[i]->isBeingEdited() && tags[i]->getText() != proc.macroNames[i])
                tags[i]->setText (proc.macroNames[i], juce::dontSendNotification);
    }

    juce::Rectangle<int> keyboardArea() const { return getBounds().withTrimmedRight (switchW + 8); }

    void paint (juce::Graphics& g) override
    {
        if (showKeys) return;
        auto r = getLocalBounds().withTrimmedRight (switchW + 8).toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        // dividers like a hardware strip
        const float cw = r.getWidth() / 8.0f;
        g.setColour (col::border);
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + cw * i), r.getY() + 10, r.getBottom() - 10);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto sw = r.removeFromRight (switchW);
        controlsBtn.setBounds (sw.removeFromTop (sw.getHeight() / 2).reduced (0, 3));
        keysBtn.setBounds (sw.reduced (0, 3));
        r.removeFromRight (8);
        const int cw = r.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
        {
            auto cell = juce::Rectangle<int> (r.getX() + i * cw, r.getY(), cw, r.getHeight()).reduced (8, 4);
            knobs[i]->setBounds (cell.removeFromLeft (cell.getHeight() + 4).withTrimmedBottom (-10));
            cell.removeFromLeft (4);
            tags[i]->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 22));
        }
    }

    std::function<void (int src, int dest)> onModDropped;
    juce::OwnedArray<Knob> knobs;

private:
    void timerCallback() override { refreshNames(); }
    static constexpr int switchW = 74;
    FourfoldProcessor& proc;
    juce::Component& keyboard;
    juce::OwnedArray<MacroTag> tags;
    juce::TextButton controlsBtn { "Controls" }, keysBtn { "Keys" };
    bool showKeys = false;
};

// ---------------------------------------------------------------------------- live mod strip
class ModStrip : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    struct Def { int src; const char* name; juce::Colour colour; bool bipolar; };

    explicit ModStrip (FourfoldProcessor& p) : proc (p)
    {
        const juce::Colour env (0xffff8a4c), lfo (0xfff2c14e), fn (0xff6fdc8c), perf (0xffe05fa8), rnd (0xff9a7cff);
        defs = { { srcEnv2, "ENV 2", env, false }, { srcEnv3, "ENV 3", env, false },
                 { srcLfo1, "LFO 1", lfo, true }, { srcLfo2, "LFO 2", lfo, true },
                 { srcMseg1, "MSEG 1", fn, false }, { srcMseg2, "MSEG 2", fn, false }, { srcMseg3, "MSEG 3", fn, false }, { srcMseg4, "MSEG 4", fn, false },
                 { srcVelocity, "VEL", perf, false }, { srcModWheel, "MOD W", perf, false }, { srcAftertouch, "AFTER", perf, false },
                 { srcKeyTrack, "KEY", perf, true }, { srcSlide, "SLIDE", perf, true }, { srcRandom, "RAND", rnd, true } };
        for (auto& d : defs) { juce::ignoreUnused (d); history.emplace_back(); history.back().fill (0.0f); }
        setTooltip ("Drag a source onto any knob to modulate it.  Lit = in use.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const int n = (int) defs.size();
        const float cw = (float) getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            auto r = juce::Rectangle<float> (i * cw, 0.0f, cw, (float) getHeight()).reduced (1.5f, 0.0f);
            const auto& d = defs[(size_t) i];
            const bool on = used[(size_t) i];
            g.setColour (on ? col::panelHi : col::panel);
            g.fillRoundedRectangle (r, 4.0f);
            if (i == hover) { g.setColour (d.colour.withAlpha (0.15f)); g.fillRoundedRectangle (r, 4.0f); }
            g.setColour (on ? d.colour : col::dim.withAlpha (0.7f));
            g.setFont (font (9.5f, true));
            g.drawText (d.name, r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centred);
            auto plot = r.reduced (4.0f, 3.0f);
            const auto& h = history[(size_t) i];
            juce::Path p;
            for (int k = 0; k < histLen; ++k)
            {
                const float v = h[(size_t) ((head + k) % histLen)];
                const float y = d.bipolar ? plot.getCentreY() - v * plot.getHeight() * 0.5f : plot.getBottom() - juce::jlimit (0.0f, 1.0f, v) * plot.getHeight();
                const float x = plot.getX() + plot.getWidth() * (float) k / (histLen - 1);
                k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
            }
            g.setColour ((on ? d.colour : col::dim).withAlpha (on ? 0.95f : 0.45f));
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4) return;
        const int i = indexAt (e.getMouseDownX());
        if (i < 0) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
            {
                juce::Image img (juce::Image::ARGB, 60, 18, true);
                juce::Graphics g (img);
                g.setColour (defs[(size_t) i].colour);
                g.fillRoundedRectangle (0, 0, 60, 18, 9);
                g.setColour (juce::Colours::black);
                g.setFont (font (10.0f, true));
                g.drawText (defs[(size_t) i].name, 0, 0, 60, 18, juce::Justification::centred);
                c->startDragging ("modsrc:" + juce::String (defs[(size_t) i].src), this, juce::ScaledImage (img));
            }
    }

private:
    int indexAt (int x) const { const int n = (int) defs.size(); const int i = x * n / juce::jmax (1, getWidth()); return juce::isPositiveAndBelow (i, n) ? i : -1; }

    void timerCallback() override
    {
        if (! isShowing()) return;
        const bool playing = proc.activeVoices.load() > 0;
        head = (head + 1) % histLen;
        const int write = (head + histLen - 1) % histLen;
        for (size_t i = 0; i < defs.size(); ++i)
        {
            const int s = defs[i].src;
            float v = proc.modView[s].load (std::memory_order_relaxed);
            if (s == srcModWheel) v = proc.liveModWheel();
            else if (! playing) v = history[i][(size_t) ((write + histLen - 1) % histLen)] * 0.85f;
            history[i][(size_t) write] = v;
        }
        // which sources are wired in the matrix?
        for (size_t i = 0; i < defs.size(); ++i) used[i] = false;
        for (int slot = 1; slot <= numModSlots; ++slot)
            if (auto* sp = proc.apvts.getRawParameterValue ("mod" + juce::String (slot) + "_src"))
                if (auto* ap = proc.apvts.getRawParameterValue ("mod" + juce::String (slot) + "_amt"); ap != nullptr && std::abs (ap->load()) > 0.001f)
                    for (size_t i = 0; i < defs.size(); ++i)
                        if ((int) sp->load() == defs[i].src) used[i] = true;
        repaint();
    }

    FourfoldProcessor& proc;
    std::vector<Def> defs;
    static constexpr int histLen = 48;
    std::vector<std::array<float, histLen>> history;
    std::array<bool, 32> used {};
    int head = 0, hover = -1;
};
} // namespace ff::gui
