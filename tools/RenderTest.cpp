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
    const bool levels = argc > 2 && juce::String (argv[2]) == "--levels";
    juce::String levelJson ("{\n");

    const double sr = 48000.0;
    const int block = 512;
    int failures = 0;

    for (int preset = 0; preset < (int) ff::factoryPresets().size(); ++preset)
    {
        FourfoldProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        const juce::String only = juce::SystemStats::getEnvironmentVariable ("OZ_ONLY", {});
        if (only.isNotEmpty() && ff::factoryPresets()[(size_t) preset].name != only) continue;
        proc.setCurrentProgram (preset);
        for (auto& kv : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("OZ_SET", {}), ",", ""))
            if (auto* p = proc.apvts.getParameter (kv.upToFirstOccurrenceOf ("=", false, false)))
                p->setValueNotifyingHost (p->convertTo0to1 (kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        if (levels)
            if (auto* p = proc.apvts.getParameter ("master")) p->setValueNotifyingHost (p->convertTo0to1 (0.7f));
        if (ff::factoryPresets()[(size_t) preset].name == "Macro Morph")
            if (auto* p = proc.apvts.getParameter ("macro1")) p->setValueNotifyingHost (0.7f);

        const int total = (int) (sr * 8.0);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        const int notes[] = { 48, 55, 60, 64 };
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            const int n = juce::jmin (block, total - pos);
            if (pos == 0) for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
            if (pos <= (int) (sr * 6.0) && pos + block > (int) (sr * 6.0))
                for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOff (1, nn), (int) (sr * 6.0) - pos);
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
        std::printf ("%-20s peak %.3f  rms %.4f  render %.0f ms for 8 s audio (%.1f%% of real time)  %s\n",
                     name.toRawUTF8(), peak, rmsHeld, ms, ms / 80.0, ok ? "OK" : "PROBLEM");
        {
            // loudest 100 ms window: fair to both slow pads and short plucks
            float rmsEarly = 0.0f;
            const int win = (int) (sr * 0.1);
            for (int st = 0; st + win <= total; st += win / 2)
                rmsEarly = juce::jmax (rmsEarly, 0.5f * (out.getRMSLevel (0, st, win) + out.getRMSLevel (1, st, win)));
            const float target = 0.16f;
            const float gain = 0.49f * target / juce::jmax (1.0e-4f, rmsEarly);
            const float byPeak = 0.7f * std::sqrt (0.85f / juce::jmax (1.0e-4f, peak));   // never clip
            const float master = juce::jlimit (0.3f, 1.5f, juce::jmin (std::sqrt (gain), byPeak));
            levelJson << "  \"" << name << "\": " << juce::String (master, 3) << ",\n";
        }
        writeWav (outDir.getChildFile (juce::String (preset) + "_" + name.replace (" ", "_") + ".wav"), out, sr);
    }

    if (levels)
    {
        levelJson = levelJson.trimEnd().dropLastCharacters (1) + "\n}\n";
        outDir.getChildFile ("levels.json").replaceWithText (levelJson);
    }

    if (screenshot)
    {
        FourfoldProcessor proc;
        proc.prepareToPlay (sr, block);
        int shot = 1;
        const auto want = juce::SystemStats::getEnvironmentVariable ("OZ_SHOT", {});
        for (int i = 0; i < (int) ff::factoryPresets().size(); ++i) if (ff::factoryPresets()[(size_t) i].name == want) shot = i;
        proc.setCurrentProgram (shot);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        if (juce::SystemStats::getEnvironmentVariable ("OZ_PAGE", {}) == "sampler")
        {
            // make a small test instrument from generated recordings
            auto folder = sol::Library::createInstrument ("Test Kalimba");
            auto writeTone = [] (const juce::File& f, double hz)
            {
                juce::AudioBuffer<float> b (2, 48000);
                for (int i = 0; i < 48000; ++i)
                {
                    const float v = (float) (std::sin (i * hz * juce::MathConstants<double>::twoPi / 48000.0) * std::exp (-i / 9000.0) * 0.6);
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                writeWav (f, b, 48000.0);
            };
            writeTone (folder.getChildFile ("C3.wav"), 261.63);
            writeTone (folder.getChildFile ("E3.wav"), 329.63);
            writeTone (folder.getChildFile ("G3.wav"), 392.0);
            writeTone (folder.getChildFile ("C4.wav"), 523.25);
            folder.getChildFile ("Muted").createDirectory();
            writeTone (folder.getChildFile ("Muted").getChildFile ("C3.wav"), 261.63);
            folder.getChildFile ("instrument.json").deleteFile();   // map from the file names
            proc.sampler->rescanLibrary();
            proc.sampler->loadNow ({ "Test Kalimba", "Library", folder });
            juce::MidiBuffer m; juce::AudioBuffer<float> b (2, 512);
            proc.processBlock (b, m);   // swap the instrument in
        }
        if (juce::SystemStats::getEnvironmentVariable ("OZ_PAGE", {}) == "sampler")
            for (auto* c : ed->getChildren())
                if (auto* page = dynamic_cast<SamplerPage*> (c)) { page->setVisible (true); page->toFront (false); }
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
