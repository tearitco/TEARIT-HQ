#!/bin/bash
# build_self.sh - v1 build-itself op. Appends a self-improvement request to
# a log the monad (or a human) can act on. Sandboxed to the monad only.
# Usage: build_self.sh "<what>"
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

WHAT="$*"
[ -z "$WHAT" ] && WHAT="(unspecified improvement)"

SELF_LOG="$BRAIN_DIR/self_improvements.txt"
printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$WHAT" >> "$SELF_LOG"
ledger_append "BuildSelf" "wants to improve itself: $WHAT" "build_self.sh"
echo "BuildSelf: logged '$WHAT' to self_improvements.txt"
