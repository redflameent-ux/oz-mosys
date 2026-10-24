#include "FmPage.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static const char* opLetters = "ABCDEF";

// ============================================================================ cell
FmCell::FmCell (RangedAudioParameter& p, bool isFeedback, bool isOutput)
    : attach (p, [this] (float v) { value = v; repaint(); }), feedback (isFeedback), output (isOutput)
{
    attach.sendInitialUpdate();
    setTooltip (output ? "How loud this operator is heard (drag up/down, double-click for 0)"
                       : feedback ? "Feedback: the operator modulates itself (gritty, saw-like)"
                                  : "How much the row's operator modulates the column's operator (drag up/down, double-click for 0)");
    setMouseCursor (MouseCursor::UpDownResizeCursor);
}

void FmCell::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = value > 0.001f;
    const auto c = output ? col::text : feedback ? col::filter : fmCol;
    g.setColour (feedback ? col::panelHi.brighter (0.05f) : col::panelHi);
    g.fillRoundedRectangle (r, 3.0f);
    if (on)
    {
        g.setColour (c.withAlpha (0.18f + 0.5f * value));
        g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - value)), 3.0f);
        g.setColour (c);
        g.setFont (font (11.5f, true));
        g.drawText (String (roundToInt (value * 100)), r.toNearestInt(), Justification::centred);
    }
    else
    {
        g.setColour (col::border);
        g.fillEllipse (r.getCentreX() - 1.5f, r.getCentreY() - 1.5f, 3.0f, 3.0f);
    }
}

void FmCell::mouseDown (const MouseEvent&) { downValue = value; attach.beginGesture(); }
void FmCell::mouseDrag (const MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 600.0f : 150.0f;
    attach.setValueAsPartOfGesture (jlimit (0.0f, 1.0f, downValue - (float) e.getDistanceFromDragStartY() / sens));
}
void FmCell::mouseUp (const MouseEvent&) { attach.endGesture(); }
void FmCell::mouseDoubleClick (const MouseEvent&) { attach.setValueAsCompleteGesture (0.0f); }
void FmCell::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    attach.setValueAsCompleteGesture (jlimit (0.0f, 1.0f, value + w.deltaY * 0.1f));
}

// ============================================================================ envelope view
void FmEnvView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 4.0f);
    const String pre = "fm" + String (index + 1);
    auto v = [this, &pre] (const char* s) { return state.getRawParameterValue (pre + s)->load(); };
    const float a = v ("_a"), d = v ("_d"), s = v ("_s"), rl = v ("_r");
    // time axis compressed with sqrt so short and long envelopes both read well
    auto tx = [] (float t) { return std::sqrt (t + 0.0001f); };
    const float total = tx (a) + tx (d) + 0.6f + tx (rl);
    auto p = r.reduced (4.0f, 5.0f);
    auto X = [&] (float u) { return p.getX() + p.getWidth() * u / total; };
    Path path;
    path.startNewSubPath (p.getX(), p.getBottom());
    float x = tx (a);
    path.lineTo (X (x), p.getY());
    for (int i = 1; i <= 16; ++i)   // exponential decay
    {
        const float t = (float) i / 16.0f;
        const float lvl = s + (1.0f - s) * std::pow (1.0e-4f, t);
        path.lineTo (X (x + tx (d) * t), p.getBottom() - lvl * p.getHeight());
    }
    x += tx (d) + 0.6f;
    path.lineTo (X (x), p.getBottom() - s * p.getHeight());
    for (int i = 1; i <= 16; ++i)
    {
        const float t = (float) i / 16.0f;
        path.lineTo (X (x + tx (rl) * t), p.getBottom() - s * std::pow (1.0e-4f, t) * p.getHeight());
    }
    Path fill (path);
    fill.lineTo (p.getRight(), p.getBottom());
    fill.closeSubPath();
    g.setColour (fmCol.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (fmCol);
    g.strokePath (path, PathStrokeType (1.4f));
}

