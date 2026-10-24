#include "PlayPage.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static const Colour chordCol { 0xffb98cff }, arpCol { 0xff4fc3f7 };

// ============================================================================ grid
const char* ArpGrid::rowName (int r)
{
    static const char* n[] = { "On", "Tie", "Accent", "Note", "Octave", "Transpose", "Velocity" };
    return n[jlimit (0, numRows - 1, r)];
}

Rectangle<float> ArpGrid::rowArea (int r) const
{
    auto b = getLocalBounds().toFloat();
    const float hNormal = (b.getHeight() - 22.0f) / (numRows + 1.5f);
    float y = 20.0f;
    for (int i = 0; i < r; ++i) y += (i == numRows - 1 ? hNormal * 2.5f : hNormal);
    return { 0.0f, y, b.getWidth(), r == numRows - 1 ? hNormal * 2.5f : hNormal };
}

Rectangle<float> ArpGrid::cell (int row, int step) const
{
    auto ra = rowArea (row);
    const float w = ra.getWidth() / perf::ArpPattern::maxSteps;
    return Rectangle<float> (ra.getX() + step * w, ra.getY(), w, ra.getHeight()).reduced (1.5f, 2.0f);
}

void ArpGrid::paint (Graphics& g)
{
    auto pat = proc.getArpPattern();
    const auto& p = haveWorking ? working : *pat;
    const int steps = (int) proc.apvts.getRawParameterValue ("arp_steps")->load();
    g.setFont (font (9.5f));
    for (int s = 0; s < perf::ArpPattern::maxSteps; ++s)
    {
        g.setColour (s == currentStep ? arpCol : (s < steps ? col::dim : col::border));
        g.drawText (String (s + 1), cell (0, s).withY (2.0f).withHeight (14.0f).toNearestInt(), Justification::centred);
    }
    for (int r = 0; r < numRows; ++r)
        for (int s = 0; s < perf::ArpPattern::maxSteps; ++s)
        {
            auto c = cell (r, s);
            const auto& st = p.steps[(size_t) s];
            const bool active = s < steps;
            const float a = active ? 1.0f : 0.3f;
            g.setColour ((s / 4) % 2 ? col::panelHi : col::panelHi.brighter (0.04f));
            g.fillRoundedRectangle (c, 2.5f);
            if (s == currentStep) { g.setColour (arpCol.withAlpha (0.25f)); g.fillRoundedRectangle (c, 2.5f); }
            switch (r)
            {
                case 0: case 1: case 2:
                {
                    const bool on = r == 0 ? st.on : r == 1 ? st.tie : st.accent;
                    if (on)
                    {
                        g.setColour ((r == 0 ? arpCol : r == 1 ? col::sampler : col::filter).withAlpha (a * 0.85f));
                        g.fillRoundedRectangle (c.reduced (2.0f), 2.0f);
                    }
                    break;
                }
                case 3:
                {
                    const String t = st.noteIdx == 0 ? "-" : st.noteIdx == 9 ? "All" : String (st.noteIdx);
                    g.setColour ((st.noteIdx ? col::text : col::dim).withAlpha (a));
                    g.setFont (font (10.0f, st.noteIdx != 0));
                    g.drawText (t, c.toNearestInt(), Justification::centred);
                    break;
                }
                case 4: case 5:
                {
                    const int v = r == 4 ? st.octave : st.transpose;
                    g.setColour ((v != 0 ? col::text : col::dim).withAlpha (a));
                    g.setFont (font (10.0f, v != 0));
                    g.drawText (v > 0 ? "+" + String (v) : String (v), c.toNearestInt(), Justification::centred);
                    break;
                }
                default:
                {
                    auto bar = c.reduced (2.0f);
                    g.setColour ((st.accent ? col::filter : arpCol).withAlpha (a * (st.on ? 0.8f : 0.25f)));
                    g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() * (1.0f - st.velocity)), 2.0f);
                    break;
                }
            }
        }
}

void ArpGrid::mouseDown (const MouseEvent& e)
{
    working = *proc.getArpPattern();
    haveWorking = true;
    dragRow = -1;
    for (int r = 0; r < numRows; ++r) if (rowArea (r).contains (e.position)) dragRow = r;
    lastStep = -1;
    edit (e, true);
}

void ArpGrid::mouseDrag (const MouseEvent& e) { edit (e, false); }
void ArpGrid::mouseUp (const MouseEvent&) { if (haveWorking) commit(); haveWorking = false; repaint(); }

