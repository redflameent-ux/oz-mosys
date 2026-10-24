#include "FxPage.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
Colour FxPage::categoryColour (const String& c)
{
    if (c == "Distortion") return Colour (0xffef5d5d);
    if (c == "Amps") return Colour (0xfff2a541);
    if (c == "Cabinets") return Colour (0xffc9a27a);
    if (c == "Delay & Echo") return Colour (0xff3ad1e8);
    if (c == "Reverb") return Colour (0xff6d9bff);
    if (c == "Modulation") return Colour (0xffa77bf3);
    if (c == "Filters") return Colour (0xffef5d8d);
    if (c == "Dynamics") return Colour (0xff5fe08c);
    if (c == "Lo-fi / Tape") return Colour (0xffd8c25a);
    if (c == "Time FX") return Colour (0xfff27ac1);
    return col::dim;
}

static String pid (int slot, const char* suffix) { return "rk" + String (slot + 1) + suffix; }

// ============================================================================ slot
RackSlotView::RackSlotView (FxPage& p, FourfoldProcessor& pr, int i, std::function<void (Knob&)> reg) : index (i), page (p), proc (pr)
{
    const char* sfx[5] = { "_a", "_b", "_c", "_d", "_mix" };
    for (int k = 0; k < 5; ++k)
    {
        auto* kn = knobs.add (new Knob (proc.apvts, pid (index, sfx[k]), k == 4 ? "Mix" : "", col::fx));
        addAndMakeVisible (kn);
        if (reg) reg (*kn);
    }
    onBtn.setTooltip ("Bypass this unit");
    addAndMakeVisible (onBtn);
    onAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid (index, "_on"), onBtn);
    for (auto* b : { &upBtn, &downBtn, &removeBtn, &irBtn }) addAndMakeVisible (b);
    upBtn.setTooltip ("Move up"); downBtn.setTooltip ("Move down"); removeBtn.setTooltip ("Remove");
    upBtn.onClick = [this] { if (index > 0) page.moveSlot (index, index - 1); };
    downBtn.onClick = [this] { if (index < numRackSlots - 1) page.moveSlot (index, index + 1); };
    removeBtn.onClick = [this] { page.clearSlot (index); };
    irBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load an impulse response", File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) { proc.setRackIr (index, fc.getResult()); refresh(); }
        });
    };
    nameL.setFont (font (14.0f, true));
    nameL.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameL);
    irL.setFont (font (10.5f));
    irL.setColour (Label::textColourId, col::dim);
    addAndMakeVisible (irL);
    refresh();
}

int RackSlotView::type() const { return (int) proc.apvts.getRawParameterValue (pid (index, "_type"))->load(); }

void RackSlotView::refresh()
{
    const int t = type();
    const auto& info = rackUnits()[(size_t) jlimit (0, (int) rackUnits().size() - 1, t)];
    const bool empty = t == 0;
    for (int k = 0; k < 4; ++k)
    {
        const String lbl = info.labels[k];
        knobs[k]->setLabelText (lbl);
        knobs[k]->setVisible (! empty && lbl.isNotEmpty());
    }
    knobs[4]->setVisible (! empty);
    for (auto* c : std::initializer_list<Component*> { &onBtn, &upBtn, &downBtn, &removeBtn }) c->setVisible (! empty);
    irBtn.setVisible (String (info.name) == "IR Loader");
    irL.setVisible (irBtn.isVisible());
    irL.setText (proc.rack.irFiles[(size_t) index].existsAsFile() ? proc.rack.irFiles[(size_t) index].getFileNameWithoutExtension() : String ("no IR loaded"),
                 dontSendNotification);
    nameL.setText (empty ? String() : String (info.name).toUpperCase(), dontSendNotification);
    nameL.setColour (Label::textColourId, FxPage::categoryColour (info.category));
    setTooltip (info.help);
    shownType = t;
    resized();
    repaint();
}

void RackSlotView::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromLeft (6);
    auto left = r.removeFromLeft (170);
    nameL.setBounds (left.removeFromTop (24));
    auto btns = left.removeFromTop (22);
    onBtn.setBounds (btns.removeFromLeft (26));
    btns.removeFromLeft (4);
    upBtn.setBounds (btns.removeFromLeft (24)); btns.removeFromLeft (2);
    downBtn.setBounds (btns.removeFromLeft (24)); btns.removeFromLeft (2);
    removeBtn.setBounds (btns.removeFromLeft (24));
    if (irBtn.isVisible())
    {
        left.removeFromTop (3);
        auto row = left.removeFromTop (18);
        irBtn.setBounds (row.removeFromLeft (76));
        irL.setBounds (row.withTrimmedLeft (4));
    }
    auto kr = r;
    const int kw = jmin (78, kr.getWidth() / 5);
    for (int k = 0; k < 4; ++k) if (knobs[k]->isVisible()) knobs[k]->setBounds (kr.removeFromLeft (kw));
    knobs[4]->setBounds (r.removeFromRight (kw));
}

