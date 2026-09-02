#!/bin/bash
# qwen.sh - shared house-wide Qwen2.5-Coder ladder wrapper.
# Single way every product talks to the LAN qwen ladder. Reads the tier map
# from net/ollama-lan.pdl (the uniform registry) - no hardcoded model names or
# endpoints anywhere.
#
#   sh net/qwen.sh ask <tier> <prompt>        # one-shot /api/generate, print reply
#   sh net/qwen.sh chat <tier> <prompt>       # /api/chat, print reply
#   sh net/qwen.sh tool <tier> <prompt>       # /api/chat (tool-call capable models)
#   sh net/qwen.sh fim <prompt> <suffix>      # fill-in-the-middle (codeqwen tier)
#   sh net/qwen.sh ladder                     # show tiers + resolved endpoint/model
#   sh net/qwen.sh status                     # ping every tier's endpoint
#
# Tier names (from ollama-lan.pdl): router, quick, coder, manager, fim.
# Default host = mac; override with QWEN_HOST=linux|local (or a URL).
# Harnecient Way: this wrapper only transports text - the CALLER decides which
# tier to use and what to do with the reply (app decides, model generates).

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PDL="$SCRIPT_DIR/ollama-lan.pdl"
HOUSE="$(cd "$SCRIPT_DIR/.." && pwd)"
# In-house tooling only (house standard: C, no jq/python). Canonical copies
# live with gem-dev agent45; override with QWEN_JSONP / QWEN_CONNOP if moved.
JSONP="${QWEN_JSONP:-$HOUSE/045.muchi-pal-agent🤖️+1++/ops/+x/json_parser.+x}"
CONNOP="${QWEN_CONNOP:-$HOUSE/045.muchi-pal-agent🤖️+1++/ops/+x/connect_op.+x}"
HOST_NAME="${QWEN_HOST:-mac}"
ACTION="${1:-ladder}"

tier_model() { awk -F'|' -v t="$1" '$1 !~ /^#/ && $1 ~ /TIER/ {gsub(/ /,"",$2); gsub(/ /,"",$3); if ($2==t) {print $3; exit}}' "$PDL"; }
host_url()   { awk -F'|' -v h="$1" '$1 !~ /^#/ && $1 ~ /HOST/ {gsub(/ /,"",$2); gsub(/ /,"",$3); if ($2==h) {print $3; exit}}' "$PDL"; }
tier_rows()  { awk -F'|' '$1 !~ /^#/ && $1 ~ /TIER/ {gsub(/ /,"",$2); gsub(/ /,"",$3); print $2, $3}' "$PDL"; }

# Build a JSON string literal in pure sh (escape \ " and control chars).
json_str() {
    local s="$1"
    s=${s//\\/\\\\}
    s=${s//\"/\\\"}
    s=${s//$'\t'/\\t}
    s=${s//$'\n'/\\n}
    s=${s//$'\r'/\\r}
    s=${s//$'\b'/\\b}
    s=${s//$'\f'/\\f}
    printf '%s' "$s"
}

# Read a dot-path field from a JSON file via the in-house parser.
json_get() { "$JSONP" "$1" "$2" 2>/dev/null; }

# House-standard ask chain (my-lawyer reference): write request JSON to a
# file, POST via connect_op (the house curl wrapper), parse via json_parser.
# $1=api url  $2=json body  $3=field path; prints field (or "error", or nothing).
api_get() {
    local req rsp r
    req="$(mktemp)"; rsp="$(mktemp)"
    printf '%s' "$2" > "$req"
    "$CONNOP" "$1" "$req" "$rsp"
    r="$(json_get "$rsp" "$3")" || r="$(json_get "$rsp" error)"
    printf '%s' "$r"
    rm -f "$req" "$rsp"
}

model="${2:-}"; prompt="${3:-}"

ask() {
    # $1 tier, $2 prompt
    local tier="$1" prompt="$2"
    local m host
    m="$(tier_model "$tier")"; host="$(host_url "$HOST_NAME")"
    [ -n "$m" ] || { echo "unknown tier '$tier'"; exit 1; }
    api_get "$host/api/generate" "{\"model\":\"$m\",\"prompt\":\"$(json_str "$prompt")\",\"stream\":false}" response
    echo
}

chat() {
    # $1 tier, $2 message text
    local tier="$1" msg="$2"
    local m host
    m="$(tier_model "$tier")"; host="$(host_url "$HOST_NAME")"
    [ -n "$m" ] || { echo "unknown tier '$tier'"; exit 1; }
    api_get "$host/api/chat" "{\"model\":\"$m\",\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":\"$(json_str "$msg")\"}]}" message.content
    echo
}

fim() {
    # $1 prompt (prefix), $2 suffix
    local prompt="$1" suffix="$2"
    local m host
    m="$(tier_model fim)"; host="$(host_url "$HOST_NAME")"
    api_get "$host/api/generate" "{\"model\":\"$m\",\"prompt\":\"$(json_str "$prompt")\",\"suffix\":\"$(json_str "$suffix")\",\"stream\":false,\"options\":{\"stop\":[\"<|fim_end|>\"]}}" response
    echo
}

case "$ACTION" in
    ask)
        ask "$model" "$prompt"
        ;;
    chat)
        chat "$model" "$prompt"
        ;;
    tool)
        # Same as chat for now; qwen2.5-coder supports native tools[] and the
        # CALLER can add a tools array to the request - wrapper stays transport.
        chat "$model" "$prompt"
        ;;
    fim)
        fim "${2:-}" "${3:-}"
        ;;
    ladder)
        echo "tier      model                    host"
        echo "--------- ------------------------ ----------------------"
        while read -r t m; do
            [ -z "$t" ] && continue
            printf "%-9s %-24s %s\n" "$t" "$m" "$(host_url "$HOST_NAME")"
        done < <(tier_rows)
        ;;
    status)
        while read -r t m; do
            [ -z "$t" ] && continue
            host="$(host_url "$HOST_NAME")"
            if curl -s --max-time 5 "$host/api/tags" | grep -q "$m"; then
                echo "OK   $t -> $m @ $host"
            else
                echo "MISS $t -> $m @ $host"
            fi
        done < <(tier_rows)
        ;;
    *)
        echo "qwen.sh — shared qwen ladder wrapper (net/ollama-lan.pdl)"
        echo ""
        echo "  sh net/qwen.sh ask <tier> <prompt>    one-shot generate"
        echo "  sh net/qwen.sh chat <tier> <prompt>   chat message"
        echo "  sh net/qwen.sh tool <tier> <prompt>   tool-call capable chat"
        echo "  sh net/qwen.sh fim <prefix> <suffix>  fill-in-the-middle (codeqwen)"
        echo "  sh net/qwen.sh ladder                 list tiers + endpoints"
        echo "  sh net/qwen.sh status                 ping every tier"
        echo ""
        echo "  QWEN_HOST=mac|linux|local to pick an inference node (default mac)"
        ;;
esac
