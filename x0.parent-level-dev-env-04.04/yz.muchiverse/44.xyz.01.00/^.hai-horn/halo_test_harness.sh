#!/bin/bash
# halo_test_harness.sh - automated test runner for HALO_CHAT/HORN_CHAT

set -euo pipefail

HAI_HORN_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find house root
HOUSE_ROOT=""
test_dir="$HAI_HORN_DIR"
for i in {1..10}; do
    if [ -d "$test_dir/&.widgits/entity-cli/ops" ]; then
        HOUSE_ROOT="$test_dir"
        break
    fi
    if [ -d "$test_dir/&.widgits" ] && [ "$(basename "$test_dir")" = "44.xyz.01.00" ]; then
        HOUSE_ROOT="$test_dir"
        break
    fi
    test_dir="$(dirname "$test_dir")"
done

if [ -z "$HOUSE_ROOT" ]; then
    echo "ERROR: Cannot find house root"
    exit 1
fi

# Build all binaries
build_bin() {
    local src="$1"
    local bin="$2"
    if [ ! -f "$bin" ] || [ "$src" -nt "$bin" ]; then
        echo "Building $(basename "$bin")..."
        gcc -o "$bin" "$src" 2>&1
        chmod +x "$bin"
    fi
}

echo "=== Building HALO_CHAT components ==="
build_bin "$HAI_HORN_DIR/ops/horn_chat_openrouter.c" "$HAI_HORN_DIR/ops/horn_chat_openrouter.+x"
build_bin "$HAI_HORN_DIR/ops/halo_chat_describe.c" "$HAI_HORN_DIR/ops/halo_chat_describe.+x"
build_bin "$HAI_HORN_DIR/ops/halo_chat_validate.c" "$HAI_HORN_DIR/ops/halo_chat_validate.+x"
build_bin "$HAI_HORN_DIR/ops/concept_bank_ctx.c" "$HAI_HORN_DIR/ops/concept_bank_ctx.+x"
build_bin "$HAI_HORN_DIR/ops/obs_feedback_write.c" "$HAI_HORN_DIR/ops/obs_feedback_write.+x"
build_bin "$HAI_HORN_DIR/ops/promotion_ledger.c" "$HAI_HORN_DIR/ops/promotion_ledger.+x"
build_bin "$HAI_HORN_DIR/ops/curricula_engine.c" "$HAI_HORN_DIR/ops/curricula_engine.+x"

# Ensure entity-cli ops are built
(cd "$HOUSE_ROOT/&.widgits/entity-cli/ops" && gcc -o connect_op.+x connect_op.c 2>&1)

echo "=== Build complete ==="
echo ""

# Test 1: HORN_CHAT basic round-trip
echo "Test 1: HORN_CHAT basic round-trip"
export HORN_DIR="$HAI_HORN_DIR"
output=$(HORN_DIR="$HAI_HORN_DIR" "$HAI_HORN_DIR/ops/horn_chat_openrouter.+x" "$HOUSE_ROOT" "Say RECEIVED" 2>&1)
if echo "$output" | grep -qi "RECEIVED"; then
    echo "✓ PASS: HORN_CHAT round-trip works"
else
    echo "✗ FAIL: HORN_CHAT did not return expected response"
    echo "Output: $output"
    exit 1
fi

# Test 2: Concept bank context injection
echo "Test 2: Concept bank context injection"
output=$(HORN_DIR="$HAI_HORN_DIR" "$HAI_HORN_DIR/ops/horn_chat_openrouter.+x" "$HOUSE_ROOT" "what is gravity" 2>&1)
if echo "$output" | grep -qi "gravity constant\|gravity_constant"; then
    echo "✓ PASS: Bank context injected into prompt"
else
    echo "✗ FAIL: Bank context not found in response"
    echo "Output: $output"
    exit 1
fi

# Test 3: DESCRIBE step produces candidate
echo "Test 3: DESCRIBE step"
# Add test chat history
mkdir -p "$HAI_HORN_DIR/.halo-sessions"
cat > "$HAI_HORN_DIR/.halo-sessions/chat_history.txt" << 'HISTORY_EOF'
[2026-10-06 10:00:00] USER: explain gravity
[2026-10-06 10:00:05] HALO: Gravity is a fundamental force...
HISTORY_EOF

