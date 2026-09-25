#!/bin/sh
# Append one window-mailbox line. Does not change send_input, which
# still writes the bare code through prisc+x.
# Usage: mr_send_window_key.sh <relay_path> <decimal_code>
set -u
relay="${1:-}"
code="${2:-}"
if [ -z "$relay" ] || [ -z "$code" ]; then
  echo "Usage: mr_send_window_key.sh <relay_path> <decimal_code>" >&2
  exit 1
fi
case "$code" in
  *[!0-9]*) echo "mr_send_window_key: code must be decimal" >&2; exit 1 ;;
esac
printf 'KEY_PRESSED: %s\n' "$code" >> "$relay"
echo "WINDOW_KEY $code >> $relay"
