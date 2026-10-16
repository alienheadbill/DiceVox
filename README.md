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

## One-click updates (Windows)

One-time setup:

1. Install the GitHub CLI and sign in (the repo is private). In PowerShell:
   ```powershell
   winget install --id GitHub.cli
   ```
   Close and reopen PowerShell, then run `gh auth login` (GitHub.com → HTTPS →
   Login with a web browser).
2. From the **Latest build** release, download **AHE-Updater-Windows.zip**, unzip it,
   and double-click **Update-DiceVox.cmd**.

That first run installs DiceVox and adds an **Update DiceVox** shortcut to your
desktop. From then on, close FL Studio and double-click the shortcut. It
checks for a newer build, downloads it, and installs it (one admin prompt). If
you're already current, it says so and does nothing.

Tests for the updater: `powershell -File tools/updater/Test-Updater.ps1`
(they also run on Windows in CI after each release).

## Project layout

```
Source/
  PluginProcessor.*   audio engine: input gain → dry tap → FxChain → mix → output gain
  PluginEditor.*      DiceVox editor layout (520x360 design canvas, scales to 150%)
  Params.*            global parameter IDs + full parameter layout
  modules/
    EffectModule.h    interface every effect implements
    FxChain.*         owns the effects, gives each an on/off switch, runs them in order
  ahe/                shared AHE Audio UI layer (reused by every AHE plug-in)
    AHETheme.h        design tokens: colours, spacing, radii, font roles
    AHEFonts.*        embedded Inter + Barlow Condensed (identical text on every computer)
    AHELookAndFeel.*  knob / button / tab / switch / tooltip styling, focus-visible rings
    AHEBrandMarks.*   alien, UFO and compact AHE wordmark from the embedded SVGs
    AHEHeader.*       shared header bar (mark, product name, maker mark)
    AHEKnob.*         labelled knob bound to one parameter (units, reset, typing)
  ui/
    RollChamber.*     DiceVox ROLL hero area (shown as unavailable until Phase 3)
design/assets/        approved brand SVGs, compiled into the plug-in
design/fonts/         OFL fonts + licences, compiled into the plug-in (see its README)
tests/TestMain.cpp    offline audio, compatibility and editor-wiring tests (no DAW needed)
tools/snapshots/      renders the editor to PNGs for design review (optional)
tools/packaging/      stages release zips (DiceVox.vst3/ + licenses/) and checks them
tools/updater/        one-click Windows updater
```

### UI review screenshots

```bash
cmake -B build -DDICEVOX_BUILD_SNAPSHOTS=ON
cmake --build build --target DiceVoxSnapshots
./build/DiceVoxSnapshots shots     # on Linux: run under xvfb + a window manager
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
