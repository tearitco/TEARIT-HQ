#!/bin/bash
# chat.sh - v1 chat op. A real LAN brain chat turn, logged to the ledger.
# Usage: chat.sh "<msg>"
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

MSG="$*"
[ -z "$MSG" ] && MSG="hello"

ledger_append "Chat" "says: $MSG" "chat.sh"

reply="$(BRAIN_DIR="$BRAIN_DIR" bash -c '
    . "$BRAIN_DIR/oplib.sh"
    model="$(read_state brain_model)"
    url="$(read_state brain_url)"
    [ -z "$model" ] && model="llama3.2:3b"
    [ -z "$url" ] && url="http://10.0.0.187:11434/api/chat"
    body=$(mktemp)
    printf "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":" "$model" > "$body"
    python3 -c "import json,sys; print(json.dumps(sys.argv[1]))" "$MSG" >> "$body"
    printf "}]}" >> "$body"
    curl -sS --max-time 120 -H "Content-Type: application/json" -d @"$body" -o "$body.out" "$url" 2>/dev/null
    rc=$?
    rm -f "$body"
    if [ $rc -eq 0 ]; then python3 -c "import json; print(json.load(open(\"$body.out\")).get(\"message\",{}).get(\"content\",\"(no reply)\"))" 2>/dev/null; fi
    rm -f "$body.out"
')"

ledger_append "Chat" "brain replied: $reply" "chat.sh"
echo "Chat: $reply"
