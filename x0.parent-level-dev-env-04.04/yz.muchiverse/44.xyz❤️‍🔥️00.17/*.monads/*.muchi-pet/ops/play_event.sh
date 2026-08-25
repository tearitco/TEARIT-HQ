#!/bin/bash
# play_event.sh <package_dir> [house_root] [trigger]
#
# REAL FIX (2026-08-12, direct instruction to build multi-page/multi-
# trigger support — see au11-hq/EVENT_AI_VISION.md §0/§1): this used to
# run event_pkg/pages/page_1/event.pal UNCONDITIONALLY, no matter how many
# pages an entity had or what trigger fired. That was the actual blocker
# standing between the one proven event (Change Gold, single page,
# single trigger) and anything richer (multiple pages, each gated by its
# own condition.pdl trigger — see khtpm's own condition.pdl format:
# `COND | trigger | on-click`).
#
# Now: scans event_pkg/pages/page_* (numeric order), finds every page
# whose condition.pdl trigger exactly matches $TRIGGER (default
# "on-click" — preserves every EXISTING caller's behavior unchanged,
# since dispatch_action() only ever passes 2 args today and no caller has
# ever passed a 3rd), and runs the HIGHEST-numbered matching page only —
# same semantics RPG Maker MV itself uses for multi-page events (highest-
# numbered page whose conditions are true wins, see EVENT_AI_VISION.md
# §1's design implication). Does NOT yet support switches/variables as
# conditions, only the trigger name itself - that's a real, separate,
# not-yet-built extension.
set -e
PKG="${1:-}"
TRIGGER="${3:-on-click}"
if [ -z "$PKG" ] || [ ! -d "$PKG" ]; then
  echo "play_event: need package dir" >&2
  exit 1
fi
PKG="$(cd "$PKG" && pwd)"
NAME="$(basename "$PKG")"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MR_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# REAL FIX (2026-08-11/12, ops migration: this script moved from
# *.monads/*.muchi-pet/ops/ to xyzfs/bin/muchi-pet/ops/, a DIFFERENT depth
# under house_root - *.monads/*.muchi-pet is 2 dirs deep, xyzfs/bin/
# muchi-pet is 3, so the old fixed "../.." HOUSE_ROOT walk silently landed
# one level short (house_root/xyzfs instead of house_root, breaking the
# prisc+x lookup). Same anchor-search fix already applied to the compiled
# cmd_N.sh wrappers in ez_menu_input.c - search upward for the house's own
# top-level "101.mutaclsym*" system dir instead of assuming a fixed depth,
# so a future relocation doesn't silently break this again. */
D="$MR_ROOT"
HOUSE_ROOT=""
while [ "$D" != "/" ]; do
  # REAL FIX (2026-08-24, cursword): the house root now has TWO dirs
  # matching 101.mutaclsym* (+18.0G and 19.00), so the old unquoted-glob
  # [ -d "$D"/101.mutaclsym*/system ] test expanded to TWO words -> bash
  # "binary operator expected" (swallowed by 2>/dev/null) at EVERY level,
  # and the upward walk fell off the top of the house: every Play click
  # died with "could not locate house root". Iterate the matches instead;
  # first dir actually containing a system/ wins - same intent as before.
  for cand in "$D"/101.mutaclsym*/system; do
    if [ -d "$cand" ]; then HOUSE_ROOT="$D"; break 2; fi
  done
  D="$(dirname "$D")"
done
if [ -z "$HOUSE_ROOT" ]; then
  echo "play_event: could not locate house root (101.mutaclsym*/system) above $MR_ROOT" >&2
  exit 1
fi
MUTA_DIR="$(ls -d "$HOUSE_ROOT"/101.mutaclsym*/system | head -1)"
PRISC="$MUTA_DIR/prisc+x"
if [ ! -x "$PRISC" ]; then
  echo "play_event: missing prisc+x at $PRISC" >&2
  exit 1
fi

# Find the HIGHEST-numbered page whose condition.pdl trigger matches
# $TRIGGER. Pages sorted numerically (page_1, page_2, ... page_10, not
# lexically "page_10" < "page_2") since a real event could have >9 pages.
PAGE_DIR=""
if [ -d "$PKG/event_pkg/pages" ]; then
  for pd in $(find "$PKG/event_pkg/pages" -maxdepth 1 -type d -name 'page_*' | sort -t_ -k2 -n); do
    cond="$pd/condition.pdl"
    [ -f "$cond" ] || continue
    t=$(awk -F'|' '/^COND[[:space:]]*\|[[:space:]]*trigger/ {print $3}' "$cond" | tail -1 | tr -d ' \r\n')
    if [ "$t" = "$TRIGGER" ]; then
      PAGE_DIR="$pd"
    fi
  done
fi

if [ -z "$PAGE_DIR" ]; then
  echo "play_event: no page matches trigger '$TRIGGER' for $NAME" >&2
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: no page matches trigger '$TRIGGER' for $NAME" >> "$PKG/master_ledger.txt"
  exit 1
fi
PAGE_NAME="$(basename "$PAGE_DIR")"
PAL="$PAGE_DIR/event.pal"

if [ ! -f "$PAL" ]; then
  echo "play_event: no event script yet: $PAL" >&2
  # still leave a readable note in ledger
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: matched $PAGE_NAME (trigger=$TRIGGER) but no event.pal for $NAME" >> "$PKG/master_ledger.txt"
  exit 1
fi
# Run from muta system dir so relative default_op.txt noise is reduced if any
cd "$(dirname "$PRISC")"
"$PRISC" "$PAL" >> "$PKG/master_ledger.txt" 2>&1 || true
# Snapshot inventory after play
if [ -f "$PKG/inventory.txt" ]; then
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event $PAGE_NAME (trigger=$TRIGGER) for $NAME | inventory: $(tr '\n' ' ' < "$PKG/inventory.txt")" >> "$PKG/master_ledger.txt"
  # keep gold.txt / menu in sync
  g=$(grep -E '^qolq=' "$PKG/inventory.txt" 2>/dev/null | head -1 | cut -d= -f2 | tr -d '\r\n ')
  [ -n "$g" ] && echo -n "$g" > "$PKG/gold.txt"
else
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event $PAGE_NAME (trigger=$TRIGGER) for $NAME | no inventory.txt yet" >> "$PKG/master_ledger.txt"
fi
echo "play_event: done $NAME ($PAGE_NAME, trigger=$TRIGGER)"
