#!/bin/sh
set -e

cd "$(dirname "$0")"

MKPSXISO="${MKPSXISO:-mkpsxiso}"

if [ ! -f ../cd_read.ps-exe ]; then
    echo "build the executable first: make -C .." 1>&2
    exit 1
fi

python3 make_tone.py tone.wav

if [ -n "$LICENSE" ]; then
    "$MKPSXISO" -y -L "$LICENSE" disc.xml
else
    echo "note: no LICENSE=<file> given, the disc will not boot on hardware" 1>&2
    "$MKPSXISO" -y disc.xml
fi
