#!/bin/bash
# tool_chat.sh - tool-using chat op. Talks to the tooled llama path and
# logs the exchange to the master ledger.
# Usage: tool_chat.sh "<goal or message>"
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

MSG="$*"
[ -z "$MSG" ] && MSG="use a tool to help me"

ledger_append "ToolChat" "says: $MSG" "tool_chat.sh"

if brain_call "$MSG"; then
    reply="$(cat "$BRAIN_DIR/llm_reply.json" 2>/dev/null)"
    ledger_append "ToolChat" "raw reply captured" "tool_chat.sh"
    echo "ToolChat: reply written to $BRAIN_DIR/llm_reply.json"
else
    ledger_append "ToolChat" "brain call failed" "tool_chat.sh"
    echo "ToolChat: brain unavailable"
    exit 1
fi
