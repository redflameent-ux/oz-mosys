// 4x4 pads (from Sol Intent). Higher on a pad = harder hit.
#include "SamplerPage.h"

using namespace juce;

Rectangle<float> PadGrid::padBounds (int pad) const
{
    auto b = getLocalBounds().toFloat();
    const float gap = 6.0f;
    const float w = (b.getWidth() - gap * 3) / 4.0f, h = (b.getHeight() - gap * 3) / 4.0f;
    const int col = pad % 4, rowFromBottom = pad / 4;
    return { b.getX() + (float) col * (w + gap), b.getBottom() - h - (float) rowFromBottom * (h + gap), w, h };
}

int PadGrid::padAt (Point<float> pt) const
{
    for (int i = 0; i < 16; ++i)
        if (padBounds (i).contains (pt)) return i;
    return -1;
}

void PadGrid::paint (Graphics& g)
{
    for (int i = 0; i < 16; ++i)
    {
        const int note = proc.padBaseNote + i;
        auto r = padBounds (i);
        const bool playable = art != nullptr && note >= art->lowestKey && note <= art->highestKey;
        const bool recorded = art != nullptr && art->hasRootAt (note);
        const float glow = jlimit (0.0f, 1.0f, std::sqrt (proc.noteGlow[(size_t) jlimit (0, 127, note)].load() * 3.0f));
        const bool down = proc.keyboardState.isNoteOn (1, note);

        g.setColour (playable ? ui::panelHi : ui::panel);
        g.fillRoundedRectangle (r, 6.0f);
        if (glow > 0.01f || down)
        {
            g.setColour (ui::teal.withAlpha (jmax (glow * 0.35f, down ? 0.30f : 0.0f)));
            g.fillRoundedRectangle (r, 6.0f);
        }
        g.setColour (down || glow > 0.05f ? ui::teal : playable ? ui::goldDim.withAlpha (0.7f) : ui::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, down ? 1.5f : 1.0f);

        g.setColour (playable ? ui::text : ui::textDim.withAlpha (0.5f));
        g.setFont (FontOptions (13.0f, Font::bold));
        g.drawText (sol::noteName (note), r.reduced (8, 6), Justification::bottomLeft);
        g.setColour (ui::textDim.withAlpha (0.7f));
        g.setFont (FontOptions (10.0f));
        g.drawText (String (i + 1), r.reduced (8, 6), Justification::topLeft);
        if (recorded)   // a real recording lives on this pad
        {
            g.setColour (ui::teal);
            g.fillEllipse (Rectangle<float> (6, 6).withCentre ({ r.getRight() - 10, r.getY() + 10 }));
        }
    }
}

void PadGrid::mouseDown (const MouseEvent& e)
{
    int pad = padAt (e.position);
    if (pad < 0) return;
    auto r = padBounds (pad);
    // higher on the pad = harder strike
    float vel = jmap (jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()), 0.35f, 1.0f);
    heldNote = proc.padBaseNote + pad;
    proc.keyboardState.noteOn (1, heldNote, vel);
}

void PadGrid::mouseDrag (const MouseEvent& e)
{
    int pad = padAt (e.position);
    if (pad >= 0 && proc.padBaseNote + pad != heldNote)
    {
        release();
        mouseDown (e);
    }
}

void PadGrid::mouseUp (const MouseEvent&) { release(); }

void PadGrid::release()
{
    if (heldNote >= 0) proc.keyboardState.noteOff (1, heldNote, 0.0f);
    heldNote = -1;
}

