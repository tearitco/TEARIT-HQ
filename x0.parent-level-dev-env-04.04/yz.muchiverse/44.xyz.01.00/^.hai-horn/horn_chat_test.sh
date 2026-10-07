#!/bin/bash
set -euo pipefail

HORN_DIR="$(cd "$(dirname "$0")" && pwd)"

HOUSE_ROOT=""
test_dir="$HORN_DIR"
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

BIN="$HORN_DIR/ops/horn_chat_openrouter.+x"
if [ ! -f "$BIN" ]; then
    echo "ERROR: $BIN missing"
    exit 1
fi

mkdir -p "$HORN_DIR/.horn-sessions"
REPORT="$HORN_DIR/.horn-sessions/test_report.txt"

echo "HORN_CHAT Test Report - $(date -u '+%Y-%m-%d %H:%M:%S UTC')" > "$REPORT"
echo "==================================================" >> "$REPORT"
echo "Binary: $BIN" >> "$REPORT"
echo "House root: $HOUSE_ROOT" >> "$REPORT"

PROMPT="Say the single word RECEIVED if you can read this."
echo "Prompt: $PROMPT" >> "$REPORT"
echo "" >> "$REPORT"

if output=$("$BIN" "$HOUSE_ROOT" "$PROMPT" 2>&1); then
    echo "Reply: $output" >> "$REPORT"
    if echo "$output" | grep -qi "RECEIVED"; then
        echo "Result: PASS" >> "$REPORT"
        echo "PASS"
    else
        echo "Result: FAIL (reply did not contain RECEIVED)" >> "$REPORT"
        echo "FAIL"
        exit 1
    fi
else
    echo "Result: FAIL (binary exited non-zero)" >> "$REPORT"
    echo "FAIL"
    exit 1
fi
