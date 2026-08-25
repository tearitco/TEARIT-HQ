#!/bin/bash
# Compatibility wrapper for the descriptive XO-PET init launcher.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR/xo-pet-init.sh" "$@"
