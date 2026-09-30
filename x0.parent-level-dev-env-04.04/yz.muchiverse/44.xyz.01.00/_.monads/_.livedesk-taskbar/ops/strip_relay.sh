#!/bin/sh
# Generic dock click/submit -> strip_history.txt
# dispatch() runs:  action 'package_dir' 'house_root' [cli_io_value]
# action is:  'this.sh' CODE   OR  'this.sh' submit
set -u
code="$1"
pkg="${2:-}"
house="${3:-}"
value="${4:-}"
[ -n "$house" ] || exit 1
hist="$house/#.desktop/strip_history.txt"
if [ "$code" = "submit" ]; then
    # one ENTER after stuffing printable bytes would lose the string;
    # write the whole value then ENTER.
    if [ -n "$value" ]; then
        printf '%s' "$value" | od -An -t u1 | tr -s ' ' '\n' | grep -v '^$' >> "$hist"
    fi
    echo 13 >> "$hist"
    exit 0
fi
echo "$code" >> "$hist"
