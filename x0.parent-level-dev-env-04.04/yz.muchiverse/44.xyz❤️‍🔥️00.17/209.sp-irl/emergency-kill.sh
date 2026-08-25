#!/bin/sh
# emergency-kill.sh — nuke any rogue spirl or glut process
# Usage: sh emergency-kill.sh

echo "=== EMERGENCY KILL: spirl + glut ==="

# Kill by pidfile
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PIDFILE="$SCRIPT_DIR/.spirl.pid"
if [ -f "$PIDFILE" ]; then
    pid=$(cat "$PIDFILE")
    kill -9 "$pid" 2>/dev/null && echo "killed pid $pid (from .spirl.pid)" || true
    rm -f "$PIDFILE"
fi

# Nuclear options
pkill -9 -x spirl      2>/dev/null && echo "killed spirl (exact)"  || true
pkill -9 -f spirl      2>/dev/null && echo "killed spirl (fuzzy)"  || true
killall -9 spirl       2>/dev/null && echo "killed spirl (all)"    || true
pkill -9 -x gb-pokemon 2>/dev/null && echo "killed gb-pokemon"    || true
pkill -9 -f gb-pokemon 2>/dev/null || true

# Kill any orphaned glut/freeglut windows
pkill -9 -x freeglut   2>/dev/null || true

echo "=== emergency kill complete ==="
