#include "SlicerPanel.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static const Colour sliceCol { 0xfff5c542 };

SlicerPanel::SlicerPanel (FourfoldProcessor& p) : proc (p)
{
    setOpaque (true);
    for (auto* b : { &loadBtn, &makeBtn, &closeBtn }) addAndMakeVisible (b);
    makeBtn.setColour (TextButton::buttonColourId, sliceCol.withAlpha (0.3f));
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose a loop", File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
    };
    makeBtn.onClick = [this] { makePads(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    modeBox.addItemList ({ "At the hits (transients)", "4 equal slices", "8 equal slices", "16 equal slices", "32 equal slices" }, 1);
    modeBox.setSelectedItemIndex (0, dontSendNotification);
    modeBox.onChange = [this] { detect(); };
    addAndMakeVisible (modeBox);
    sensitivity.setSliderStyle (Slider::LinearHorizontal);
    sensitivity.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    sensitivity.setRange (0.0, 1.0, 0.01);
    sensitivity.setValue (0.5, dontSendNotification);
    sensitivity.setColour (Slider::trackColourId, sliceCol);
    sensitivity.onDragEnd = [this] { detect(); };
    addAndMakeVisible (sensitivity);
    for (auto [l, t] : { std::pair<Label*, const char*> { &sensL, "Sensitivity" }, { &modeL, "Cut" }, { &nameL, "Name" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    info.setFont (font (11.5f));
    info.setColour (Label::textColourId, sliceCol);
    addAndMakeVisible (info);
    nameEd.setFont (font (13.0f));
    addAndMakeVisible (nameEd);
    info.setText ("Drop a loop (WAV / AIFF / FLAC / MP3) here, or press Load loop...", dontSendNotification);
}

bool SlicerPanel::isInterestedInFileDrag (const StringArray& f)
{
    for (auto& s : f) if (File (s).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void SlicerPanel::filesDropped (const StringArray& f, int, int)
{
    dragOver = false;
    for (auto& s : f) if (File (s).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { loadFile (File (s)); break; }
    repaint();
}

void SlicerPanel::loadFile (const File& f)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { info.setText ("Couldn't read that file", dontSendNotification); return; }
    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60.0));
    audio.setSize (2, len);
    r->read (&audio, 0, len, 0, true, true);
    if (r->numChannels == 1) audio.copyFrom (1, 0, audio, 0, 0, len);
    fileRate = r->sampleRate;
    source = f;
    nameEd.setText (f.getFileNameWithoutExtension(), false);
    // peaks for drawing
    peaks.assign (800, 0.0f);
    for (int i = 0; i < len; ++i)
    {
        const int b = (int) ((int64) i * 800 / jmax (1, len));
        peaks[(size_t) b] = jmax (peaks[(size_t) b], std::abs (audio.getSample (0, i)), std::abs (audio.getSample (1, i)));
    }
    // tempo guess: assume a whole number of bars of 4/4 between 70 and 180 bpm
    const double secs = len / fileRate;
    bpmGuess = 0;
    for (int bars : { 1, 2, 4, 8, 16 })
    {
        const double bpm = bars * 4 * 60.0 / secs;
        if (bpm >= 70 && bpm <= 180) { bpmGuess = bpm; break; }
    }
    detect();
}

void SlicerPanel::detect()
{
    slices.clear();
    const int len = audio.getNumSamples();
    if (len <= 0) { repaint(); return; }
    const int mode = modeBox.getSelectedItemIndex();
    if (mode >= 1)
    {
        const int n = mode == 1 ? 4 : mode == 2 ? 8 : mode == 3 ? 16 : 32;
        for (int i = 0; i < n; ++i) slices.push_back ((int64) len * i / n);
    }
    else
    {
        // onset detection: rises in short-term energy (log), adaptive threshold
        const int hop = 256;
        std::vector<float> e;
        for (int s = 0; s + hop <= len; s += hop)
        {
            double sum = 0;
            for (int i = 0; i < hop; ++i) { const float v = audio.getSample (0, s + i) + audio.getSample (1, s + i); sum += v * v; }
            e.push_back ((float) std::log10 (1.0e-9 + sum / hop));
        }
        std::vector<float> flux (e.size(), 0.0f);
        for (size_t i = 1; i < e.size(); ++i) flux[i] = jmax (0.0f, e[i] - e[i - 1]);
        double mean = 0, var = 0;
        for (auto v : flux) mean += v;
        mean /= jmax ((size_t) 1, flux.size());
        for (auto v : flux) var += (v - mean) * (v - mean);
        const double sd = std::sqrt (var / jmax ((size_t) 1, flux.size()));
        const double th = mean + sd * (2.5 - 2.2 * sensitivity.getValue());
        const int minGap = (int) (fileRate * 0.07 / hop);
        slices.push_back (0);
        int last = -minGap;
        for (int i = 1; i + 1 < (int) flux.size(); ++i)
            if (flux[(size_t) i] > th && flux[(size_t) i] >= flux[(size_t) i - 1] && flux[(size_t) i] >= flux[(size_t) i + 1] && i - last >= minGap)
            {
                // step back to the quiet just before the hit
                slices.push_back ((int64) jmax (0, (i - 1) * hop));
                last = i;
                if (slices.size() >= 64) break;
            }
    }
    std::sort (slices.begin(), slices.end());
    slices.erase (std::unique (slices.begin(), slices.end()), slices.end());
    info.setText (String ((int) slices.size()) + " slices" + (bpmGuess > 0 ? "   -   loop looks like about " + String (bpmGuess, 1) + " BPM" : String())
                  + "   -   drag a marker to move it, double-click to add, right-click to remove", dontSendNotification);
    repaint();
}

Rectangle<float> SlicerPanel::waveArea() const { return getLocalBounds().toFloat().reduced (16.0f, 0.0f).withTrimmedTop (96.0f).withTrimmedBottom (60.0f); }
float SlicerPanel::xFor (int64 s) const { auto w = waveArea(); return w.getX() + w.getWidth() * (float) s / (float) jmax (1, audio.getNumSamples()); }
int64 SlicerPanel::sampleFor (float x) const { auto w = waveArea(); return std::clamp<juce::int64> ((int64) ((x - w.getX()) / w.getWidth() * audio.getNumSamples()), 0, std::max (0, audio.getNumSamples() - 1)); }

void SlicerPanel::mouseDown (const MouseEvent& e)
{
    if (audio.getNumSamples() == 0 || ! waveArea().contains (e.position)) return;
    dragSlice = -1;
    for (int i = 0; i < (int) slices.size(); ++i)
        if (std::abs (xFor (slices[(size_t) i]) - e.position.x) < 6.0f) dragSlice = i;
    if (e.mods.isPopupMenu() && dragSlice > 0) { slices.erase (slices.begin() + dragSlice); dragSlice = -1; detect(), (void) 0; }
    if (e.getNumberOfClicks() >= 2 && dragSlice < 0) { slices.push_back (sampleFor (e.position.x)); std::sort (slices.begin(), slices.end()); }
    repaint();
}

void SlicerPanel::mouseDrag (const MouseEvent& e)
{
    if (dragSlice <= 0) return;   // the first slice always starts at 0
    slices[(size_t) dragSlice] = sampleFor (e.position.x);
    repaint();
}

void SlicerPanel::mouseUp (const MouseEvent&) { if (dragSlice > 0) std::sort (slices.begin(), slices.end()); dragSlice = -1; }

static String noteNameFor (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (names[n % 12]) + String (n / 12 - 2);   // C3 = middle C, like the rest of VIBRA-VII
}

void SlicerPanel::makePads()
{
    if (audio.getNumSamples() == 0 || slices.empty()) { info.setText ("Load a loop first", dontSendNotification); return; }
    auto name = File::createLegalFileName (nameEd.getText().trim());
    if (name.isEmpty()) name = "Slices";
    auto folder = sol::Library::createInstrument ("Slices - " + name);
    if (folder == File()) { info.setText ("Couldn't create the instrument folder", dontSendNotification); return; }
    const int base = proc.sampler->padBaseNote;
    WavAudioFormat wav;
    const int fade = (int) (fileRate * 0.002);
    sol::InstrumentSpec spec;
    spec.name = folder.getFileName();
    spec.letRing = true;
    spec.articulations.push_back ({ "Slices", {} });
    for (size_t i = 0; i < slices.size() && i < 64; ++i)
    {
        const int64 start = slices[i], end = i + 1 < slices.size() ? slices[i + 1] : audio.getNumSamples();
        const int len = (int) std::max<juce::int64> (16, end - start);
        AudioBuffer<float> part (2, len);
        for (int ch = 0; ch < 2; ++ch) part.copyFrom (ch, 0, audio, ch, (int) start, len);
        part.applyGainRamp (0, jmin (fade, len / 2), 0.0f, 1.0f);
        part.applyGainRamp (len - jmin (fade * 2, len / 2), jmin (fade * 2, len / 2), 1.0f, 0.0f);
        const int note = jmin (127, base + (int) i);
        auto f = folder.getChildFile ("Slice " + String ((int) i + 1).paddedLeft ('0', 2) + ".wav");
        sol::ZoneSpec z;
        z.file = f.getFileName();
        z.root = z.lo = z.hi = note;
        spec.articulations[0].zones.push_back (z);
        std::unique_ptr<OutputStream> os (f.createOutputStream().release());
        if (os == nullptr) continue;
        if (auto* w = wav.createWriterFor (os.get(), fileRate, 2, 24, {}, 0))
        {
            os.release();
            std::unique_ptr<AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (part, 0, len);
        }
    }
    sol::Library::writeSpec (folder, spec);
    info.setText ("Made " + String ((int) std::min<size_t> (64, slices.size())) + " pads starting at " + noteNameFor (base), dontSendNotification);
    if (onMade) onMade (folder);
}

void SlicerPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (30);
    closeBtn.setBounds (top.removeFromLeft (90)); top.removeFromLeft (16);
    loadBtn.setBounds (top.removeFromLeft (120)); top.removeFromLeft (16);
    modeL.setBounds (top.removeFromLeft (30));
    modeBox.setBounds (top.removeFromLeft (200)); top.removeFromLeft (16);
    sensL.setBounds (top.removeFromLeft (76));
    sensitivity.setBounds (top.removeFromLeft (160)); top.removeFromLeft (16);
    makeBtn.setBounds (top.removeFromRight (120)); top.removeFromRight (8);
    nameEd.setBounds (top.removeFromRight (180)); top.removeFromRight (4);
    nameL.setBounds (top.removeFromRight (44));
    r.removeFromTop (10);
    info.setBounds (r.removeFromTop (20));
    sensitivity.setEnabled (modeBox.getSelectedItemIndex() == 0);
}

void SlicerPanel::paint (Graphics& g)
{
    g.fillAll (col::bg);
    auto w = waveArea();
    g.setColour (col::panel);
    g.fillRoundedRectangle (w, 6.0f);
    if (peaks.empty())
    {
        g.setColour (col::dim);
        g.setFont (font (16.0f, true));
        g.drawText ("Drop a drum loop or any loop here", w.toNearestInt(), Justification::centred);
    }
    else
    {
        const float mid = w.getCentreY();
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float x = w.getX() + w.getWidth() * (float) i / (float) peaks.size();
            const float h = peaks[i] * w.getHeight() * 0.46f;
            g.setColour (sliceCol.withAlpha (0.55f));
            g.drawVerticalLine ((int) x, mid - h, mid + h);
        }
        const int base = proc.sampler->padBaseNote;
        for (size_t i = 0; i < slices.size(); ++i)
        {
            const float x = xFor (slices[i]);
            g.setColour ((int) i == dragSlice ? Colours::white : col::text.withAlpha (0.8f));
            g.drawLine (x, w.getY(), x, w.getBottom(), 1.5f);
            auto tag = Rectangle<float> (x, w.getY(), 34.0f, 16.0f);
            g.setColour (sliceCol);
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (Colours::black);
            g.setFont (font (10.0f, true));
            g.drawText (String ((int) i + 1), tag.toNearestInt(), Justification::centred);
            g.setColour (col::dim);
            g.drawText (noteNameFor (base + (int) i), Rectangle<float> (x + 2.0f, w.getBottom() + 4.0f, 40.0f, 14.0f).toNearestInt(), Justification::left);
        }
    }
    g.setColour (col::dim);
    g.setFont (font (11.0f));
    g.drawText ("Each slice goes on its own key / pad from the pad page's lowest note. Turn on CHOKE so one slice cuts the last, like an MPC.",
                getLocalBounds().reduced (16, 10).removeFromBottom (18), Justification::centredLeft);
    if (dragOver)
    {
        g.setColour (sliceCol.withAlpha (0.12f));
        g.fillRect (getLocalBounds());
    }
}
} // namespace ff::gui
