#include "EnvelopePage.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static const Colour msegCol { 0xff5fe08c };

// ============================================================================ MSEG editor
MsegEditor::MsegEditor (FourfoldProcessor& p, int i) : proc (p), index (i) { reload(); }

void MsegEditor::reload()
{
    shape = *proc.getMseg (index);
    repaint();
}

void MsegEditor::commit()
{
    proc.setMseg (index, shape);
    if (onChange) onChange();
    repaint();
}

Rectangle<float> MsegEditor::plot() const { return getLocalBounds().toFloat().reduced (14.0f, 6.0f).withTrimmedTop (12.0f).withTrimmedBottom (12.0f); }
double MsegEditor::viewLength() const { return frozenLength > 0 ? frozenLength : juce::jmax (1.0, shape.length() * 1.15); }
float MsegEditor::xForTime (double t) const { auto r = plot(); return r.getX() + (float) (t / viewLength()) * r.getWidth(); }
double MsegEditor::timeForX (float x) const { auto r = plot(); return juce::jmax (0.0, (double) (x - r.getX()) / r.getWidth() * viewLength()); }
float MsegEditor::yForLevel (float l) const { auto r = plot(); return r.getBottom() - l * r.getHeight(); }
float MsegEditor::levelForY (float y) const { auto r = plot(); return juce::jlimit (0.0f, 1.0f, (r.getBottom() - y) / r.getHeight()); }

int MsegEditor::pointAt (Point<float> p) const
{
    for (int i = (int) shape.points.size() - 1; i >= 0; --i)
        if (Point<float> (xForTime (shape.points[(size_t) i].time), yForLevel (shape.points[(size_t) i].level)).getDistanceFrom (p) < 8.0f)
            return i;
    return -1;
}

int MsegEditor::segmentAt (Point<float> p) const
{
    const double t = timeForX (p.x);
    for (int i = 0; i + 1 < (int) shape.points.size(); ++i)
        if (t >= shape.points[(size_t) i].time && t <= shape.points[(size_t) i + 1].time)
            return std::abs (yForLevel (shape.valueAt (t)) - p.y) < 14.0f ? i : -1;
    return -1;
}

int MsegEditor::stepAt (float x) const
{
    auto r = plot();
    return juce::jlimit (0, shape.numSteps - 1, (int) ((x - r.getX()) / r.getWidth() * shape.numSteps));
}

void MsegEditor::paintSteps (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (selected ? msegCol.withAlpha (0.8f) : col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, selected ? 1.5f : 1.0f);
    auto r = plot();
    const int n = shape.numSteps;
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight());
        g.setColour ((i / 4) % 2 ? Colour (0xff12151c) : Colour (0xff0f1117));
        g.fillRect (cell.reduced (1.0f, 0.0f));
        const float lvl = shape.stepLevel[(size_t) i];
        Path p;
        for (int k = 0; k <= 24; ++k)
        {
            const float u = (float) k / 24.0f;
            const float y = cell.getBottom() - MsegShape::stepCurve (shape.stepShape[(size_t) i], u) * lvl * cell.getHeight();
            if (k == 0) p.startNewSubPath (cell.getX() + 1.0f, y); else p.lineTo (cell.getX() + 1.0f + u * (w - 2.0f), y);
        }
        Path fill (p);
        fill.lineTo (cell.getRight() - 1.0f, cell.getBottom()); fill.lineTo (cell.getX() + 1.0f, cell.getBottom()); fill.closeSubPath();
        g.setColour (msegCol.withAlpha (0.15f));
        g.fillPath (fill);
        g.setColour (msegCol);
        g.strokePath (p, PathStrokeType (1.6f));
        // level handle
        g.setColour (col::text.withAlpha (0.5f));
        g.drawHorizontalLine ((int) (cell.getBottom() - lvl * cell.getHeight()), cell.getX() + 3.0f, cell.getRight() - 3.0f);
        g.setColour (col::dim);
        g.setFont (font (9.0f));
        g.drawText (String (i + 1), Rectangle<float> (cell.getX(), r.getBottom() + 1.0f, w, 11.0f).toNearestInt(), Justification::centred);
    }
    g.setColour (selected ? msegCol : col::dim);
    g.setFont (font (11.0f, true));
    g.drawText ((shape.name.isNotEmpty() ? shape.name : "MSEG " + String (index + 1)) + "  -  PERFORMER", getLocalBounds().reduced (10, 3), Justification::topLeft);
    g.setColour (col::dim);
    g.setFont (font (10.0f));
    g.drawText ("click: paint shape + level   drag: draw   Shift-drag: level only   right-click: next shape", getLocalBounds().reduced (10, 3), Justification::topRight);
}

