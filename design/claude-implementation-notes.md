# Claude handoff — UI milestone 01

Task: [GitHub Issue #1](https://github.com/alienheadbill/DiceVox/issues/1)

## First action
Review the code and leave a short implementation proposal *as a comment on the issue*: proposed files, JUCE LookAndFeel strategy, SVG handling, window-size constraints, required assets, and how current APVTS attachments will be preserved. Then create a **draft PR** on a NEW feature branch from latest `main`.

## Only implement
- Consistent AHE theme and polished dark UI for current controls.
- Restyled existing Input / Mix / Output and Bypass.
- Accessible product header with original brand-derived alien mark.
- Prominent but **disabled** ROLL chamber with explanation: no randomizer yet.
- Responsive or compact layout behavior without reducing clarity.
- Optional clearly nonfunctional Advanced concept only if approved, not implied DSP.

## Do not change
DSP, `FxChain`, `Params`, `PluginProcessor`, plugin IDs, state serialization, CMake AHE identity, release pipeline, or audio feature roadmap.

## Review evidence
- Before and after screenshots at compact size.
- Tests / CI status.
- One paragraph identifying every clickable/automated control.
- Known limits and what remains future Phase 2, 3, or 5.

No auto-merge. Founder/ChatGPT review the draft PR first.