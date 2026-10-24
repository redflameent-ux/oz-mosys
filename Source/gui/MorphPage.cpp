#include "MorphPage.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static const Colour morphCol { 0xff7ee0c3 };
static const char* cornerName[] = { "A", "B", "C", "D" };

Colour MorphPad::cornerColour (int i)
{
    static const Colour c[] = { Colour (0xff4fc3f7), Colour (0xffb98cff), Colour (0xfff5c542), Colour (0xffff7a6b) };
    return c[jlimit (0, 3, i)];
}

Rectangle<float> MorphPad::square() const
{
    auto b = getLocalBounds().toFloat().reduced (24.0f);
    const float s = jmin (b.getWidth(), b.getHeight());
    return b.withSizeKeepingCentre (s, s);
}

Point<float> MorphPad::toXY (Point<float> p) const
{
    auto s = square();
    return { jlimit (0.0f, 1.0f, (p.x - s.getX()) / s.getWidth()), jlimit (0.0f, 1.0f, 1.0f - (p.y - s.getY()) / s.getHeight()) };
}

void MorphPad::paint (Graphics& g)
{
    auto s = square();
    auto& m = *proc.morph;
    // corner glow: each corner's colour fades toward the middle
    const Point<float> pts[] = { s.getTopLeft(), s.getTopRight(), s.getBottomLeft(), s.getBottomRight() };
    g.setColour (col::panel);
    g.fillRoundedRectangle (s, 10.0f);
    for (int i = 0; i < 4; ++i)
    {
        ColourGradient grad (cornerColour (i).withAlpha (m.hasCorner (i) ? 0.30f : 0.06f), pts[i], Colours::transparentBlack, s.getCentre(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (s, 10.0f);
    }
    g.setColour (col::border);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (s.getX() + s.getWidth() * i / 4), s.getY(), s.getBottom());
        g.drawHorizontalLine ((int) (s.getY() + s.getHeight() * i / 4), s.getX(), s.getRight());
    }
    g.drawRoundedRectangle (s, 10.0f, 1.0f);
    // recorded path
    const auto& path = m.getPath();
    if (path.size() > 1)
    {
        Path pp;
        for (size_t i = 0; i < path.size(); ++i)
        {
            Point<float> q (s.getX() + path[i].x * s.getWidth(), s.getBottom() - path[i].y * s.getHeight());
            if (i == 0) pp.startNewSubPath (q); else pp.lineTo (q);
        }
        g.setColour (col::text.withAlpha (0.25f));
        g.strokePath (pp, PathStrokeType (1.2f));
    }
    // trail
    for (size_t i = 0; i < trail.size(); ++i)
    {
        const float a = (float) i / (float) trail.size();
        Point<float> q (s.getX() + trail[i].x * s.getWidth(), s.getBottom() - trail[i].y * s.getHeight());
        g.setColour (morphCol.withAlpha (a * 0.5f));
        g.fillEllipse (Rectangle<float> (4.0f + a * 6.0f, 4.0f + a * 6.0f).withCentre (q));
    }
    // corner labels
    for (int i = 0; i < 4; ++i)
    {
        auto tag = Rectangle<float> (26.0f, 26.0f).withCentre (pts[i]);
        g.setColour (m.hasCorner (i) ? cornerColour (i) : col::panelHi);
        g.fillEllipse (tag);
        g.setColour (m.hasCorner (i) ? Colours::black : col::dim);
        g.setFont (font (13.0f, true));
        g.drawText (cornerName[i], tag.toNearestInt(), Justification::centred);
    }
    // puck
    auto p = m.puck();
    Point<float> q (s.getX() + p.x * s.getWidth(), s.getBottom() - p.y * s.getHeight());
    const bool on = proc.apvts.getRawParameterValue ("morph_on")->load() > 0.5f;
    g.setColour ((on ? morphCol : col::dim).withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (40.0f, 40.0f).withCentre (q));
    g.setColour (on ? morphCol : col::dim);
    g.fillEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (q));
    g.setColour (Colours::white);
    g.drawEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (q), 1.5f);
    if (! m.hasCorner (0))
    {
        g.setColour (col::text);
        g.setFont (font (14.0f, true));
        g.drawFittedText ("Press  Make B C D from this sound  to start,\nor set each corner yourself.", s.reduced (40.0f).toNearestInt(), Justification::centred, 3);
    }
}

