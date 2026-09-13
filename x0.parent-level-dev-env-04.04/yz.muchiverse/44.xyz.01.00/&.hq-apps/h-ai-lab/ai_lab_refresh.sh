#!/bin/sh
# ai_lab_refresh.sh - the <module> backend for h-ai-lab.xhtpm.
# Re-publishes state/ui.txt every REFRESH_SEC. The renderer SIGTERMs
# this on window close (kh_cleanup_modules), same convention as
# proc-mon's mon_refresh.sh - no guard needed.
#
# argv (appended by launch_module): <house_root> <package_dir>
set -u
REFRESH_SEC="${AI_LAB_REFRESH_SEC:-4}"
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
PKG="${2:-${KHTPM_PKG:-}}"
[ -n "$PKG" ] || PKG=$(cd "$(dirname "$0")" && pwd)

trap 'exit 0' TERM INT HUP
mkdir -p "$PKG/state"
while :; do
    sh "$PKG/ai_lab_scan.sh" "$HOUSE_ROOT" publish "$PKG/state/ui.txt" >/dev/null 2>&1 || true
    sleep "$REFRESH_SEC"
done