void RackSlotView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const int t = type();
    const auto& info = rackUnits()[(size_t) jlimit (0, (int) rackUnits().size() - 1, t)];
    const auto c = FxPage::categoryColour (info.category);
    if (t == 0)
    {
        g.setColour (dropHover ? col::fx.withAlpha (0.15f) : col::bg.withAlpha (0.5f));
        g.fillRoundedRectangle (b, 6.0f);
        Path dash; dash.addRoundedRectangle (b.reduced (1.0f), 6.0f);
        Path out; const float d[] = { 5.0f, 4.0f };
        PathStrokeType (1.0f).createDashedStroke (out, dash, d, 2);
        g.setColour (dropHover ? col::fx : col::border.brighter (0.2f));
        g.fillPath (out);
        g.setColour (col::dim);
        g.setFont (font (11.5f));
        g.drawText ("Slot " + String (index + 1) + "  -  drag a unit here, or double-click one in the list", b.toNearestInt(), Justification::centred);
        return;
    }
    const bool on = onBtn.getToggleState();
    ColourGradient grad (c.withAlpha (on ? 0.22f : 0.08f), b.getX(), b.getY(), col::panelHi, b.getX() + 260.0f, b.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (dropHover ? col::fx : col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, dropHover ? 1.5f : 1.0f);
    g.setColour (c.withAlpha (on ? 1.0f : 0.4f));
    g.fillRoundedRectangle (b.getX(), b.getY(), 5.0f, b.getHeight(), 2.0f);
    g.setColour (col::dim);
    g.setFont (font (9.5f));
    g.drawText (String (index + 1), getLocalBounds().reduced (8, 4), Justification::bottomLeft);
}

bool RackSlotView::isInterestedInDragSource (const SourceDetails& d)
{
    const auto s = d.description.toString();
    return s.startsWith ("rackunit:") || s.startsWith ("rackslot:");
}

void RackSlotView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const auto s = d.description.toString();
    if (s.startsWith ("rackunit:")) page.setUnit (index, s.fromFirstOccurrenceOf (":", false, false).getIntValue());
    repaint();
}

// ============================================================================ page
FxPage::FxPage (FourfoldProcessor& p, std::function<void (Knob&)> reg) : proc (p)
{
    setOpaque (true);
    auto addCat = [this] (const String& name)
    {
        auto* b = catButtons.add (new TextButton (name));
        b->setColour (TextButton::buttonOnColourId, categoryColour (name).withAlpha (0.3f));
        b->setColour (TextButton::textColourOnId, categoryColour (name).brighter (0.3f));
        b->onClick = [this, name] { showCategory (name); };
        addAndMakeVisible (b);
    };
    addCat ("All");
    for (auto& c : rackCategories()) addCat (c);
    unitList.setRowHeight (26);
    unitList.setColour (ListBox::backgroundColourId, col::bg);
    addAndMakeVisible (unitList);

    for (int i = 0; i < numRackSlots; ++i) rackHolder.addAndMakeVisible (slots.add (new RackSlotView (*this, proc, i, reg)));
    rackView.setViewedComponent (&rackHolder, false);
    rackView.setScrollBarsShown (true, false);
    rackView.setScrollBarThickness (8);
    addAndMakeVisible (rackView);

    splitBtn.setColour (ToggleButton::tickColourId, col::fx);
    addAndMakeVisible (splitBtn);
    splitAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "rk_split", splitBtn);
    splitBtn.setTooltip ("Slots 1-4 get the lows, slots 5-8 the highs (like Guitar Rig's split)");
    xoverKnob = std::make_unique<Knob> (proc.apvts, "rk_xover", "Split at", col::fx);
    addAndMakeVisible (*xoverKnob);
    if (reg) reg (*xoverKnob);
    clearAllBtn.onClick = [this] { for (int i = 0; i < numRackSlots; ++i) clearSlot (i); };
    addAndMakeVisible (clearAllBtn);
    showCategory ("All");
    startTimerHz (10);
}

FxPage::~FxPage() { stopTimer(); }

