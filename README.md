# OZ-mosys

A four-in-one synth plugin: VST3 + stand-alone on Windows, AU + VST3 + stand-alone on Mac.

| Engine | Inspired by | What it does |
|---|---|---|
| **Wavetable** | Serum | 6 built-in tables + import your own (.wav, 2048-sample frames), position morphing, 5 warp modes, up to 7-voice unison |
| **Analog** | Analog Lab | Band-limited triangle / saw / pulse morph, sub, noise, analog drift |
| **Granular** | Omnisphere | Turns any audio file into pads and textures: position, scan, size, density, spray, pitch randomness, reverse, stereo width |
| **Resonator** | physical modelling | Plucked strings and sitar (with jawari buzz and sympathetic strings), Tibetan and crystal bowls, gong, handpan, tongue drum, tingsha, music box, tabla, hand drum, clay pot, 808 cowbell, scrap metal, anvil, pipe, vibraphone, marimba, e-piano. Strike or bow/rub, auto re-pluck |
| **Mod system** | Pigments | 3 envelopes, 2 LFOs, 4 macros, velocity, mod wheel, aftertouch, key track, random. Drag a green chip onto any knob to route it; right-click a knob to remove |

Plus a ladder / state-variable filter; Drive (Warm / Rat / Fuzz), Bit-crusher, Tape (wow & flutter, age, hiss), Ring mod, Chorus, Ping-pong Delay and Reverb.
World tunings (Raga Bhairav, Maqam Rast / Bayati / Hijaz, just, Pythagorean) with any root, A4 tuning (432 / 444 Hz), and mono mode for choked hats and sliding 808s.

165 factory presets in 18 banks, from Sacred World, Resonance & Drones and Memphis Drums to Industrial, Japanese Acid Jazz, 60s Psych, Dungeon Synth, 8-Bit Lo-Fi, Krautrock, Zamrock and Jazz Noir.
Every preset has Macro 1 (brightness) and Macro 2 (character) ready to tweak.

## Sampler page
Press **SAMPLER** at the top. Your own recordings, played from **4x4 pads** or the keyboard, layered with the synth or on their own.
- **Drop a folder** (or WAV files) on the left. It becomes an instrument; folders inside it become articulations.
- The **mapper** on the right sets each recording's root note, key range and velocity range; changes save automatically.
- Put note names in file names (`C3.wav`, `Kick F#2 rr2.wav`) and it maps itself. C3 = middle C.
- Samples live in `Documents\OZ-mosys\Samples` (or point **Library...** at any folder, including your Sol Intent library).

## Installing
**Mac:** download **OZ-mosys-Mac.pkg** from the **Releases** page, right-click it and choose **Open**, then follow the steps.
It installs the AU (Logic, GarageBand), the VST3 and a stand-alone app.

**Windows:** download **OZ-mosys-Setup.exe** from this repo's **Releases** page and run it. It puts the plugin in
`C:\Program Files\Common Files\VST3`. In FL Studio: *Options > Manage plugins > Find more plugins*, then look for OZ-mosys under Generators.

## Where things are saved
Your presets: `Documents\OZ-mosys\Presets`.

## Building
Every push to `main` builds the Windows and Mac installers automatically (GitHub Actions). Presets are generated from `tools/gen_presets.py`. All original code; no code or sounds from other products.
