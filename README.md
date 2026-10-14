# DiceVox

Vocal multi-effect plugin (VST3). One **ROLL** button generates a new random
effect chain each click; a separate **Edit panel** exposes every parameter.

Current status: **Phase 1 — passthrough skeleton.** Loads in a DAW, Input / Mix /
Output / Bypass work, the dice button is a placeholder.

## Build on Windows (to load it in FL Studio)

One-time setup:

1. Install **Visual Studio 2022 Community** (free). In the installer, tick the
   workload **"Desktop development with C++"**. That includes CMake.
2. Install **Git for Windows**.

Build (open **"Developer PowerShell for VS 2022"** from the Start menu):

```powershell
cd path\to\DiceVox
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target DiceVox_VST3 DiceVox_Standalone DiceVoxTests
build\Release\DiceVoxTests.exe
```

The first configure downloads JUCE automatically (a few minutes).
The test run should end with `All tests passed.`

## Load it in FL Studio

1. Copy the folder `build\DiceVox_artefacts\Release\VST3\DiceVox.vst3` into
   `C:\Program Files\Common Files\VST3\`.
2. FL Studio: **Options → Manage plugins → Find more plugins**.
3. DiceVox shows up under Effects. Drop it on a mixer insert.

Quick test without FL: run `build\DiceVox_artefacts\Release\Standalone\DiceVox.exe`.

## Build without installing anything (GitHub)

Push this folder to a GitHub repo. The workflow in `.github/workflows/build.yml`
builds Windows and macOS VST3s on every push and runs the tests.

- Every push to `main` refreshes the **Latest build** release. Download it from
  the **Releases** box on the right side of the repo's main page.
- To make a permanent version, push a tag: `git tag v0.2.0 && git push --tags`.

Note: macOS builds are unsigned, so Mac users will get a security warning until
the plugin is code-signed and notarized (needed before a public release).

## Project layout

```
Source/
  PluginProcessor.*   audio engine: input gain → dry tap → FxChain → mix → output gain
  PluginEditor.*      UI
  Params.*            global parameter IDs + full parameter layout
  modules/
    EffectModule.h    interface every effect implements
    FxChain.*         owns the effects, gives each an on/off switch, runs them in order
tests/TestMain.cpp    offline audio tests (no DAW needed)
```

### Adding an effect (Phase 2 onward)

1. Create `Source/modules/<Name>Module.h/.cpp` implementing `EffectModule`.
   Parameter IDs must start with the module id, e.g. `delay_time`.
2. Register it in the `FxChain` constructor (`FxChain.cpp`), in signal order.
3. Add it to `target_sources` in `CMakeLists.txt` and add a test.

## Roadmap

| Phase | Scope |
|---|---|
| 1 | Skeleton, parameters, passthrough, tests ✅ |
| 2 | First modules: filter, saturation, synced delay |
| 3 | Randomizer: archetypes, intensity, per-module locks, roll history, auto gain match |
| 4 | Remaining modules: pitch shift (+formant), doubler, reverb, reverse/stutter, width, wobble, radio |
| 5 | Main UI + Edit panel, presets, A/B |
