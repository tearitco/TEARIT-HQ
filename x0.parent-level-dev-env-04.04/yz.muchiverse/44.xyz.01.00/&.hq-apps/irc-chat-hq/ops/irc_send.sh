#!/bin/sh
# <cli_io id="composer"> action: $1=pkg_dir $2=house_root $3=live typed text
PKG="$1"; TEXT="$3"
[ -z "$TEXT" ] && exit 0
TEXT=$(printf '%s' "$TEXT" | tr '\r\n\t' '   ')
SEQF="$PKG/irc_chat.seq"
N=$(( $(cat "$SEQF" 2>/dev/null || echo 0) + 1 )); echo "$N" > "$SEQF"
printf 'seq=%s\ncmd=SEND:%s\n' "$N" "$TEXT" > "$PKG/irc_chat_action.txt"
