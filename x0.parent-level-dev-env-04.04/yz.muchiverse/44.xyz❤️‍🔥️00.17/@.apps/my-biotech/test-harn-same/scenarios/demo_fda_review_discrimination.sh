#!/bin/bash
# demo_fda_review_discrimination.sh - does the FDA_REVIEW gemma call
# ACTUALLY discriminate between an obviously-dangerous dossier and an
# obviously-safe one, or does it just default toward one answer
# regardless of content? Direct user question, 2026-08-02: "so let me
# ask bluntly, our gemma tools have been verified to let gemma read
# from documents and write to them?"
#
# Two ad-hoc manual tests immediately before this harness was written
# (raw curl, not through this harness) found a CONCERNING result: an
# obviously lethal compound ("100% cardiac arrest... banned as a
# chemical weapon") got APPROVED, and an obviously safe one (Vitamin C)
# ALSO got APPROVED - suggesting gemma3:270m may be strongly biased
# toward APPROVED regardless of content with this prompt shape. This
# harness runs MULTIPLE trials of each case (not just N=1 each) through
# the REAL production code path (ops/mybiotech_fda_verdict.+x - the
# exact same prompt construction as mybiotech_research_worker.c's own
# FDA_REVIEW step, kept in sync deliberately) to get real statistical
# signal instead of two anecdotal samples, and reports the finding
# HONESTLY either way - this is a real capability/limitation question
# worth surfacing clearly, not something to spin as a pass.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"
FDA_OP="$PROJECT_DIR/ops/+x/mybiotech_fda_verdict.+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

echo "=== my-biotech FDA_REVIEW discrimination test (real gemma3:270m calls) ==="

if ! curl -s --max-time 5 http://10.0.0.144:11434/api/tags | grep -q "gemma3:270m"; then
    fail "gemma-lan endpoint NOT reachable - cannot run this test"
    exit 1
fi
pass "gemma-lan endpoint reachable"

if [ ! -x "$FDA_OP" ]; then
    echo "mybiotech_fda_verdict.+x not built - compiling..."
    (cd "$PROJECT_DIR" && bash scripts/build.sh > /dev/null 2>&1)
fi
if [ ! -x "$FDA_OP" ]; then
    fail "ops/+x/mybiotech_fda_verdict.+x still missing after build - cannot run this test"
    exit 1
fi

BAD_DOSSIER="$PROOF_DIR/bad_dossier.txt"
GOOD_DOSSIER="$PROOF_DIR/good_dossier.txt"

cat > "$BAD_DOSSIER" << 'EOF'
COMPOUND: Deathium
DISCOVERED FROM: arsenic

[Use Case] No known use case.
[Effect] Causes immediate cardiac arrest in 100% of test subjects within seconds of exposure.
[Side Effect] Extremely lethal, no known antidote, banned as a chemical weapon in every country.
[Market Price] Unknown.
EOF

cat > "$GOOD_DOSSIER" << 'EOF'
COMPOUND: Vitamin C
DISCOVERED FROM: carbon

[Use Case] Used as a dietary supplement and antioxidant.
[Effect] Supports immune function and collagen production.
[Side Effect] Generally safe, rare mild stomach upset at high doses.
[Market Price] $5 per unit.
EOF

TRIALS="${FDA_TRIALS:-3}"
echo "--- running $TRIALS trial(s) each (set FDA_TRIALS=N to change) ---"

BAD_APPROVED=0
BAD_REJECTED=0
echo "--- BAD dossier trials (obviously lethal - a correct judge should REJECT) ---"
echo "--- now WITH gemma's own stated explanation (diagnostic value) ---"
for i in $(seq 1 "$TRIALS"); do
    VERDICT=$("$FDA_OP" "$PROJECT_DIR" "$BAD_DOSSIER" 2>/dev/null)
    echo "  trial $i: $VERDICT"
    case "$VERDICT" in
        APPROVED:*) BAD_APPROVED=$((BAD_APPROVED+1)) ;;
        *) BAD_REJECTED=$((BAD_REJECTED+1)) ;;
    esac
done

GOOD_APPROVED=0
GOOD_REJECTED=0
echo "--- GOOD dossier trials (obviously safe - a correct judge should APPROVE) ---"
echo "--- now WITH gemma's own stated explanation (diagnostic value) ---"
for i in $(seq 1 "$TRIALS"); do
    VERDICT=$("$FDA_OP" "$PROJECT_DIR" "$GOOD_DOSSIER" 2>/dev/null)
    echo "  trial $i: $VERDICT"
    case "$VERDICT" in
        APPROVED:*) GOOD_APPROVED=$((GOOD_APPROVED+1)) ;;
        *) GOOD_REJECTED=$((GOOD_REJECTED+1)) ;;
    esac
done

{
    echo "BAD dossier ($TRIALS trials): $BAD_APPROVED APPROVED, $BAD_REJECTED REJECTED"
    echo "GOOD dossier ($TRIALS trials): $GOOD_APPROVED APPROVED, $GOOD_REJECTED REJECTED"
} | tee "$PROOF_DIR/results_summary.txt"

echo
echo "=== VERDICT ON THE REAL QUESTION: does FDA_REVIEW discriminate on content? ==="
if [ "$BAD_REJECTED" -eq "$TRIALS" ] && [ "$GOOD_APPROVED" -eq "$TRIALS" ]; then
    pass "PERFECT discrimination: bad dossier always REJECTED, good dossier always APPROVED"
elif [ "$BAD_REJECTED" -gt 0 ] || [ "$GOOD_APPROVED" -lt "$TRIALS" ]; then
    fail "PARTIAL discrimination: results are mixed, not a hard bias but not reliable either (see results_summary.txt)"
else
    fail "NO DISCRIMINATION DETECTED: gemma3:270m appears biased toward APPROVED regardless of dossier content with this prompt shape. This is a REAL, honest finding - the FDA_REVIEW mechanic as currently prompted may not provide meaningful signal. Options worth considering (not decided): (1) switch this specific call to gemma-lan-1b (pulled and registered this session, may discriminate better), (2) redesign the prompt (few-shot examples, more constrained framing), (3) supplement/replace with a deterministic keyword-scoring heuristic (e.g. flag dossiers containing 'lethal'/'banned'/'no antidote' as REJECTED regardless of LLM verdict) rather than trusting the raw LLM judgment alone."
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL (see above - may be a real model-capability finding, not a code bug) ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
