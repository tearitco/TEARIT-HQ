#!/bin/sh
# ai_lab_register_drop.sh - h-ai-lab's real drop_action target.
#
# Wired via <window drop_action="..."> in h-ai-lab.xhtpm - this is the
# ALREADY-generic, already-proven XDND mechanism in khtpm_core_render.c
# (see its own g_drop_action block comment, first used for dragging a
# directory onto bookmarks). Zero new per-project C - a data-driven
# .xhtpm attribute, exactly like this task's own instruction requires.
#
# Invocation convention (confirmed directly against
# khtpm_core_render.c's real drop-dispatch call site, ~line 9485-9488):
#   $DROP_PATH   - exported env var, the dropped item's own real path
#                  (a directory, for a dragged terumon seed folder)
#   argv1        - package_dir (this app's own dir, $PKG)
#   argv2        - house_root
# Same positional convention dispatch() already uses for action=.
#
# What this does: if $DROP_PATH looks like a real terumon seed dir
# (has a learning_limits.pdl and a dustball_state.pdl next to it, per
# TERUMON-SPEC.md §3's real seed layout), register it into the real,
# house-wide AI registry (&.widgits/ai-lab/ops/ai_registry.sh) so it
# shows up in h-ai-lab's own sidebar like any other AI instance.
#
# KIND= decision: ai_registry.sh's own valid_kind() vocabulary is
# currently fsm|attention-net|decision-pal|other (checked directly,
# ai_registry.sh header comment + valid_kind()). A terumon is none of
# the three concrete kinds - it's not an FSM state table, not an
# attention-net HTTP service, not a decision-pal state.txt pal. Its
# own real interface today is the file-backed Concept-Bank/learner-
# instance shape TERUMON-SPEC.md/A-TEARIT-IS-ALL-YOU-NEED.md describe,
# which none of the three concrete KINDs name. ai_registry.sh's own
# header comment names "other" as the deliberate escape hatch for
# exactly this ("add a new KIND when a real instance needs one, don't
# pre-design for hypotheticals") - so this registers terumon as
# KIND=other, not a new KIND value invented on the spot. OPEN: if/when
# a second terumon-shaped registry entry exists, a real KIND=terumon
# (or KIND=concept-bank) value may be worth adding to valid_kind() -
# not done here, per that same header comment's own "don't pre-design"
# rule - one real instance isn't evidence a new KIND is needed yet.
set -u
PKG="${1:-}"
HOUSE_ROOT="${2:-}"
DROP_PATH="${DROP_PATH:-}"

[ -n "$PKG" ] && [ -n "$HOUSE_ROOT" ] || {
    echo "ai_lab_register_drop.sh: missing package_dir/house_root" >&2
    exit 1
}
mkdir -p "$PKG/state"
RESULT_FILE="$PKG/state/last_action_result.txt"

if [ -z "$DROP_PATH" ] || [ ! -d "$DROP_PATH" ]; then
    echo "drop ignored: \$DROP_PATH ('$DROP_PATH') is not a real directory" > "$RESULT_FILE"
    exit 0
fi

if [ ! -f "$DROP_PATH/learning_limits.pdl" ] || [ ! -f "$DROP_PATH/dustball_state.pdl" ]; then
    echo "drop ignored: '$DROP_PATH' doesn't look like a real terumon seed dir (missing learning_limits.pdl/dustball_state.pdl, per TERUMON-SPEC.md §3)" > "$RESULT_FILE"
    exit 0
fi

NAME="$(basename "$DROP_PATH")"
REG_SH="$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh"
[ -x "$REG_SH" ] || REG_SH="sh $REG_SH"

OUT="$(sh "$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh" add "$HOUSE_ROOT" "$NAME" other "$DROP_PATH" "$DROP_PATH/learning_limits.pdl" 2>&1)"
RC=$?
if [ "$RC" -eq 0 ]; then
    echo "registered terumon '$NAME' (KIND=other, PATH=$DROP_PATH) via drop - $OUT" > "$RESULT_FILE"
else
    echo "terumon drop registration failed for '$NAME': $OUT" > "$RESULT_FILE"
fi

sh "$PKG/ai_lab_scan.sh" "$HOUSE_ROOT" publish "$PKG/state/ui.txt" >/dev/null 2>&1 || true
exit 0
