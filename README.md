# OZ-mosys

A four-in-one synth plugin: VST3 + stand-alone on Windows, AU + VST3 + stand-alone on Mac.

| Engine | Inspired by | What it does |
|---|---|---|
| **Wavetable** | Serum | 6 built-in tables + import your own (.wav, 2048-sample frames), position morphing, 5 warp modes, up to 7-voice unison |
| **Analog** | Analog Lab | Band-limited triangle / saw / pulse morph, sub, noise, analog drift |
| **Granular** | Omnisphere | Turns any audio file into pads and textures: position, scan, size, density, spray, pitch randomness, reverse, stereo width |
| **Mod system** | Pigments | 3 envelopes, 2 LFOs, 4 macros, velocity, mod wheel, aftertouch, key track, random. Drag a green chip onto any knob to route it; right-click a knob to remove |

Plus a ladder / state-variable filter and Drive, Chorus, Ping-pong Delay and Reverb.

61 factory presets in five banks: Signature, Dark & Cinematic, Reggae & Dub, Trance & Electronic, Hip-Hop & Trap.
Every preset has Macro 1 (brightness) and Macro 2 (character) ready to tweak.

## Installing
**Mac:** download **OZ-mosys-Mac.pkg** from the **Releases** page, right-click it and choose **Open**, then follow the steps.
It installs the AU (Logic, GarageBand), the VST3 and a stand-alone app.

**Windows:** download **OZ-mosys-Setup.exe** from this repo's **Releases** page and run it. It puts the plugin in
`C:\Program Files\Common Files\VST3`. In FL Studio: *Options > Manage plugins > Find more plugins*, then look for OZ-mosys under Generators.

## Where things are saved
Your presets: `Documents\OZ-mosys\Presets`.

## Building
Every push to `main` builds the Windows and Mac installers automatically (GitHub Actions). Presets are generated from `tools/gen_presets.py`. All original code; no code or sounds from other products.
