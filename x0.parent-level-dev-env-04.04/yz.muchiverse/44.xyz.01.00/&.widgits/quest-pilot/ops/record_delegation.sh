#!/bin/sh
# record_delegation.sh - the delegation -> scoring dataflow, one command. Safe to re-run (everything is idempotent / replayed from the logs).
#   sh record_delegation.sh [house_root]      house_root defaults to the house this script sits in (no absolute path anywhere; HOUSE env also works)
# For each line of the WORKERS table below: quest ledger -> ledger_to_feedback (rows into delegation-bank/workers/<worker>/obs_feedback_log.txt)
# -> promotion_ledger add_candidate (once) + replay + score (grade = (reward+1)/(reward+punish+2), one candidate per worker x task family).
# Promotion stays a person's act: this only COUNTS. Add a line to WORKERS when a quest finishes.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)                      # .../&.widgits/quest-pilot/ops
HOUSE=${1:-${HOUSE:-$(cd "$HERE/../../.." && pwd)}}
QP="$HOUSE/&.widgits/quest-pilot"; BANK="$QP/delegation-bank"
L2F="$HERE/+x/ledger_to_feedback.+x"; PL="$HOUSE/^.hai-horn/ops/+x/promotion_ledger.+x"
[ -x "$L2F" ] || sh "$HERE/build_ledger_to_feedback.sh" >/dev/null
[ -x "$PL" ] || { echo "promotion_ledger.+x is not built ($PL)"; exit 2; }
# promotion_ledger "replay" ADDS to the candidate's counts (replaying one log twice double-counts), so the ledger is DERIVED data here:
# it is rebuilt from the append-only per-worker logs on every run, each candidate replayed exactly once. The logs are the source of truth.
rm -rf "$BANK/promotion_ledger"; mkdir -p "$BANK/candidates"; "$PL" "$BANK" init >/dev/null
# quest-dir (under quest-pilot/) or a ledger path relative to the house root (contains a /) | worker | task family
WORKERS="q001-clamp|nemotron-3-ultra|c_op
q002-weighted-pick|groq-gpt-oss-120b|c_op
q019-phrases|groq-gpt-oss-120b|data_rows
q020-var-cmp|groq-gpt-oss-120b|c_op
&.widgits/concept-bank/proposals/phrase_assoc/ledger.txt|groq-gpt-oss-120b|assoc_rows"
echo "$WORKERS" | while IFS='|' read -r Q W F; do                 # 1. ledgers -> per-worker FEEDBACK logs (idempotent by id)
  mkdir -p "$BANK/workers/$W"
  printf '%s %s/%s: ' "$Q" "$W" "$F"; case "$Q" in */*) LED="$HOUSE/$Q";; *) LED="$QP/$Q/quest_ledger.txt";; esac; "$L2F" "$LED" "$BANK/workers/$W" "$F"
done
echo "$WORKERS" | cut -d'|' -f2,3 | sort -u | while IFS='|' read -r W F; do   # 2. one candidate per worker x family, replayed once
  ID="g-$W-$F"; C="$BANK/candidates/$ID.txt"
  printf 'EDIT | id=%s | type=worker_family_grade | target=%s | slot=%s | delta=0.0 | reason="grade of this worker on this task family, counted from harness verdicts" | proposer=ledger | status=candidate\n' "$ID" "$W" "$F" > "$C"
  "$PL" "$BANK" add_candidate "$C" >/dev/null
  "$PL" "$BANK" replay "$ID" "$BANK/workers/$W/obs_feedback_log.txt" >/dev/null
done
echo "--- grades (Laplace (reward+1)/(reward+punish+2); one count per harness verdict)"; "$PL" "$BANK" list 0
