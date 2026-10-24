#include "PatchPage.h"
#include "FmPage.h"
#include "../PluginProcessor.h"
#include "../dsp/Modules.h"

using namespace juce;

namespace ff::gui
{
PatchPage::PatchPage (FourfoldProcessor& p, std::function<void (Knob&)> reg) : proc (p), registerKnob (std::move (reg))
{
    setOpaque (true);
    struct Def { const char* pre; const char* name; Colour c; int levelDest; const char* panId; const char* panLabel; };
    const Def defs[5] = { { "wt", "WAVETABLE", col::wavetable, dstWtLevel, "wt_pan", "Pan" },
                          { "an", "ANALOG", col::analog, dstAnLevel, "an_pan", "Pan" },
                          { "gr", "GRANULAR", col::granular, dstGrLevel, "gr_stereo", "Width" },
                          { "res", "RESONATOR", col::resonator, dstResLevel, "res_pan", "Pan" },
                          { "fm", "FM", fmCol, dstFmLevel, "fm_pan", "Pan" } };
    for (int e = 0; e < 5; ++e)
    {
        auto& ch = channels[(size_t) e];
        const auto& d = defs[e];
        ch.colour = d.c; ch.name = d.name;
        ch.on = toggle (String (d.pre) + "_on", d.name, d.c);
        ch.level = knob (String (d.pre) + "_level", "Level", d.c, d.levelDest);
        ch.pan = knob (d.panId, d.panLabel, d.c);
        for (int s = 0; s < 2; ++s)
        {
            const String sp = String (d.pre) + "_s" + String (s + 1);
            ch.slotType[s] = combo (sp + "_type", slotTypeNames());
            ch.slotKnob[s][0] = knob (sp + "_a", "A", d.c);
            ch.slotKnob[s][1] = knob (sp + "_b", "B", d.c);
            ch.slotKnob[s][2] = knob (sp + "_mix", "Mix", d.c);
        }
    }

    moKnobs = { knob ("mo_pitch", "Pitch", col::mod, dstMoPitch), knob ("mo_ring", "Ring", col::mod), knob ("mo_phase", "Phase", col::mod),
                knob ("mo_pos", "Position", col::mod), knob ("mo_ffm", "Filter FM", col::mod) };
    moTarget = combo ("mo_ringdst", StringArray { "All engines", "Wavetable", "Analog", "Granular", "Resonator", "FM" });
    noiseColour = combo ("noise_color", StringArray { "White", "Pink", "Brown", "Digital", "Crackle" });
    noiseKnobs = { knob ("noise_level", "Noise", col::text, dstNoiseLevel), knob ("fb_amt", "Feedback", col::filter, dstFeedback) };
    f1On = toggle ("flt_on", "FILTER 1", col::filter);
    f1Type = combo ("flt_type", filterTypeNames());
    f1Knobs = { knob ("flt_cutoff", "Cutoff", col::filter, dstCutoff), knob ("flt_res", "Reso", col::filter, dstResonance),
                knob ("flt_drive", "Drive", col::filter, dstFltDrive), knob ("flt_env", "Env 2", col::filter) };
    f2On = toggle ("flt2_on", "FILTER 2", col::filter);
    f2Type = combo ("flt2_type", filterTypeNames());
    f2Knobs = { knob ("flt2_cutoff", "Cutoff", col::filter, dstCutoff2), knob ("flt2_res", "Reso", col::filter, dstRes2),
                knob ("flt2_drive", "Drive", col::filter), knob ("flt2_env", "Env 2", col::filter) };
    route = combo ("flt_route", StringArray { "Series", "Parallel" });
    f2Knobs.push_back (knob ("flt_balance", "F1 <> F2", col::filter));
    glideCurve = combo ("glide_curve", StringArray { "Smooth", "Linear", "Glissando" });
    velCurve = combo ("vel_curve", StringArray { "Linear", "Soft touch", "Hard touch", "Fixed" });
    voiceKnobs = { knob ("glide", "Glide", col::text), knob ("amp_vel", "Velocity", col::text) };
    route->setTooltip ("Series: filter 1 then filter 2.  Parallel: both side by side, blended with F1 <> F2");
    moTarget->setTooltip ("Which engine the mod oscillator ring-modulates");
    startTimerHz (8);
    timerCallback();
}

PatchPage::~PatchPage() { stopTimer(); }

Knob* PatchPage::knob (const String& id, const String& label, Colour c, int dest)
{
    auto* k = knobs.add (new Knob (proc.apvts, id, label, c, dest));
    addAndMakeVisible (k);
    if (registerKnob) registerKnob (*k);
    return k;
}

ComboBox* PatchPage::combo (const String& id, const StringArray& items)
{
    auto* b = combos.add (new ComboBox());
    b->addItemList (items, 1);
    addAndMakeVisible (b);
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *b));
    return b;
}

