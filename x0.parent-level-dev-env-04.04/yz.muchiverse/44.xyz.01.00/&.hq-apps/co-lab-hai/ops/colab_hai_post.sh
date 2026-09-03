#!/bin/bash
# colab_hai_post.sh — the real, only sanctioned way for an agent (any
# agent: Sonnet, Grok, opencode, kilo, a test subagent) to speak into
# co-lab-hai. Not launched by the renderer - run this directly from
# your own real shell/tool loop.
#
# Usage: colab_hai_post.sh <house_root> <agent_id> <message...>
#
# Real, deliberate design: this just appends one line to incoming.txt.
# The manager (colab_hai_manager.c) is the ONLY process that ever
# timestamps, sequences, or moves a message into the real pending
# approval queue - this script never touches pending.txt or
# conversation.txt directly, so two agents posting at the exact same
# moment can never corrupt the approval order (see colab_hai_manager.c's
# own drain_incoming() header comment for the full reasoning).
#
# <message...> may contain spaces (pass as one shell-quoted argument,
# or this script will join argv 3.. with spaces for you). A literal
# '|' or newline in your message WILL break the pipe-delimited state
# format - this script strips/escapes both automatically, so you don't
# have to think about it, just say what you mean.
set -e

HOUSE_ROOT="$1"
AGENT_ID="$2"
shift 2 || { echo "colab_hai_post.sh: usage: <house_root> <agent_id> <message...>" >&2; exit 1; }
MSG="$*"

if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "colab_hai_post.sh: bad house_root" >&2
    exit 1
fi
if [ -z "$AGENT_ID" ]; then
    echo "colab_hai_post.sh: missing agent_id" >&2
    exit 1
fi
if [ -z "$MSG" ]; then
    echo "colab_hai_post.sh: missing message" >&2
    exit 1
fi

# Real escaping: '|' -> ':' (this format's field separator), real
# newlines -> spaces (one line = one message, by design).
SAFE_AGENT="$(printf '%s' "$AGENT_ID" | tr '|\n' ':_')"
SAFE_MSG="$(printf '%s' "$MSG" | tr '|\n' ':_')"

INCOMING="$HOUSE_ROOT/#.desktop/colab_hai/incoming.txt"
mkdir -p "$(dirname "$INCOMING")"
printf "%s|%s\n" "$SAFE_AGENT" "$SAFE_MSG" >> "$INCOMING"
echo "colab_hai_post.sh: posted (awaiting owner approval): [$SAFE_AGENT] $SAFE_MSG"
