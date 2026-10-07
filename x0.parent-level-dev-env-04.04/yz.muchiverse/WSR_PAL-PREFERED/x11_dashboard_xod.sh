#!/bin/bash
# x11_dashboard_xod.sh - X11 HQ Dashboard for XOD Tournament
#
# Opens an xterm window running the tournament dashboard.
# Uses x11-hq convention: a dedicated X11 window for monitoring.
#
# Usage: ./x11_dashboard_xod.sh [tournament_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"

# Find tournament dir
if [ -n "$1" ] && [ -d "$1" ]; then
    TOURNAMENT_DIR="$1"
else
    TOURNAMENT_DIR=$(ls -td ${XOD_DIR}/pieces/tournament_* 2>/dev/null | head -1)
fi

if [ -z "$TOURNAMENT_DIR" ]; then
    echo "No tournament found. Run tournament_xod.sh first."
    exit 1
fi

# Launch xterm with dashboard
xterm \
    -title "XOD Tournament Dashboard" \
    -geometry 120x40+100+100 \
    -bg black \
    -fg white \
    -fa 'Monospace' \
    -fs 10 \
    -e "bash -c 'cd ${XOD_DIR} && exec ./dashboard_xod.sh ${TOURNAMENT_DIR}'" &