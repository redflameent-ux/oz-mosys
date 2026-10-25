# Reading undocumented binary presets (the plugin-oracle)

Formats like DUNE 3 / Serum / Nexus (`.fxp`) and Spire (`.spf`) store their parameters as an
opaque binary blob with no names - nothing an importer can read directly. This is how we get at
them anyway, without reverse-engineering a single byte.

## The idea: let the plugin decode its own format

Every VST3/AU plugin exposes its parameters to a host **by name** (`getName`, `getValue`). So
instead of parsing the file, we host the real plugin headlessly, load the preset into it, and
read the parameters back:

1. Load the plugin with no GUI.
2. For a `.fxp`, the file *is* a plugin state chunk - hand it to `setStateInformation`, and the
   plugin decodes its own secret format internally.
3. Walk every parameter and print `name = value`.

Out comes a labelled dump for a format that was opaque on disk. A small per-synth converter then
maps those names to OZ, exactly like the Omnisphere / Arturia / Hive importers.

**This only runs where the plugins are installed and licensed** (your machine, not CI). That is
why `PluginProbe` is a separate, opt-in tool.

## Build it

```
cmake -S . -B build-tools -DFOURFOLD_BUILD_TOOLS=ON
cmake --build build-tools --target PluginProbe --config Release
```

## Run it

```
PluginProbe "C:\Program Files\Common Files\VST3\Dune 3.vst3" "F:\Presets\...\Hypnotic\"
```

It prints a JSON array:

```json
[
  { "preset": "Sky Oasis BA", "params": { "Osc1 Unison": 0.75, "Filter1 Cutoff": 0.62, ... } }
]
```

Pipe that to a file, and that JSON is the input to a DUNE/Serum/... importer (to be written, one
small converter per synth, mapping the names it reports to OZ parameters).

## When the plugin won't cooperate: offset diffing

A few plugins expose only a subset of their parameters to a host. For those, use the synth as an
oracle a different way - **change one knob, save, diff:**

1. Save an init patch as the baseline.
2. Move exactly one parameter to its minimum, save as A; to its maximum, save as B.
3. Diff A vs B. The bytes that changed are that parameter's location; the two values give its
   range / encoding.
4. Repeat per parameter to build an `offset -> parameter` map. For a fixed-layout format (Spire's
   `.spf` is a fixed 3133-byte record), that map then works for the whole library.

`PluginProbe` can automate this too: set a parameter by name via the API, save the state, record
which bytes moved - which labels the offsets for you.

## Status and caveats

`PluginProbe` is a **starting scaffold**, meant to be built and iterated locally against your
plugins (it can't be runtime-tested in CI):

- Handles the VST2 `.fxp` **opaque-chunk** form (FPCh). Whether a given plugin's VST3 build
  accepts its old `.fxp` chunk via `setStateInformation` is plugin-specific; many that share a
  chunk format across VST2/VST3 do.
- The `.fxp` **parameter-list** form (FxCk) and VST3 `.vstpreset` are TODO.
- Some plugins expose fewer parameters than their full internal set.

See `IMPORT_ROADMAP.md` for which synths are readable directly vs behind this wall.
