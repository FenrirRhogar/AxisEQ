#!/usr/bin/env bash
# Removes AxisEQ as installed by install.sh. Pass --system for a system-wide install.
# User presets in ~/Documents/AxisEQ are kept.
set -euo pipefail

vst3_dir="$HOME/.vst3"
lv2_dir="$HOME/.lv2"
sudo=""

if [[ "${1:-}" == "--system" ]]; then
    vst3_dir="/usr/lib/vst3"
    lv2_dir="/usr/lib/lv2"
    sudo="sudo"
fi

echo "Removing AxisEQ:"
for bundle in "$vst3_dir"/AxisEQ.vst3 "$lv2_dir"/AxisEQ.lv2; do
    [[ -e "$bundle" ]] || continue
    $sudo rm -rf "$bundle"
    echo "  $bundle"
done
echo "Done. Your presets in ~/Documents/AxisEQ were left in place."