void FxPage::showCategory (const String& cat)
{
    category = cat;
    shownUnits.clear();
    for (int i = 1; i < (int) rackUnits().size(); ++i)
        if (cat == "All" || cat == rackUnits()[(size_t) i].category) shownUnits.push_back (i);
    for (auto* b : catButtons) b->setToggleState (b->getButtonText() == cat, dontSendNotification);
    unitList.updateContent();
    unitList.repaint();
}

void FxPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
{
    if (! isPositiveAndBelow (row, (int) shownUnits.size())) return;
    const auto& u = rackUnits()[(size_t) shownUnits[(size_t) row]];
    const auto c = categoryColour (u.category);
    if (sel) { g.setColour (col::panelHi); g.fillRect (0, 0, w, h); }
    g.setColour (c);
    g.fillRoundedRectangle (6.0f, 6.0f, 4.0f, (float) h - 12.0f, 2.0f);
    g.setColour (col::text);
    g.setFont (font (12.5f, true));
    g.drawText (u.name, 16, 0, w - 20, h, Justification::centredLeft);
}

void FxPage::listBoxItemClicked (int row, const MouseEvent&)
{
    if (isPositiveAndBelow (row, (int) shownUnits.size())) { hoverHelp = rackUnits()[(size_t) shownUnits[(size_t) row]].help; repaint (flowArea); }
}
void FxPage::listBoxItemDoubleClicked (int row, const MouseEvent&) { if (isPositiveAndBelow (row, (int) shownUnits.size())) addUnit (shownUnits[(size_t) row]); }
var FxPage::getDragSourceDescription (const SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! isPositiveAndBelow (rows[0], (int) shownUnits.size())) return {};
    return "rackunit:" + String (shownUnits[(size_t) rows[0]]);
}

void FxPage::setParam (const String& id, float real)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
        prm->endChangeGesture();
    }
}

void FxPage::setUnit (int slot, int type)
{
    const auto& info = rackUnits()[(size_t) jlimit (0, (int) rackUnits().size() - 1, type)];
    const char* sfx[4] = { "_a", "_b", "_c", "_d" };
    for (int k = 0; k < 4; ++k) setParam (pid (slot, sfx[k]), info.defaults[k]);
    setParam (pid (slot, "_mix"), rackDefaultMix (type));
    setParam (pid (slot, "_on"), 1.0f);
    setParam (pid (slot, "_type"), (float) type);
    proc.syncRack();
    slots[slot]->refresh();
    repaint (flowArea);
}

void FxPage::addUnit (int type)
{
    for (int i = 0; i < numRackSlots; ++i)
        if (slots[i]->type() == 0) { setUnit (i, type); return; }
    hoverHelp = "The rack is full - remove a unit first.";
    repaint (flowArea);
}

void FxPage::clearSlot (int slot)
{
    setParam (pid (slot, "_type"), 0.0f);
    proc.setRackIr (slot, File());
    proc.syncRack();
    slots[slot]->refresh();
    repaint (flowArea);
}

void FxPage::moveSlot (int from, int to)
{
    const char* sfx[7] = { "_type", "_on", "_a", "_b", "_c", "_d", "_mix" };
    float a[7], b[7];
    for (int k = 0; k < 7; ++k)
    {
        a[k] = proc.apvts.getRawParameterValue (pid (from, sfx[k]))->load();
        b[k] = proc.apvts.getRawParameterValue (pid (to, sfx[k]))->load();
    }
    for (int k = 0; k < 7; ++k) { setParam (pid (to, sfx[k]), a[k]); setParam (pid (from, sfx[k]), b[k]); }
    const auto fa = proc.rack.irFiles[(size_t) from], fb = proc.rack.irFiles[(size_t) to];
    proc.setRackIr (to, fa); proc.setRackIr (from, fb);
    proc.syncRack();
    slots[from]->refresh(); slots[to]->refresh();
    repaint (flowArea);
}

void FxPage::timerCallback()
{
    if (! isShowing()) return;
    bool changed = false;   // presets / undo can change units behind our back
    for (int i = 0; i < slots.size(); ++i)
    {
        const int t = slots[i]->type();
        if (slotTypesSeen[(size_t) i] != t) { slotTypesSeen[(size_t) i] = t; slots[i]->refresh(); changed = true; }
    }
    if (changed) repaint (flowArea);
}

