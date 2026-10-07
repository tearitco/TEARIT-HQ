#!/bin/sh
# build_music_daemon.sh - compile the music daemon v0 ops (music_seq, music_synth, music_daemon) into ops/+x/. Picked up by $crypts/compile-runner.sh (ops/build_*.sh).
# Self-contained C (libm only); nothing is started. Scratch/staging only until the owner has listened to it.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/+x"
for op in music_seq music_synth music_daemon; do
    nice -n 15 gcc -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -Wno-format-truncation -O2 -o "$HERE/+x/$op.+x" "$HERE/$op.c" -lm
    echo "OK $HERE/+x/$op.+x"
done
