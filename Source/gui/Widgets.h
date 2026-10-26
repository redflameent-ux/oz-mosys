#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Params.h"

namespace ff::gui
{
namespace col
{
    // VIBRA-VII "chakra" palette: an amethyst base (dark purple + lavender = the crown chakra),
    // with each section glowing its chakra colour - root red through crown violet, the white
    // light of a preset split through the prism (Dark Side of the Moon).
    const juce::Colour bg        { 0xff0c0718 };   // amethyst-black
    const juce::Colour panel     { 0xff160f28 };   // dark amethyst
    const juce::Colour panelHi   { 0xff211634 };   // raised amethyst
    const juce::Colour border    { 0xff3f3560 };   // lavender-amethyst edge
    const juce::Colour text      { 0xffece9f6 };   // lavender-white
    const juce::Colour dim       { 0xffa79fc0 };   // muted lavender
    const juce::Colour track     { 0xff2b2348 };   // amethyst track
    const juce::Colour silver    { 0xffc9b8f0 };   // lavender sheen (glints / accents)
    const juce::Colour wavetable { 0xffb57cff };   // crown - violet
    const juce::Colour analog    { 0xfff38a2d };   // sacral - orange
    const juce::Colour granular  { 0xff39abef };   // throat - blue
    const juce::Colour resonator { 0xff3ed07e };   // heart - green
    const juce::Colour sampler   { 0xfff5cb3a };   // solar plexus - yellow
    const juce::Colour mod       { 0xff3ed07e };   // heart - green
    const juce::Colour filter    { 0xffe8392e };   // root - red
    const juce::Colour fx        { 0xffb57cff };   // crown - violet
}

inline juce::Font font (float h, bool bold = false)
{
    auto o = juce::FontOptions (h);
    if (bold) o = o.withStyle ("Bold");
    return juce::Font (o);
}

// ---------------------------------------------------------------------------
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panelHi);
        setColour (juce::ComboBox::outlineColourId, col::border);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::silver);
        setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::border);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, col::panelHi);
        setColour (juce::TextButton::buttonOnColourId, col::border);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderOutlineColourId, col::track);
        setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::TooltipWindow::outlineColourId, col::border);
        setColour (juce::ListBox::backgroundColourId, col::bg);
        setColour (juce::ListBox::outlineColourId, col::border);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::border);
        setColour (juce::TextEditor::focusedOutlineColourId, col::sampler);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::AlertWindow::outlineColourId, col::border);
        setColour (juce::Slider::textBoxBackgroundColourId, col::bg);
        setColour (juce::Slider::trackColourId, col::sampler);
        setColour (juce::ScrollBar::thumbColourId, col::border.brighter (0.3f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = startA + pos * (endA - startA);
        const bool bipolar = s.getProperties()["bipolar"];

        // body
        g.setColour (col::panelHi.brighter (0.05f));
        g.fillEllipse (c.x - r * 0.68f, c.y - r * 0.68f, r * 1.36f, r * 1.36f);
        g.setColour (col::border.brighter (0.2f));
        g.drawEllipse (c.x - r * 0.68f, c.y - r * 0.68f, r * 1.36f, r * 1.36f, 1.0f);

        // track
        const float arcR = r * 0.86f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
        g.setColour (col::track);
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value
        const float from = bipolar ? (startA + endA) * 0.5f : startA;
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, from, angle, true);
        g.setColour (s.isEnabled() ? accent : accent.withSaturation (0.1f));
        g.strokePath (val, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // modulation ring (Pigments-style): how far the mod matrix pushes this knob
        const float modAmt = (float) (double) s.getProperties()["mod"];
        if (std::abs (modAmt) > 0.0005f)
        {
            const float span = endA - startA;
            const float to = juce::jlimit (startA, endA, angle + modAmt * span);
            juce::Path m;
            m.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (angle, to), juce::jmax (angle, to), true);
            g.setColour (col::mod.withAlpha (0.95f));
            g.strokePath (m, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-1.2f, -r * 0.62f, 2.4f, r * 0.34f, 1.0f);
        g.setColour (col::text);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));

        if ((bool) s.getProperties()["dropHover"])
        {
            g.setColour (col::mod.withAlpha (0.35f));
            g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
        if (style == juce::Slider::TwoValueHorizontal)   // range filters
        {
            const float cy0 = b.getCentreY();
            g.setColour (col::track);
            g.fillRoundedRectangle (b.getX(), cy0 - 2.0f, b.getWidth(), 4.0f, 2.0f);
            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (minPos, cy0 - 2.0f, juce::jmax (1.0f, maxPos - minPos), 4.0f, 2.0f);
            g.setColour (col::text);
            for (float px : { minPos, maxPos }) g.fillRoundedRectangle (px - 3.0f, cy0 - 6.0f, 6.0f, 12.0f, 2.0f);
            return;
        }
        if (style == juce::Slider::LinearBar)   // the mapper's number fields
        {
            g.setColour (col::bg);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (col::sampler.withAlpha (s.isEnabled() ? 0.22f : 0.08f));
            g.fillRoundedRectangle (b.withRight (sliderPos), 4.0f);
            g.setColour (col::border.brighter (0.2f));
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
            return;
        }
        const float cy = b.getCentreY();
        g.setColour (col::track);
        g.fillRoundedRectangle (b.getX(), cy - 2.0f, b.getWidth(), 4.0f, 2.0f);
        const float mid = b.getX() + b.getWidth() * 0.5f;
        const bool bipolar = s.getProperties()["bipolar"];
        const float from = bipolar ? mid : b.getX();
        g.setColour (s.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (juce::jmin (from, sliderPos), cy - 2.0f, std::abs (sliderPos - from), 4.0f, 2.0f);
        g.setColour (col::text);
        g.fillEllipse (sliderPos - 4.5f, cy - 4.5f, 9.0f, 9.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.18f) : col::panelHi);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? accent : (hover ? col::dim : col::border));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (on ? accent : col::dim);
        g.setFont (font (11.0f, true));
        g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f, true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
        g.setColour (col::dim);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }
};

