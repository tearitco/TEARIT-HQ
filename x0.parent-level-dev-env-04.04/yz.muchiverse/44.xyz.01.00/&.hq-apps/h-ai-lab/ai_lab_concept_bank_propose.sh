#!/bin/sh
# ai_lab_concept_bank_propose.sh - Part 4's real first step, wired to
# the Concept Bank validator built for this same pass (step 1 of this
# task). Per H-AI-LAB-DESIGN.md Part 4: reuse the EXISTING "Propose
# weights"/"Score curriculum" buttons - do not add new ones - and per
# this task's direct instruction, when the selected registry entry is
# a terumon/Concept-Bank-backed instance (KIND=other + a real
# learning_limits.pdl at its PATH, see TERUMON-SPEC.md §3), both
# buttons call through to producing + validating a real candidate EDIT
# record (A-TEARIT-IS-ALL-YOU-NEED.md §2.3/§2.4) instead of talking to
# Gemma directly - Gemma's own real /api call is a separate, later
# step (this doc's own Part 4 already scopes "ONE button... prove
# Gemma's call + the draft write before building the review/accept UI
# on top of it" - this pass proves the validator half of that chain,
# not the Gemma-call half, which stays real, named, and not yet built).
#
# Real, separate artifact per action (Part 4's own explicit rule):
# never write straight into a live Concept Bank file. This writes ONE
# line into <instance PATH>/pending_review.txt - what it is, the
# scorer's raw text (stubbed here, see below), a timestamp, and the
# validator's own PASS/REJECT verdict on the resulting candidate EDIT.
# Accept/merge into the real spoke record is a separate, NOT-yet-built
# step (Part 4 explicitly defers this - "NO accept/merge path yet").
#
# Usage: ai_lab_concept_bank_propose.sh <pkg> <house_root> <mode>
#   mode = propose_weights | score_curriculum   (FSM states PROPOSING/
#          JUDGING per Part 4's own table - same call shape either way)
set -u
PKG="${1:-}"
HOUSE_ROOT="${2:-}"
MODE="${3:-propose_weights}"

[ -n "$PKG" ] && [ -n "$HOUSE_ROOT" ] || {
    echo "usage: ai_lab_concept_bank_propose.sh <pkg> <house_root> <mode>" >&2
    exit 1
}
mkdir -p "$PKG/state"
RESULT_FILE="$PKG/state/last_action_result.txt"
SEL_FILE="$PKG/state/selected.txt"
REG_SH="$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh"
BANK_DIR="$HOUSE_ROOT/&.widgits/concept-bank"
VALIDATOR="$BANK_DIR/ops/+x/concept_edit_validate.+x"

SEL=""
[ -f "$SEL_FILE" ] && SEL="$(cat "$SEL_FILE" 2>/dev/null)"
if [ -z "$SEL" ]; then
    echo "no instance selected - select an instance first" > "$RESULT_FILE"
    exit 0
fi

SEL_LINE="$(sh "$REG_SH" list "$HOUSE_ROOT" 2>/dev/null | awk -F'|' -v n="NAME=$SEL" '$1==n')"
d_kind=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $2}' | sed 's/^KIND=//')
d_path=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $3}' | sed 's/^PATH=//')

if [ "$d_kind" != "other" ] || [ ! -f "$d_path/learning_limits.pdl" ]; then
    echo "'$SEL' is not a Concept-Bank-backed (terumon) instance - Propose weights/Score curriculum only reach the Concept Bank for KIND=other entries with a real learning_limits.pdl (see TERUMON-SPEC.md §3)" > "$RESULT_FILE"
    exit 0
fi

# direction.allow's first real entry is this terumon's own scoped
# concept target - e.g. terumon_001_ember's learning_limits.pdl has
# "allow: [force, motion, gravity_constant]" (TERUMON-SPEC.md seed).
# Real, small, deterministic extraction - no free-text parsing.
FIRST_ALLOW="$(awk -F'[][, ]+' '/^  allow:/{for(i=1;i<=NF;i++){ if($i!="" && $i!="allow:") { print $i; exit } }}' "$d_path/learning_limits.pdl")"
[ -n "$FIRST_ALLOW" ] || FIRST_ALLOW="force"

# Gemma's real call (llm_choice()-shaped, NIGHT 9's tool per Part 4's
# table) is NOT wired in this pass - stubbed with a clearly-labeled
# placeholder reason, matching this task's own "don't fake a working
# ledger with invented numbers" discipline applied to the same
# not-yet-built dependency.
TS="$(date '+%Y-%m-%d %H:%M:%S')"
REASON="STUB: real Gemma llm_choice() call not wired this pass (Part 4 defers it) - placeholder reason for mode=$MODE against target concept '$FIRST_ALLOW'"
EDIT_ID="edit-$(date +%s)-$$"
TMP_EDIT="$PKG/state/.tmp_candidate_edit.$$"
# target=gravity_constant is hardcoded - it's the ONLY real spoke
# record this task's step 1 hand-authored (concept-bank/data/spokes/).
# Real future work: derive target from the terumon's own corpus/
# observation instead of a fixed name, once more spoke records exist.
printf 'EDIT | id=%s | type=spoke_weight_delta | target=gravity_constant | slot=%s | delta=+0.01 | reason="%s" | proposer=gemma | status=candidate\n' \
    "$EDIT_ID" "$FIRST_ALLOW" "$REASON" > "$TMP_EDIT"

VERDICT="$("$VALIDATOR" "$BANK_DIR" "$TMP_EDIT" 2>&1)"
VRC=$?
rm -f "$TMP_EDIT"

mkdir -p "$d_path"
printf '%s | mode=%s | instance=%s | verdict_rc=%s | %s\n' "$TS" "$MODE" "$SEL" "$VRC" "$VERDICT" >> "$d_path/pending_review.txt"

if [ "$VRC" -eq 0 ]; then
    echo "$MODE for '$SEL': candidate edit VALIDATED (id=$EDIT_ID, target concept=$FIRST_ALLOW) - appended to $d_path/pending_review.txt, NOT yet applied to live state (no accept/merge path this pass, per Part 4)" > "$RESULT_FILE"
else
    echo "$MODE for '$SEL': candidate edit REJECTED by validator - $VERDICT - logged to $d_path/pending_review.txt" > "$RESULT_FILE"
fi

sh "$PKG/ai_lab_scan.sh" "$HOUSE_ROOT" publish "$PKG/state/ui.txt" >/dev/null 2>&1 || true
exit 0
