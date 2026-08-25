#!/bin/sh
# Pre-renders the small status-overlay icons egg_window.c draws in a
# corner (poop/sleep indicators) via the already-working emoji_gen_atlas
# -> emoji_xtract pipeline, same one hatch_egg.c uses for a pet's own
# sprite. Idempotent - skips any icon whose .csv already exists, matching
# the "if it's not in a file, it's a lie" convention: run once, the data
# just sits there afterward. Run this once before using egg_window's
# overlays (./button.sh compile does not run it automatically since it's
# data generation, not a build step).
set -e
cd "$(dirname "$0")/.."

mkdir -p pieces/registry/icons

# Prefer <name>.exe over a same-named extension-less binary if both exist -
# see button.sh's own bin_path() for why (this tree can ship a pre-built
# system/<name> from whichever platform packaged it, and gcc only appends
# .exe to a dot-less -o name, so it never overwrites that leftover file).
bin_path() {
    if [ -x "system/$1.exe" ]; then
        echo "system/$1.exe"
    else
        echo "system/$1"
    fi
}

gen_icon() {
    name="$1"
    emoji="$2"
    csv="pieces/registry/icons/$name.csv"
    if [ -f "$csv" ]; then
        echo "-- $name.csv already exists, skipping"
        return
    fi
    echo "-- generating $name.csv from $emoji"
    "$(bin_path emoji_gen_atlas)" "$emoji" "/tmp/egg-pals-icon-$name.png"
    "$(bin_path emoji_xtract)" "/tmp/egg-pals-icon-$name.png" 0 16 "$csv"
    rm -f "/tmp/egg-pals-icon-$name.png"
}

gen_icon poop "💩"
gen_icon sleep "💤"

echo "icons ok"
