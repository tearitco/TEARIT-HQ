#!/bin/bash
# vvarware/run.sh - open vvarware alone (subfolder wrapper around
# *.monads/*.hard-vvar-agent-Q0000/button.sh). Subcommands:
#   run (brain loop)  window  read  kill  check
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec bash "$HOUSE_DIR/*.monads/*.hard-vvar-agent-Q0000/button.sh" "$@"
