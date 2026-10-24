# Fourfold

A four-in-one synth plugin (VST3 + stand-alone) for Windows.

| Engine | Inspired by | What it does |
|---|---|---|
| **Wavetable** | Serum | 6 built-in tables + import your own (.wav, 2048-sample frames), position morphing, 5 warp modes, up to 7-voice unison |
| **Analog** | Analog Lab | Band-limited triangle / saw / pulse morph, sub, noise, analog drift |
| **Granular** | Omnisphere | Turns any audio file into pads and textures: position, scan, size, density, spray, pitch randomness, reverse, stereo width |
| **Mod system** | Pigments | 3 envelopes, 2 LFOs, 4 macros, velocity, mod wheel, aftertouch, key track, random. Drag a green chip onto any knob to route it; right-click a knob to remove |

Plus a ladder / state-variable filter and Drive, Chorus, Ping-pong Delay and Reverb.

## Installing
Download **Fourfold-Setup.exe** from this repo's **Releases** page and run it. It puts the plugin in
`C:\Program Files\Common Files\VST3`. In FL Studio: *Options > Manage plugins > Find more plugins*, then look for Fourfold under Generators.

## Where things are saved
Your presets: `Documents\Fourfold\Presets`.

## Building
Every push to `main` builds the Windows installer automatically (GitHub Actions). All original code; no code or sounds from other products.