// ============================================================================ page
FmPage::FmPage (FourfoldProcessor& p, std::function<void (Knob&)> reg) : proc (p), registerKnob (std::move (reg))
{
    setOpaque (true);
    auto& st = proc.apvts;
    for (int from = 0; from < numFmOps; ++from)
    {
        for (int to = 0; to < numFmOps; ++to)
            addAndMakeVisible (cells.add (new FmCell (*st.getParameter ("fm_m" + String (from + 1) + String (to + 1)), from == to, false)));
        addAndMakeVisible (cells.add (new FmCell (*st.getParameter ("fm" + String (from + 1) + "_level"), false, true)));
    }

    onBtn.setColour (ToggleButton::tickColourId, fmCol);
    addAndMakeVisible (onBtn);
    onAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, "fm_on", onBtn);

    algoBox.setTextWhenNothingSelected ("Start from a routing...");
    int id = 1;
    for (auto& a : fmAlgorithms()) algoBox.addItem (a.name, id++);
    algoBox.onChange = [this] { if (algoBox.getSelectedItemIndex() >= 0) applyAlgorithm (algoBox.getSelectedItemIndex()); algoBox.setSelectedId (0, dontSendNotification); };
    addAndMakeVisible (algoBox);
    clearBtn.onClick = [this]
    {
        for (int i = 0; i < numFmOps; ++i)
            for (int j = 0; j < numFmOps; ++j)
                if (auto* prm = proc.apvts.getParameter ("fm_m" + String (i + 1) + String (j + 1))) prm->setValueNotifyingHost (0.0f);
    };
    addAndMakeVisible (clearBtn);

    globalKnobs = { knob ("fm_level", "Level", dstFmLevel), knob ("fm_index", "Depth", dstFmIndex), knob ("fm_pan", "Pan"),
                    knob ("fm_oct", "Octave"), knob ("fm_semi", "Semi") };

    for (int i = 0; i < numFmOps; ++i)
    {
        const String pre = "fm" + String (i + 1);
        auto* wb = waveBoxes.add (new ComboBox());
        wb->addItemList (fmWaveNames(), 1);
        addAndMakeVisible (wb);
        comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (st, pre + "_wave", *wb));
        opKnobs[(size_t) i] = { knob (pre + "_ratio", "Ratio"), knob (pre + "_fine", "Fine"), knob (pre + "_vel", "Velocity"),
                                knob (pre + "_a", "Attack"), knob (pre + "_d", "Decay"), knob (pre + "_s", "Sustain"), knob (pre + "_r", "Release") };
        addAndMakeVisible (envViews.add (new FmEnvView (st, i)));
    }

    help.setText ("Rows modulate columns. OUT = how loud each operator is heard. Drag a cell up/down (Shift = fine), double-click to clear. "
                  "Simple start: B modulating A with B's ratio at 1, 2 or 3.", dontSendNotification);
    help.setFont (font (11.0f));
    help.setColour (Label::textColourId, col::dim);
    help.setJustificationType (Justification::topLeft);
    addAndMakeVisible (help);
    startTimerHz (12);
}

FmPage::~FmPage() { stopTimer(); }

Knob* FmPage::knob (const String& id, const String& label, int dest)
{
    auto* k = knobs.add (new Knob (proc.apvts, id, label, fmCol, dest));
    addAndMakeVisible (k);
    if (registerKnob) registerKnob (*k);
    return k;
}

void FmPage::applyAlgorithm (int index)
{
    const auto& a = fmAlgorithms()[(size_t) index];
    auto set = [this] (const String& id, float v) { if (auto* prm = proc.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); } };
    for (int i = 0; i < numFmOps; ++i)
    {
        for (int j = 0; j < numFmOps; ++j) set ("fm_m" + String (i + 1) + String (j + 1), 0.0f);
        set ("fm" + String (i + 1) + "_level", a.out[(size_t) i]);
    }
    for (auto& l : a.links) set ("fm_m" + String ((int) l[0] + 1) + String ((int) l[1] + 1), l[2]);
}

void FmPage::timerCallback()
{
    if (! isShowing()) return;
    for (auto* e : envViews) e->repaint();
    repaint (opsArea.withWidth (150));
}

