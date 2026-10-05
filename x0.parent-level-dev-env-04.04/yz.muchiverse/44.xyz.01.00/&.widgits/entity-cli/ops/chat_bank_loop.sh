#!/bin/sh
# chat_bank_loop.sh <entity_dir> <house_root>
#
# "Chat-bank" - the real start of ROBOT-CHAT-BLUEPRINT.md §4 build-order
# step 5, direct instruction 2026-09-29 ("i want to build the pipeline
# now"). Same OpenRouter round trip as chat_openrouter_loop.sh
# ("Chat-api"), PLUS a real ai_describe.+x call after every turn - each
# real conversation turn now genuinely feeds the TEARIT/Concept Bank
# pipeline's DESCRIBE step (17.ai/AI-BACKENDS-DISAMBIGUATION.md), not
# just a raw chat log nothing reads.
#
# Honest about what this still is NOT: ai_describe.+x only writes a
# candidate line to pending_review.txt. It does not validate, replay,
# or promote anything - those steps (concept_edit_validate.+x, the
# promotion ledger) are real, separate, still-manual/still-unbuilt
# pieces, same scope line every op in this pipeline draws for itself.
set -u
ENT="${1:?usage: chat_bank_loop.sh <entity_dir> <house_root>}"
HOUSE="${2:?usage: chat_bank_loop.sh <entity_dir> <house_root>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
CHAT_BIN="$HERE/+x/ai_chat_openrouter.+x"
DESCRIBE_BIN="$HERE/+x/ai_describe.+x"
NAME="$(basename "$ENT")"

echo "=== Chatting with $NAME (OpenRouter -> feeds ai_describe/Concept Bank pipeline) - type 'quit' to exit ==="
while true; do
    printf "you> "
    read -r line || break
    [ "$line" = "quit" ] && break
    [ -z "$line" ] && continue
    "$CHAT_BIN" "$ENT" "$HOUSE" "$line"
    out="$("$DESCRIBE_BIN" "$ENT" "$HOUSE" 2>&1)"
    echo "  [pipeline] $out"
done
echo "(chat closed)"
