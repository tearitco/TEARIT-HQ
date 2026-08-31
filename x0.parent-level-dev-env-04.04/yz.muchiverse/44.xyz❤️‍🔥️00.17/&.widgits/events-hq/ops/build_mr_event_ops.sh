#!/bin/sh
# build_mr_event_ops.sh — build the shared event-command op binaries
# (mr_change_gold.c, mr_character.c, mr_input_number.c, mr_select_item.c,
# mr_scrolling_text.c, mr_show_choices.c, mr_show_text.c) and the shared
# common_events_manager.c, same real -o +x/<name>.+x pattern as this
# dir's own build_events_hq_manager.sh.
#
# REAL FIX 2026-08-29 ("mr was just one project using events (probably
# the first) but it doesn't own events"): these ops used to live under
# *.monads/*.muchi-pet/ops/ - muchi-pet's own project dir - even though
# every entity/project in the house (not just muchi-pet) compiles events
# that call them, via #.ref/menu/event_commands.registry.pdl's TEMPLATE
# exec lines. Moved here, events-hq's own shared ops dir, alongside
# khtpm_events_hq_manager.c which already drives every entity's event
# compilation. No dedicated build_*.sh for these existed at the old
# location (grepped for one before this move - none found; each binary
# was built ad hoc when first introduced) - this script is the first
# real one, and now the canonical way to (re)build all of them at once.
# No X11/Xft dependency - none of these binaries open a window, they
# only write real state files + relay commands text.
#
# mr_clock_common.h is a shared header (#include'd directly by
# mr_input_number.c/mr_character.c/mr_select_item.c/mr_scrolling_text.c,
# not compiled on its own) - no separate build line needed for it, but
# it must stay a sibling of the .c files that include it.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

for src in mr_change_gold mr_character mr_actor_string mr_input_number \
           mr_select_item mr_scrolling_text mr_show_choices mr_show_text; do
  echo "-- $src.c -> +x/$src.+x"
  $CC $CFLAGS -o "+x/$src.+x" "$src.c"
done

echo "-- common_events_manager.c -> +x/common_events_manager.+x"
$CC $CFLAGS -pthread -o +x/common_events_manager.+x common_events_manager.c

echo "OK: all mr_* event ops + common_events_manager built into +x/"
