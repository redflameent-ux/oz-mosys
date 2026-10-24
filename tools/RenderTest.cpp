// Offline test: renders every factory preset to a WAV, reports levels and speed,
// and (optionally) saves a screenshot of the plugin window.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/gui/SlicerPanel.h"
#include "../Source/import/OmniImport.h"
#include "../Source/Presets.h"
#include "../Source/design/SoundFeatures.h"

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

    // ---- Omnisphere import test: OZ_OMNI=/folder converts, then renders every result ----
    if (auto omni = juce::SystemStats::getEnvironmentVariable ("OZ_OMNI", {}); omni.isNotEmpty())
    {
        FourfoldProcessor proc;
        juce::String summary;
        ff::import::importOmnisphereFolder (juce::File (omni), proc.apvts, nullptr, summary);
        std::printf ("%s\n", summary.toRawUTF8());
        auto exp = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("OZ-mosys/Expansions");
        int bad = 0, n = 0;
        for (auto& f : exp.findChildFiles (juce::File::findFiles, true, "*.ozpreset"))
        {
            if (! f.getFullPathName().contains ("Omni - ")) continue;
            FourfoldProcessor p2;
            p2.setPlayConfigDetails (0, 2, sr, block);
            p2.prepareToPlay (sr, block);
            if (! p2.loadPresetFromFile (f)) { std::printf ("LOAD FAIL %s\n", f.getFileName().toRawUTF8()); ++bad; continue; }
            juce::AudioBuffer<float> buf (2, block);
            float peak = 0; double sum = 0; int count = 0;
            for (int b = 0; b < (int) (sr * 3 / block); ++b)
            {
                juce::MidiBuffer midi;
                if (b == 2) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.8f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0); }
                if (b == (int) (sr * 2 / block)) { midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 55), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0); }
                buf.clear();
                p2.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < block; ++i) { const float v = buf.getSample (ch, i); peak = std::max (peak, std::abs (v)); sum += v * v; ++count; }
            }
            const double rms = std::sqrt (sum / std::max (1, count));
            const bool ok = std::isfinite (peak) && peak > 0.005f && peak < 1.5f;
            if (! ok) ++bad;
            ++n;
            std::printf ("%-48s peak %.3f rms %.4f %s\n", f.getFileNameWithoutExtension().substring (0, 48).toRawUTF8(), peak, rms, ok ? "OK" : "CHECK");
        }
        std::printf ("%d rendered, %d to check\n", n, bad);
        return 0;
    }

    // ---- morph self-test ----
    if (juce::SystemStats::getEnvironmentVariable ("OZ_MORPHTEST", {}) == "1")
    {
        FourfoldProcessor proc;
        auto& m = *proc.morph;
        auto a = m.captureNow(), b = a, c = a, d = a;
        b["flt_cutoff"] = 1.0f; c["flt_cutoff"] = 0.0f; d["flt_cutoff"] = 1.0f; a["flt_cutoff"] = 0.0f;
        m.setCorner (0, a); m.setCorner (1, b); m.setCorner (2, c); m.setCorner (3, d);
        proc.apvts.getParameter ("morph_on")->setValueNotifyingHost (1.0f);
        proc.apvts.getParameter ("morph_x")->setValueNotifyingHost (0.25f);
        for (int i = 0; i < 10; ++i) { juce::Thread::sleep (30); m.tick(); }
        std::printf ("morph x=0.25 -> cutoff %.3f (want 0.250)\n", proc.apvts.getParameter ("flt_cutoff")->getValue());
        proc.apvts.getParameter ("orb_mode")->setValueNotifyingHost (proc.apvts.getParameter ("orb_mode")->convertTo0to1 (3.0f));
        for (int i = 0; i < 25; ++i) { juce::Thread::sleep (30); m.tick(); }
        std::printf ("attractor moved puck to %.3f,%.3f\n", m.puck().x, m.puck().y);
        juce::ValueTree t ("S"); m.saveTo (t); m.clearCorners(); m.restoreFrom (t);
        std::printf ("corners after save/restore: %d\n", (int) m.hasCorner (3));
        return 0;
    }

    // ---- measure every factory preset for the BROWSE map ----
    if (juce::SystemStats::getEnvironmentVariable ("OZ_FEATURES", {}) == "1")
    {
        juce::String json ("{\n");
        for (int preset = 0; preset < (int) ff::factoryPresets().size(); ++preset)
        {
            FourfoldProcessor proc;
            proc.setCurrentProgram (preset);
            auto audio = ff::design::renderAudition (proc, sr);
            auto f = ff::design::analyse (audio, sr);
            json << "  \"" << ff::factoryPresets()[(size_t) preset].name << "\": [" << juce::String (f.brightness, 4) << ", "
                 << juce::String (f.aggression, 4) << ", " << juce::String (f.movement, 4) << "],\n";
        }
        json = json.trimEnd().dropLastCharacters (1) + "\n}\n";
        outDir.getChildFile ("features.json").replaceWithText (json);
        std::printf ("features written\n");
        return 0;
    }

    // ---- ROLL test: one gliding line, rendered with the internal transport ----
    if (juce::SystemStats::getEnvironmentVariable ("OZ_ROLL", {}) == "1")
    {
        FourfoldProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        proc.setCurrentProgram (0);
        if (auto* p = proc.apvts.getParameter ("rev_mix")) p->setValueNotifyingHost (0.0f);
        if (auto* p = proc.apvts.getParameter ("flt_on")) p->setValueNotifyingHost (0.0f);
        if (auto* p = proc.apvts.getParameter ("wt_pos")) p->setValueNotifyingHost (0.0f);   // pure sine
        ff::roll::Pattern pat;
        pat.lengthBeats = 16;
        ff::roll::Line l; l.id = pat.nextId++;
        l.nodes = { { 0.0, 57.0f, 0.0f }, { 4.0, 64.0f, 0.0f }, { 8.0, 64.0f, 0.0f } };   // A3 -> E4 over 2 s, then hold
        pat.lines.push_back (l);
        proc.roll.setPattern (pat);
        proc.roll.play();
        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
        }
        auto freqAt = [&] (double t)
        {
            const int s0 = (int) (t * sr), len = 4800;
            int crossings = 0; float prev = out.getSample (0, s0);
            for (int i = 1; i < len; ++i) { const float v = out.getSample (0, s0 + i); if (prev < 0 && v >= 0) ++crossings; prev = v; }
            return crossings / (len / sr);
        };
        std::printf ("ROLL glide: %.1f Hz at 0.05 s (A3 = 220), %.1f Hz at 1.0 s, %.1f Hz at 3.0 s (E4 = 329.6)\n", freqAt (0.05), freqAt (1.0), freqAt (3.0));
        writeWav (outDir.getChildFile ("roll_glide.wav"), out, sr);
        return 0;
    }

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
        proc.syncRack();
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
        for (auto& kv : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("OZ_SET", {}), ",", ""))
            if (auto* p = proc.apvts.getParameter (kv.upToFirstOccurrenceOf ("=", false, false)))
                p->setValueNotifyingHost (p->convertTo0to1 (kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        proc.syncRack();
        if (juce::SystemStats::getEnvironmentVariable ("OZ_PAGE", {}) == "roll")
        {
            ff::roll::Pattern pat;
            pat.lengthBeats = 16;
            pat.modShape = 1; pat.modAmount = 40; pat.modRateBeats = 0.25; pat.modFade = 0.6f;
            const float from[] = { 48, 55, 60, 63, 67, 70 };
            const float to[]   = { 43, 55, 58, 62, 65, 74 };
            for (int i = 0; i < 6; ++i)
            {
                ff::roll::Line l; l.id = pat.nextId++;
                l.velocity = 0.6f + 0.06f * i;
                l.nodes = { { 0.0, from[i], 0.4f }, { 3.0, from[i], 0.0f }, { 5.0, to[i], 0.0f }, { 7.5, to[i], 0.0f } };
                l.pressure = { { 0.0, 0.1f }, { 4.0, 0.8f }, { 7.5, 0.2f } };
                pat.lines.push_back (l);
            }
            ff::roll::Line lead; lead.id = pat.nextId++;
            lead.nodes = { { 8.0, 72.0f, 0.0f }, { 9.0, 75.0f, 0.6f }, { 10.5, 74.0f, -0.5f }, { 12.0, 79.0f, 0.0f }, { 15.5, 77.0f, 0.0f } };
            lead.slide = { { 8.0, 0.5f }, { 12.0, 0.9f }, { 15.5, 0.3f } };
            pat.lines.push_back (lead);
            proc.roll.setPattern (pat);
        }
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
        if (juce::SystemStats::getEnvironmentVariable ("OZ_PAGE", {}) == "roll")
            for (auto* c : ed->getChildren())
                if (auto* page = dynamic_cast<ff::gui::RollPage*> (c)) { page->setVisible (true); page->toFront (false); }
        // OZ_CLICK="BROWSE,Expansions": press buttons (by their text) in order
        {
            std::function<juce::TextButton* (juce::Component*, const juce::String&)> find = [&] (juce::Component* c, const juce::String& t) -> juce::TextButton*
            {
                for (auto* ch : c->getChildren())
                {
                    if (! ch->isVisible()) continue;
                    if (auto* b = dynamic_cast<juce::TextButton*> (ch); b != nullptr && b->getButtonText() == t) return b;
                    if (auto* r = find (ch, t)) return r;
                }
                return nullptr;
            };
            for (auto& t : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("OZ_CLICK", {}), ",", ""))
                if (auto* b = find (ed.get(), t)) { if (b->onClick) b->onClick(); }
                else std::printf ("no button '%s'\n", t.toRawUTF8());
        }
        // OZ_SLICE=/path/loop.wav (after clicking "Slice a loop..."): load it; OZ_SLICE_MAKE=1 also makes pads
        if (auto sf = juce::SystemStats::getEnvironmentVariable ("OZ_SLICE", {}); sf.isNotEmpty())
        {
            std::function<ff::gui::SlicerPanel* (juce::Component*)> findS = [&] (juce::Component* c) -> ff::gui::SlicerPanel*
            {
                for (auto* ch : c->getChildren())
                {
                    if (auto* s = dynamic_cast<ff::gui::SlicerPanel*> (ch)) return s;
                    if (auto* r = findS (ch)) return r;
                }
                return nullptr;
            };
            if (auto* s = findS (ed.get()))
            {
                s->loadFile (juce::File (sf));
                if (juce::SystemStats::getEnvironmentVariable ("OZ_SLICE_MAKE", {}) == "1") s->makePads();
            }
            else std::printf ("no slicer\n");
        }
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
