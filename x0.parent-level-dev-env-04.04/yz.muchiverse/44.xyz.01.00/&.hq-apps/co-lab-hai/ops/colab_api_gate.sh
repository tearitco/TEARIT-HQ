#!/bin/bash
# colab_api_gate.sh <prompt> - a drop-in BACKEND for ghost_run (same call as horn_chat_backend: prompt = argv[1], reply on stdout, provider lines on stderr, same exit codes)
# that loops every API call through co-lab-hai so the owner SEES it and APPROVES it:
#   1. the prompt is posted into the room as  claude -> @groq-worker  "[gate-<id>] ..."  (it waits in the pending queue: the owner approves or rejects it in the window);
#   2. only after it shows up approved in the room's conversation does the real backend get called (rejected / timeout = no API call, exit 3);
#   3. the API's reply is posted back into the room as  groq-worker -> @claude  "[gate-<id> reply] ..." (also pending, so the owner can read it) and printed on stdout for ghost_run.
# The full prompt and reply are kept in #.desktop/colab_hai/gate/<id>.{prompt,reply}.txt (the room line is cut to ~1800 chars). Never prints or logs a key.
# Env: COLAB_HOUSE (default: the house this script lives in), COLAB_GATE_BACKEND (default ^.hai-horn/ops/+x/horn_chat_backend.+x), COLAB_GATE_TIMEOUT s (default 900),
#      COLAB_GATE_AUTO=1 (post but do not wait for approval: for runs while the owner is away; the room still shows everything), COLAB_GATE_AGENT (default groq-worker).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; HOUSE="${COLAB_HOUSE:-$(cd "$HERE/../.." && pwd)}"; PROMPT="${1:-}"; [ -n "$PROMPT" ] || { echo "colab_api_gate: no prompt" >&2; exit 2; }
BACK="${COLAB_GATE_BACKEND:-$HOUSE/^.hai-horn/ops/+x/horn_chat_backend.+x}"; AGENT="${COLAB_GATE_AGENT:-groq-worker}"; TMO="${COLAB_GATE_TIMEOUT:-900}"
CH="$HOUSE/#.desktop/colab_hai"; GD="$CH/gate"; mkdir -p "$GD"; ID="$(date +%s)-$$"; printf '%s' "$PROMPT" > "$GD/$ID.prompt.txt"
POST="$HERE/ops/colab_hai_post.sh"; short() { printf '%s' "$1" | tr '\n\r\t|' '    ' | tr -s ' ' | cut -c1-1800; }
n=$(printf '%s' "$PROMPT" | wc -c)
bash "$POST" "$HOUSE" claude "@$AGENT [gate-$ID] API call for review ($n chars; full text: #.desktop/colab_hai/gate/$ID.prompt.txt): $(short "$PROMPT")" >/dev/null 2>&1
if [ "${COLAB_GATE_AUTO:-0}" != 1 ]; then
    echo "colab_api_gate: waiting for the owner to approve [gate-$ID] in co-lab-hai (open it: bash $HERE/button.sh $HOUSE)" >&2
    t0=$(date +%s); ok=0
    while :; do
        S=$(cat "$CH/current_session.txt" 2>/dev/null)
        if [ -n "$S" ] && grep -qF "[gate-$ID]" "$CH/sessions/$S/conversation.txt" 2>/dev/null; then ok=1; break; fi
        if [ -n "$S" ] && grep -qF "[gate-$ID]" "$CH/sessions/$S/rejected.txt" 2>/dev/null; then echo "colab_api_gate: the owner rejected [gate-$ID]; no API call made" >&2; exit 3; fi
        [ $(( $(date +%s) - t0 )) -ge "$TMO" ] && { echo "colab_api_gate: no approval within ${TMO}s; no API call made" >&2; exit 3; }
        sleep 2
    done
fi
REPLY="$("$BACK" "$PROMPT")"; rc=$?
printf '%s\n' "$REPLY" > "$GD/$ID.reply.txt"
if [ "$rc" = 0 ]; then bash "$POST" "$HOUSE" "$AGENT" "@claude [gate-$ID reply] $(short "$REPLY") (full: #.desktop/colab_hai/gate/$ID.reply.txt)" >/dev/null 2>&1
else bash "$POST" "$HOUSE" "$AGENT" "@claude [gate-$ID reply] the API call failed (exit $rc); see the quest attempt log" >/dev/null 2>&1; fi
printf '%s\n' "$REPLY"; exit "$rc"
