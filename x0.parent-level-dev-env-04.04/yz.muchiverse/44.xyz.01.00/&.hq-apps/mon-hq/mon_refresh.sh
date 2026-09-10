#!/bin/sh
# mon_refresh.sh - the <module> backend for mon-hq.xhtpm.
# Re-publishes state/ui.txt every REFRESH_SEC. The renderer SIGTERMs
# this on window close (kh_cleanup_modules), so no guard needed.
#
# argv (appended by launch_module): <house_root> <package_dir>
set -u
# weak-box friendly: one publish is ~1.5s of ps/awk/proc reads, so a
# 4s gap keeps the monitor's own footprint well under 30% of a core.
REFRESH_SEC="${MON_REFRESH_SEC:-4}"
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
PKG="${2:-${KHTPM_PKG:-}}"
[ -n "$PKG" ] || PKG=$(cd "$(dirname "$0")" && pwd)
export MON_HOUSE="$HOUSE_ROOT"

trap 'exit 0' TERM INT HUP
mkdir -p "$PKG/state"
while :; do
    sh "$PKG/mon_scan.sh" publish "$PKG/state/ui.txt" >/dev/null 2>&1 || true
    sleep "$REFRESH_SEC"
done
