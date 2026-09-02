#!/bin/bash
# launch_h_ai.sh — taskbar "Open h-ai" strip-menu launcher, referenced by
# #.desktop/livedesk_taskbar.pdl (strip_btn_14_menu_0_cmd). That pdl row
# shells out to this path; it delegates to ../button.sh, which is the real
# single-instance-safe launcher (build-if-missing + kill-before-launch +
# pid confirmation, see its own header). This script previously did NOT
# exist while the pdl referenced it — the HQ menu's "Open open-hai" row
# (khtpm_taskbar_manager.c) uses button.sh directly, so this is the
# missing link for the strip-level path. Usage: launch_h_ai.sh [house_root]
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="${1:-$(cd "$HERE/../../.." && pwd)}"
exec bash "$HERE/../button.sh" "$HOUSE"
