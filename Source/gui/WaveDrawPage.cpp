#include "WaveDrawPage.h"
#include "../PluginProcessor.h"
#include <complex>

using namespace juce;

namespace ff::gui
{
// ============================================================================ canvas
void WaveCanvas::setSine()
{
    for (int i = 0; i < size; ++i) wave[(size_t) i] = std::sin (MathConstants<float>::twoPi * i / size);
    toHarmonics();
}

void WaveCanvas::fromHarmonics()
{
    std::fill (wave.begin(), wave.end(), 0.0f);
    for (int k = 0; k < numHarmonics; ++k)
        if (harm[(size_t) k] > 0.0001f)
            for (int i = 0; i < size; ++i)
                wave[(size_t) i] += harm[(size_t) k] * std::sin (MathConstants<float>::twoPi * (float) (k + 1) * i / size);
    float pk = 0.0001f;
    for (auto v : wave) pk = jmax (pk, std::abs (v));
    if (pk > 1.0f) for (auto& v : wave) v /= pk;
}

void WaveCanvas::toHarmonics()
{
    // amplitude of the first harmonics (simple DFT, only 32 bins)
    for (int k = 0; k < numHarmonics; ++k)
    {
        double re = 0, im = 0;
        for (int i = 0; i < size; i += 2)
        {
            const double ph = MathConstants<double>::twoPi * (k + 1) * i / size;
            re += wave[(size_t) i] * std::cos (ph); im += wave[(size_t) i] * std::sin (ph);
        }
        harm[(size_t) k] = (float) jmin (1.0, 2.0 * std::sqrt (re * re + im * im) / (size / 2));
    }
}

void WaveCanvas::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    auto r = plot();
    if (harmonicMode)
    {
        const float w = r.getWidth() / numHarmonics;
        for (int k = 0; k < numHarmonics; ++k)
        {
            auto bar = Rectangle<float> (r.getX() + k * w + 2.0f, r.getY(), w - 4.0f, r.getHeight());
            g.setColour (col::panelHi);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (col::wavetable.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() * (1.0f - harm[(size_t) k])), 2.0f);
            g.setColour (col::dim);
            g.setFont (font (9.0f));
            g.drawText (String (k + 1), bar.withTop (bar.getBottom() - 12).toNearestInt(), Justification::centred);
        }
        return;
    }
    g.setColour (col::border);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4), r.getY(), r.getBottom());
    Path p;
    for (int i = 0; i <= 400; ++i)
    {
        const int idx = jmin (size - 1, i * size / 400);
        const float x = r.getX() + r.getWidth() * i / 400.0f;
        const float y = r.getCentreY() - wave[(size_t) idx] * r.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (col::wavetable.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (6.0f));
    g.setColour (col::wavetable);
    g.strokePath (p, PathStrokeType (2.0f));
}

void WaveCanvas::mouseDown (const MouseEvent& e) { last = e.position; drawTo (e.position); }
void WaveCanvas::mouseDrag (const MouseEvent& e) { drawTo (e.position); }

void WaveCanvas::drawTo (Point<float> pos)
{
    auto r = plot();
    if (harmonicMode)
    {
        const int k = jlimit (0, numHarmonics - 1, (int) ((pos.x - r.getX()) / r.getWidth() * numHarmonics));
        harm[(size_t) k] = jlimit (0.0f, 1.0f, (r.getBottom() - pos.y) / r.getHeight());
        fromHarmonics();
    }
    else
    {
        // draw a straight line from the last point so fast strokes leave no gaps
        auto toIdx = [&] (float x) { return jlimit (0, size - 1, (int) ((x - r.getX()) / r.getWidth() * size)); };
        auto toVal = [&] (float y) { return jlimit (-1.0f, 1.0f, (r.getCentreY() - y) / (r.getHeight() * 0.48f)); };
        int i0 = toIdx (last.x), i1 = toIdx (pos.x);
        float v0 = toVal (last.y), v1 = toVal (pos.y);
        if (i0 > i1) { std::swap (i0, i1); std::swap (v0, v1); }
        for (int i = i0; i <= i1; ++i)
            wave[(size_t) i] = i1 == i0 ? v1 : v0 + (v1 - v0) * (float) (i - i0) / (float) (i1 - i0);
        toHarmonics();
    }
    last = pos;
    repaint();
    if (onEdit) onEdit();
}

