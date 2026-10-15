#!/usr/bin/env bash
# Self-test for check-package.py: builds small fake zips and confirms the
# checker accepts a correct package and rejects each kind of broken one.
# Needs: bash, python3, zip. Runs in CI (package job) and locally.
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
checker="$here/check-package.py"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
failures=0

# A fake package for <platform> in <dir>, laid out like the real one.
make_good () {
    local platform="$1" dir="$2"
    rm -rf "$dir"; mkdir -p "$dir/licenses"
    if [ "$platform" = windows ]; then
        mkdir -p "$dir/DiceVox.vst3/Contents/x86_64-win"
        echo "binary" > "$dir/DiceVox.vst3/Contents/x86_64-win/DiceVox.vst3"
    else
        mkdir -p "$dir/DiceVox.vst3/Contents/MacOS"
        echo "binary" > "$dir/DiceVox.vst3/Contents/MacOS/DiceVox"
        chmod +x "$dir/DiceVox.vst3/Contents/MacOS/DiceVox"
    fi
    cp "$repo_root"/design/fonts/*-OFL.txt "$dir/licenses/"
}

zip_dir () { (cd "$1" && rm -f "$2" && zip -qr "$2" .); }

expect () {
    local want="$1" name="$2" platform="$3" zipfile="$4"
    python3 "$checker" "$zipfile" --platform "$platform" > "$work/out.txt" 2>&1
    local code=$?
    if { [ "$want" = pass ] && [ $code -eq 0 ]; } || { [ "$want" = fail ] && [ $code -ne 0 ]; }; then
        echo "  PASS  $name (checker said: $([ $code -eq 0 ] && echo OK || echo REJECTED))"
    else
        echo "  FAIL  $name (expected $want, exit $code)"; sed 's/^/        /' "$work/out.txt"
        failures=$((failures + 1))
    fi
}

echo "check-package.py self-test"
for platform in windows macos; do
    d="$work/$platform"
    make_good "$platform" "$d"; zip_dir "$d" "$work/good.zip"
    expect pass "$platform: correct package accepted" "$platform" "$work/good.zip"

    make_good "$platform" "$d"; echo "edited" >> "$d/licenses/Inter-OFL.txt"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: modified licence text rejected" "$platform" "$work/bad.zip"

    make_good "$platform" "$d"; rm "$d/licenses/BarlowCondensed-OFL.txt"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: missing licence rejected" "$platform" "$work/bad.zip"

    make_good "$platform" "$d"; rm -r "$d/licenses"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: no licenses/ folder rejected (old release layout)" "$platform" "$work/bad.zip"

    make_good "$platform" "$d"; cp "$repo_root/design/fonts/Inter-Regular.ttf" "$d/licenses/"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: raw font file in zip rejected" "$platform" "$work/bad.zip"

    make_good "$platform" "$d"; echo "x" > "$d/licenses/extra.txt"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: unexpected file in licenses/ rejected" "$platform" "$work/bad.zip"

    make_good "$platform" "$d"; mkdir "$d/nested"; mv "$d/DiceVox.vst3" "$d/nested/"; zip_dir "$d" "$work/bad.zip"
    expect fail "$platform: DiceVox.vst3 not at zip root rejected" "$platform" "$work/bad.zip"
done

make_good macos "$work/mac"; chmod -x "$work/mac/DiceVox.vst3/Contents/MacOS/DiceVox"; zip_dir "$work/mac" "$work/bad.zip"
expect fail "macos: non-executable binary rejected" macos "$work/bad.zip"

make_good windows "$work/win"; zip_dir "$work/win" "$work/good.zip"
expect fail "windows package checked as macos rejected (wrong binary)" macos "$work/good.zip"

[ $failures -eq 0 ] && echo "All packaging self-tests passed." || echo "$failures packaging self-test(s) failed."
exit $failures
