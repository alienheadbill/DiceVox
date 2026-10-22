# DiceVox

Vocal multi-effect plugin (VST3). One **ROLL** button generates a new random
effect chain each click; every effect also has its own on-screen controls.

Two views, chosen with the tabs in the header:

- **SIMPLE:** just the dice (style, LOW / MED / HIGH, ROLL / BACK) plus presets and
  Bypass / Input / Mix / Output. New instances open here.
- **ADVANCED:** adds every effect with its switch, lock and knobs. The window grows
  to fit.

The view is saved with the project.

Current status: **v2 (test build), the alien vocal lab.** Ten effects (Filter, Pitch,
Drive, Lo-Fi, Alien Voice, Chorus, Chop, Reverse, Delay, Reverb), each with an
ON/OFF switch, a dice lock and knobs on screen. The look is "alien tech": frosted-glass
panels with HUD corner brackets, scanner knobs (tick rings + glowing arcs) over a starfield
and deck grid; effects
glow green while they're processing, and the UFO beams down each new roll. **ROLL**
builds a chain with settings that suit vocals and names it ("Abducted Chipmunk
Mothership"), steered by:

- **Style:** Dreamy Pop, Dark Toy Box, Pitched Rage, Lo-Fi Bedroom, Glitch or Area 51.
  - **Recipes:** each style has three recipes, and a roll never repeats the last
    one. For example, Dark Toy Box has "music box", "creepy phone" and "uncanny
    whisper"; Pitched Rage has "chipmunk", "demon" and "octave stack"; Area 51 has
    "mothership choir", "crop circle radio" and "android abduction".
  - **What a recipe sets:** its two core effects, which extras may join, and how
    each one is set.
  - **Any Vibe** borrows recipes from every style, or rolls free-form.
- **Intensity:** LOW rolls 1–2 effects leaning gentle, MED rolls 2–4, HIGH rolls
  3–5 leaning wild. Settings range widely on every level.
- **Locks:** a locked effect keeps its on/off state and settings.

**BACK** returns to the previous roll. Windows gets a one-click installer
(`DiceVox-Setup.exe`).

**Presets:** the bar at the top of the window saves the current sound (**SAVE**),
lists presets (click the name) and steps through them (**<** / **>**).

- **Alien Pack:** ten factory presets built into the plug-in, listed first:
  Mothership Choir, Crop Circle Phone, Area 51 Transmission, Android Abduction,
  Zero-G Float, Martian Chipmunk, Saucer Wobble, Nebula Swell, Signal Lost and UFO
  Landing.
- **Your presets:** plain files in `Documents\AHE Audio\DiceVox\Presets` (`<name>.dicevox`), so you can
back them up or share them. A preset holds the whole sound (every effect plus Input,
Mix and Output), but not Bypass or the dice controls. BACK undoes a preset load.
FL Studio's own preset saving and project recall also work, as before.

### Effect parameters (on screen and host automation)

Every effect is off by default, so it never changes the sound of an existing project.
Parameters are appended in release order (v1 effects, then v2 effects, then the dice
controls); never reorder or remove them after release.

| ID | Host name | Range | Default |
|---|---|---|---|
| `filter_on` | Filter On | off / on | off |
| `filter_lowcut` | Filter Low Cut | 20 – 2000 Hz (12 dB/oct high-pass) | 80 Hz |
| `filter_highcut` | Filter High Cut | 200 – 20000 Hz (12 dB/oct low-pass) | 20000 Hz |
| `filter_resonance` | Filter Resonance | 0 – 100 % (peak up to about +8 dB, also when the cutoffs meet; see below) | 0 % |
| `drive_on` | Drive On | off / on | off |
| `drive_amount` | Drive Amount | 0 – 100 % (0 to +24 dB into a tanh saturator, level-compensated) | 35 % |
| `drive_tone` | Drive Tone | 0 – 100 % (low-pass 2 kHz – 20 kHz after the drive) | 60 % |
| `chorus_on` | Chorus On | off / on | off |
| `chorus_rate` | Chorus Rate | 0.1 – 5 Hz (right channel runs 13 % faster for width) | 0.8 Hz |
| `chorus_depth` | Chorus Depth | 0 – 100 % | 40 % |
| `delay_on` | Delay On | off / on | off |
| `delay_time` | Delay Time | 1/16, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/4D, 1/2 (host tempo; 120 BPM if none) | 1/4 |
| `delay_feedback` | Delay Feedback | 0 – 85 % (repeats band-limited 150 Hz – 5 kHz, soft-clipped) | 35 % |
| `delay_mix` | Delay Mix | 0 – 100 % echo level (the dry vocal stays at full level) | 30 % |
| `reverb_on` | Reverb On | off / on | off |
| `reverb_size` | Reverb Size | 0 – 100 % (room to hall) | 50 % |
| `reverb_mix` | Reverb Mix | 0 – 100 % | 25 % |
| `pitch_on` | Pitch On | off / on | off |
| `pitch_shift` | Pitch Shift | −12 – +12 semitones (grain shifter, no formant correction) | −12 |
| `pitch_mix` | Pitch Mix | 0 – 100 % (constant-power blend; 100 % = only the pitched voice) | 40 % |
| `lofi_on` | Lo-Fi On | off / on | off |
| `lofi_crush` | Lo-Fi Crush | 0 – 100 % (16 → 4 bits, 44.1 → 4.4 kHz, darkening low-pass) | 50 % |
| `lofi_wobble` | Lo-Fi Wobble | 0 – 100 % (tape wow + flutter, up to about ±3.5 ms) | 30 % |
| `lofi_mix` | Lo-Fi Mix | 0 – 100 % (crushed vs clean) | 100 % |
| `chop_on` | Chop On | off / on | off |
| `chop_rate` | Chop Rate | 1/32, 1/16, 1/16T, 1/8, 1/8T, 1/4 (locks to the song position while playing) | 1/16 |
| `chop_depth` | Chop Depth | 0 – 100 % (100 % = silent gaps; 2 ms edges) | 80 % |
| `reverse_on` | Reverse On | off / on | off |
| `reverse_length` | Reverse Length | 1/16, 1/8, 1/4, 1/2 slices (max 1.5 s) | 1/4 |
| `reverse_mix` | Reverse Mix | 0 – 100 % reversed level (the dry vocal stays at full level) | 40 % |
| `alien_on` | Alien Voice On | off / on | off |
| `alien_mode` | Alien Mode | Saucer (ring mod 20 Hz – 1.2 kHz), Android (robot comb 60 – 240 Hz), Transmission (steep radio band 300 Hz – 4 kHz narrowing to 700 Hz – 2.2 kHz, gentle saturation, distant-signal fade; no added noise) | Saucer |
| `alien_amount` | Alien Amount | 0 – 100 % (carrier / robot note / how narrow and distant the radio is) | 50 % |
| `alien_mix` | Alien Mix | 0 – 100 % | 60 % |
| `dice_level` | Dice Intensity | Low, Medium, High | Medium |
| `dice_style` | Dice Style | Any Vibe, Dreamy Pop, Dark Toy Box, Pitched Rage, Lo-Fi Bedroom, Glitch, Area 51 | Any Vibe |
| `dice_lock_<effect>` | Dice Lock … | off / on, one per effect | off |

Chain order: Filter → Pitch → Drive → Lo-Fi → Alien Voice → Chorus → Chop → Reverse → Delay → Reverb.
No effect reports latency. The pitch and reverse effects trail the dry vocal
slightly (by design: they're doubles and swells, not replacements for it), so
their delay isn't compensated. The roll's name is saved with the project; BACK
history isn't (it starts empty when a project opens). ROLL and BACK never change
Input, Mix, Output, Bypass or the dice controls.

Resonance safety: the low cut and high cut share one Resonance control. When the
cutoffs move close together their peaks would stack (up to +16 dB), so each
stage's resonance is reduced smoothly as they converge
(`Source/modules/FilterQTable.h`, generated by `tools/dsp/gen-filter-q-table.py`).
The filter's loudest point never exceeds one stage's peak at the chosen
resonance. Measured responses: `docs/review/phase-2a/`.

## Install on Windows (no building needed)

1. Download **DiceVox-Setup.exe**: from the **Latest build** release, or for a test
   branch from its GitHub Actions run (artifact **DiceVox-Windows-Installer**).
2. Close FL Studio and run it. Windows may say the installer is unsigned: click
   **More info → Run anyway**. It installs DiceVox into
   `C:\Program Files\Common Files\VST3` and replaces any older DiceVox there.
3. FL Studio: **Options → Manage plugins → Find installed plugins**. DiceVox shows up
   under Effects (vendor AHE). Uninstall from Windows **Settings → Apps**.

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
  PluginEditor.*      DiceVox editor: SIMPLE (860x292) / ADVANCED (860x780) views, scales 80%-130%
  Params.*            global parameter IDs + full parameter layout
  modules/
    EffectModule.h    interface every effect implements
    FxChain.*         owns the effects, gives each an on/off switch (click-free
                      crossfade), runs them in order
    FilterModule.*    low cut + high cut + resonance
    FilterQTable.h    generated per-stage resonance limit (combined peak safety)
    DriveModule.*     tanh saturation + tone
    ChorusModule.*    stereo doubler / chorus
    DelayModule.*     tempo-synced ping-pong delay
    ReverbModule.*    room-to-hall reverb
    PitchModule.*     pitch shift up/down (pitched doubles or the whole voice)
    LoFiModule.*      bitcrush, sample-rate crunch, tape wobble
    ChopModule.*      tempo-synced stutter gate
    ReverseModule.*   tempo-synced reverse slices
    AlienModule.*     Alien Voice: saucer ring-mod, android robot comb, space-radio transmission
  dsp/
    GrainShifter.*    two-head delay-line pitch shifter (also plays reverse)
  dice/
    DiceRoller.*      ROLL: style recipes, intensity, locks, vocal-friendly settings, names
  presets/
    PresetStore.*     preset files (Documents/AHE Audio/DiceVox/Presets, *.dicevox)
    FactoryPresets.*  the built-in Alien Pack (10 presets)
  ahe/                shared AHE Audio UI layer (reused by every AHE plug-in)
    AHETheme.h        design tokens: colours, spacing, radii, font roles
    AHEFonts.*        embedded Inter + Barlow Condensed (identical text on every computer)
    AHELookAndFeel.*  knob / button / tab / switch / tooltip styling, focus-visible rings
    AHEBrandMarks.*   alien, UFO and compact AHE wordmark from the embedded SVGs
    AHEHeader.*       shared header bar (mark, product name, maker mark)
    AHEKnob.*         labelled knob bound to one parameter (units, reset, typing)
  ui/
    RollChamber.*     ROLL / BACK, style, intensity and the current roll's name
    LevelSelector.*   segmented LOW / MED / HIGH buttons for a choice parameter
    EffectCard.*      one effect on screen: ON/OFF switch, dice lock, knobs
    PresetBar.*       header preset bar: < menu > SAVE
installer/DiceVox.iss Inno Setup script for DiceVox-Setup.exe (built by CI)
docs/review/          DSP review evidence (measured responses)
design/assets/        approved brand SVGs, compiled into the plug-in
design/fonts/         OFL fonts + licences, compiled into the plug-in (see its README)
tests/TestMain.cpp    offline audio, compatibility and editor-wiring tests (no DAW needed)
tools/dsp/            generators for DSP lookup tables (Python + numpy, offline only)
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
   `getParameterVersion()` returns the release that first ships it (Phase 2 = 2).
2. Register it at the end of the `FxChain` constructor (`FxChain.cpp`).
3. Add it to `target_sources` in `CMakeLists.txt`, give it a card in
   `PluginEditor.cpp`, teach `dice/DiceRoller.cpp` how to roll it, and add a test.

## Roadmap

Reworked around getting a usable plug-in first, then making it better by ear.

| Step | Scope |
|---|---|
| v1 | 5 effects on screen, ROLL + BACK, Windows installer ✅ |
| v2 (this build) | Pitch, Lo-Fi, Chop, Reverse, Alien Voice; dice styles with recipes, LOW/MED/HIGH, locks; presets + Alien Pack; alien look ✅ |
| Next | Founder feedback by ear: tune styles and ranges; maybe pitch correction (auto-tune), formant control, favourites |
| Later | More effects (e.g. vocoder, harmonizer), macOS installer |
