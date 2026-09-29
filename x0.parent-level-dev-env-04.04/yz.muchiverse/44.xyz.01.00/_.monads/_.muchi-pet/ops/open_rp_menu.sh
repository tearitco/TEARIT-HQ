#!/bin/bash
# open_rp_menu.sh — RPG Menu for ranch entity (KHTPM).
# Usage: open_rp_menu.sh <package_dir>
set -e
PKG="${1:-}"
if [ -z "$PKG" ] || [ ! -d "$PKG" ]; then
  echo "open_rp_menu: need package dir" >&2
  exit 1
fi
PKG="$(cd "$PKG" && pwd)"
NAME="$(basename "$PKG")"
SELF="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"

read_or_default() {
  local f="$1" d="$2"
  if [ -f "$f" ]; then
    tr -d '\r\n' < "$f" | head -c 32
  else
    echo -n "$d" > "$f"
    echo -n "$d"
  fi
}
LVL=$(read_or_default "$PKG/level.txt" "1")
XP=$(read_or_default "$PKG/xp.txt" "0")
XP_NEXT=$(read_or_default "$PKG/xp_next.txt" "100")
HP=$(read_or_default "$PKG/hp.txt" "100")
HP_MAX=$(read_or_default "$PKG/hp_max.txt" "100")
MP=$(read_or_default "$PKG/mp.txt" "50")
MP_MAX=$(read_or_default "$PKG/mp_max.txt" "50")
GOLD="0"
if [ -f "$PKG/inventory.txt" ]; then
  g=$(grep -E '^qolq=' "$PKG/inventory.txt" 2>/dev/null | head -1 | cut -d= -f2 | tr -d '\r\n ')
  [ -n "$g" ] && GOLD="$g"
fi
if [ "$GOLD" = "0" ] && [ -f "$PKG/gold.txt" ]; then
  GOLD=$(tr -d '\r\n' < "$PKG/gold.txt")
fi
echo -n "$GOLD" > "$PKG/gold.txt"

# REAL FIX 2026-08-10, direct report ("event-ez button no longer opens
# event editor - path issue, should use relatives not absolutes"): this
# used to climb MR_ROOT from $PKG (package_dir, the ENTITY's own
# location) via a fixed "../.." - correct only for the dev-tree depth
# (.../MUCHI_RANCHER/entities/<name>) and broken for pals-migrated
# entities (xyzfs/users/<uuid>/home/livedesk/pals/<name>, a different
# depth). Worse: this script REGENERATES objects.pdl every time "Menu"
# is clicked, so a broken MR_ROOT here silently re-corrupts
# Play/Events(ez) even after fixing objects.pdl by hand. Fix: derive
# MR_ROOT from THIS SCRIPT's own location (SCRIPT_DIR, i.e. dirname
# "$0" of the script file, NOT the entity), which never moves regardless
# of where the calling entity currently lives - same pattern
# play_event.sh already used correctly.
SCRIPT_DIR="$(cd "$(dirname "$SELF")" && pwd)"
MR_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EZ_OPEN="$MR_ROOT/ops/open_event_ez.sh"
PLAY="$MR_ROOT/ops/play_event.sh"

# Quoted delimiter so only intentional $vars expand (no broken heredoc tail)
cat > "$PKG/objects.pdl" << OBJ
PAGE | main
OBJECT | label=Feed | action=void
OBJECT | label=Menu | action=$SELF
OBJECT | label=Play | action=$PLAY
OBJECT | label=Events (ez) | action=$EZ_OPEN
OBJECT | label=More | action=GOTO:activities
OBJECT | label=Rename | action=STATE:pending_name
OBJECT | label=Dir | action=xdg-open
OBJECT | label=Close | action=CLOSE
OBJECT | label=Cancel | action=void

PAGE | activities
OBJECT | label=Train | action=void
OBJECT | label=Rest | action=void
OBJECT | label=Tournament | action=void
OBJECT | label=Errantry | action=void
OBJECT | label=Back | action=BACK
OBJECT | label=Cancel | action=void

PAGE | menu
OBJECT | label=-- $NAME -- | action=void
OBJECT | label=Level: $LVL | action=void
OBJECT | label=XP: $XP / $XP_NEXT to next | action=void
OBJECT | label=qolq (gold): $GOLD | action=void
OBJECT | label=HP: $HP / $HP_MAX | action=void
OBJECT | label=MP: $MP / $MP_MAX | action=void
OBJECT | label=Items | action=GOTO:items
OBJECT | label=Skills | action=GOTO:skills
OBJECT | label=Status | action=GOTO:status
OBJECT | label=Back | action=BACK
OBJECT | label=Cancel | action=void

PAGE | items
OBJECT | label=-- Items (soon) -- | action=void
OBJECT | label=(no items yet) | action=void
OBJECT | label=Back | action=BACK
OBJECT | label=Cancel | action=void

PAGE | skills
OBJECT | label=-- Skills (soon) -- | action=void
OBJECT | label=(no skills yet) | action=void
OBJECT | label=Back | action=BACK
OBJECT | label=Cancel | action=void

PAGE | status
OBJECT | label=-- Status -- | action=void
OBJECT | label=Name: $NAME | action=void
OBJECT | label=Level: $LVL | action=void
OBJECT | label=XP: $XP / $XP_NEXT | action=void
OBJECT | label=qolq (gold): $GOLD | action=void
OBJECT | label=HP: $HP / $HP_MAX | action=void
OBJECT | label=MP: $MP / $MP_MAX | action=void
OBJECT | label=Back | action=BACK
OBJECT | label=Cancel | action=void
OBJ

printf 'OPEN_PAGE:menu\n' > "$PKG/interact_relay.txt"
echo "open_rp_menu: $NAME Lv$LVL XP $XP/$XP_NEXT qolq $GOLD HP $HP/$HP_MAX MP $MP/$MP_MAX"