void FmPage::resized()
{
    auto r = getLocalBounds();
    matrixArea = r.removeFromLeft (430);
    r.removeFromLeft (8);
    opsArea = r;

    auto m = matrixArea.reduced (12, 10);
    m.removeFromTop (22);
    auto top = m.removeFromTop (26);
    onBtn.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (8);
    clearBtn.setBounds (top.removeFromRight (110));
    top.removeFromRight (6);
    algoBox.setBounds (top);
    m.removeFromTop (10);
    globalArea = m.removeFromBottom (86);
    // grid: 1 label column + 6 operator columns + OUT, 1 label row + 6 rows
    const int cols = 8, rows = 7;
    const int cs = jmin (m.getWidth() / cols, m.getHeight() / rows);
    gridArea = Rectangle<int> (m.getX(), m.getY(), cs * cols, cs * rows);
    for (int from = 0; from < numFmOps; ++from)
        for (int c = 0; c <= numFmOps; ++c)
            cells[from * (numFmOps + 1) + c]->setBounds (gridArea.getX() + (c + 1) * cs, gridArea.getY() + (from + 1) * cs, cs, cs);
    help.setBounds (m.getX(), gridArea.getBottom() + 10, m.getWidth(), 48);
    const int kw = globalArea.getWidth() / (int) globalKnobs.size();
    auto ga = globalArea;
    for (auto* k : globalKnobs) k->setBounds (ga.removeFromLeft (kw).reduced (2, 4));

    auto o = opsArea.reduced (10, 8);
    o.removeFromTop (20);
    const int rh = o.getHeight() / numFmOps;
    for (int i = 0; i < numFmOps; ++i)
    {
        auto row = o.removeFromTop (rh).reduced (0, 3);
        row.removeFromLeft (30);   // letter
        auto wcol = row.removeFromLeft (104);
        waveBoxes[i]->setBounds (wcol.withSizeKeepingCentre (wcol.getWidth(), 24).translated (0, -8));
        envViews[i]->setBounds (row.removeFromRight (170).reduced (0, 4));
        row.removeFromRight (6);
        const int w = row.getWidth() / (int) opKnobs[(size_t) i].size();
        for (auto* k : opKnobs[(size_t) i]) k->setBounds (row.removeFromLeft (w).reduced (1, 0));
    }
}

void FmPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    for (auto [r, t] : { std::pair<Rectangle<int>, const char*> { matrixArea, "FM MATRIX" }, { opsArea, "OPERATORS" } })
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (fmCol);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (11.0f, true));
        g.drawText (t, r.reduced (12, 6).removeFromTop (16), Justification::centredLeft);
    }
    // grid labels
    const int cs = gridArea.getWidth() / 8;
    g.setFont (font (12.0f, true));
    for (int c = 0; c < numFmOps; ++c)
    {
        g.setColour (fmCol);
        g.drawText (String::charToString ((juce_wchar) opLetters[c]), gridArea.getX() + (c + 1) * cs, gridArea.getY(), cs, cs, Justification::centred);
        g.drawText (String::charToString ((juce_wchar) opLetters[c]), gridArea.getX(), gridArea.getY() + (c + 1) * cs, cs, cs, Justification::centred);
    }
    g.setColour (col::text);
    g.drawText ("OUT", gridArea.getX() + 7 * cs, gridArea.getY(), cs, cs, Justification::centred);
    g.setColour (col::dim);
    g.setFont (font (9.5f));
    g.drawText ("to >", gridArea.getX(), gridArea.getY(), cs, cs / 2, Justification::centred);
    g.drawText ("from", gridArea.getX(), gridArea.getY() + cs / 2, cs, cs / 2, Justification::centred);

    // operator letters + live ratio readout
    auto o = opsArea.reduced (10, 8);
    o.removeFromTop (20);
    const int rh = o.getHeight() / numFmOps;
    for (int i = 0; i < numFmOps; ++i)
    {
        auto row = o.removeFromTop (rh);
        const float lvl = proc.apvts.getRawParameterValue ("fm" + String (i + 1) + "_level")->load();
        g.setColour (lvl > 0.001f ? fmCol : col::dim);
        g.setFont (font (20.0f, true));
        g.drawText (String::charToString ((juce_wchar) opLetters[i]), row.removeFromLeft (30), Justification::centred);
        g.setColour (col::dim);
        g.setFont (font (10.5f));
        const float ratio = proc.apvts.getRawParameterValue ("fm" + String (i + 1) + "_ratio")->load();
        g.drawText ("x " + String (ratio, ratio < 10 ? 3 : 2) + (lvl > 0.001f ? "   heard" : "   modulator"),
                    row.removeFromLeft (104).translated (0, 16), Justification::centred);
        if (i > 0)
        {
            g.setColour (col::border);
            g.drawHorizontalLine (row.getY(), (float) opsArea.getX() + 10.0f, (float) opsArea.getRight() - 10.0f);
        }
    }
}
} // namespace ff::gui