void ArpGrid::edit (const MouseEvent& e, bool first)
{
    if (dragRow < 0) return;
    const float w = getWidth() / (float) perf::ArpPattern::maxSteps;
    const int s = jlimit (0, perf::ArpPattern::maxSteps - 1, (int) (e.position.x / w));
    auto& st = working.steps[(size_t) s];
    const bool down = e.mods.isPopupMenu();
    switch (dragRow)
    {
        case 0: case 1: case 2:
        {
            bool& v = dragRow == 0 ? st.on : dragRow == 1 ? st.tie : st.accent;
            if (first) paintValue = ! v;
            if (s != lastStep) v = paintValue;
            break;
        }
        case 3:
            if (s != lastStep) st.noteIdx = (st.noteIdx + (down ? 9 : 1)) % 10;
            break;
        case 4: case 5:
        {
            int& v = dragRow == 4 ? st.octave : st.transpose;
            const int lim = dragRow == 4 ? 2 : 12;
            if (first || s != lastStep) { dragStartY = e.position.y; dragStartVal = v; if (! first || true) v = jlimit (-lim, lim, v + (down ? -1 : 1)); dragStartVal = v; }
            else v = jlimit (-lim, lim, dragStartVal + (int) std::round ((dragStartY - e.position.y) / 8.0f));
            break;
        }
        default:
        {
            auto ra = rowArea (6);
            st.velocity = jlimit (0.05f, 1.0f, (ra.getBottom() - e.position.y) / ra.getHeight());
            break;
        }
    }
    lastStep = s;
    commit();
    repaint();
}

void ArpGrid::commit() { proc.setArpPattern (working); }

void ArpGrid::resetRow (int r)
{
    auto p = *proc.getArpPattern();
    for (auto& st : p.steps)
        switch (r) { case 0: st.on = true; break; case 1: st.tie = false; break; case 2: st.accent = false; break;
                     case 3: st.noteIdx = 0; break; case 4: st.octave = 0; break; case 5: st.transpose = 0; break; default: st.velocity = 0.8f; break; }
    proc.setArpPattern (p);
    repaint();
}

void ArpGrid::randomRow (int r)
{
    auto p = *proc.getArpPattern();
    for (auto& st : p.steps)
        switch (r)
        {
            case 0: st.on = rng.nextFloat() > 0.3f; break;
            case 1: st.tie = rng.nextFloat() > 0.8f; break;
            case 2: st.accent = rng.nextFloat() > 0.7f; break;
            case 3: st.noteIdx = rng.nextFloat() > 0.6f ? rng.nextInt (5) : 0; break;
            case 4: st.octave = rng.nextFloat() > 0.7f ? (rng.nextBool() ? 1 : -1) : 0; break;
            case 5: { static const int iv[] = { 0, 0, 0, 0, 7, 5, 12, -12, 3, 10 }; st.transpose = iv[rng.nextInt (10)]; break; }
            default: st.velocity = 0.4f + 0.6f * rng.nextFloat(); break;
        }
    proc.setArpPattern (p);
    repaint();
}

