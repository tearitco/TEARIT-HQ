#!/bin/bash
# Compatibility wrapper for the descriptive XO-PET tank setup launcher.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR/xo-pet-setup-tank.sh" "$@"
