#!/bin/bash
# test_apis.sh - Diagnostic tool for cpp-llm API connectivity
# Usage: ./test_apis.sh

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
CONFIG_FILE="$PROJECT_ROOT/config/apis.txt"

echo "=== cpp-llm API Connectivity Test ==="
echo "Date: $(date)"
echo "----------------------------------------"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found at $CONFIG_FILE"
    exit 1
fi

while IFS='|' read -r name url; do
    echo "Testing [$name] at $url..."
    
    # 1. GET Check (Model Discovery)
    is_llamacpp=0
    if [[ "$url" == *":8080"* ]]; then
        is_llamacpp=1
        endpoint="/v1/models"
    else
        endpoint="/api/tags"
    fi
    
    echo -n "  [GET] $endpoint: "
    get_resp=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "$url$endpoint")
    if [ "$get_resp" == "200" ]; then
        echo "OK (200)"
    else
        echo "FAILED ($get_resp)"
    fi

    # 2. POST Check (Inference)
    if [ "$is_llamacpp" -eq 1 ]; then
        post_endpoint="/v1/chat/completions"
        payload='{"model":"llama3-groq.gguf", "messages":[{"role":"user", "content":"hi"}], "stream":false}'
    else
        post_endpoint="/api/chat"
        # Using a tiny model for local test to avoid massive waits
        payload='{"model":"gemma3:270M", "messages":[{"role":"user", "content":"hi"}], "stream":false}'
    fi

    echo -n "  [POST] $post_endpoint: "
    post_resp=$(curl -s -o /dev/null -w "%{http_code}" --max-time 15 -H "Content-Type: application/json" -d "$payload" "$url$post_endpoint")
    
    if [ "$post_resp" == "200" ]; then
        echo "OK (200)"
    elif [ "$post_resp" == "000" ]; then
        echo "TIMEOUT (>15s)"
    else
        echo "FAILED ($post_resp)"
    fi
    echo "----------------------------------------"
done < "$CONFIG_FILE"

echo "=== Test Complete ==="
