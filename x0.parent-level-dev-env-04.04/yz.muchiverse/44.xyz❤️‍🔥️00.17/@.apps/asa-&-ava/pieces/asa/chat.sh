#!/bin/bash
# chat.sh - real chat with asa, via gemma-lan.
# Usage: chat.sh <package_dir>  (package_dir passed by tp_desktop_window.c's
# real fo-menu-sys.md-style METHOD dispatch - see methods.pdl's own
# "Chat | .../chat.sh" row, and &.widgits/tile-picker/ops/tp_desktop_window.c
# for the dispatch convention itself).
#
# Real request shape ported directly from 045.muchi-pal-agent's own
# send_message.c build_gemma_request() (provider_kind="gemma", confirmed
# real endpoint http://10.0.0.144:11434, model gemma3:270m) - not
# reinvented, same real JSON shape, same LAN host.
#
# Direct instruction: pulls a compacted summary of asa's own last events
# + last conversations from its own ledger for context, and treats this
# chat session itself as an "event" - every real turn gets appended to
# the SAME real master_ledger.txt (asa's own event log). REAL FIX
# 2026-08-04, direct instruction ("all things should go like in a
# master ledger, that's always the intention for master ledger"): this
# used to be a bespoke "ledger.txt" with an invented pipe-delimited
# format - replaced with this house's own REAL, already-established
# master-ledger convention/format instead (same one egg_window.c's own
# append_window_ledger() and mutaclysm's pieces/system/master_ledger.txt
# already use): "[timestamp] EventType: details | Trigger: source".
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NAME="asa"
GEMMA_URL="http://10.0.0.144:11434/api/chat"
GEMMA_MODEL="gemma3:270m"
LEDGER="$SCRIPT_DIR/master_ledger.txt"
PERSONALITY="$SCRIPT_DIR/personality.txt"

touch "$LEDGER"

ledger_append() {
    local event_type="$1" details="$2"
    local ts
    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    printf '[%s] %s: %s | Trigger: chat.sh\n' "$ts" "$event_type" "$details" >> "$LEDGER"
}

# Real, compacted context: last 12 ledger lines (events + past chat turns,
# same real master_ledger.txt file - "compacted via their own ledger" per
# direct instruction).
context_summary() {
    tail -n 12 "$LEDGER"
}

call_gemma() {
    local user_msg="$1"
    local persona
    persona="$(cat "$PERSONALITY" 2>/dev/null)"
    local ctx
    ctx="$(context_summary)"
    python3 - "$GEMMA_MODEL" "$persona" "$ctx" "$user_msg" <<'PYEOF' > /tmp/asa_chat_req.json
import json, sys
model, persona, ctx, user_msg = sys.argv[1:5]
system_text = persona
if ctx.strip():
    system_text += "\n\nRecent events/conversation (for context, most recent last):\n" + ctx
req = {
    "model": model,
    "stream": False,
    "messages": [
        {"role": "system", "content": system_text},
        {"role": "user", "content": user_msg},
    ],
}
print(json.dumps(req))
PYEOF
    curl -sS --max-time 60 -H "Content-Type: application/json" "$GEMMA_URL" -d @/tmp/asa_chat_req.json -o /tmp/asa_chat_resp.json
    python3 -c '
import json
try:
    with open("/tmp/asa_chat_resp.json") as f:
        d = json.load(f)
    print(d.get("message", {}).get("content", "(no reply)"))
except Exception as e:
    print(f"(asa chat error: {e})")
'
}

echo "=== Chatting with asa (gemma-lan, $GEMMA_MODEL) - type 'quit' to exit ==="
while true; do
    printf "you> "
    read -r line || break
    [ "$line" = "quit" ] && break
    [ -z "$line" ] && continue
    ledger_append "Chat" "user said \"$line\""
    reply="$(call_gemma "$line")"
    echo "asa> $reply"
    ledger_append "Chat" "asa replied \"$reply\""
done
echo "(chat closed)"
