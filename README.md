# OZ-mosys

A four-in-one synth plugin: VST3 + stand-alone on Windows, AU + VST3 + stand-alone on Mac.

| Engine | Inspired by | What it does |
|---|---|---|
| **Wavetable** | Serum | 6 built-in tables + import your own (.wav, 2048-sample frames), position morphing, 8 warp modes (incl. Formant, Steps, Fold), up to 7-voice unison |
| **Analog** | Analog Lab | Band-limited triangle / saw / pulse morph, sub, noise, analog drift |
| **Granular** | Omnisphere | Turns any audio file into pads and textures: position, scan, size, density, spray, pitch randomness, reverse, stereo width |
| **Resonator** | physical modelling | Plucked strings and sitar (with jawari buzz and sympathetic strings), Tibetan and crystal bowls, gong, handpan, tongue drum, tingsha, music box, tabla, hand drum, clay pot, 808 cowbell, scrap metal, anvil, pipe, vibraphone, marimba, e-piano. Strike or bow/rub, auto re-pluck |
| **Mod system** | Pigments | 3 envelopes, 4 breakpoint envelopes (MSEG), 2 LFOs, 8 macros, velocity, mod wheel, aftertouch, key track, slide, random. Drag a source from the live mod strip (or a macro name) onto any knob to route it; right-click a knob to remove |

Plus a ladder / state-variable filter; Drive (Warm / Rat / Fuzz), Bit-crusher, Tape (wow & flutter, age, hiss), Ring mod, Chorus, Ping-pong Delay and Reverb.
World tunings (Raga Bhairav, Maqam Rast / Bayati / Hijaz, just, Pythagorean) with any root, A4 tuning (432 / 444 Hz), and mono mode for choked hats and sliding 808s.

165 factory presets in 18 banks, from Sacred World, Resonance & Drones and Memphis Drums to Industrial, Japanese Acid Jazz, 60s Psych, Dungeon Synth, 8-Bit Lo-Fi, Krautrock, Zamrock and Jazz Noir.
Every preset has Macro 1 (brightness) and Macro 2 (character) ready to tweak.

## Macro bar
Eight macros sit under every page. Double-click a name to rename it; drag the name onto any knob to make that macro move it.
**Controls / Keys** swaps the macros for the on-screen keyboard.

## ENV page (breakpoint envelopes)
Four drawable envelopes, stacked or one at a time. Double-click to add or remove points, Alt-drag a line to curve it,
right-click a point for sustain / loop start. Modes: one shot, sustain, loop while held, loop forever. Tempo sync,
grid snap, an LFO on top and sample & hold. Pick a **Target** and press **Assign** to send an envelope anywhere.

## BROWSE page (Absynth-style)
- **List or Explorer:** every preset in a list (star favourites, search, filter by bank) or on a **sound map** where
  each sound is placed by how dark/bright and soft/aggressive it is. Click to hear it, double-click to keep it, Revert to go back.
- **Properties:** name, author, bank, types and comments for your presets; Save / Save As (your sounds land on the map too).
- **Subpresets:** save, load, copy, paste, init or randomise just one section (e.g. only the filter or only the effects).
- **Mutation:** Mutate makes a variation (moves toward a similar preset + some randomness), Retry tries again,
  Module Lock keeps sections untouched, eight **Fine Tuning** knobs adjust the whole sound, and the History lets you jump back.

**Sound-type chips** (Bass, Pad, Keys, Lead, Pluck...) and **mode chips** (Percussive, Slow Attack, Evolving, Mono,
Wavetable, Analog, Granular...) narrow the list; pick several types to widen, several modes to narrow.
In **Module Lock**, drag sideways on a section to set how much it may change when you mutate (0-100%).

## MIDI Learn
Right-click any knob (or macro) and choose **MIDI Learn**, then move a knob or fader on your controller.
Linked knobs show their CC number. **Show all MIDI links...** lists every link with remove / clear all.
Links belong to your setup, so they stay the same across presets and projects.

## Expansions
Press **Expansions** in the browser: every collection as a cover-art tile. Click one to browse it.
**Install Pack...** (or drop an `.ozpack` file on the page) adds a collection; **Make a Pack...** bundles a bank of your
own presets plus cover art into one `.ozpack` file you can share. Packs live in `Documents\OZ-mosys\Expansions`.

## ROLL page (bend every note)
Press **ROLL**. Notes are lines you can bend: click empty space for a note, click a line to add a bend point,
drag points to glide, Alt-drag a line to curve it. Every note has its own **velocity, pressure and slide**
(the *Edit* menu), plus **Pitch Mod** (wobble with amount, speed, fade-in and polarity), loop length, grid,
quantise, record, undo/redo, copy/paste and save/open (`Documents\OZ-mosys\Rolls`).
It follows your DAW's transport (or its own Play button), so it works in FL Studio too.
**Export MIDI** writes an MPE file that keeps every bend for Logic, Ableton, Bitwig, Cubase or Studio One.

OZ-mosys is also a full **MPE instrument**: per-note pitch bend, pressure (Aftertouch) and slide (CC74) from MPE
keyboards or DAWs that draw per-note bends. Channel 1 is the normal keyboard; channels 2-16 carry one note each (+/-48 st).

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
