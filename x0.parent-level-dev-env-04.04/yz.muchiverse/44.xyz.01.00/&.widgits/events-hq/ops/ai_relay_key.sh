#!/bin/sh
# The one key an attrition-model page may inject into a window relay.
# 13 is Enter. On the Database window that locks the actor row.
# 201 is Down. It only moves the highlight, so this script refuses it.
# send_input is not called. The line is KEY_PRESSED, via mr_send_window_key.sh.
# Usage: ai_relay_key.sh <relay_path> <decimal_code>
set -u
relay="${1:-}"
code="${2:-}"
if [ -z "$relay" ] || [ -z "$code" ]; then
  echo "Usage: ai_relay_key.sh <relay_path> <decimal_code>" >&2
  exit 1
fi
if [ "$code" != "13" ]; then
  echo "ai_relay_key: refused $code. Only 13 (Enter) may be injected." >&2
  exit 1
fi
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
exec sh "$here/mr_send_window_key.sh" "$relay" 13
