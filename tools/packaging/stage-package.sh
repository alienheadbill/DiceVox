#!/usr/bin/env bash
# Stages the distributable DiceVox package for one platform:
#
#   <out>/DiceVox.vst3/        the plug-in bundle (unchanged; stays at the root
#                              so install instructions and the updater keep working)
#   <out>/licenses/*-OFL.txt   licence texts for every font embedded in the plug-in
#
# Used by CI for both the internal test artifact and the release zip, so the
# two always have the same layout. Font files themselves are NOT copied: they
# are embedded in the plug-in binary and are not distributed separately.
#
# Usage: tools/packaging/stage-package.sh <built VST3 folder> <output folder>
#   e.g. tools/packaging/stage-package.sh build/DiceVox_artefacts/Release/VST3 package
set -euo pipefail

vst3_dir="${1:?usage: stage-package.sh <built VST3 folder> <output folder>}"
out_dir="${2:?usage: stage-package.sh <built VST3 folder> <output folder>}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fonts_dir="$repo_root/design/fonts"

if [ ! -d "$vst3_dir/DiceVox.vst3" ]; then
    echo "error: $vst3_dir/DiceVox.vst3 not found" >&2
    exit 1
fi

rm -rf "$out_dir"
mkdir -p "$out_dir/licenses"
cp -R "$vst3_dir/DiceVox.vst3" "$out_dir/"

shopt -s nullglob
licences=("$fonts_dir"/*-OFL.txt)
fonts=("$fonts_dir"/*.ttf "$fonts_dir"/*.otf)

if [ ${#fonts[@]} -gt 0 ] && [ ${#licences[@]} -eq 0 ]; then
    echo "error: fonts are embedded (${#fonts[@]} in design/fonts) but no *-OFL.txt licence texts were found" >&2
    exit 1
fi

for licence in "${licences[@]}"; do
    cp "$licence" "$out_dir/licenses/"
done

echo "Staged $out_dir:"
echo "  DiceVox.vst3/"
for licence in "${licences[@]}"; do
    echo "  licenses/$(basename "$licence")"
done