void MsegEditor::stepMouse (const MouseEvent& e, bool isDown)
{
    auto r = plot();
    const int i = stepAt (e.position.x);
    const float lvl = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
    if (isDown && e.mods.isPopupMenu())
    {
        shape.stepShape[(size_t) i] = (shape.stepShape[(size_t) i] + 1) % MsegShape::numStepShapes;
        commit();
        return;
    }
    if (! isDown && i == lastStep && ! e.mods.isShiftDown()) { shape.stepLevel[(size_t) i] = lvl; commit(); return; }
    lastStep = i;
    shape.stepLevel[(size_t) i] = lvl;
    if (! e.mods.isShiftDown()) shape.stepShape[(size_t) i] = brush;
    commit();
}

void MsegEditor::paint (Graphics& g)
{
    if (shape.stepper) { paintSteps (g); return; }
    auto b = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (selected ? msegCol.withAlpha (0.8f) : col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, selected ? 1.5f : 1.0f);
    auto r = plot();

    // grid
    const double len = viewLength();
    const double step = shape.sync ? (len > 8 ? 1.0 : 0.25) : (len > 8 ? 1.0 : len > 3 ? 0.5 : 0.1);
    g.setFont (font (9.5f));
    for (double t = 0; t <= len + 1.0e-6; t += step)
    {
        const bool major = std::fmod (t + 1.0e-9, shape.sync ? 1.0 : 1.0) < 1.0e-6;
        g.setColour (major ? col::border.brighter (0.2f) : Colour (0xff181b23));
        g.drawVerticalLine ((int) xForTime (t), r.getY(), r.getBottom());
        if (major)
        {
            g.setColour (col::dim);
            g.drawText (shape.sync ? String ((int) t + 1) : String (t, 0) + "s", (int) xForTime (t) + 2, (int) r.getBottom() + 2, 40, 12, Justification::left);
        }
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (Colour (0xff181b23));
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
    }

    // loop region and sustain
    if (shape.loop)
    {
        const double ls = shape.loopStart >= 0 ? shape.points[(size_t) shape.loopStart].time : 0.0;
        const double le = shape.sustain >= 0 ? shape.points[(size_t) shape.sustain].time : shape.length();
        g.setColour (msegCol.withAlpha (0.08f));
        g.fillRect (Rectangle<float> (xForTime (ls), r.getY(), xForTime (le) - xForTime (ls), r.getHeight()));
        g.setColour (msegCol.withAlpha (0.6f));
        g.setFont (font (10.0f, true));
        g.drawText ("LOOP", (int) xForTime (ls) + 4, (int) r.getY() + 2, 40, 12, Justification::left);
    }
    if (shape.sustain >= 0)
    {
        const float x = xForTime (shape.points[(size_t) shape.sustain].time);
        Path dash; dash.startNewSubPath (x, r.getY()); dash.lineTo (x, r.getBottom());
        Path out; const float d[] = { 4.0f, 3.0f };
        PathStrokeType (1.0f).createDashedStroke (out, dash, d, 2);
        g.setColour (col::sampler.withAlpha (0.8f));
        g.fillPath (out);
        g.setFont (font (10.0f, true));
        g.drawText ("SUSTAIN", (int) x + 4, (int) r.getY() + 14, 60, 12, Justification::left);
    }

    // curve
    Path p, fill;
    bool first = true;
    for (float x = r.getX(); x <= xForTime (shape.length()) + 0.5f; x += 1.5f)
    {
        const float y = yForLevel (shape.valueAt (timeForX (x)));
        if (first) { p.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); fill.lineTo (x, y); first = false; }
        else { p.lineTo (x, y); fill.lineTo (x, y); }
    }
    fill.lineTo (xForTime (shape.length()), r.getBottom());
    fill.closeSubPath();
    g.setColour (msegCol.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (msegCol.withAlpha (0.2f));
    g.strokePath (p, PathStrokeType (6.0f));
    g.setColour (msegCol);
    g.strokePath (p, PathStrokeType (2.0f));

    for (int i = 0; i < (int) shape.points.size(); ++i)
    {
        const auto& pt = shape.points[(size_t) i];
        auto c = Point<float> (xForTime (pt.time), yForLevel (pt.level));
        const float rad = i == hoverPoint || i == dragPoint ? 6.5f : 5.0f;
        g.setColour (col::bg);
        g.fillEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (c));
        g.setColour (i == shape.sustain ? col::sampler : (i == shape.loopStart && shape.loop ? Colours::white : msegCol));
        g.drawEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (c), 1.8f);
    }

    g.setColour (selected ? msegCol : col::dim);
    g.setFont (font (11.0f, true));
    g.drawText (shape.name.isNotEmpty() ? shape.name : "MSEG " + String (index + 1), getLocalBounds().reduced (10, 3), Justification::topLeft);
    g.setColour (col::dim);
    g.setFont (font (10.0f));
    g.drawText (String ((int) shape.points.size()) + " / 68 points", getLocalBounds().reduced (10, 3), Justification::topRight);
}

