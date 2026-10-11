#!/bin/sh
# pet_teach_backend.sh <provider> <prompt> - ask ONE provider of the ladder (ladder.pdl) and print its reply on stdout. Exit: 0 ok, 3 the owner rejected the call in co-lab, other = the call failed.
# mac_gemma: the Mac's Ollama (gemma_lan_url / gemma_lan_model in #.desktop/ai_backend.pdl). groq/poolside/openrouter: ^.hai-horn's horn_chat_backend pinned to that provider, via the co-lab approval gate (PET_TEACH_GATE=0 = direct). Never prints a key.
HERE="$(cd "$(dirname "$0")/.." && pwd)"; HOUSE="$(cd "$HERE/../.." && pwd)"; P="$1"; Q="$2"
[ -n "$P" ] && [ -n "$Q" ] || { echo "usage: pet_teach_backend.sh <provider> <prompt>" >&2; exit 2; }
case "$P" in
  mac_gemma)
    CFG="$HOUSE/#.desktop/ai_backend.pdl"; URL=$(sed -n 's/^gemma_lan_url=//p' "$CFG" | head -1); MODEL=$(sed -n 's/^gemma_lan_model=//p' "$CFG" | head -1); [ -n "$URL" ] || { echo "no gemma_lan_url" >&2; exit 4; }
    BODY=$(PROMPT="$Q" MODEL="$MODEL" python3 -I -c 'import json,os; print(json.dumps({"model":os.environ["MODEL"],"prompt":os.environ["PROMPT"],"stream":False}))') || exit 4
    curl -s --max-time "${PET_TEACH_TIMEOUT:-60}" -H 'Content-Type: application/json' -d "$BODY" "${URL%/}/api/generate" | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["response"].strip())' ;;
  groq|poolside|openrouter)
    BACK="$HOUSE/^.hai-horn/ops/+x/horn_chat_backend.+x"; export HORN_PIN_PROVIDER="$P"
    if [ "${PET_TEACH_GATE:-1}" = 1 ]; then exec bash "$HOUSE/&.hq-apps/co-lab-hai/ops/colab_api_gate.sh" "$Q"; fi
    mkdir -p "$HOUSE/pieces/horn"; cd "$HOUSE" && HORN_TOOLS=off PRISC_PROJECT_ROOT="$HOUSE" HORN_CURL_TIMEOUT=120 exec "$BACK" "$Q" ;;
  *) echo "unknown provider $P" >&2; exit 2 ;;
esac
