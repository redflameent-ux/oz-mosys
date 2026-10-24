# OZ-mosys roadmap

The idea: bring back the sound-design depth of the older plugins that are hard to find or no longer run
(Absynth 5, FM8, Massive), plus favourite parts of Absynth 6, Omnisphere, Pigments, Analog Lab, Serum and Kontakt -
each rebuilt in the OZ-mosys style. Priority goes to **making your own presets**: deep editing, routing and
customisation come before performance extras.

## Done
- v0.6: Absynth-style browser + sound map, mutator, fine tuning, subpresets, ENV page (4 MSEGs),
  macro bar with Controls/Keys, live mod strip, Expansions page with installable `.ozpack` collections.

## Soon (small)
- Sound-type chips in the browser (Bass, Pad, Keys, Lead, Drums, Vocal...) and Type / Sub-type / Mode attributes
- MIDI learn table (map hardware knobs to any control; macro names show their CC)
- Wavetable warp modes: Spectrum, Bend+, Bend-, Formant
- Per-section amount for Randomize

## v0.7 Layers + deep sound design
- Engines become layers A-D, each with 2 module slots (ladder LP/HP, comb, supercomb, freq shifter, ring mod, waveshaper, notch), volume, pan
- 5th engine: FM with a 6-operator matrix and per-operator breakpoint envelopes
- Quadzone-style layer blend (fader / keyboard position / velocity)
- Dual filter (series / parallel, mix), modulation oscillator (ring / phase / position / filter FM), feedback path, coloured noise
- Glide curves and velocity curves
- Performer / Stepper mode for MSEGs (16 steps of drawn curves) - moved up from Performance
- Wave editor: draw your own oscillator waveforms and edit their harmonics (Absynth 5 / FM8 spectrum style)

## v0.8 FX racks
- Racks of up to 4 effects with reordering, FX A / FX B / Aux send
- Aetherizer-style granular delay, 3-tap Multitap, tube saturator, ensemble chorus, half-speed tape, shimmer verb, analog echo

## Sampler
- Leap-style loop slicer: 16 slice pads, tempo/key lock, slice grid + snap, Group/Single edit, Hold/Latch, Choke
- Coloured keyboard zones

## v0.9 Performance
- 32-step arpeggiator (On / Tie / Accent / Note order / Octave / Transpose rows, Reset + Random per row, swing, humanity, MIDI groove drop)
- Chords tool (key, one-finger chords, octave, humanize, strum) and Phrases tool (patterns that follow chords)
- Orb (circular morph controller with Dice, Inertia, Attractor, record) and Easy/Morph XY pad between 4 sounds

## Later
- Multi mode: up to 8 sounds, mixer, Stack (splits / layers) and Live mode
