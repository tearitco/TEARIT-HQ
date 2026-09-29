#!/bin/sh
# event_retroactive_sweep.sh - EVENT-MODULARITY-AND-BUILD-SPEED.md §1's
# "making this real RETROACTIVELY too": one real, generic op covering
# every existing event/page authored before the 🎬️/⚙️ feature existed -
# no separate migration path, this just calls event_auto_clacker.sh
# (the same primitive the live manager's new_page hook uses) once per
# existing page. Idempotent: event_auto_clacker.sh already skips a page
# that already has an ⚙️ item, so re-running this sweep is always safe.
#
# CAUTION (direct instruction, "we will be careful retrofitting ergo
# testing"): this walks and WRITES under every entity dir it finds a
# page in. Always dry-run first (default). Only pass --apply after a
# dry run's output has been reviewed, and only ever test --apply against
# a disposable copy of the house tree before running it against the
# real one.
#
# Usage:
#   event_retroactive_sweep.sh <house_root> [--apply]
#   (no --apply => dry run: prints what WOULD be done, touches nothing)
set -e

HOUSE_ROOT="$1"
MODE="${2:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "Usage: event_retroactive_sweep.sh <house_root> [--apply]" >&2
    exit 1
fi

OPS_DIR="$(cd "$(dirname "$0")" && pwd)"
HOOK="$OPS_DIR/event_auto_clacker.sh"
DRY=1
[ "$MODE" = "--apply" ] && DRY=0

n_total=0
n_skip=0
n_would=0
n_done=0
n_fail=0

# Every entity's event_pkg/pages/page_N, house-wide. Restricted to real
# desktop pals and inventory items under xyzfs/users/*/home - the
# common_events/ library (greet_player etc.) is a shared template
# library, not a per-entity pal, and doesn't own an inventory/ of its
# own to nest a clacker into; out of scope for this sweep by design, not
# an oversight.
find "$HOUSE_ROOT/xyzfs/users" -type d -path "*/event_pkg/pages/page_*" 2>/dev/null | while read -r pagedir; do
    n_total=$((n_total + 1))

    # CRITICAL: skip any page that is ALREADY inside a 🎬️ clacker this
    # tool created (real bug caught in testing, 2026-09-28 - a re-run
    # otherwise treats every ⚙️ item's OWN copied event_pkg/pages/page_N
    # as yet another source page needing its own clacker, recursing one
    # level deeper every single run, forever). Only a path this tool
    # itself produced ever contains "/inventory/event_clacker_" (numbered
    # 2026-09-28: "clacker should be numbered like pages") - a genuine,
    # never-yet-retrofitted source page never does.
    case "$pagedir" in
        */inventory/event_clacker_*)
            n_skip=$((n_skip + 1))
            continue
            ;;
    esac

    page_name="$(basename "$pagedir")"
    pkg_dir="$(dirname "$(dirname "$pagedir")")"
    ent_dir="$(dirname "$pkg_dir")"
    ent_name="$(basename "$ent_dir")"

    clacker_item="$ent_dir/inventory/event_clacker_1/inventory/$page_name"
    if [ -d "$clacker_item" ]; then
        n_skip=$((n_skip + 1))
        continue
    fi

    if [ "$DRY" = "1" ]; then
        echo "WOULD MATERIALIZE: $ent_name / $page_name  ($pagedir)"
        n_would=$((n_would + 1))
    else
        echo "-- $ent_name / $page_name"
        if sh "$HOOK" "$ent_dir" "$page_name" "$HOUSE_ROOT"; then
            n_done=$((n_done + 1))
        else
            echo "   FAILED: $ent_name / $page_name" >&2
            n_fail=$((n_fail + 1))
        fi
    fi
done

echo ""
if [ "$DRY" = "1" ]; then
    echo "DRY RUN complete. Re-run with --apply to actually materialize the above."
else
    echo "Sweep complete."
fi
