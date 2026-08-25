#!/bin/bash
# Compatibility wrapper for the descriptive XO-PET build launcher.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR/xo-pet-build-ops.sh" "$@"
