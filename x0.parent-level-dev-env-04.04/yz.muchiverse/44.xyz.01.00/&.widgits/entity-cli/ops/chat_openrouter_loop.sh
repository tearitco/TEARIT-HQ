#!/bin/sh
# chat_openrouter_loop.sh <entity_dir> <house_root>
#
# Interactive terminal loop for ai_chat_openrouter.+x - same real shape
# asa's own chat.sh uses for its Gemma chat (read line -> call the real
# op -> print reply -> repeat), generic across any entity instead of
# one pal's own bespoke script, since ai_chat_openrouter.+x itself
# already is entity-agnostic (argv: entity_dir house_root message).
#
# Real, deliberate scope match with ai_chat_openrouter.c's own header:
# this is the "Chat-bank" button's terminal surface for the SAME raw
# round trip ai_chat.c/"Chat-gemma" already has, OpenRouter backend
# instead of LAN Gemma - not the TEARIT/Concept Bank pipeline itself
# (see 17.ai/AI-BACKENDS-DISAMBIGUATION.md). "Chat-bank" names intent
# (OpenRouter is the documented backend capable of eventually driving
# that pipeline - PIPELINE-EMOJI-DIAGRAM-REVISED.md's 2026-09-29
# addendum), not a feature that exists yet.
set -u
ENT="${1:?usage: chat_openrouter_loop.sh <entity_dir> <house_root>}"
HOUSE="${2:?usage: chat_openrouter_loop.sh <entity_dir> <house_root>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/+x/ai_chat_openrouter.+x"
NAME="$(basename "$ENT")"

echo "=== Chatting with $NAME (OpenRouter, auto-cycling free models) - type 'quit' to exit ==="
while true; do
    printf "you> "
    read -r line || break
    [ "$line" = "quit" ] && break
    [ -z "$line" ] && continue
    "$BIN" "$ENT" "$HOUSE" "$line"
done
echo "(chat closed)"