// ============================================================================ page
WaveDrawPage::WaveDrawPage (FourfoldProcessor& p) : proc (p)
{
    setOpaque (true);
    for (auto& f : frames) { addChildComponent (f); f.onEdit = [this] { repaint (previewArea); }; }
    frames[0].setSine();
    // end wave starts as a bright saw so the morph does something right away
    for (int i = 0; i < WaveCanvas::size; ++i) frames[1].wave[(size_t) i] = 1.0f - 2.0f * (float) i / WaveCanvas::size;
    frames[1].toHarmonics();
    frames[1].fromHarmonics();

    for (auto* b : { &startBtn, &endBtn, &drawBtn, &harmBtn })
    {
        b->setClickingTogglesState (false);
        b->setColour (TextButton::buttonOnColourId, col::wavetable.withAlpha (0.3f));
        addAndMakeVisible (b);
    }
    startBtn.onClick = [this] { selectFrame (0); };
    endBtn.onClick = [this] { selectFrame (1); };
    drawBtn.onClick = [this] { for (auto& f : frames) f.harmonicMode = false; selectFrame (editing); };
    harmBtn.onClick = [this] { for (auto& f : frames) f.harmonicMode = true; selectFrame (editing); };

    auto setWave = [this] (std::function<float (float)> fn)
    {
        auto& c = canvas();
        for (int i = 0; i < WaveCanvas::size; ++i) c.wave[(size_t) i] = fn ((float) i / WaveCanvas::size);
        c.toHarmonics();
        if (c.harmonicMode) c.fromHarmonics();
        c.repaint(); repaint (previewArea);
    };
    sineBtn.onClick = [setWave] { setWave ([] (float x) { return std::sin (MathConstants<float>::twoPi * x); }); };
    sawBtn.onClick = [setWave] { setWave ([] (float x) { return 1.0f - 2.0f * x; }); };
    squareBtn.onClick = [setWave] { setWave ([] (float x) { return x < 0.5f ? 0.8f : -0.8f; }); };
    triBtn.onClick = [setWave] { setWave ([] (float x) { return x < 0.25f ? 4 * x : x < 0.75f ? 2 - 4 * x : 4 * x - 4; }); };
    noiseBtn.onClick = [setWave] { Random r; setWave ([&r] (float) { return r.nextFloat() * 2.0f - 1.0f; }); };
    smoothBtn.onClick = [this]
    {
        auto& w = canvas().wave;
        std::vector<float> tmp (w);
        const int n = (int) w.size();
        for (int i = 0; i < n; ++i)
        {
            float s = 0;
            for (int k = -24; k <= 24; ++k) s += tmp[(size_t) ((i + k + n) % n)];
            w[(size_t) i] = s / 49.0f;
        }
        canvas().toHarmonics(); canvas().repaint(); repaint (previewArea);
    };
    normBtn.onClick = [this]
    {
        auto& w = canvas().wave;
        float mean = 0; for (auto v : w) mean += v; mean /= (float) w.size();
        float pk = 0.0001f; for (auto& v : w) { v -= mean; pk = jmax (pk, std::abs (v)); }
        for (auto& v : w) v /= pk;
        canvas().toHarmonics(); canvas().repaint(); repaint (previewArea);
    };
    copyBtn.onClick = [this]
    {
        auto& other = frames[(size_t) (1 - editing)];
        other.wave = canvas().wave; other.harm = canvas().harm;
        repaint (previewArea);
    };
    grabBtn.setTooltip ("Copy the wavetable frame you're hearing now into this wave, to edit it");
    grabBtn.onClick = [this] { grabCurrent(); };
    applyBtn.setColour (TextButton::buttonColourId, col::wavetable.withAlpha (0.3f));
    applyBtn.setTooltip ("Turn the two waves into a wavetable and play it (saved in Documents/VIBRA-VII/Wavetables)");
    applyBtn.onClick = [this] { apply(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { &sineBtn, &sawBtn, &squareBtn, &triBtn, &noiseBtn, &smoothBtn, &normBtn, &copyBtn, &grabBtn, &applyBtn, &closeBtn })
        addAndMakeVisible (b);

    morphBox.addItemList ({ "Spectral morph", "Crossfade" }, 1);
    morphBox.setSelectedItemIndex (0);
    morphBox.onChange = [this] { repaint (previewArea); };
    framesBox.addItemList ({ "8", "16", "32", "64" }, 1);
    framesBox.setSelectedItemIndex (1);
    nameEd.setText ("My Wave", false);
    nameEd.setFont (font (13.0f));
    for (auto* c : std::initializer_list<Component*> { &morphBox, &framesBox, &nameEd }) addAndMakeVisible (c);
    for (auto [l, t] : { std::pair<Label*, const char*> { &morphL, "Between them" }, { &framesL, "Frames" }, { &nameL, "Name" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (11.0f));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    status.setFont (font (11.5f));
    status.setColour (Label::textColourId, col::wavetable);
    addAndMakeVisible (status);
    selectFrame (0);
}

void WaveDrawPage::selectFrame (int which)
{
    editing = which;
    for (int i = 0; i < 2; ++i) frames[(size_t) i].setVisible (i == which);
    startBtn.setToggleState (which == 0, dontSendNotification);
    endBtn.setToggleState (which == 1, dontSendNotification);
    drawBtn.setToggleState (! canvas().harmonicMode, dontSendNotification);
    harmBtn.setToggleState (canvas().harmonicMode, dontSendNotification);
    canvas().repaint();
    repaint();
}

void WaveDrawPage::grabCurrent()
{
    auto t = proc.getDisplayTable();
    if (t == nullptr) return;
    const float pos = proc.apvts.getRawParameterValue ("wt_pos")->load();
    auto& c = canvas();
    for (int i = 0; i < WaveCanvas::size; ++i) c.wave[(size_t) i] = t->read (pos, (float) i / WaveCanvas::size, 0);
    c.toHarmonics();
    c.repaint(); repaint (previewArea);
    status.setText ("Grabbed the current frame of " + t->name, dontSendNotification);
}

std::vector<std::vector<float>> WaveDrawPage::buildTable (int n) const
{
    const int N = WaveCanvas::size;
    std::vector<std::vector<float>> out;
    if (morphBox.getSelectedItemIndex() == 1)
    {
        for (int f = 0; f < n; ++f)
        {
            const float t = n > 1 ? (float) f / (float) (n - 1) : 0.0f;
            std::vector<float> fr ((size_t) N);
            for (int i = 0; i < N; ++i) fr[(size_t) i] = frames[0].wave[(size_t) i] * (1.0f - t) + frames[1].wave[(size_t) i] * t;
            out.push_back (std::move (fr));
        }
        return out;
    }
    // spectral morph: blend harmonic strengths and glide the phases
    juce::dsp::FFT fft (11);
    std::array<std::vector<std::complex<float>>, 2> spec;
    for (int w = 0; w < 2; ++w)
    {
        std::vector<std::complex<float>> in ((size_t) N), o ((size_t) N);
        for (int i = 0; i < N; ++i) in[(size_t) i] = frames[(size_t) w].wave[(size_t) i];
        fft.perform (in.data(), o.data(), false);
        spec[(size_t) w] = o;
    }
    for (int f = 0; f < n; ++f)
    {
        const float t = n > 1 ? (float) f / (float) (n - 1) : 0.0f;
        std::vector<std::complex<float>> s ((size_t) N), time ((size_t) N);
        for (int k = 1; k < N / 2; ++k)
        {
            const auto a = spec[0][(size_t) k], b = spec[1][(size_t) k];
            const float mag = std::abs (a) * (1.0f - t) + std::abs (b) * t;
            float pa = std::arg (a), pb = std::arg (b);
            float d = pb - pa;
            while (d > MathConstants<float>::pi) d -= MathConstants<float>::twoPi;
            while (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
            const auto v = std::polar (mag, pa + d * t);
            s[(size_t) k] = v;
            s[(size_t) (N - k)] = std::conj (v);
        }
        fft.perform (s.data(), time.data(), true);
        std::vector<float> fr ((size_t) N);
        for (int i = 0; i < N; ++i) fr[(size_t) i] = time[(size_t) i].real();
        out.push_back (std::move (fr));
    }
    return out;
}

void WaveDrawPage::apply()
{
    auto table = buildTable (framesBox.getText().getIntValue());
    auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VIBRA-VII").getChildFile ("Wavetables");
    dir.createDirectory();
    auto name = File::createLegalFileName (nameEd.getText().trim());
    if (name.isEmpty()) name = "My Wave";
    auto file = dir.getChildFile (name + ".wav");
    AudioBuffer<float> buf (1, (int) table.size() * WaveCanvas::size);
    float pk = 0.0001f;
    for (auto& fr : table) for (auto v : fr) pk = jmax (pk, std::abs (v));
    for (size_t f = 0; f < table.size(); ++f)
        for (int i = 0; i < WaveCanvas::size; ++i) buf.setSample (0, (int) f * WaveCanvas::size + i, table[f][(size_t) i] / pk * 0.9f);
    file.deleteFile();
    {
        WavAudioFormat wav;
        std::unique_ptr<OutputStream> os (file.createOutputStream().release());
        if (os == nullptr) { status.setText ("Couldn't save the wavetable", dontSendNotification); return; }
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 1, 24, {}, 0));
        if (w == nullptr) { status.setText ("Couldn't save the wavetable", dontSendNotification); return; }
        os.release();
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
    if (proc.loadUserWavetable (file))
    {
        if (auto* prm = proc.apvts.getParameter ("wt_on")) prm->setValueNotifyingHost (1.0f);
        status.setText ("Playing \"" + name + "\" - sweep Position on the SYNTH page to morph start > end", dontSendNotification);
    }
    else status.setText ("Couldn't load the wavetable", dontSendNotification);
}

void WaveDrawPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (30);
    closeBtn.setBounds (top.removeFromLeft (90)); top.removeFromLeft (20);
    startBtn.setBounds (top.removeFromLeft (110)); top.removeFromLeft (4);
    endBtn.setBounds (top.removeFromLeft (110)); top.removeFromLeft (20);
    drawBtn.setBounds (top.removeFromLeft (80)); top.removeFromLeft (4);
    harmBtn.setBounds (top.removeFromLeft (100));
    applyBtn.setBounds (top.removeFromRight (110)); top.removeFromRight (8);
    nameEd.setBounds (top.removeFromRight (160)); top.removeFromRight (4);
    nameL.setBounds (top.removeFromRight (44));
    r.removeFromTop (10);
    auto tools = r.removeFromTop (28);
    for (auto* b : { &sineBtn, &sawBtn, &squareBtn, &triBtn, &noiseBtn, &smoothBtn, &normBtn, &copyBtn, &grabBtn })
    {
        b->setBounds (tools.removeFromLeft (b == &copyBtn || b == &grabBtn ? 104 : 76));
        tools.removeFromLeft (4);
    }
    framesBox.setBounds (tools.removeFromRight (70)); tools.removeFromRight (4);
    framesL.setBounds (tools.removeFromRight (50)); tools.removeFromRight (10);
    morphBox.setBounds (tools.removeFromRight (140)); tools.removeFromRight (4);
    morphL.setBounds (tools.removeFromRight (90));
    r.removeFromTop (10);
    status.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (6);
    previewArea = r.removeFromBottom (120);
    r.removeFromBottom (10);
    for (auto& f : frames) f.setBounds (r);
}

void WaveDrawPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    // morph preview: the frames of the table, stacked
    auto p = previewArea.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (p, 6.0f);
    g.setColour (col::dim);
    g.setFont (font (10.0f, true));
    g.drawText ("START  >  END   (what the Position knob sweeps through)", previewArea.reduced (10, 4), Justification::topLeft);
    const int show = 8;
    auto area = p.reduced (12.0f, 18.0f).withTrimmedTop (4.0f);
    const float w = area.getWidth() / show;
    const auto preview = buildTable (show);
    for (int f = 0; f < (int) preview.size() && f < show; ++f)
    {
        auto cell = Rectangle<float> (area.getX() + f * w + 3.0f, area.getY(), w - 6.0f, area.getHeight());
        float pk = 0.0001f; for (auto v : preview[(size_t) f]) pk = jmax (pk, std::abs (v));
        Path path;
        for (int i = 0; i <= 64; ++i)
        {
            const float v = preview[(size_t) f][(size_t) jmin (WaveCanvas::size - 1, i * WaveCanvas::size / 64)] / pk;
            const float x = cell.getX() + cell.getWidth() * i / 64.0f, y = cell.getCentreY() - v * cell.getHeight() * 0.45f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (col::wavetable.withAlpha (0.4f + 0.6f * (float) f / (show - 1)));
        g.strokePath (path, PathStrokeType (1.4f));
    }
}
} // namespace ff::gui
