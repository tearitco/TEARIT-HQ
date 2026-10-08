#!/bin/sh
# open_knowledge.sh - HQ menu (h-ai cell) "Knowledge" row entry point. Thin wrapper under a glob-safe path (the app lives at &.hq-apps/knowledge-hq/ and a leading '&'
# in an `sh -c` .pdl row is job-control). Same pattern as open_concept_bank.sh.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# ops -> _.livedesk-taskbar -> _.monads -> house_root
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
exec sh "$HOUSE_ROOT/&.hq-apps/knowledge-hq/open_knowledge_hq.sh" "$HOUSE_ROOT"