ToggleButton* PatchPage::toggle (const String& id, const String& text, Colour c)
{
    auto* t = toggles.add (new ToggleButton (text));
    t->setColour (ToggleButton::tickColourId, c);
    addAndMakeVisible (t);
    buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *t));
    return t;
}

void PatchPage::timerCallback()
{
    // knob names follow the module type; unused knobs dim
    for (auto& ch : channels)
        for (int s = 0; s < 2; ++s)
        {
            const int t = ch.slotType[s]->getSelectedItemIndex();
            ch.slotKnob[s][0]->setLabelText (ModuleSlot::labelA (t));
            ch.slotKnob[s][1]->setLabelText (ModuleSlot::labelB (t));
            for (auto* k : ch.slotKnob[s]) k->setAlpha (t > 0 ? 1.0f : 0.35f);
        }
    const bool f2 = f2On->getToggleState();
    for (auto* k : f2Knobs) k->setAlpha (f2 ? 1.0f : 0.4f);
    f2Knobs.back()->setAlpha (f2 && route->getSelectedItemIndex() == 1 ? 1.0f : 0.35f);
}

void PatchPage::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (236);
    r.removeFromBottom (30);   // the mix bus line
    mixLine = Rectangle<int> (0, r.getBottom() + 4, getWidth(), 22);
    const int gap = 8;
    const int cw = (r.getWidth() - 4 * gap) / 5;
    for (int e = 0; e < 5; ++e)
    {
        auto& ch = channels[(size_t) e];
        ch.area = Rectangle<int> (r.getX() + e * (cw + gap), r.getY(), cw, r.getHeight());
        auto a = ch.area.reduced (10, 8);
        auto head = a.removeFromTop (26);
        ch.on->setBounds (head.withWidth (140));
        a.removeFromTop (4);
        auto lp = a.removeFromTop (76);
        ch.level->setBounds (lp.removeFromLeft (lp.getWidth() / 2).reduced (6, 0));
        ch.pan->setBounds (lp.reduced (6, 0));
        a.removeFromTop (8);
        const int sh = (a.getHeight() - 8) / 2;
        for (int s = 0; s < 2; ++s)
        {
            ch.slotArea[s] = a.removeFromTop (sh);
            a.removeFromTop (8);
            auto sa = ch.slotArea[s].reduced (8, 6);
            sa.removeFromTop (14);
            ch.slotType[s]->setBounds (sa.removeFromTop (24));
            sa.removeFromTop (4);
            const int kw = sa.getWidth() / 3;
            for (int k = 0; k < 3; ++k) ch.slotKnob[s][k]->setBounds (sa.removeFromLeft (kw));
        }
    }

    moArea = bottom.removeFromLeft (360); bottom.removeFromLeft (gap);
    voiceArea = bottom.removeFromRight (200); bottom.removeFromRight (gap);
    noiseArea = bottom.removeFromLeft (180); bottom.removeFromLeft (gap);
    filterArea = bottom;

    auto lay = [] (std::vector<Knob*>& ks, Rectangle<int> area)
    {
        const int w = area.getWidth() / juce::jmax (1, (int) ks.size());
        for (auto* k : ks) k->setBounds (area.removeFromLeft (w).reduced (2, 0));
    };
    {
        auto a = moArea.reduced (10, 8); a.removeFromTop (22);
        auto row = a.removeFromTop (24);
        row.removeFromLeft (100);
        moTarget->setBounds (row);
        a.removeFromTop (16);
        lay (moKnobs, a.removeFromTop (90));
    }
    {
        auto a = noiseArea.reduced (10, 8); a.removeFromTop (22);
        noiseColour->setBounds (a.removeFromTop (24));
        a.removeFromTop (16);
        lay (noiseKnobs, a.removeFromTop (90));
    }
    {
        auto a = filterArea.reduced (10, 8); a.removeFromTop (22);
        auto left = a.removeFromLeft (a.getWidth() * 4 / 9);
        auto right = a;
        right.removeFromLeft (10);
        auto h1 = left.removeFromTop (24);
        f1On->setBounds (h1.removeFromLeft (96)); h1.removeFromLeft (6); f1Type->setBounds (h1);
        left.removeFromTop (16);
        lay (f1Knobs, left.removeFromTop (90));
        auto h2 = right.removeFromTop (24);
        f2On->setBounds (h2.removeFromLeft (96)); h2.removeFromLeft (6);
        route->setBounds (h2.removeFromRight (96)); h2.removeFromRight (6);
        f2Type->setBounds (h2);
        right.removeFromTop (16);
        lay (f2Knobs, right.removeFromTop (90));
    }
    {
        auto a = voiceArea.reduced (10, 8); a.removeFromTop (22);
        glideCurve->setBounds (a.removeFromTop (24));
        a.removeFromTop (6);
        velCurve->setBounds (a.removeFromTop (24));
        a.removeFromTop (10);
        lay (voiceKnobs, a.removeFromTop (80));
    }
}

void PatchPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    auto panel = [&g] (Rectangle<int> r, const String& t, Colour accent, bool dim = false)
    {
        g.setColour (dim ? col::panel.darker (0.2f) : col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        if (t.isNotEmpty())
        {
            g.setColour (col::dim);
            g.setFont (font (11.0f, true));
            g.drawText (t, r.reduced (10, 6).removeFromTop (16), Justification::centredLeft);
        }
    };
    for (auto& ch : channels)
    {
        const bool on = ch.on->getToggleState();
        panel (ch.area, {}, ch.colour, ! on);
        for (int s = 0; s < 2; ++s)
        {
            auto sa = ch.slotArea[s].toFloat();
            g.setColour (col::bg.withAlpha (0.6f));
            g.fillRoundedRectangle (sa, 6.0f);
            g.setColour (col::border);
            g.drawRoundedRectangle (sa.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (col::dim);
            g.setFont (font (9.5f, true));
            g.drawText ("SLOT " + String (s + 1), ch.slotArea[s].reduced (8, 4).removeFromTop (12), Justification::centredLeft);
        }
        // flow line down to the bus
        g.setColour (on ? ch.colour.withAlpha (0.7f) : col::border);
        const float x = (float) ch.area.getCentreX();
        g.drawLine (x, (float) ch.area.getBottom(), x, (float) mixLine.getCentreY(), 2.0f);
    }
    // mix bus
    g.setColour (col::border.brighter (0.2f));
    g.drawLine ((float) channels[0].area.getCentreX(), (float) mixLine.getCentreY(), (float) channels[4].area.getCentreX(),
                (float) mixLine.getCentreY(), 2.0f);
    g.drawArrow (Line<float> ((float) filterArea.getCentreX(), (float) mixLine.getCentreY(), (float) filterArea.getCentreX(),
                              (float) filterArea.getY() - 1.0f), 2.0f, 8.0f, 7.0f);
    g.setColour (col::dim);
    g.setFont (font (10.0f, true));
    g.drawText ("MIX", Rectangle<int> (channels[2].area.getCentreX() - 30, mixLine.getY() - 2, 60, 12), Justification::centred);

    panel (moArea, "MOD OSCILLATOR  (ring / phase / position / filter FM)", col::mod);
    {
        g.setColour (col::dim);
        g.setFont (font (10.5f));
        g.drawText ("Ring target", moArea.reduced (10, 8).withTrimmedTop (22).removeFromTop (24), Justification::centredLeft);
    }
    panel (noiseArea, "NOISE + FEEDBACK", col::text);
    panel (filterArea, "FILTERS", col::filter);
    panel (voiceArea, "GLIDE + VELOCITY", col::text);
}
} // namespace ff::gui
