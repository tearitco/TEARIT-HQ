#!/bin/bash

# HORN_CHAT launcher - terminal OpenRouter chat with chtpm layout rendering
# Compiles binary if needed, runs main loop with terminal UI

set -e

HORN_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find house root - go up until we find 44.xyz.01.00 with &.widgits
HOUSE_ROOT=""
test_dir="$HORN_DIR"
for i in {1..10}; do
    if [ -d "$test_dir/&.widgits/entity-cli/ops" ]; then
        HOUSE_ROOT="$test_dir"
        break
    fi
    # Also check if we're directly at 44.xyz.01.00
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

# Build OpenRouter op if needed
HORN_BIN="$HORN_DIR/ops/horn_chat_openrouter.+x"
if [ ! -f "$HORN_BIN" ] || [ "$HORN_DIR/ops/horn_chat_openrouter.c" -nt "$HORN_BIN" ]; then
    echo "Building horn_chat_openrouter..."
    gcc -o "$HORN_BIN" "$HORN_DIR/ops/horn_chat_openrouter.c" 2>&1
    chmod +x "$HORN_BIN"
fi

# Setup session dir
mkdir -p "$HORN_DIR/.horn-sessions"
HISTORY="$HORN_DIR/.horn-sessions/chat_history.txt"
RELAY="$HORN_DIR/.horn-sessions/relay.txt"
STATE="$HORN_DIR/.horn-sessions/state.txt"

# Init files if not present
touch "$HISTORY" "$RELAY" "$STATE"

# Main loop
cd "$HORN_DIR"

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║       H O R N   C H A T   v 0 . 1       ║"
echo "║     OpenRouter models, terminal UI       ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Commands: type a prompt, press Enter"
echo "          type @ for completions"
echo "          type q or Ctrl+C to quit"
echo ""

while true; do
    printf "you: "
    read -r user_input || break

    # Exit on 'q' or empty
    if [ "$user_input" = "q" ] || [ "$user_input" = "quit" ]; then
        echo "Goodbye."
        break
    fi

    if [ -z "$user_input" ]; then
        continue
    fi

    # Handle @ completion (simple file listing)
    if [[ "$user_input" == "@"* ]]; then
        echo "  → Files:"
        ls -1 "$HORN_DIR" 2>/dev/null | head -8 | sed 's/^/     /'
        continue
    fi

    # Send to OpenRouter
    printf "horn: "
    if output=$(HORN_DIR="$HORN_DIR" "$HORN_BIN" "$HOUSE_ROOT" "$user_input" 2>&1); then
        echo "$output"
    else
        echo "[error - check API key or network]"
    fi
    echo ""
done

# vim:set et sw=4 ts=4:
