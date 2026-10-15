#!/usr/bin/env python3
"""
Packaging assertion for DiceVox release zips (Issue #4).

Checks one zip and fails (exit 1) unless ALL of these hold:
  1. Top level holds only DiceVox.vst3/ and licenses/ (DiceVox.vst3 stays at the
     zip root, so install instructions and the Windows updater keep working).
  2. The plug-in binary for the platform is present (and executable on macOS).
  3. Every font licence in design/fonts/*-OFL.txt is in licenses/ and is
     byte-for-byte identical to the repository copy (unmodified OFL text).
  4. If any font is embedded (design/fonts/*.ttf|*.otf), at least one licence
     is required, and each OFL text names a copyright holder.
  5. No raw font files (.ttf/.otf/.woff*) are distributed in the zip.

Usage:
  tools/packaging/check-package.py DiceVox-Windows.zip --platform windows
  tools/packaging/check-package.py DiceVox-macOS.zip   --platform macos
"""
import argparse
import hashlib
import stat
import sys
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FONTS_DIR = REPO_ROOT / "design" / "fonts"

BINARIES = {
    "windows": "DiceVox.vst3/Contents/x86_64-win/DiceVox.vst3",
    "macos": "DiceVox.vst3/Contents/MacOS/DiceVox",
}
FONT_SUFFIXES = (".ttf", ".otf", ".woff", ".woff2")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("zip", type=Path)
    parser.add_argument("--platform", choices=sorted(BINARIES), required=True)
    args = parser.parse_args()

    failures = []
    passes = []

    def check(ok: bool, message: str):
        (passes if ok else failures).append(message)

    with zipfile.ZipFile(args.zip) as z:
        infos = {i.filename.rstrip("/"): i for i in z.infolist()}
        names = list(infos)

        # 1. Layout
        top = sorted({n.split("/")[0] for n in names if n})
        check(top == ["DiceVox.vst3", "licenses"],
              f"top level is exactly DiceVox.vst3/ + licenses/ (found: {', '.join(top) or 'nothing'})")

        # 2. Plug-in binary
        binary = BINARIES[args.platform]
        check(binary in infos, f"plug-in binary present: {binary}")
        if args.platform == "macos" and binary in infos:
            mode = (infos[binary].external_attr >> 16) & 0o777
            check(bool(mode & stat.S_IXUSR), f"macOS binary is executable (mode {oct(mode)})")

        # 3 + 4. Licences match the repository copies exactly
        repo_licences = sorted(FONTS_DIR.glob("*-OFL.txt"))
        embedded_fonts = sorted(p for p in FONTS_DIR.iterdir() if p.suffix.lower() in (".ttf", ".otf")) \
            if FONTS_DIR.is_dir() else []

        if embedded_fonts:
            check(len(repo_licences) > 0,
                  f"{len(embedded_fonts)} embedded font file(s) have licence texts in design/fonts")

        for licence in repo_licences:
            entry = f"licenses/{licence.name}"
            if entry not in infos:
                check(False, f"{entry} present")
                continue
            packaged = z.read(entry)
            original = licence.read_bytes()
            check(packaged == original,
                  f"{entry} identical to design/fonts/{licence.name} (sha256 {sha256(packaged)[:12]})")
            text = packaged.decode("utf-8", errors="replace")
            check("SIL OPEN FONT LICENSE Version 1.1" in text and "Copyright" in text,
                  f"{entry} contains the copyright notice and the full SIL OFL 1.1 text")

        unexpected = sorted(n for n in names if n.startswith("licenses/") and n != "licenses"
                            and Path(n).name not in {p.name for p in repo_licences})
        check(not unexpected, f"no unexpected files in licenses/ ({', '.join(unexpected) or 'none'})")

        # 5. No raw font files distributed
        raw_fonts = sorted(n for n in names if n.lower().endswith(FONT_SUFFIXES))
        check(not raw_fonts, f"no raw font files in the zip ({', '.join(raw_fonts) or 'none'})")

    print(f"Package check: {args.zip.name} ({args.platform})")
    for message in passes:
        print(f"  PASS  {message}")
    for message in failures:
        print(f"  FAIL  {message}")
    print("Package OK." if not failures else f"{len(failures)} packaging check(s) failed.")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
