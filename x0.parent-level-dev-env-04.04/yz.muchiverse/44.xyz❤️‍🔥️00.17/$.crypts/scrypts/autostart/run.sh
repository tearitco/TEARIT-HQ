#!/bin/bash
# autostart/run.sh - the house-wide autostart control (subfolder wrapper
# around $.crypts/button.sh, the proven entry point). Subcommands:
#   run|restart  on|off  status  compile  check  install-xdg
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CRYPTS_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
exec bash "$CRYPTS_DIR/button.sh" "$@"
