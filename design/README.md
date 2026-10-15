# AHE Audio — design references

Visual design source for AHE Audio plug-ins, starting with DiceVox. Originally added as review-only references in PR #2. Merged into the UI branch (PR #3) so the code and its design sources live together.

- **Company:** Alien Head Entertainment
- **Software imprint:** AHE Audio
- **Product:** DiceVox
- **Approach:** balanced-hybrid *modern digital* studio UX with a restrained alien identity.
- **Primary brand mark:** simplified alien head. **Secondary:** UFO motif.
- **Control philosophy:** accessible to a new artist, precise enough for an engineer.

## What's here

| Path | What it is | Used by the plug-in? |
|---|---|---|
| `ahe-ui-tokens.json` | Colours, spacing, radii, typography, logo usage (v0.2) | Yes. `Source/ahe/AHETheme.h` mirrors it; keep the two in sync. |
| `assets/ahe-alien-icon.svg` | Primary mark (alien head) | Yes, embedded: plug-in header |
| `assets/ahe-ufo-icon.svg` | Secondary motif (UFO) | Yes, embedded: ROLL area (single muted tone until the randomizer exists) |
| `assets/ahe-alien-horizontal.svg` | Full lockup: alien + AHE + "AUDIO" | Only its AHE letter **shapes** are drawn (compact header maker mark). The full lockup is for installers, splash screens and marketing. Its "AUDIO" is live SVG text, which JUCE does not render. |
| `fonts/` | Inter (Regular, SemiBold) + Barlow Condensed ExtraBold Italic, SIL OFL 1.1, with licence texts | Yes, embedded. Their licence texts must ship with every distributed build (Issue #4; see `fonts/README.md`). |
| `review/ui-milestone-01/` | Before/after screenshots from UI milestone 01 | No, review evidence only |
| `claude-implementation-notes.md` | Original hand-off notes for UI milestone 01 (historical) | No |

## Rules
- These files describe appearance. They don't define DSP features or certify DAW support.
- Never show ROLL, presets, effect racks or meters as working until they are implemented.
- Green means processing is actually on. Never use it decoratively, and never rely on colour alone to show state.
- **Protect:** the AHE / `Ahen` / `Dvox` identity, the parameter IDs `input_gain`, `mix`, `output_gain`, `bypass`, saved state, DSP, tests and the release workflow.
- When changing a brand SVG, change it here once. There is a single copy of each asset.
