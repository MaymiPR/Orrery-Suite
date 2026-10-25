#!/bin/sh
# Installs The Antiparticle in this folder for the current user (~/.vst3, ~/.clap).
#   sh install-linux.sh              install
#   sh install-linux.sh --uninstall  remove again
set -e
cd "$(dirname "$0")"
V="$HOME/.vst3"
C="$HOME/.clap"
if [ "$1" = "--uninstall" ]; then
    rm -rf "$V/The Antiparticle.vst3" "$C/The Antiparticle.clap"
    echo "The Antiparticle removed from $V and $C"
    exit 0
fi
mkdir -p "$V" "$C"
rm -rf "$V/The Antiparticle.vst3"
cp -r "VST3/The Antiparticle.vst3" "$V/"
cp "CLAP/The Antiparticle.clap" "$C/"
echo "Installed to $V and $C. Rescan plug-ins in your DAW."
