#!/bin/bash
# chat.sh - real chat with ava, via 047.scm (the trainable "select, don't
# generate" text system - see 047.scm🎓️+1/!.SCM-DESIGN.md). Uses its own
# real, verified CLI primitive directly: `scm_cli select <curriculum> <msg>`
# (see 047.scm🎓️+1/prog-rep-au2.txt - "steps 1+2 DONE and verified").
#
# Direct instruction: pulls a compacted summary of ava's own last events
# + last conversations from its own ledger for context (same real ledger
# convention asa's own chat.sh uses), and this chat session itself is
# treated as an "event" - every turn appends to the SAME real
# master_ledger.txt. REAL FIX 2026-08-04, direct instruction ("all
# things should go like in a master ledger"): uses this house's own
# real master-ledger format (see asa's own chat.sh for the same fix +
# citation), not an invented pipe-delimited one.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
SCM_CLI="$HOUSE_ROOT/047.scm🎓️+1/+x/scm_cli"
SCM_CURRICULUM="$HOUSE_ROOT/047.scm🎓️+1/corpuses/small-talk"
LEDGER="$SCRIPT_DIR/master_ledger.txt"

touch "$LEDGER"

ledger_append() {
    local event_type="$1" details="$2"
    local ts
    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    printf '[%s] %s: %s | Trigger: chat.sh\n' "$ts" "$event_type" "$details" >> "$LEDGER"
}

if [ ! -x "$SCM_CLI" ]; then
    echo "MISSING: $SCM_CLI (build 047.scm's own scm_cli first)"
    exit 1
fi

echo "=== Chatting with ava (047.scm, curriculum: small-talk) - type 'quit' to exit ==="
echo "(ava is trainable - her replies are selected from a real, learnable phrase bank, not generated)"
while true; do
    printf "you> "
    read -r line || break
    [ "$line" = "quit" ] && break
    [ -z "$line" ] && continue
    ledger_append "Chat" "user said \"$line\""
    reply="$("$SCM_CLI" select "$SCM_CURRICULUM" "$line" 2>/dev/null)"
    [ -z "$reply" ] && reply="(...)"
    echo "ava> $reply"
    ledger_append "Chat" "ava replied \"$reply\""
done
echo "(chat closed)"