void FxPage::resized()
{
    auto r = getLocalBounds();
    browserArea = r.removeFromLeft (230); r.removeFromLeft (8);
    flowArea = r.removeFromRight (220); r.removeFromRight (8);
    rackArea = r;

    auto b = browserArea.reduced (8, 8);
    b.removeFromTop (20);
    const int cols = 2, rows = (catButtons.size() + 1) / 2;
    auto catArea = b.removeFromTop (rows * 24);
    const int cw = catArea.getWidth() / cols;
    for (int i = 0; i < catButtons.size(); ++i)
        catButtons[i]->setBounds (Rectangle<int> (catArea.getX() + (i % cols) * cw, catArea.getY() + (i / cols) * 24, cw, 24).reduced (2));
    b.removeFromTop (8);
    unitList.setBounds (b);

    auto ra = rackArea.reduced (8, 8);
    ra.removeFromTop (20);
    rackView.setBounds (ra);
    const int sh = jmax (72, (ra.getHeight() - 4) / numRackSlots);
    rackHolder.setSize (ra.getWidth() - (sh * numRackSlots > ra.getHeight() ? 10 : 0), sh * numRackSlots);
    for (int i = 0; i < slots.size(); ++i) slots[i]->setBounds (0, i * sh, rackHolder.getWidth(), sh);

    auto f = flowArea.reduced (10, 8);
    f.removeFromTop (20);
    auto bottom = f.removeFromBottom (150);
    flowBox = f;
    splitBtn.setBounds (bottom.removeFromTop (24));
    auto row = bottom.removeFromTop (80);
    xoverKnob->setBounds (row.removeFromLeft (90));
    bottom.removeFromTop (8);
    clearAllBtn.setBounds (bottom.removeFromTop (26));
}

void FxPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    for (auto [r, t] : { std::pair<Rectangle<int>, const char*> { browserArea, "COMPONENTS" }, { rackArea, "FX RACK  -  after the effects on the SYNTH page" },
                         { flowArea, "SIGNAL FLOW" } })
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (col::fx);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (11.0f, true));
        g.drawText (t, r.reduced (10, 6).removeFromTop (16), Justification::centredLeft);
    }

    // signal flow: boxes for each unit, top to bottom (two columns when split)
    const bool split = proc.apvts.getRawParameterValue ("rk_split")->load() > 0.5f;
    auto f = flowBox.toFloat();
    auto drawChain = [&] (Rectangle<float> col, int from, int to)
    {
        std::vector<int> used;
        for (int i = from; i < to; ++i) if (slots[i]->type() != 0) used.push_back (i);
        const float bh = 24.0f, gap = 12.0f;
        float y = col.getY() + 16.0f;
        g.setColour (col::border.brighter (0.3f));
        g.drawLine (col.getCentreX(), col.getY(), col.getCentreX(), col.getY() + 16.0f, 1.5f);
        for (int i : used)
        {
            const auto& info = rackUnits()[(size_t) slots[i]->type()];
            const bool on = proc.apvts.getRawParameterValue (pid (i, "_on"))->load() > 0.5f;
            auto box = Rectangle<float> (col.getX() + 4.0f, y, col.getWidth() - 8.0f, bh);
            g.setColour (categoryColour (info.category).withAlpha (on ? 0.85f : 0.25f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.setFont (font (10.5f, true));
            g.drawFittedText (info.name, box.toNearestInt().reduced (3, 0), Justification::centred, 1);
            y += bh;
            g.setColour (col::border.brighter (0.3f));
            g.drawLine (col.getCentreX(), y, col.getCentreX(), y + gap, 1.5f);
            y += gap;
        }
        return y;
    };
    float endY;
    if (split)
    {
        auto l = f.withWidth (f.getWidth() / 2), h = f.withTrimmedLeft (f.getWidth() / 2);
        g.setColour (col::dim); g.setFont (font (9.5f, true));
        g.drawText ("LOW", l.withHeight (14).toNearestInt(), Justification::centred);
        g.drawText ("HIGH", h.withHeight (14).toNearestInt(), Justification::centred);
        endY = jmax (drawChain (l.withTrimmedTop (14), 0, 4), drawChain (h.withTrimmedTop (14), 4, 8));
        g.setColour (col::border.brighter (0.3f));
        g.drawLine (l.getCentreX(), endY, h.getCentreX(), endY, 1.5f);
    }
    else endY = drawChain (f, 0, numRackSlots);
    g.setColour (col::text);
    g.drawArrow (Line<float> (f.getCentreX(), endY, f.getCentreX(), jmin (f.getBottom() - 30.0f, endY + 24.0f)), 1.5f, 7.0f, 7.0f);
    if (hoverHelp.isNotEmpty())
    {
        g.setColour (col::dim);
        g.setFont (font (11.0f));
        g.drawFittedText (hoverHelp, flowBox.withTop (flowBox.getBottom() - 60), Justification::bottomLeft, 4);
    }
}
} // namespace ff::gui
