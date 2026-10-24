// Offline test: renders every factory preset to a WAV, reports levels and speed,
// and (optionally) saves a screenshot of the plugin window.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool screenshot = argc > 2 && juce::String (argv[2]) == "--screenshot";

    const double sr = 48000.0;
    const int block = 512;
    int failures = 0;

    for (int preset = 0; preset < (int) ff::factoryPresets().size(); ++preset)
    {
        FourfoldProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        proc.setCurrentProgram (preset);
        if (ff::factoryPresets()[(size_t) preset].name == "Macro Morph")
            if (auto* p = proc.apvts.getParameter ("macro1")) p->setValueNotifyingHost (0.7f);

        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        const int notes[] = { 48, 55, 60, 64 };
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            const int n = juce::jmin (block, total - pos);
            if (pos == 0) for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
            if (pos <= (int) (sr * 2.0) && pos + block > (int) (sr * 2.0))
                for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOff (1, nn), (int) (sr * 2.0) - pos);
            buf.setSize (2, n, false, false, true);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;

        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (ch, i))) finite = false;
        const float peak = out.getMagnitude (0, total);
        const float rmsHeld = (out.getRMSLevel (0, 0, (int) sr) + out.getRMSLevel (1, 0, (int) sr)) * 0.5f;
        const auto name = ff::factoryPresets()[(size_t) preset].name;
        const bool ok = finite && peak > 0.01f && peak < 1.5f && rmsHeld > 0.003f;
        if (! ok) ++failures;
        std::printf ("%-14s peak %.3f  rms %.4f  render %.0f ms for 4 s audio (%.1f%% of real time)  %s\n",
                     name.toRawUTF8(), peak, rmsHeld, ms, ms / 40.0, ok ? "OK" : "PROBLEM");
        writeWav (outDir.getChildFile (juce::String (preset) + "_" + name.replace (" ", "_") + ".wav"), out, sr);
    }

    if (screenshot)
    {
        FourfoldProcessor proc;
        proc.prepareToPlay (sr, block);
        proc.setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("screenshot.png");
        f.deleteFile();
        if (auto s = f.createOutputStream()) png.writeImageToStream (img, *s);
        std::printf ("screenshot written %dx%d\n", img.getWidth(), img.getHeight());
    }

    std::printf ("%s\n", failures == 0 ? "ALL PRESETS OK" : "SOME PRESETS HAVE PROBLEMS");
    return failures == 0 ? 0 : 1;
}
