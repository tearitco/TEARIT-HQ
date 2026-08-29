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

RAN_ANYTHING=0

if [ -n "$PAGE_DIR" ]; then
  PAGE_NAME="$(basename "$PAGE_DIR")"
  PAL="$PAGE_DIR/event.pal"
  if [ ! -f "$PAL" ]; then
    echo "play_event: no event script yet: $PAL" >&2
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: matched $PAGE_NAME (trigger=$TRIGGER) but no event.pal for $NAME" >> "$PKG/master_ledger.txt"
  else
    # Run from muta system dir so relative default_op.txt noise is reduced if any
    (cd "$(dirname "$PRISC")" && "$PRISC" "$PAL") >> "$PKG/master_ledger.txt" 2>&1 || true
    RAN_ANYTHING=1
    # Snapshot inventory after play
    if [ -f "$PKG/inventory.txt" ]; then
      echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event $PAGE_NAME (trigger=$TRIGGER) for $NAME | inventory: $(tr '\n' ' ' < "$PKG/inventory.txt")" >> "$PKG/master_ledger.txt"
      # keep gold.txt / menu in sync
      g=$(grep -E '^qolq=' "$PKG/inventory.txt" 2>/dev/null | head -1 | cut -d= -f2 | tr -d '\r\n ')
      [ -n "$g" ] && echo -n "$g" > "$PKG/gold.txt"
    else
      echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event $PAGE_NAME (trigger=$TRIGGER) for $NAME | no inventory.txt yet" >> "$PKG/master_ledger.txt"
    fi
    echo "play_event: ran entity page $NAME ($PAGE_NAME, trigger=$TRIGGER)"
  fi
fi

# REAL FIX (2026-08-25, direct instruction: "have u been duplicating and
# testing our events in common events yet? (player play would trigger
# those)") - common events previously had ZERO runtime wiring: db-hq's
# Common Events feature could edit them (via events-hq, pointed at
# common_events/<name> exactly like any entity's own event_pkg - see
# khtpm_hq_manager.c's open_in_editor()), but nothing ever RAN one.
# Additive only - the entity-local block above is byte-for-byte
# unchanged behavior. Each common event is independently named (not a
# multi-page variant of ONE thing like an entity's own pages), so every
# matching one runs, not just the highest-numbered - "highest wins" is
# scoped to page_N variants inside a single named event, not across
# distinct common events.
#
# KNOWN LIMITATION (real, not silently glossed over): a common event's
# compiled cmd_N.sh resolves its OWN target directory via
# `cd "$(dirname "$0")/../../.."` (see ez_menu_input.c/khtpm_events_hq_
# manager.c's compile_page()) - i.e. it acts on common_events/<name>/
# itself, NOT on $PKG (the entity that was actually Played). This proves
# the real DISPATCH wiring (Play now finds and runs matching common
# events) but does NOT yet let a common event modify the calling
# player's own state - that needs cmd_N.sh's compiler to accept an
# explicit target-dir override, a separate follow-up (see
# EVENTS_ROADMAP_NEXT_STEPS.md).
CE_ROOT="$HOUSE_ROOT/common_events"
if [ -d "$CE_ROOT" ]; then
  for ce_dir in "$CE_ROOT"/*/; do
    [ -d "$ce_dir" ] || continue
    CE_NAME="$(basename "$ce_dir")"
    CE_PAGES="$ce_dir/event_pkg/pages"
    [ -d "$CE_PAGES" ] || continue
    CE_PAGE_DIR=""
    for pd in $(find "$CE_PAGES" -maxdepth 1 -type d -name 'page_*' 2>/dev/null | sort -t_ -k2 -n); do
      cond="$pd/condition.pdl"
      [ -f "$cond" ] || continue
      t=$(awk -F'|' '/^COND[[:space:]]*\|[[:space:]]*trigger/ {print $3}' "$cond" | tail -1 | tr -d ' \r\n')
      if [ "$t" = "$TRIGGER" ]; then CE_PAGE_DIR="$pd"; fi
    done
    [ -n "$CE_PAGE_DIR" ] || continue
    CE_PAL="$CE_PAGE_DIR/event.pal"
    CE_LEDGER="$ce_dir/master_ledger.txt"
    if [ ! -f "$CE_PAL" ]; then
      echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: matched $(basename "$CE_PAGE_DIR") (trigger=$TRIGGER) but no event.pal for common event $CE_NAME" >> "$CE_LEDGER"
      continue
    fi
    # REAL FIX 2026-08-25 (Show Choices needs a VISIBLE popup on the real
    # player entity, not the common event's own directory - see
    # mr_show_choices.c's own header comment for the full story): export
    # the calling entity's real package_dir so any command run from a
    # common event can route player-visible UI (relay writes) to the
    # entity that was actually Played, while still logging/writing its
    # own result files into the common event's own directory.
    (cd "$(dirname "$PRISC")" && MUCHI_CALLER_PKG="$PKG" "$PRISC" "$CE_PAL") >> "$CE_LEDGER" 2>&1 || true
    RAN_ANYTHING=1
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play triggered common event $CE_NAME ($(basename "$CE_PAGE_DIR"), trigger=$TRIGGER) via $NAME" >> "$CE_LEDGER"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: also triggered common event $CE_NAME (trigger=$TRIGGER)" >> "$PKG/master_ledger.txt"
    echo "play_event: ran common event $CE_NAME ($(basename "$CE_PAGE_DIR"), trigger=$TRIGGER)"
  done
fi

if [ "$RAN_ANYTHING" = "0" ]; then
  echo "play_event: no page matches trigger '$TRIGGER' for $NAME (entity or common events)" >&2
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: no page matches trigger '$TRIGGER' for $NAME (entity or common events)" >> "$PKG/master_ledger.txt"
  exit 1
fi
echo "play_event: done $NAME (trigger=$TRIGGER)"
