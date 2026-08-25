#!/bin/bash
# oPlib.sh - shared helpers for the vvarware monad's feature ops.
# Every feature op sources this file: ledger_append() (the house
# master-ledger convention), read_state()/write_state() (key=value
# state.txt), and the big goal-list parser.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRAIN_DIR="$(cd "$SCRIPT_DIR" && pwd)"
MONAD_DIR="$(cd "$BRAIN_DIR/../.." && pwd)"
STATE="$BRAIN_DIR/state.txt"
LEDGER="$BRAIN_DIR/master_ledger.txt"
GOALS="$BRAIN_DIR/goal_queue.txt"
HOUSE_ROOT="$(cd "$MONAD_DIR/../.." && pwd)"

touch "$STATE" "$LEDGER" "$GOALS"

ledger_append() {
    local event_type="$1" details="$2"
    local ts
    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    printf '[%s] %s: %s | Trigger: %s\n' "$ts" "$event_type" "$details" "${3:-op}"
    printf '[%s] %s: %s | Trigger: %s\n' "$ts" "$event_type" "$details" "${3:-op}" >> "$LEDGER"
}

read_state() {
    local key="$1"
    grep "^${key}=" "$STATE" 2>/dev/null | head -1 | cut -d= -f2-
}

write_state() {
    local key="$1" value="$2"
    if grep -q "^${key}=" "$STATE" 2>/dev/null; then
        sed -i "s|^${key}=.*|${key}=${value}|" "$STATE"
    else
        echo "${key}=${value}" >> "$STATE"
    fi
}

next_goal() {
    local g
    g="$(grep -v '^#' "$GOALS" 2>/dev/null | head -1)"
    [ -n "$g" ] && echo "$g"
}

pop_goal() {
    local tmp
    tmp="$(mktemp)"
    grep -v '^#' "$GOALS" 2>/dev/null | tail -n +2 > "$tmp"
    grep '^#' "$GOALS" 2>/dev/null >> "$tmp"
    mv "$tmp" "$GOALS"
}

add_goal() {
    echo "$1" >> "$GOALS"
}

# brain_call - one llama3.2:3b chat round trip with the vvarware tools
# schema. Reads the system prompt from pieces/brain/system.txt, sends
# <goal> as the user message, writes the raw ollama reply to
# pieces/brain/llm_reply.json and prints it. Exit 0 on a JSON reply,
# 1 on network/model failure.
brain_call() {
    local goal="$1"
    local model url sysprompt
    model="$(read_state brain_model)"
    [ -z "$model" ] && model="llama3.2:3b"
    url="$(read_state brain_url)"
    [ -z "$url" ] && url="http://10.0.0.187:11434/api/chat"
    sysprompt="$(cat "$BRAIN_DIR/system.txt" 2>/dev/null)"

    local body_file
    body_file="$(mktemp)"
    {
        printf '{"model":"%s","stream":false,"messages":[' "$model"
        if [ -n "$sysprompt" ]; then
            printf '{"role":"system","content":'
            python3 -c "import json,sys; print(json.dumps(sys.argv[1]))" "$sysprompt"
            printf '}'
        fi
        printf ',{"role":"user","content":'
        python3 -c "import json,sys; print(json.dumps(sys.argv[1]))" "$goal"
        printf '}],'
        cat "$BRAIN_DIR/tools.json"
        printf '}'
    } > "$body_file"

    local out_file
    out_file="$(mktemp)"
    curl -sS --max-time 120 -H "Content-Type: application/json" \
        -d @"$body_file" -o "$out_file" "$url" 2>/dev/null
    local rc=$?
    rm -f "$body_file"
    if [ $rc -ne 0 ]; then
        rm -f "$out_file"
        ledger_append "BrainFail" "model $model unreachable (curl rc=$rc)" "brain_call"
        return 1
    fi
    cp "$out_file" "$BRAIN_DIR/llm_reply.json"
    rm -f "$out_file"
    return 0
}

# tool_name - extract the first function name from llm_reply.json
tool_name() {
    python3 - <<'EOF' 2>/dev/null
import json
try:
    d = json.load(open("llm_reply.json"))
except Exception:
    raise SystemExit(1)
calls = d.get("message", {}).get("tool_calls", [])
for c in calls:
    print(c.get("function", {}).get("name", ""))
    break
EOF
}

# tool_args - extract first tool call args as "k=v k2=v2"
tool_args() {
    python3 - <<'EOF' 2>/dev/null
import json
try:
    d = json.load(open("llm_reply.json"))
except Exception:
    raise SystemExit(1)
calls = d.get("message", {}).get("tool_calls", [])
if not calls:
    raise SystemExit(1)
a = calls[0].get("function", {}).get("arguments", {})
print(" ".join(f"{k}={v}" for k, v in a.items()))
EOF
}
