#!/bin/bash
# watchdog.sh - PAL Watchdog button for Linux/macOS
# Usage: ./#.buttons/linux/watchdog.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LEGACY_DIR="$SCRIPT_DIR/legacy"

echo "=== Starting PAL Watchdog (Linux) ==="
bash "$LEGACY_DIR/pal_watchdog.sh"