// ============================================================================ page
PlayPage::PlayPage (FourfoldProcessor& p, std::function<void (Knob&)> reg) : proc (p), registerKnob (std::move (reg)), grid (p)
{
    setOpaque (true);
    toggle ("ch_on", "CHORDS", chordCol);
    combo ("ch_follow", { "In key", "Fixed shape" });
    combo ("ch_key", noteNames());
    combo ("ch_scale", perf::chordScaleNames());
    combo ("ch_type", perf::chordTypeNames());
    combo ("ch_voicing", { "Close", "Open", "Drop 2", "Bass + chord" });
    knob ("ch_octave", "Octave", chordCol);
    knob ("ch_strum", "Strum", chordCol);
    knob ("ch_humanize", "Humanize", chordCol);

    toggle ("arp_on", "ARPEGGIATOR", arpCol);
    toggle ("arp_latch", "Latch", arpCol);
    combo ("arp_rate", perf::arpRateNames());
    combo ("arp_mode", perf::arpModeNames());
    knob ("arp_oct", "Octaves", arpCol);
    knob ("arp_gate", "Gate", arpCol);
    knob ("arp_swing", "Swing", arpCol);
    knob ("arp_steps", "Steps", arpCol);
    for (auto t : { "Mode", "Key", "Scale", "Type", "Voicing", "Rate", "Pattern" })
    {
        auto* l = labels.add (new Label ({}, t));
        l->setFont (font (10.5f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    patternBox.setTextWhenNothingSelected ("Load a pattern...");
    patternBox.addItemList (perf::ArpPattern::presetNames(), 1);
    patternBox.onChange = [this]
    {
        if (patternBox.getSelectedItemIndex() >= 0) proc.setArpPattern (perf::ArpPattern::preset (patternBox.getSelectedItemIndex()));
        patternBox.setSelectedId (0, dontSendNotification);
        grid.repaint();
    };
    addAndMakeVisible (patternBox);
    addAndMakeVisible (grid);
    for (int r = 0; r < ArpGrid::numRows; ++r)
    {
        auto* x = rowReset.add (new TextButton ("x"));
        x->setTooltip (String ("Reset the ") + ArpGrid::rowName (r) + " row");
        x->onClick = [this, r] { grid.resetRow (r); };
        addAndMakeVisible (x);
        auto* d = rowRandom.add (new TextButton ("?"));
        d->setTooltip (String ("Randomise the ") + ArpGrid::rowName (r) + " row");
        d->onClick = [this, r] { grid.randomRow (r); };
        addAndMakeVisible (d);
    }
    morphBtn.setColour (TextButton::buttonColourId, Colour (0xff7ee0c3).withAlpha (0.3f));
    morphBtn.setTooltip ("FM8-style morph square and Omnisphere-style Orb: blend four versions of this sound");
    morphBtn.onClick = [this]
    {
        morphPage = std::make_unique<MorphPage> (proc, registerKnob);
        Component::SafePointer<PlayPage> sp (this);
        morphPage->onClose = [sp] { MessageManager::callAsync ([sp] { if (sp != nullptr) sp->morphPage.reset(); }); };
        addAndMakeVisible (*morphPage);
        morphPage->setBounds (getLocalBounds());
    };
    addAndMakeVisible (morphBtn);
    startTimerHz (25);
}

PlayPage::~PlayPage() { stopTimer(); }

Knob* PlayPage::knob (const String& id, const String& label, Colour c)
{
    auto* k = knobs.add (new Knob (proc.apvts, id, label, c));
    addAndMakeVisible (k);
    if (registerKnob) registerKnob (*k);
    return k;
}
ComboBox* PlayPage::combo (const String& id, const StringArray& items)
{
    auto* b = combos.add (new ComboBox());
    b->addItemList (items, 1);
    addAndMakeVisible (b);
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *b));
    return b;
}
ToggleButton* PlayPage::toggle (const String& id, const String& text, Colour c)
{
    auto* t = toggles.add (new ToggleButton (text));
    t->setColour (ToggleButton::tickColourId, c);
    addAndMakeVisible (t);
    buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *t));
    return t;
}

int PlayPage::degreeRoot (int d) const
{
    static const int steps[9][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
                                     { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 3, 5, 6, 8, 10 }, { 0, 1, 4, 5, 7, 8, 10 } };
    const int key = (int) proc.apvts.getRawParameterValue ("ch_key")->load();
    const int scale = jlimit (0, 8, (int) proc.apvts.getRawParameterValue ("ch_scale")->load());
    return 60 + key + steps[scale][d];
}

void PlayPage::mouseDown (const MouseEvent& e)
{
    // click a chord circle to hear it
    for (int d = 0; d < 7; ++d)
        if (circles[(size_t) d].contains (e.position))
        {
            if (auditionNote >= 0) proc.keyboardState.noteOff (1, auditionNote, 0.0f);
            auditionNote = degreeRoot (d);
            proc.keyboardState.noteOn (1, auditionNote, 0.8f);
            auditionOff = Time::getMillisecondCounterHiRes() + 900.0;
        }
}

void PlayPage::timerCallback()
{
    if (! isShowing()) return;
    if (auditionNote >= 0 && Time::getMillisecondCounterHiRes() > auditionOff) { proc.keyboardState.noteOff (1, auditionNote, 0.0f); auditionNote = -1; }
    const int cs = proc.perform.currentStep.load();
    if (cs != grid.currentStep) { grid.currentStep = cs; grid.repaint(); }
    repaint (circlesArea);
}

bool PlayPage::isInterestedInFileDrag (const StringArray& f)
{
    for (auto& s : f) if (s.endsWithIgnoreCase (".mid") || s.endsWithIgnoreCase (".midi")) return true;
    return false;
}

void PlayPage::filesDropped (const StringArray& f, int, int)
{
    dragOver = false;
    for (auto& s : f) if (s.endsWithIgnoreCase (".mid") || s.endsWithIgnoreCase (".midi")) { importGroove (File (s)); break; }
    repaint();
}

