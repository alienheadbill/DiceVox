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
- **Release requirement:** when distributing the plug-in, include the OFL text (`Inter-OFL.txt`, `BarlowCondensed-OFL.txt`) with it, for example in the release zip or installer, or in an About/credits screen. This is still **to do** before sharing builds publicly. The current CI release zips contain only the `.vst3`.
- The fonts must not be sold on their own.