void MsegEditor::mouseMove (const MouseEvent& e)
{
    if (shape.stepper) return;
    const int h = pointAt (e.position);
    if (h != hoverPoint) { hoverPoint = h; repaint(); }
}

void MsegEditor::mouseDown (const MouseEvent& e)
{
    if (onSelect) onSelect();
    if (shape.stepper) { lastStep = -1; stepMouse (e, true); return; }
    downPos = e.position;
    dragPoint = pointAt (e.position);
    dragSegment = -1;
    frozenLength = viewLength();

    if (e.mods.isPopupMenu())
    {
        const int pt = dragPoint;
        dragPoint = -1;
        PopupMenu m;
        if (pt >= 0)
        {
            m.addItem (1, "Delete point", pt > 0 && shape.points.size() > 2);
            m.addItem (2, "Sustain here (hold while the key is down)", true, shape.sustain == pt);
            m.addItem (3, "Loop starts here", pt < (int) shape.points.size() - 1, shape.loopStart == pt && shape.loop);
            m.addSeparator();
        }
        m.addItem (4, "No sustain", shape.sustain >= 0);
        m.addItem (5, shape.loop ? "Turn loop off" : "Turn loop on");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, pt] (int r)
        {
            if (r == 1) { shape.points.erase (shape.points.begin() + pt); if (shape.sustain >= pt) shape.sustain = shape.sustain == pt ? -1 : shape.sustain - 1; if (shape.loopStart >= pt) shape.loopStart = -1; }
            if (r == 2) shape.sustain = pt;
            if (r == 3) { shape.loopStart = pt; shape.loop = true; }
            if (r == 4) shape.sustain = -1;
            if (r == 5) { shape.loop = ! shape.loop; if (shape.loop && shape.loopStart < 0) shape.loopStart = 0; }
            if (r > 0) { shape.sanitise(); frozenLength = 0; commit(); }
        });
        return;
    }
    if (dragPoint < 0 && e.mods.isAltDown())
    {
        dragSegment = segmentAt (e.position);
        if (dragSegment >= 0) downCurve = shape.points[(size_t) dragSegment].curve;
    }
}

void MsegEditor::mouseDrag (const MouseEvent& e)
{
    if (shape.stepper) { if (! e.mods.isPopupMenu()) stepMouse (e, false); return; }
    if (dragPoint >= 0)
    {
        auto& pts = shape.points;
        const int i = dragPoint;
        if (i > 0)
        {
            double t = timeForX (e.position.x);
            if (gridStep > 0 && ! e.mods.isShiftDown()) t = std::round (t / gridStep) * gridStep;
            const double lo = pts[(size_t) i - 1].time + 0.005;
            const double hi = i + 1 < (int) pts.size() ? pts[(size_t) i + 1].time - 0.005 : 60.0;
            pts[(size_t) i].time = juce::jlimit (lo, hi, t);
        }
        pts[(size_t) i].level = levelForY (e.position.y);
        commit();
    }
    else if (dragSegment >= 0)
    {
        auto& a = shape.points[(size_t) dragSegment];
        const float dir = shape.points[(size_t) dragSegment + 1].level >= a.level ? -1.0f : 1.0f;
        a.curve = juce::jlimit (-1.0f, 1.0f, downCurve + dir * (e.position.y - downPos.y) * 0.01f);
        commit();
    }
}

void MsegEditor::mouseUp (const MouseEvent&)
{
    dragPoint = -1; dragSegment = -1;
    frozenLength = 0;
    repaint();
}

void MsegEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (shape.stepper) return;
    const int pt = pointAt (e.position);
    if (pt > 0 && shape.points.size() > 2)
    {
        shape.points.erase (shape.points.begin() + pt);
        if (shape.sustain == pt) shape.sustain = -1; else if (shape.sustain > pt) --shape.sustain;
        if (shape.loopStart >= pt) shape.loopStart = -1;
    }
    else if (pt < 0 && (int) shape.points.size() < MsegShape::maxPoints)
    {
        const double t = timeForX (e.position.x);
        const int susBefore = shape.sustain >= 0 ? (shape.points[(size_t) shape.sustain].time < t ? shape.sustain : shape.sustain + 1) : -1;
        const int loopBefore = shape.loopStart >= 0 ? (shape.points[(size_t) shape.loopStart].time < t ? shape.loopStart : shape.loopStart + 1) : -1;
        shape.points.push_back ({ t, levelForY (e.position.y), 0.0f });
        shape.sustain = susBefore; shape.loopStart = loopBefore;
    }
    shape.sanitise();
    commit();
}


// one shape in the Performer brush palette
struct BrushIcon : public Component, public SettableTooltipClient
{
    BrushIcon (int s, std::function<void (int)> pick, std::function<int()> current) : shape (s), onPick (std::move (pick)), cur (std::move (current))
    {
        setTooltip (MsegShape::stepShapeNames()[s] + " - click, then click steps to paint it");
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = cur() == shape;
        g.setColour (on ? msegCol.withAlpha (0.25f) : col::panelHi);
        g.fillRoundedRectangle (r, 3.0f);
        if (on) { g.setColour (msegCol); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
        auto p = r.reduced (4.0f, 4.0f);
        Path path;
        for (int k = 0; k <= 16; ++k)
        {
            const float u = (float) k / 16.0f;
            const float y = p.getBottom() - MsegShape::stepCurve (shape, u) * p.getHeight();
            if (k == 0) path.startNewSubPath (p.getX(), y); else path.lineTo (p.getX() + u * p.getWidth(), y);
        }
        g.setColour (on ? msegCol : col::text.withAlpha (0.7f));
        g.strokePath (path, PathStrokeType (1.3f));
    }
    void mouseDown (const MouseEvent&) override { onPick (shape); }
    int shape;
    std::function<void (int)> onPick;
    std::function<int()> cur;
};

// ============================================================================ page
static const char* gridNames[] = { "Off", "1/4", "1/8", "1/16", "1/32", "0.1 s", "0.05 s" };

EnvelopePage::EnvelopePage (FourfoldProcessor& p) : proc (p)
{
    setOpaque (true);
    list.setRowHeight (46);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    addAndMakeVisible (list);
    for (int i = 0; i < FourfoldProcessor::numMsegs; ++i)
    {
        auto* e = editors.add (new MsegEditor (proc, i));
        e->onSelect = [this, i] { select (i); };
        e->onChange = [this] { list.repaint(); refreshControls(); };
        addAndMakeVisible (e);
    }

    for (auto* b : { &stackBtn, &syncBtn, &lfoBtn, &shBtn, &shapesBtn, &assignBtn })
    {
        b->setColour (TextButton::buttonOnColourId, col::mod.withAlpha (0.3f));
        addAndMakeVisible (b);
    }
    stackBtn.onClick = [this] { stacked = ! stacked; resized(); refreshControls(); };
    stepBtn.setTooltip ("Performer: turn this envelope into a loop of steps, each a little curve you pick (Massive-style)");
    stepBtn.setColour (TextButton::buttonOnColourId, col::mod.withAlpha (0.3f));
    addAndMakeVisible (stepBtn);
    stepBtn.onClick = [this] { applyToSelected ([] (MsegShape& m) { m.stepper = ! m.stepper; if (m.stepper) m.sync = true; }); resized(); };
    stepsBox.addItemList ({ "4", "8", "12", "16", "24", "32" }, 1);
    stepsBox.onChange = [this]
    {
        if (updating) return;
        const int v = stepsBox.getText().getIntValue();
        applyToSelected ([v] (MsegShape& m) { m.numSteps = v; });
    };
    stepLenBox.addItemList ({ "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" }, 1);
    stepLenBox.onChange = [this]
    {
        if (updating) return;
        static const float beats[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        const float b = beats[juce::jmax (0, stepLenBox.getSelectedItemIndex())];
        applyToSelected ([b] (MsegShape& m) { m.stepBeats = b; m.sync = true; });
    };
    for (auto* c : { &stepsBox, &stepLenBox }) addAndMakeVisible (c);
    for (auto [l, t] : { std::pair<Label*, const char*> { &stepsL, "Steps" }, { &stepLenL, "Step" }, { &brushL, "Brush" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    for (int s = 0; s < MsegShape::numStepShapes; ++s)
        addAndMakeVisible (brushes.add (new BrushIcon (s, [this] (int b)
        {
            brush = b;
            for (auto* e : editors) e->brush = b;
            for (auto* br : brushes) br->repaint();
        }, [this] { return brush; })));
    syncBtn.onClick = [this] { applyToSelected ([] (MsegShape& m) { m.sync = ! m.sync; }); };
    lfoBtn.onClick = [this] { applyToSelected ([] (MsegShape& m) { m.lfoDepth = m.lfoDepth > 0 ? 0.0f : 0.3f; }); };
    shBtn.onClick = [this] { applyToSelected ([] (MsegShape& m) { m.shRate = m.shRate > 0 ? 0.0f : 8.0f; }); };
    lfoBtn.setTooltip ("Add an LFO wobble on top of the envelope");
    shBtn.setTooltip ("Sample & hold: turn the envelope into steps");
    shapesBtn.onClick = [this]
    {
        PopupMenu m;
        const char* names[] = { "Swell (sustain)", "Rhythm (tempo loop)", "Ramp up", "Ramp down", "Pluck", "Random steps (tempo loop)", "Slow drift (loop)", "Flat" };
        for (int i = 0; i < 8; ++i) m.addItem (i + 1, names[i]);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&shapesBtn), [this] (int r) { if (r > 0) loadShape (r - 1); });
    };

    modeBox.addItemList ({ "One shot", "Sustain", "Loop while held", "Loop forever" }, 1);
    modeBox.onChange = [this]
    {
        if (updating) return;
        const int mode = modeBox.getSelectedItemIndex();
        applyToSelected ([mode] (MsegShape& m)
        {
            const int last = (int) m.points.size() - 1;
            switch (mode)
            {
                case 0: m.sustain = -1; m.loop = false; break;
                case 1: if (m.sustain < 0) m.sustain = juce::jmax (1, last - 1); m.loop = false; break;
                case 2: if (m.sustain < 0) m.sustain = juce::jmax (1, last - 1); m.loop = true; if (m.loopStart < 0 || m.loopStart >= m.sustain) m.loopStart = 0; break;
                default: m.sustain = -1; m.loop = true; if (m.loopStart < 0) m.loopStart = 0; break;
            }
        });
    };
    for (int i = 0; i < 7; ++i) gridBox.addItem (gridNames[i], i + 1);
    gridBox.setSelectedItemIndex (3, dontSendNotification);
    gridBox.onChange = [this] { refreshControls(); };
    targetBox.addItemList (modDestNames(), 1);
    targetBox.setSelectedId (dstCutoff + 1, dontSendNotification);
    for (auto* c : { &modeBox, &gridBox, &targetBox }) addAndMakeVisible (c);

    auto slider = [this] (Slider& s, double lo, double hi, double mid, std::function<void (MsegShape&, float)> set)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setRange (lo, hi, 0.01);
        if (mid > 0) s.setSkewFactorFromMidPoint (mid);
        s.setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
        s.setColour (Slider::trackColourId, col::mod);
        s.onValueChange = [this, &s, set] { if (! updating) { const float v = (float) s.getValue(); applyToSelected ([set, v] (MsegShape& m) { set (m, v); }); } };
        addAndMakeVisible (s);
    };
    slider (rateSlider, 0.1, 8.0, 1.0, [] (MsegShape& m, float v) { m.rate = v; });
    slider (lfoRate, 0.05, 32.0, 4.0, [] (MsegShape& m, float v) { m.lfoRate = v; });
    slider (lfoDepth, 0.0, 1.0, 0.0, [] (MsegShape& m, float v) { m.lfoDepth = v; });
    slider (shRate, 0.0, 32.0, 6.0, [] (MsegShape& m, float v) { m.shRate = v; });
    amount.setSliderStyle (Slider::LinearHorizontal);
    amount.setRange (-1.0, 1.0, 0.01);
    amount.setValue (0.5, dontSendNotification);
    amount.setTextBoxStyle (Slider::TextBoxRight, false, 40, 18);
    amount.getProperties().set ("bipolar", true);
    amount.setColour (Slider::trackColourId, col::mod);
    addAndMakeVisible (amount);
    assignBtn.onClick = [this] { assignTarget(); };

    const std::pair<Label*, const char*> labels[] = { { &modeL, "Mode" }, { &rateL, "Speed" }, { &gridL, "Grid" }, { &lfoRateL, "Rate" },
                                                      { &lfoDepthL, "Depth" }, { &shRateL, "Steps/s" }, { &targetL, "Target" }, { &amountL, "Amount" } };
    for (auto& [l, t] : labels)
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    for (auto* l : { &help, &targetsInfo })
    {
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        l->setJustificationType (Justification::topLeft);
        addAndMakeVisible (l);
    }
    help.setText ("Double-click: add / remove a point.  Drag points (Shift = off grid).  Alt-drag a line to curve it.  "
                  "Right-click a point: sustain / loop start / delete.", dontSendNotification);

    select (0);
    startTimerHz (10);
}

EnvelopePage::~EnvelopePage() { stopTimer(); }

void EnvelopePage::visibilityChanged() { if (isVisible()) { reloadAll(); refreshControls(); } }

void EnvelopePage::reloadAll()
{
    for (auto* e : editors) e->reload();
    list.repaint();
    shownPreset = proc.currentPresetName;
}

void EnvelopePage::select (int i)
{
    current = juce::jlimit (0, FourfoldProcessor::numMsegs - 1, i);
    for (auto* e : editors) e->selected = e->getIndex() == current;
    list.selectRow (current, false, true);
    if (! stacked) resized();
    for (auto* e : editors) e->repaint();
    refreshControls();
}

void EnvelopePage::applyToSelected (std::function<void (MsegShape&)> change)
{
    auto* e = editors[current];
    change (e->shape);
    e->shape.sanitise();
    proc.setMseg (current, e->shape);
    e->reload();
    list.repaint();
    refreshControls();
}

void EnvelopePage::refreshControls()
{
    const ScopedValueSetter<bool> svs (updating, true);
    const auto& m = editors[current]->shape;
    modeBox.setSelectedItemIndex (m.loop ? (m.sustain >= 0 ? 2 : 3) : (m.sustain >= 0 ? 1 : 0), dontSendNotification);
    syncBtn.setToggleState (m.sync, dontSendNotification);
    lfoBtn.setToggleState (m.lfoDepth > 0, dontSendNotification);
    shBtn.setToggleState (m.shRate > 0, dontSendNotification);
    stackBtn.setToggleState (stacked, dontSendNotification);
    stepBtn.setToggleState (m.stepper, dontSendNotification);
    {
        const int idx = StringArray { "4", "8", "12", "16", "24", "32" }.indexOf (String (m.numSteps));
        stepsBox.setSelectedItemIndex (juce::jmax (0, idx), dontSendNotification);
        static const float beats[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        int li = 1;
        for (int i = 0; i < 6; ++i) if (std::abs (beats[i] - m.stepBeats) < 1.0e-3f) li = i;
        stepLenBox.setSelectedItemIndex (li, dontSendNotification);
    }
    for (auto* c : std::initializer_list<Component*> { &stepsBox, &stepLenBox, &stepsL, &stepLenL, &brushL }) c->setVisible (m.stepper);
    for (auto* b : brushes) b->setVisible (m.stepper);
    modeBox.setEnabled (! m.stepper);
    rateSlider.setValue (m.rate, dontSendNotification);
    lfoRate.setValue (m.lfoRate, dontSendNotification);
    lfoDepth.setValue (m.lfoDepth, dontSendNotification);
    shRate.setValue (m.shRate, dontSendNotification);
    lfoRate.setEnabled (m.lfoDepth > 0); lfoDepth.setEnabled (m.lfoDepth > 0); shRate.setEnabled (m.shRate > 0);
    static const double steps[] = { 0.0, 1.0, 0.5, 0.25, 0.125, 0.1, 0.05 };
    for (auto* e : editors)
    {
        const int gi = gridBox.getSelectedItemIndex();
        double st = steps[juce::jmax (0, gi)];
        if (! e->shape.sync && gi >= 1 && gi <= 4) st = 0.05;          // beat grids only make sense when synced
        if (e->shape.sync && gi >= 5) st = 0.125;
        e->gridStep = st;
    }
    targetsInfo.setText ("Sends to:  " + targetsOf (current), dontSendNotification);
}

juce::String EnvelopePage::targetsOf (int mseg) const
{
    juce::StringArray t;
    for (int i = 0; i < numModSlots; ++i)
    {
        auto* ps = proc.apvts.getParameter (slotSrc (i));
        auto* pd = proc.apvts.getParameter (slotDst (i));
        auto* pa = proc.apvts.getParameter (slotAmt (i));
        if ((int) ps->convertFrom0to1 (ps->getValue()) == srcMseg1 + mseg)
        {
            const int d = (int) pd->convertFrom0to1 (pd->getValue());
            if (d > 0) t.add (modDestNames()[d] + " (" + String (juce::roundToInt (pa->convertFrom0to1 (pa->getValue()) * 100)) + "%)");
        }
    }
    return t.isEmpty() ? String ("nothing yet - pick a target below") : t.joinIntoString (",  ");
}

void EnvelopePage::assignTarget()
{
    const int src = srcMseg1 + current;
    const int dst = targetBox.getSelectedItemIndex();
    if (dst <= 0) return;
    for (int i = 0; i < numModSlots; ++i)
    {
        auto* ps = proc.apvts.getParameter (slotSrc (i));
        auto* pd = proc.apvts.getParameter (slotDst (i));
        const int s = (int) ps->convertFrom0to1 (ps->getValue()), d = (int) pd->convertFrom0to1 (pd->getValue());
        if (s == 0 || d == 0 || (s == src && d == dst))
        {
            auto set = [] (juce::RangedAudioParameter* p, float real) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (real)); p->endChangeGesture(); };
            set (ps, (float) src);
            set (pd, (float) dst);
            set (proc.apvts.getParameter (slotAmt (i)), (float) amount.getValue());
            refreshControls();
            list.repaint();
            return;
        }
    }
    targetsInfo.setText ("The mod matrix is full - clear a slot on the SYNTH page first.", dontSendNotification);
}

void EnvelopePage::loadShape (int which)
{
    MsegShape m;
    juce::Random r ((int64) Time::getMillisecondCounter());
    switch (which)
    {
        case 0: m = MsegShape::swell(); break;
        case 1: m = MsegShape::rhythm(); break;
        case 2: m.points = { { 0.0, 0.0f, -0.3f }, { 2.0, 1.0f, 0.0f } }; break;
        case 3: m.points = { { 0.0, 1.0f, 0.4f }, { 2.0, 0.0f, 0.0f } }; break;
        case 4: m.points = { { 0.0, 1.0f, 0.7f }, { 0.4, 0.15f, 0.3f }, { 1.5, 0.0f, 0.0f } }; break;
        case 5:
            m.sync = true; m.loop = true; m.loopStart = 0;
            for (int i = 0; i < 8; ++i) { const float l = r.nextFloat(); m.points.push_back ({ i * 0.25, l, 0.0f }); m.points.push_back ({ i * 0.25 + 0.24, l, 0.0f }); }
            m.points.push_back ({ 2.0, m.points.front().level, 0.0f });
            break;
        case 6:
            m.loop = true; m.loopStart = 0;
            for (int i = 0; i <= 6; ++i) m.points.push_back ({ i * 1.3, i == 0 || i == 6 ? 0.5f : r.nextFloat(), 0.0f });
            break;
        default: m.points = { { 0.0, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }; break;
    }
    m.name = editors[current]->shape.name;
    m.sanitise();
    proc.setMseg (current, m);
    editors[current]->reload();
    list.repaint();
    refreshControls();
}

void EnvelopePage::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
{
    if (! juce::isPositiveAndBelow (row, editors.size())) return;
    const auto& m = editors[row]->shape;
    if (sel)
    {
        g.setColour (col::panelHi);
        g.fillRoundedRectangle (2.0f, 2.0f, (float) w - 4, (float) h - 4, 5.0f);
    }
    g.setColour (sel ? col::mod : col::text);
    g.setFont (font (13.0f, sel));
    g.drawText (m.name.isNotEmpty() ? m.name : "MSEG " + String (row + 1), 10, 4, w - 80, 18, Justification::left);
    g.setColour (col::dim);
    g.setFont (font (10.0f));
    g.drawText (targetsOf (row).upToFirstOccurrenceOf (",", false, false), 10, 22, w - 80, 16, Justification::left);
    // mini preview
    auto r = Rectangle<float> ((float) w - 70, 8.0f, 60.0f, (float) h - 16);
    Path p;
    const double len = juce::jmax (0.01, m.length());
    for (int i = 0; i <= 40; ++i)
    {
        const float x = r.getX() + r.getWidth() * i / 40.0f;
        const float y = r.getBottom() - m.valueAt (len * i / 40.0) * r.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (col::mod.withAlpha (sel ? 1.0f : 0.6f));
    g.strokePath (p, PathStrokeType (1.4f));
}

void EnvelopePage::listBoxItemClicked (int row, const MouseEvent&) { select (row); }

void EnvelopePage::listBoxItemDoubleClicked (int row, const MouseEvent&)
{
    select (row);
    auto* w = new AlertWindow ("Rename envelope", {}, MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", editors[row]->shape.name);
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    if (auto* top = findParentComponentOfClass<AudioProcessorEditor>()) { top->addAndMakeVisible (w); w->centreAroundComponent (top, w->getWidth(), w->getHeight()); }
    w->enterModalState (true, ModalCallbackFunction::create ([this, w, row] (int r)
    {
        if (r == 1) { const auto n = w->getTextEditorContents ("name").trim(); select (row); applyToSelected ([n] (MsegShape& m) { m.name = n; }); }
    }), true);
}

void EnvelopePage::timerCallback()
{
    if (! isVisible()) return;
    if (shownPreset != proc.currentPresetName) { reloadAll(); refreshControls(); }
}

void EnvelopePage::resized()
{
    auto r = getLocalBounds();
    listArea = r.removeFromLeft (230);
    r.removeFromLeft (8);
    mainArea = r;

    auto la = listArea.reduced (8, 8);
    la.removeFromTop (20);
    auto bottom = la.removeFromBottom (150);
    list.setBounds (la);
    targetsInfo.setBounds (bottom.removeFromTop (48));
    auto tr = bottom.removeFromTop (24);
    targetL.setBounds (tr.removeFromLeft (46)); targetBox.setBounds (tr);
    bottom.removeFromTop (4);
    tr = bottom.removeFromTop (22);
    amountL.setBounds (tr.removeFromLeft (46)); amount.setBounds (tr);
    bottom.removeFromTop (6);
    assignBtn.setBounds (bottom.removeFromTop (26));

    auto m = mainArea.reduced (8, 8);
    headerArea = m.removeFromTop (92);
    auto h1 = headerArea.removeFromTop (28);
    auto place = [] (Rectangle<int>& row, Component& c, int w, int gap = 4) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (gap); };
    place (h1, stackBtn, 76, 12);
    place (h1, modeL, 36); place (h1, modeBox, 130, 12);
    place (h1, syncBtn, 54); place (h1, gridL, 30); place (h1, gridBox, 76, 12);
    place (h1, rateL, 40); place (h1, rateSlider, 170, 12);
    shapesBtn.setBounds (h1.removeFromRight (80));
    headerArea.removeFromTop (6);
    auto h2 = headerArea.removeFromTop (24);
    place (h2, lfoBtn, 50); place (h2, lfoRateL, 34); place (h2, lfoRate, 150, 8); place (h2, lfoDepthL, 40); place (h2, lfoDepth, 150, 16);
    place (h2, shBtn, 50); place (h2, shRateL, 50); place (h2, shRate, 150);
    headerArea.removeFromTop (6);
    auto h3 = headerArea.removeFromTop (26);
    place (h3, stepBtn, 90, 12);
    place (h3, stepsL, 40); place (h3, stepsBox, 64, 12);
    place (h3, stepLenL, 34); place (h3, stepLenBox, 76, 16);
    place (h3, brushL, 42);
    for (auto* b : brushes) place (h3, *b, 30, 3);

    help.setBounds (m.removeFromBottom (18));
    m.removeFromBottom (4);
    if (stacked)
    {
        const int n = editors.size();
        const int each = (m.getHeight() - (n - 1) * 6) / n;
        for (auto* e : editors) { e->setVisible (true); e->setBounds (m.removeFromTop (each)); m.removeFromTop (6); }
    }
    else
        for (auto* e : editors) { e->setVisible (e->getIndex() == current); e->setBounds (m); }
}

void EnvelopePage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    for (auto [r, t] : { std::pair<Rectangle<int>, const char*> { listArea, "ENVELOPES" }, { mainArea, "" } })
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (col::mod);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (11.0f, true));
        g.drawText (t, r.reduced (10, 6).removeFromTop (16), Justification::centredLeft);
    }
}
} // namespace ff::gui
