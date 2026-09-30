#!/bin/bash
# book-stack/run.sh - open book-stack alone (subfolder wrapper around
# _.monads/_.book-stack/button.sh). Subcommands:
#   run (window + reader)  window  read  kill  check
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec bash "$HOUSE_DIR/_.monads/_.book-stack/button.sh" "$@"
