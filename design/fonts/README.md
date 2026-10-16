# AHE Audio fonts

Embedded in the plug-in (via `juce_add_binary_data`) so the UI renders identically on every computer. Founder decision, Oct 15, 2026: use one identical font everywhere rather than system fonts.

| File | Family / style | Used for | Licence | Source |
|---|---|---|---|---|
| `Inter-Regular.ttf` | Inter Regular (v4.1) | body text, tooltips, typed values | SIL Open Font License 1.1 (`Inter-OFL.txt`) | https://github.com/rsms/inter (release v4.1) |
| `Inter-SemiBold.ttf` | Inter SemiBold (v4.1) | labels, values, "AUDIO" in the maker mark | SIL Open Font License 1.1 (`Inter-OFL.txt`) | same |
| `BarlowCondensed-ExtraBoldItalic.ttf` | Barlow Condensed ExtraBold Italic | product names only (e.g. DICEVOX, ROLL) | SIL Open Font License 1.1 (`BarlowCondensed-OFL.txt`) | https://github.com/google/fonts/tree/main/ofl/barlowcondensed |

## Licence notes
- The OFL allows embedding these fonts in commercial software, including software that is sold.
- The files are **unmodified** (not subset or renamed), so the Reserved Font Name rules don't apply.
- **Distribution:** every DiceVox zip (Windows and macOS, both the CI test artifacts and the release zips) contains `licenses/Inter-OFL.txt` and `licenses/BarlowCondensed-OFL.txt` next to `DiceVox.vst3/`. Staging is done by `tools/packaging/stage-package.sh`. `tools/packaging/check-package.py` blocks publishing unless both texts are present and byte-identical to these files, and no raw font files are in the zip. The font files themselves are only distributed inside the plug-in binary.
- **Adding a font:** put its `.ttf` and its licence text, named `<Family>-OFL.txt`, in this folder. Staging and the check pick up every `*-OFL.txt` automatically.
- The fonts must not be sold on their own.
