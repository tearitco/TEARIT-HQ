#!/bin/bash
# muchi-pet/run.sh - open muchi-pet alone (subfolder wrapper around
# *.monads/*.muchi-pet/button.sh). Subcommands:
#   run (open active monster)  window  kill  check
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec bash "$HOUSE_DIR/*.monads/*.muchi-pet/button.sh" "$@"
