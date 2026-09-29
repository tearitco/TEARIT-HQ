#!/bin/sh
# robot_chat_001's real page_1 Common Event - the ai_chat COMMAND
# registered 2026-09-28 in #.ref/menu/event_commands.registry.pdl,
# compiled the same real shape khtpm_events_hq_manager.c's own
# compiler produces (event.ir.pdl -> event.pal -> cmd_N.sh), hand-
# authored here rather than driven through the live events-hq UI this
# pass (same precedent as door_civ's own cmd_1.sh header comment).
#
# HONEST SCOPE: this is the "/command" scripted-line chat mode (a
# fixed, author-chosen message baked into this event, same as
# show_text's own text= param) - proving the real ai_chat.+x round
# trip on a real robot pal. It is NOT yet the live, type-anything chat
# window (a persistent cli_io-based small chat UI, same shape as
# events-hq's own fld_amount / ai-cell's session view) - that is
# real, designed future work, not built here, per this house's own
# honesty convention (see AI-PUSH-ROADMAP-AND-NUANCES.md's 2026-09-28
# addendum).
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
HOUSE_ROOT="$D"

exec "$HOUSE_ROOT/&.widgits/entity-cli/ops/ai_chat.+x" "$ENT" "$HOUSE_ROOT" 'Hello! What are you?'
