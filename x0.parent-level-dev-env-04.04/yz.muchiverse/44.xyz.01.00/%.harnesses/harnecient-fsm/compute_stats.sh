#!/bin/bash
# compute_stats.sh — real usage/savings dashboard for Harnecient
# delegation, direct instruction 2026-08-13: "record whenever
# 'harnicient' was used to explain to user how many tokens were
# saved and the $ amount... estimates is fine and u should fill in
# the values from today."
#
# What this actually does, real not aspirational: reads the SAME
# observations.log run_plan.sh already writes on every real delegated
# call (no new recording mechanism invented - see
# au11-hq/HARNESS-DELEGATION-PIPELINE.md §7.2's Stage 0 log), counts
# real model-invoking rows (NAVIGATE/STEP/STEP-RETRY/CHOOSE_MODEL -
# SET_MODEL/INCLUDE/BRANCH/LOOP_UNTIL are control-flow, they don't
# themselves call a model), and estimates what that work would have
# cost if Claude had generated it directly instead of delegating.
#
# METHODOLOGY, stated plainly (this is an ESTIMATE, not a measurement):
#   - per-call output-token estimate: 300 tokens (matches this
#     session's real observed shape - short single-line answers,
#     small scripts, short classifications - NOT large generations)
#   - Claude Sonnet API pricing used as the comparison baseline:
#     $15 / 1M output tokens (public Anthropic pricing, 2026-08).
#     Input-token cost is treated as a wash (Claude would need to
#     read/verify a self-authored answer either way) - deliberately
#     NOT counted, to keep this a conservative estimate, not an
#     inflated one.
#   - Real, unavoidable caveat: this counts LOCAL MODEL calls that
#     replaced would-be Claude generation. It does NOT net out the
#     Claude tokens spent BUILDING/ORCHESTRATING the delegation
#     pipeline itself (writing run_plan.sh, debugging it, etc.) -
#     that's real, separate cost, not what this number claims to
#     measure. This number answers "if the SAME 51 short tasks had
#     been generated directly by Claude instead of delegated, what
#     would that specific generation have cost" - not "was building
#     this pipeline worth it overall."
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$SCRIPT_DIR/observations.log"
OUT="$SCRIPT_DIR/stats_summary.txt"

EST_TOKENS_PER_CALL=300
CLAUDE_OUTPUT_PRICE_PER_M=15.00  # USD per 1M output tokens, Sonnet-class
# REAL ADDITION 2026-08-13 (direct request: "estimate how many claude
# tokens were used during same calls to harnecient... general
# estimate... general ratio"). Same estimate-style methodology as
# EST_TOKENS_PER_CALL above, just for the OTHER side of the same
# calls: Claude's own orchestration overhead per delegated call (short
# relay dispatch + brief result-check), not the full-generation cost
# that call would have cost if Claude had done the WORK itself
# (that's what EST_TOKENS_PER_CALL already measures). Deliberately
# much smaller than EST_TOKENS_PER_CALL - orchestrating one call is a
# few short lines, not the equivalent of writing the output content.
EST_CLAUDE_ORCHESTRATION_TOKENS_PER_CALL=60

if [ ! -f "$LOG" ]; then
    echo "no observations.log yet - nothing delegated" > "$OUT"
    cat "$OUT"
    exit 0
fi

total_rows=$(wc -l < "$LOG")
# REAL FIX 2026-08-13 (direct live report: "sais 38 pass 41 failed 51
# delegated. that math doesn't make sense"): passes/fails used to be
# counted across the WHOLE log (every row type - BRANCH/INCLUDE/
# LOOP_UNTIL/SET_MODEL included), not scoped to the same
# NAVIGATE|STEP|STEP-RETRY|CHOOSE_MODEL row types model_calls itself
# uses - so passes+fails never reconciled against model_calls. `fails`
# also used a bare unanchored `FAIL` substring match against the WHOLE
# LINE, which false-matched 3 real PASS rows whose PROMPT TEXT
# happened to contain the word "FAIL" (asking the model itself to
# "print PASS or FAIL") even though their actual verdict was PASS.
# Fixed: both now scope to the exact same row-type prefix as
# model_calls, and match the verdict field only (anchored at end of
# line), so passes+fails == model_calls always, and prompt text can
# never be mistaken for a verdict.
call_rows_re='^\S+ \| (NAVIGATE|STEP|STEP-RETRY|CHOOSE_MODEL) \|'
model_calls=$(grep -cE "$call_rows_re" "$LOG" || true)
passes=$(grep -E "$call_rows_re" "$LOG" | grep -c '| PASS$' || true)
fails=$(grep -E "$call_rows_re" "$LOG" | grep -cE '\| FAIL[A-Z-]*$' || true)

est_tokens=$((model_calls * EST_TOKENS_PER_CALL))
est_dollars=$(awk -v t="$est_tokens" -v p="$CLAUDE_OUTPUT_PRICE_PER_M" 'BEGIN { printf "%.4f", (t/1000000)*p }')

