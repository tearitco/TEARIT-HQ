#!/bin/bash

# HALO_CHAT launcher - terminal OpenRouter chat with Concept Bank pipeline
# Inherits HORN_CHAT features + runs DESCRIBE + validate after each turn

set -e

HALO_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find house root
HOUSE_ROOT=""
test_dir="$HALO_DIR"
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

# Binaries: scripts/build.sh compiles every ops/*.c into ops/+x/ (HALO's ops included). The transport is the multi-provider
# horn_chat_backend (it replaced the old single-provider horn_chat_openrouter, ported 2026-10-06, Q001).
OPSX="$HALO_DIR/ops/+x"
HALO_DESCRIBE_BIN="$OPSX/halo_chat_describe.+x"
HALO_VALIDATE_BIN="$OPSX/halo_chat_validate.+x"
HALO_CTX_BIN="$OPSX/concept_bank_ctx.+x"
HORN_BIN="$OPSX/horn_chat_backend.+x"

for bin in "$HALO_DESCRIBE_BIN" "$HALO_VALIDATE_BIN" "$HALO_CTX_BIN" "$HORN_BIN"; do
    if [ ! -x "$bin" ]; then
        echo "Building ^.hai-horn (missing $(basename "$bin"))..."
        sh "$HALO_DIR/scripts/build.sh" >/dev/null 2>&1 || { echo "ERROR: scripts/build.sh failed"; exit 1; }
        break
    fi
done

# HALO_CHAT uses the same entity dir concept - we'll use HALO_DIR as the entity
ENTITY_DIR="$HALO_DIR"

# Setup session dir
mkdir -p "$HALO_DIR/.halo-sessions"
HISTORY="$HALO_DIR/.halo-sessions/chat_history.txt"
RELAY="$HALO_DIR/.halo-sessions/relay.txt"
STATE="$HALO_DIR/.halo-sessions/state.txt"
PENDING_REVIEW="$ENTITY_DIR/pending_review.txt"

# Init files
touch "$HISTORY" "$RELAY" "$STATE" "$PENDING_REVIEW"

# Main loop
cd "$HALO_DIR"

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║       H A L O   C H A T   v 0 . 1       ║"
echo "║   OpenRouter + Concept Bank pipeline    ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Commands: type a prompt, press Enter"
echo "          type @ for completions"
echo "          type q or Ctrl+C to quit"
echo ""

turn=0

while true; do
    printf "you: "
    read -r user_input || break

    if [ "$user_input" = "q" ] || [ "$user_input" = "quit" ]; then
        echo "Goodbye."
        break
    fi

    if [ -z "$user_input" ]; then
        continue
    fi

    if [[ "$user_input" == "@"* ]]; then
        echo "  → Files:"
        ls -1 "$HALO_DIR" 2>/dev/null | head -8 | sed 's/^/     /'
        continue
    fi

    # Send through HORN's provider chain (same transport as HORN_CHAT)
    printf "halo: "
    # Concept Bank context (top-weighted spokes) goes in front of the user turn; empty or "[]" means no context
    bank_ctx="$("$HALO_CTX_BIN" "$HOUSE_ROOT" 2>/dev/null || true)"
    if [ -n "$bank_ctx" ] && [ "$bank_ctx" != "[]" ]; then
        prompt="CONCEPT BANK CONTEXT (highest-weighted relations):
$bank_ctx

USER: $user_input"
    else
        prompt="USER: $user_input"
    fi
    if output=$("$HORN_BIN" "$prompt" 2>/dev/null); then
        echo "$output"
    else
        echo "[error - check API key or network]"
    fi
    echo ""

    # Run DESCRIBE step
    turn=$((turn + 1))
    printf "  [pipeline turn %d] DESCRIBE... " "$turn"
    if desc_output=$("$HALO_DESCRIBE_BIN" "$ENTITY_DIR" "$HOUSE_ROOT" 2>&1); then
        echo "$desc_output"
    else
        echo "[describe failed]"
    fi

    # Run validation step
    printf "  [pipeline turn %d] VALIDATE... " "$turn"
    if val_output=$("$HALO_VALIDATE_BIN" "$ENTITY_DIR" "$HOUSE_ROOT" 2>&1); then
        echo "$val_output"
    else
        echo "[validate failed or no candidate]"
    fi

    echo ""
done

# vim:set et sw=4 ts=4: