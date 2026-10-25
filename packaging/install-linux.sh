#!/bin/sh
# Installs every Newton Suite plug-in in this folder for the current user (~/.vst3, ~/.clap).
#   sh install-linux.sh              install
#   sh install-linux.sh --uninstall  remove again
set -e
cd "$(dirname "$0")"
V="$HOME/.vst3/Newton Suite"
C="$HOME/.clap/Newton Suite"
if [ "$1" = "--uninstall" ]; then
    rm -rf "$V" "$C"
    echo "Newton Suite removed from $V and $C"
    exit 0
fi
mkdir -p "$V" "$C"
for p in VST3/*.vst3; do
    [ -e "$p" ] || continue
    rm -rf "$V/$(basename "$p")"
    cp -r "$p" "$V/"
done
for p in CLAP/*.clap; do
    [ -e "$p" ] && cp "$p" "$C/"
done
echo "Installed to $V and $C:"
ls "$V" "$C"
echo "Rescan plug-ins in your DAW."