# REAL ADDITION 2026-08-13 (direct request, general-estimate ratio of
# Claude-orchestration-tokens vs delegated-work-tokens for these same
# calls - see EST_CLAUDE_ORCHESTRATION_TOKENS_PER_CALL's own comment
# above for why this constant is much smaller than EST_TOKENS_PER_CALL).
est_claude_orch_tokens=$((model_calls * EST_CLAUDE_ORCHESTRATION_TOKENS_PER_CALL))
if [ "$est_claude_orch_tokens" -gt 0 ]; then
    delegation_ratio=$(awk -v s="$est_tokens" -v c="$est_claude_orch_tokens" 'BEGIN { printf "%.1f", s/c }')
else
    delegation_ratio="0.0"
fi
# REAL ADDITION 2026-08-13 (direct follow-up: "i see 5x but what is
# that? 500%? dont make them do [the] math ... i want it to show
# percent saved"). Same relationship as delegation_ratio, just shown
# as a direct percent so nobody has to convert 5.0x -> 80% themselves.
# Framing: cost WITHOUT delegation = est_tokens (Claude generates the
# work itself, no orchestration needed since there's nothing to
# dispatch). Cost WITH delegation = est_claude_orch_tokens (Claude
# only pays the dispatch+check overhead; the local model bears the
# generation cost, off Claude's own ledger). percent_saved is the
# reduction from the first to the second - mathematically the same
# relationship as delegation_ratio (1/5.0x = 20% remaining = 80%
# saved), not a second, independently-derived estimate.
if [ "$est_tokens" -gt 0 ]; then
    percent_saved=$(awk -v s="$est_tokens" -v c="$est_claude_orch_tokens" 'BEGIN { printf "%.1f", ((s-c)/s)*100 }')
else
    percent_saved="0.0"
fi
if [ "$total_rows" -gt 0 ]; then
    delegation_rate=$(awk -v m="$model_calls" -v t="$total_rows" 'BEGIN { printf "%.1f", (m/t)*100 }')
else
    delegation_rate="0.0"
fi
if [ "$model_calls" -gt 0 ]; then
    success_rate=$(awk -v p="$passes" -v m="$model_calls" 'BEGIN { printf "%.1f", (p/m)*100 }')
else
    success_rate="0.0"
fi

{
    echo "Harnecient Delegation Stats (estimate)"
    echo "======================================="
    echo ""
    echo "Source: $LOG"
    echo "Generated: $(date)"
    echo ""
    echo "Total log rows (all kinds):     $total_rows"
    echo "Real model calls delegated:     $model_calls"
    echo "  (NAVIGATE/STEP/STEP-RETRY/CHOOSE_MODEL - each is one real"
    echo "   local Ollama call that did NOT go through Claude's API)"
    echo "  PASS:                         $passes"
    echo "  FAIL (any kind):              $fails"
    echo ""
    echo "Delegation rate:                 ${delegation_rate}%"
    echo "  ($model_calls of $total_rows total plan actions were routed to a"
    echo "   local model instead of Claude - the rest were Claude-side"
    echo "   control-flow: BRANCH/INCLUDE/LOOP_UNTIL/SET_MODEL)"
    echo ""
    echo "Success rate (of delegated calls): ${success_rate}%"
    echo "  ($passes of $model_calls delegated calls passed without falling"
    echo "   back to Claude)"
    echo ""
    echo "Estimated tokens saved:         ~$est_tokens output tokens"
    echo "  (assumes ~$EST_TOKENS_PER_CALL output tokens per call - matches"
    echo "   this session's real observed shape: short answers, small"
    echo "   scripts, short classifications, not large generations)"
    echo ""
    echo "Estimated Claude tokens for same calls (orchestration only): ~$est_claude_orch_tokens"
    echo "  (assumes ~$EST_CLAUDE_ORCHESTRATION_TOKENS_PER_CALL output tokens per call -"
    echo "   Claude's own overhead to DISPATCH+CHECK one delegated call,"
    echo "   short relay commands, not the full generation - general"
    echo "   estimate, not a measurement)"
    echo ""
    echo "Delegation ratio:               ${delegation_ratio}x  (${percent_saved}% tokens saved)"
    echo "  (for every 1 output token Claude spent orchestrating,"
    echo "   ~${delegation_ratio}x that many were saved by delegating the"
    echo "   actual work to a local model instead - i.e. ${percent_saved}%"
    echo "   lower Claude-side token cost than generating it directly)"
    echo ""
    echo "Estimated \$ saved (Claude API): ~\$$est_dollars"
    echo "  (at \$$CLAUDE_OUTPUT_PRICE_PER_M / 1M output tokens, Sonnet-class"
    echo "   pricing - input-token cost treated as a wash, not counted,"
    echo "   to keep this conservative)"
    echo ""
    echo "CAVEAT: this measures the SAME short tasks generated locally"
    echo "instead of by Claude - it does not net out the Claude tokens"
    echo "spent building/debugging the delegation pipeline itself."
    echo "Both token estimates above are general estimates, not"
    echo "precise measurements of either side's real usage."
} > "$OUT"

cat "$OUT"
