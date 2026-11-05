#!/bin/sh
# <item action="'obs_action.sh' '<CMD>'"> : $1=ROOM:<name>|KILL|APPROVE|VETO  $2=pkg. Writes seq+cmd for the manager (same shape as irc_item.sh).
PKG="$2"; SEQF="$PKG/pet_observer.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=%s\n' "$N" "$1" > "$PKG/pet_observer_action.txt"
