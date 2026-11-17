#!/bin/sh
# colab_room.sh <house_root> [CODE]   (2026-11-17)
# No CODE: print the current room's 4-character code and its session id.
# With CODE (0-9 A-Z, case ignored): print the session id whose code it is; exit 1 if none, 2 if two sessions share it.
# Same arithmetic as room_code() in colab_hai_manager.c: unix-seconds id mod 36^4, base 36, 4 characters. Read only.
HOUSE=${1:?house_root}; WANT=$(printf '%s' "${2:-}" | tr 'a-z' 'A-Z')
D="$HOUSE/#.desktop/colab_hai"
code_of() { v=$(( $1 % 1679616 )); out=""; i=0; while [ $i -lt 4 ]; do d=$(( v % 36 )); if [ $d -lt 10 ]; then c=$d; else c=$(printf "\\$(printf '%03o' $(( 55 + d )))"); fi; out="$c$out"; v=$(( v / 36 )); i=$(( i + 1 )); done; printf '%s' "$out"; }
if [ -z "$WANT" ]; then s=$(cat "$D/current_session.txt" 2>/dev/null | tr -d ' \r\n'); [ -n "$s" ] || { echo "no current session" >&2; exit 1; }; echo "room $(code_of "$s") session $s"; exit 0; fi
found=""; n=0
for p in "$D"/sessions/*/; do s=$(basename "$p"); case "$s" in *[!0-9]*|"") continue ;; esac; [ "$(code_of "$s")" = "$WANT" ] && { found="$s"; n=$(( n + 1 )); }; done
[ "$n" -eq 0 ] && { echo "no session has code $WANT" >&2; exit 1; }
[ "$n" -gt 1 ] && { echo "code $WANT is shared by $n sessions: use the full id" >&2; exit 2; }
echo "$found"