void PlayPage::importGroove (const File& file)
{
    FileInputStream in (file);
    MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { status = "Couldn't read that MIDI file"; return; }
    const int tpq = mf.getTimeFormat() > 0 ? mf.getTimeFormat() : 96;
    static const double rateBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    const double stepBeats = rateBeats[jlimit (0, 5, (int) proc.apvts.getRawParameterValue ("arp_rate")->load())];
    perf::ArpPattern p = *proc.getArpPattern();
    for (auto& st : p.steps) { st.on = false; st.accent = false; st.tie = false; st.velocity = 0.0f; }
    double first = -1, last = 0;
    int count = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* ev : *mf.getTrack (t))
            if (ev->message.isNoteOn())
            {
                const double beat = ev->message.getTimeStamp() / tpq;
                if (first < 0 || beat < first) first = beat;
            }
    if (first < 0) { status = "No notes in that MIDI file"; return; }
    first = std::floor (first);   // keep the groove's position in the bar
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* ev : *mf.getTrack (t))
            if (ev->message.isNoteOn())
            {
                const double beat = ev->message.getTimeStamp() / tpq - first;
                const int s = (int) std::floor (beat / stepBeats + 0.25);
                if (s < 0 || s >= perf::ArpPattern::maxSteps) continue;
                auto& st = p.steps[(size_t) s];
                st.on = true;
                st.velocity = jmax (st.velocity, ev->message.getFloatVelocity());
                st.accent = ev->message.getVelocity() >= 100;
                last = jmax (last, (double) s);
                ++count;
            }
    for (auto& st : p.steps) if (st.velocity <= 0.0f) st.velocity = 0.7f;
    proc.setArpPattern (p);
    const int steps = jlimit (1, 32, (int) (std::ceil ((last + 1) / 4.0) * 4));
    if (auto* prm = proc.apvts.getParameter ("arp_steps")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) steps));
    status = "Groove copied from " + file.getFileName() + " (" + String (count) + " notes, " + String (steps) + " steps)";
    grid.repaint();
}

void PlayPage::resized()
{
    auto r = getLocalBounds();
    if (morphPage != nullptr) morphPage->setBounds (r);
    auto top = r.removeFromTop (250);
    r.removeFromTop (8);
    gridArea = r;
    chordArea = top.removeFromLeft (top.getWidth() * 52 / 100);
    top.removeFromLeft (8);
    arpArea = top;

    // chords
    {
        auto a = chordArea.reduced (12, 10);
        auto row = a.removeFromTop (26);
        toggles[0]->setBounds (row.removeFromLeft (110));
        morphBtn.setBounds (row.removeFromRight (150));
        a.removeFromTop (8);
        auto labelsRow = a.removeFromTop (14);
        auto combosRow = a.removeFromTop (24);
        const int cw = combosRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            labels[i]->setBounds (labelsRow.removeFromLeft (cw).reduced (2, 0));
            combos[i]->setBounds (combosRow.removeFromLeft (cw).reduced (2, 0));
        }
        a.removeFromTop (8);
        auto kr = a.removeFromBottom (82);
        circlesArea = a;
        const float d = jmin ((float) a.getHeight() - 16.0f, (float) a.getWidth() / 7.0f - 8.0f);
        for (int i = 0; i < 7; ++i)
            circles[(size_t) i] = Rectangle<float> (d, d).withCentre ({ a.getX() + (i + 0.5f) * a.getWidth() / 7.0f, (float) a.getCentreY() - 6.0f });
        const int kw = 90;
        for (int i = 0; i < 3; ++i) knobs[i]->setBounds (kr.removeFromLeft (kw));
    }
    // arp
    {
        auto a = arpArea.reduced (12, 10);
        auto row = a.removeFromTop (26);
        toggles[1]->setBounds (row.removeFromLeft (150));
        toggles[2]->setBounds (row.removeFromLeft (80));
        a.removeFromTop (8);
        auto labelsRow = a.removeFromTop (14);
        auto combosRow = a.removeFromTop (24);
        const int cw = combosRow.getWidth() / 3;
        labels[5]->setBounds (labelsRow.removeFromLeft (cw).reduced (2, 0));
        combos[5]->setBounds (combosRow.removeFromLeft (cw).reduced (2, 0));
        labelsRow.removeFromLeft (cw);
        combos[6]->setBounds (combosRow.removeFromLeft (cw).reduced (2, 0));
        labels[6]->setBounds (labelsRow.removeFromLeft (cw).reduced (2, 0));
        patternBox.setBounds (combosRow.reduced (2, 0));
        a.removeFromTop (16);
        auto kr = a.removeFromTop (86);
        const int kw = kr.getWidth() / 4;
        for (int i = 3; i < 7; ++i) knobs[i]->setBounds (kr.removeFromLeft (kw));
    }
    // grid
    {
        auto g = gridArea.reduced (12, 10);
        g.removeFromTop (20);
        g.removeFromBottom (18);
        auto side = g.removeFromLeft (130);
        grid.setBounds (g);
        for (int r = 0; r < ArpGrid::numRows; ++r)
        {
            auto ra = grid.rowArea (r).toNearestInt().translated (side.getX(), g.getY());
            auto btn = Rectangle<int> (side.getRight() - 50, ra.getY() + 2, 22, jmin (22, ra.getHeight() - 4));
            rowReset[r]->setBounds (btn);
            rowRandom[r]->setBounds (btn.translated (24, 0));
        }
    }
}

void PlayPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    for (auto [r, t, c] : { std::tuple<Rectangle<int>, const char*, Colour> { chordArea, "", chordCol }, { arpArea, "", arpCol },
                            { gridArea, "ARP PATTERN  -  click / drag to paint, right-click to go down.  Drop a MIDI file here to copy its groove.", arpCol } })
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (c);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (11.0f, true));
        g.drawText (t, r.reduced (12, 6).removeFromTop (16), Justification::centredLeft);
    }
    // chord circles (one per scale degree)
    const int key = (int) proc.apvts.getRawParameterValue ("ch_key")->load();
    const int scale = (int) proc.apvts.getRawParameterValue ("ch_scale")->load();
    const int type = (int) proc.apvts.getRawParameterValue ("ch_type")->load();
    const bool follow = proc.apvts.getRawParameterValue ("ch_follow")->load() < 0.5f;
    const bool on = proc.apvts.getRawParameterValue ("ch_on")->load() > 0.5f;
    const int last = proc.perform.lastChordRoot.load();
    static const char* roman[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    for (int d = 0; d < 7; ++d)
    {
        auto c = circles[(size_t) d];
        const int root = degreeRoot (d);
        const bool hit = last >= 0 && (last % 12) == (root % 12);
        const auto colr = chordCol.withRotatedHue (-0.1f + 0.05f * d);
        for (int ring = 0; ring < 3; ++ring)
        {
            g.setColour (colr.withAlpha ((on ? 0.25f : 0.1f) + (hit ? 0.35f : 0.0f) - ring * 0.06f));
            g.drawEllipse (c.reduced (ring * 6.0f), hit ? 2.5f : 1.5f);
        }
        if (hit) { g.setColour (colr.withAlpha (0.18f)); g.fillEllipse (c); }
        g.setColour (on ? col::text : col::dim);
        g.setFont (font (13.0f, true));
        g.drawText (perf::chordName (root, key, scale, type, follow), c.toNearestInt(), Justification::centred);
        g.setColour (col::dim);
        g.setFont (font (10.0f));
        g.drawText (roman[d], c.withY (c.getBottom() + 2.0f).withHeight (12.0f).toNearestInt(), Justification::centred);
    }
    g.setColour (col::dim);
    g.setFont (font (10.0f));
    g.drawText ("one finger plays the whole chord  -  click a chord to hear it", chordArea.reduced (14, 12).removeFromTop (24), Justification::centredRight);

    // grid row names
    auto ga = gridArea.reduced (12, 10);
    ga.removeFromTop (20);
    for (int r = 0; r < ArpGrid::numRows; ++r)
    {
        auto ra = grid.rowArea (r).toNearestInt().translated (ga.getX(), grid.getY());
        g.setColour (col::text);
        g.setFont (font (12.0f, true));
        g.drawText (ArpGrid::rowName (r), Rectangle<int> (ga.getX(), ra.getY(), 76, ra.getHeight()), Justification::centredLeft);
    }
    if (status.isNotEmpty())
    {
        g.setColour (arpCol);
        g.setFont (font (11.0f));
        g.drawText (status, gridArea.reduced (14, 6).removeFromBottom (14), Justification::centredLeft);
    }
    if (dragOver)
    {
        g.setColour (arpCol.withAlpha (0.12f));
        g.fillRoundedRectangle (gridArea.toFloat(), 8.0f);
        g.setColour (arpCol);
        g.setFont (font (18.0f, true));
        g.drawText ("Drop to copy this groove into the arp", gridArea, Justification::centred);
    }
}
} // namespace ff::gui
