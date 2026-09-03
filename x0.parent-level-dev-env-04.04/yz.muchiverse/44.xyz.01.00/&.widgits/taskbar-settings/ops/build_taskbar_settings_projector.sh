#!/bin/sh
# build_taskbar_settings_projector.sh - compile the taskbar-settings-pal
# UI projector. Mirrors events-hq/ops/build_evhq_projector.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$HERE/+x/taskbar_settings_projector.+x" "$HERE/taskbar_settings_projector.c"
echo "OK $HERE/+x/taskbar_settings_projector.+x"