void MorphPad::mouseDown (const MouseEvent& e)
{
    if (! square().expanded (12.0f).contains (e.position)) return;
    lastPos = toXY (e.position); lastT = Time::getMillisecondCounterHiRes(); vel = {};
    proc.morph->grab (lastPos.x, lastPos.y);
}

void MorphPad::mouseDrag (const MouseEvent& e)
{
    const auto p = toXY (e.position);
    const double t = Time::getMillisecondCounterHiRes();
    const float dt = (float) jmax (1.0, t - lastT) * 0.001f;
    vel = vel * 0.6f + ((p - lastPos) / dt) * 0.4f;
    lastPos = p; lastT = t;
    proc.morph->grab (p.x, p.y);
}

void MorphPad::mouseUp (const MouseEvent&)
{
    if (proc.morph->held) proc.morph->release (vel.x, vel.y);
}

// ============================================================================ page
MorphPage::MorphPage (FourfoldProcessor& p, std::function<void (Knob&)> reg) : proc (p), pad (p)
{
    setOpaque (true);
    addAndMakeVisible (pad);
    for (auto* b : { &closeBtn, &fillBtn, &clearBtn, &recBtn }) addAndMakeVisible (b);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    fillBtn.setColour (TextButton::buttonColourId, morphCol.withAlpha (0.3f));
    fillBtn.setTooltip ("Corner A becomes the sound you have now; B, C and D become variations of it");
    fillBtn.onClick = [this] { fillVariations(); };
    clearBtn.onClick = [this] { proc.morph->clearCorners(); repaint(); };
    recBtn.setClickingTogglesState (false);
    recBtn.setTooltip ("Press, then drag the puck around. Let go to stop. Set the Orb to Recorded path to loop it.");
    recBtn.onClick = [this]
    {
        if (proc.morph->isRecording()) proc.morph->stopRecording();
        else
        {
            proc.morph->startRecording();
            if (auto* m = proc.apvts.getParameter ("orb_mode")) m->setValueNotifyingHost (m->convertTo0to1 (0.0f));   // hold still while recording
        }
    };
    for (int i = 0; i < 4; ++i)
    {
        auto* s = setBtns.add (new TextButton (String ("Set ") + cornerName[i]));
        s->setTooltip (String ("Corner ") + cornerName[i] + " becomes the sound you hear right now");
        s->setColour (TextButton::buttonColourId, MorphPad::cornerColour (i).withAlpha (0.25f));
        s->onClick = [this, i] { proc.morph->setCorner (i, proc.morph->captureNow()); repaint(); };
        addAndMakeVisible (s);
        auto* d = diceBtns.add (new TextButton ("Dice"));
        d->setTooltip (String ("Corner ") + cornerName[i] + " becomes a new variation of corner A");
        d->onClick = [this, i]
        {
            if (! proc.morph->hasCorner (0)) proc.morph->setCorner (0, proc.morph->captureNow());
            auto v = proc.mutator->variationValues ((float) varAmount.getValue());
            perf::MorphEngine::Values keep;
            for (auto& [id, val] : v) if (perf::MorphEngine::morphable (id)) keep[id] = val;
            proc.morph->setCorner (i, keep);
            repaint();
        };
        addAndMakeVisible (d);
    }
    onToggle.setColour (ToggleButton::tickColourId, morphCol);
    addAndMakeVisible (onToggle);
    onAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "morph_on", onToggle);
    orbMode.addItemList ({ "Still", "Dice", "Inertia", "Attractor", "Recorded path" }, 1);
    addAndMakeVisible (orbMode);
    modeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "orb_mode", orbMode);
    varAmount.setSliderStyle (Slider::LinearHorizontal);
    varAmount.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    varAmount.setRange (0.05, 1.0, 0.01);
    varAmount.setValue (0.45, dontSendNotification);
    varAmount.setColour (Slider::trackColourId, morphCol);
    addAndMakeVisible (varAmount);
    for (auto [l, t] : { std::pair<Label*, const char*> { &varL, "How different the corners are" }, { &orbL, "ORB - moves the puck for you" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    help.setFont (font (11.0f));
    help.setColour (Label::textColourId, col::dim);
    help.setJustificationType (Justification::topLeft);
    help.setText ("Drag the puck to blend the four corners. Smooth settings glide; switches jump to the nearest corner.\n\n"
                  "Dice: jumps to random spots.  Inertia: throw the puck and it keeps bouncing.  "
                  "Attractor: circles the middle.  Recorded path: loops the motion you recorded.\n\n"
                  "Morph X / Y can be MIDI-learned or automated like any knob. Saving the preset keeps the corners.", dontSendNotification);
    addAndMakeVisible (help);
    for (auto [id, label] : { std::pair<const char*, const char*> { "morph_x", "Morph X" }, { "morph_y", "Morph Y" }, { "orb_speed", "Speed" }, { "orb_depth", "Depth" } })
    {
        auto* k = knobs.add (new Knob (proc.apvts, id, label, morphCol));
        addAndMakeVisible (k);
        if (reg) reg (*k);
    }
    startTimerHz (30);
}

MorphPage::~MorphPage() { stopTimer(); }

void MorphPage::fillVariations()
{
    auto& m = *proc.morph;
    m.setCorner (0, m.captureNow());
    for (int i = 1; i < 4; ++i)
    {
        auto v = proc.mutator->variationValues ((float) varAmount.getValue() * (0.7f + 0.3f * (float) i / 3.0f));
        perf::MorphEngine::Values keep;
        for (auto& [id, val] : v) if (perf::MorphEngine::morphable (id)) keep[id] = val;
        m.setCorner (i, keep);
    }
    if (auto* p = proc.apvts.getParameter ("morph_on")) p->setValueNotifyingHost (1.0f);
    m.grab (0.0f, 1.0f);   // start at A so nothing jumps
    m.release (0.0f, 0.0f);
    repaint();
}

void MorphPage::timerCallback()
{
    if (! isShowing()) return;
    auto p = proc.morph->puck();
    pad.trail.push_back (p);
    while (pad.trail.size() > 24) pad.trail.pop_front();
    const bool rec = proc.morph->isRecording();
    recBtn.setButtonText (rec ? "Recording... (let go to stop)" : "Record motion");
    recBtn.setColour (TextButton::buttonColourId, rec ? Colour (0xffd04040) : col::panelHi);
    pad.repaint();
}

void MorphPage::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto top = r.removeFromTop (28);
    closeBtn.setBounds (top.removeFromLeft (140)); top.removeFromLeft (16);
    onToggle.setBounds (top.removeFromLeft (100));
    r.removeFromTop (6);
    side = r.removeFromRight (420);
    r.removeFromRight (10);
    pad.setBounds (r);

    auto a = side.reduced (12, 10);
    fillBtn.setBounds (a.removeFromTop (30));
    a.removeFromTop (6);
    varL.setBounds (a.removeFromTop (16));
    varAmount.setBounds (a.removeFromTop (22));
    a.removeFromTop (8);
    for (int row = 0; row < 2; ++row)
    {
        auto rr = a.removeFromTop (26);
        const int w = rr.getWidth() / 2;
        for (int c = 0; c < 2; ++c)
        {
            const int i = row * 2 + c;
            auto cell = rr.removeFromLeft (w).reduced (3, 0);
            setBtns[i]->setBounds (cell.removeFromLeft (cell.getWidth() * 3 / 5));
            cell.removeFromLeft (4);
            diceBtns[i]->setBounds (cell);
        }
        a.removeFromTop (6);
    }
    clearBtn.setBounds (a.removeFromTop (24).removeFromRight (130));
    a.removeFromTop (14);
    orbL.setBounds (a.removeFromTop (16));
    auto orow = a.removeFromTop (26);
    orbMode.setBounds (orow.removeFromLeft (180));
    orow.removeFromLeft (8);
    recBtn.setBounds (orow);
    a.removeFromTop (8);
    auto kr = a.removeFromTop (90);
    const int kw = kr.getWidth() / 4;
    for (auto* k : knobs) k->setBounds (kr.removeFromLeft (kw));
    a.removeFromTop (8);
    help.setBounds (a);
}

void MorphPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::panel);
    g.fillRoundedRectangle (side.toFloat(), 8.0f);
}
} // namespace ff::gui