desc_output=$("$HAI_HORN_DIR/ops/halo_chat_describe.+x" "$HAI_HORN_DIR" "$HOUSE_ROOT" 2>&1)
if echo "$desc_output" | grep -q "candidate written"; then
    echo "✓ PASS: DESCRIBE produces candidate"
else
    echo "✗ FAIL: DESCRIBE did not produce candidate"
    echo "Output: $desc_output"
    exit 1
fi

# Test 4: VALIDATE step promotes
echo "Test 4: VALIDATE step"
val_output=$("$HAI_HORN_DIR/ops/halo_chat_validate.+x" "$HAI_HORN_DIR" "$HOUSE_ROOT" 2>&1)
if echo "$val_output" | grep -q "PROMOTED"; then
    echo "✓ PASS: VALIDATE promotes candidate"
else
    echo "✗ FAIL: VALIDATE did not promote"
    echo "Output: $val_output"
    exit 1
fi

# Test 5: Promotion ledger
echo "Test 5: Promotion ledger"
BANK_DIR="$HOUSE_ROOT/&.widgits/concept-bank"
"$HAI_HORN_DIR/ops/promotion_ledger.+x" "$BANK_DIR" init >/dev/null 2>&1

# Add a test candidate
cat > "$HAI_HORN_DIR/test_ledger_candidate.txt" << 'CAND_EOF'
EDIT | id=harness-test-001 | type=spoke_weight_delta | target=gravity_constant | slot=force | delta=0.05 | reason="test" | proposer=halo_chat | status=candidate
CAND_EOF

"$HAI_HORN_DIR/ops/promotion_ledger.+x" "$BANK_DIR" add_candidate "$HAI_HORN_DIR/test_ledger_candidate.txt" >/dev/null 2>&1

# Add feedback
"$HAI_HORN_DIR/ops/obs_feedback_write.+x" "$HAI_HORN_DIR" FEEDBACK target="test" valence=+1 concept=force intensity=1.0 >/dev/null 2>&1
"$HAI_HORN_DIR/ops/obs_feedback_write.+x" "$HAI_HORN_DIR" FEEDBACK target="test" valence=+1 concept=force intensity=1.0 >/dev/null 2>&1

"$HAI_HORN_DIR/ops/promotion_ledger.+x" "$BANK_DIR" replay "harness-test-001" "$HAI_HORN_DIR/obs_feedback_log.txt" >/dev/null 2>&1

score=$("$HAI_HORN_DIR/ops/promotion_ledger.+x" "$BANK_DIR" score "harness-test-001" 2>&1)
score_val=$(echo "$score" | sed 's/ (.*//')
if (( $(echo "$score_val > 0.5" | bc -l) )); then
    echo "✓ PASS: Promotion ledger computes Laplace score"
else
    echo "✗ FAIL: Promotion ledger score unexpected: $score"
    exit 1
fi

# Test 6: Curricula engine
echo "Test 6: Curricula engine"
curricula=$("$HAI_HORN_DIR/ops/curricula_engine.+x" "$BANK_DIR" -1 text 2>&1)
if echo "$curricula" | grep -q "gravity_constant" && echo "$curricula" | grep -q "kinetic_energy"; then
    echo "✓ PASS: Curricula engine generates curriculum"
else
    echo "✗ FAIL: Curricula engine output unexpected"
    echo "Output: $curricula"
    exit 1
fi

# Test 7: Curricula JSON format
echo "Test 7: Curricula JSON output"
curricula_json=$("$HAI_HORN_DIR/ops/curricula_engine.+x" "$BANK_DIR" -1 json 2>&1)
if echo "$curricula_json" | grep -q '"spoke"' && echo "$curricula_json" | grep -q '"weight"'; then
    echo "✓ PASS: Curricula JSON format valid"
else
    echo "✗ FAIL: Curricula JSON invalid"
    exit 1
fi

# Test 8: Grade filtering
echo "Test 8: Grade filtering"
curricula_grade1=$("$HAI_HORN_DIR/ops/curricula_engine.+x" "$BANK_DIR" 1 text 2>&1)
if echo "$curricula_grade1" | grep -q "Elementary" && ! echo "$curricula_grade1" | grep -q "Preschool"; then
    echo "✓ PASS: Grade filtering works"
else
    echo "✗ FAIL: Grade filtering failed"
    echo "Output: $curricula_grade1"
    exit 1
fi

echo ""
echo "=== ALL TESTS PASSED ==="