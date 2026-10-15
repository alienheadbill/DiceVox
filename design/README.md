# AHE Audio — DiceVox design references (review branch)

**Purpose:** Visual design source for a UI-only update to the existing JUCE plug-in.

- **Company:** Alien Head Entertainment
- **Software imprint:** AHE Audio
- **Product:** DiceVox
- **Approach:** Balanced-hybrid *modern digital* studio UX + restrained alien identity.
- **Primary brand mark:** simplified alien head; **secondary:** UFO motif.
- **Control philosophy:** accessible to a new artist, precise enough for an engineer.
- **Not code:** These SVGs/tokens describe a proposed appearance. They do not define DSP features or certify other DAW support.

## What exists in `main`

JUCE 8 / VST3 + Standalone; existing APVTS Input, Mix, Output, Bypass controls; disabled ROLL; empty FX module chain; Phase 1 passthrough. Do not portray ROLL, presets, effect racks, or meters as working until implemented.

## Start here

1. Read [issue #1](https://github.com/alienheadbill/DiceVox/issues/1) for Claude's scoped UI implementation assignment.
2. Inspect `Source/PluginEditor.cpp`, `Source/PluginEditor.h` and current `520×360` layout.
3. Inspect `ahe-ui-tokens.json` and `assets/`. SVGs are working logo vectors; compare against founder-approved visual previews before declaring final.
4. For implementation, use a fresh branch from **current main** rather than coding on this reference branch. Create a **draft PR**. No main changes without founder approval.

**Protect these:** the AHE/`Ahen`/`Dvox` identity, global parameter IDs `input_gain`, `mix`, `output_gain`, `bypass`, saved state, DSP, tests and release workflow.

**Status:** review-only brand references. This pull request should be reviewed separately from any JUCE UI implementation.