// ---------------------------------------------------------------------------
// A knob that accepts dragged-in modulation sources and right-click removal.
// ---------------------------------------------------------------------------
class ModSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

class Knob : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText,
          juce::Colour accent, int modDest = 0)
        : dest (modDest), paramId (paramId)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            auto range = p->getNormalisableRange();
            slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
            setTooltip (p->getName (64) + (dest > 0 ? "  -  drag a mod source here, right-click to remove" : ""));
        }
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (font (10.5f));
        label.setColour (juce::Label::textColourId, col::dim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromBottom (13));
        slider.setBounds (b);
    }

    void setModAmount (float a)
    {
        if (std::abs ((float) (double) slider.getProperties()["mod"] - a) > 1.0e-4f)
        {
            slider.getProperties().set ("mod", a);
            slider.repaint();
        }
    }

    // ---- drag and drop ----
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return dest > 0 && d.description.toString().startsWith ("modsrc:");
    }
    void itemDragEnter (const SourceDetails&) override { setHover (true); }
    void itemDragExit (const SourceDetails&) override  { setHover (false); }
    void itemDropped (const SourceDetails& d) override
    {
        setHover (false);
        if (onModDropped) onModDropped (d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue(), dest);
    }

    void setLabelText (const juce::String& t) { if (label.getText() != t) label.setText (t, juce::dontSendNotification); }

    // MIDI learn feedback
    void setMidiState (int cc, bool isLearning)
    {
        if (cc != midiCC || isLearning != learning) { midiCC = cc; learning = isLearning; repaint(); }
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        auto b = slider.getBounds().toFloat();
        if (learning)
        {
            const float d = juce::jmin (b.getWidth(), b.getHeight()) - 2.0f;
            g.setColour (col::sampler.withAlpha (0.9f));
            g.drawEllipse (juce::Rectangle<float> (d, d).withCentre (b.getCentre()), 2.0f);
            g.setFont (font (9.0f, true));
            g.drawText ("MOVE A KNOB", getLocalBounds().removeFromTop (12), juce::Justification::centred);
        }
        else if (midiCC >= 0)
        {
            g.setColour (col::mod.withAlpha (0.8f));
            g.setFont (font (8.5f, true));
            g.drawText ("CC" + juce::String (midiCC), getLocalBounds().removeFromTop (11).withTrimmedRight (1), juce::Justification::topRight);
        }
    }

    std::function<void (int src, int dst)> onModDropped;
    ModSlider slider;
    const int dest;
    const juce::String paramId;
    int midiCC = -1;
    bool learning = false;

private:
    void setHover (bool h) { slider.getProperties().set ("dropHover", h); slider.repaint(); }
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Little draggable chip that represents a modulation source.
// ---------------------------------------------------------------------------
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModChip (int sourceIndex, const juce::String& text) : source (sourceIndex), label (text)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag onto any knob with a green ring area to modulate it");
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (col::mod.withAlpha (isMouseOver() ? 0.3f : 0.15f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (col::mod);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setFont (font (10.0f, true));
        g.drawFittedText (label, getLocalBounds(), juce::Justification::centred, 1);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
                c->startDragging ("modsrc:" + juce::String (source), this);
    }
    const int source;
private:
    juce::String label;
};

} // namespace ff::gui
