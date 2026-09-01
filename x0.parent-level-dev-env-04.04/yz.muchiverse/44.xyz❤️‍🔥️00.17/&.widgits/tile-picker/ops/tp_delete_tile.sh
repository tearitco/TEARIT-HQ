#!/bin/sh
# tp_delete_tile.sh <package_dir> <house_root> - real METHOD action for
# a placed tile's own "Delete" row (direct instruction 2026-09-01:
# placed tiles never had a real way to be removed short of hand-editing
# #.desktop files).
#
# Real, safe two-step shutdown before removal - same TERM-then-KILL
# escalation convention run_khtpm_strip.sh's own kill_khtpm() already
# uses: lets the live renderer's own real shutdown path
# (livedesk_registry_remove()/nav_release_pid(), tp_main()'s own
# SIGTERM handler in khtpm_core_render.c) run cleanly before the
# package directory itself is removed, instead of yanking storage out
# from under a still-running process.
PKG="$1"

if [ -z "$PKG" ] || [ ! -d "$PKG" ]; then
    exit 1
fi

# Escape regex metacharacters in PKG - same real bug class
# tp_place_desktop.c's own pgrep duplicate-guard already had to fix
# (unescaped '.'/'+'/etc in `pgrep -f`'s own EXTENDED REGEX argument
# silently matches far more than intended).
ESC=$(printf '%s' "$PKG" | sed 's/[.[\*^$/]/\\&/g')
PAT="khtpm_core_render\.\+x $ESC\$"

pkill -f "$PAT" 2>/dev/null
i=0
while [ "$i" -lt 20 ]; do
    pgrep -f "$PAT" >/dev/null 2>&1 || break
    sleep 0.1
    i=$((i + 1))
done
pgrep -f "$PAT" >/dev/null 2>&1 && pkill -KILL -f "$PAT" 2>/dev/null

rm -rf "$PKG"
