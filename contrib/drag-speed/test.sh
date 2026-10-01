#!/bin/sh
# Verify patch application and speed behavior without input-device access.
set -eu
if [ "$#" -ne 1 ] || [ ! -f "$1/src/evdev-mt-touchpad-gestures.c" ]; then
    echo "Usage: $0 /path/to/unpatched/libinput-1.28.1" >&2
    exit 2
fi
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/source"
cp -a "$1/." "$tmp/source/"
patch --batch --fuzz=0 -d "$tmp/source" -p1 < "$here/libinput-1.28.1.patch"
${CC:-cc} -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror \
    -I "$tmp/source/src" "$here/test-speed.c" -o "$tmp/test-speed"
"$tmp/test-speed"
