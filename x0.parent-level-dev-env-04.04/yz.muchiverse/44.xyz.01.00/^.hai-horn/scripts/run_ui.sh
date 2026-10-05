#!/bin/bash
# Start the HORN_CHAT UI stack with every key resolved correctly.
#
# The `&` in this house's &.widgits path breaks any construct that goes
# through /bin/sh word-splitting - a heredoc, a double-quoted export, an
# unquoted $(...) - and the failure mode is always the same: the export
# silently becomes empty and every provider is reported as "has no key".
# Quoting each path once, here, is the whole fix.
# scripts/ is the directory this lives in, so the project root is its
# PARENT. Using dirname $0 alone left us in scripts/, which made every key
# lookup miss (all three came back length 0, so every provider reported
# "has no key") and horn_chat.sh not found.
cd "$(dirname "$0")/.." || exit 1
H="$(cd .. && pwd)"

export PRISC_PROJECT_ROOT="$PWD"
export PRISC_PROJECT_ID="hai-horn"
export HORN_SESSIONS="$PWD/chats/HORN_SESSIONS"
export HORN_ENTITY_DIR="$H/&.widgits/open-hai/state"

for f in raw_groq.txt raw_poolside.txt openrouter_api_key.txt; do
    [ -f "$H/&.widgits/open-hai/state/$f" ] || continue
    case "$f" in
        raw_groq.txt)          export GROQ_API_KEY="$(tr -d ' \t\n\r' < "$H/&.widgits/open-hai/state/$f")" ;;
        raw_poolside.txt)      export HORN_POOLSIDE_KEY="$(tr -d ' \t\n\r' < "$H/&.widgits/open-hai/state/$f")" ;;
        openrouter_api_key.txt) export HORN_API_KEY="$(tr -d ' \t\n\r' < "$H/&.widgits/open-hai/state/$f")" ;;
    esac
done

echo "keys loaded: groq=${#GROQ_API_KEY} poolside=${#HORN_POOLSIDE_KEY} openrouter=${#HORN_API_KEY}" >&2
exec ./horn_chat.sh run