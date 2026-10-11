# brain_dispatch.asm - Hybrid relay-to-events dispatcher
# Reads PET_ACTION rows from the relay event bus and dispatches
# to the appropriate behavior script via the .pdl method table.
#
# This bridges the relay pattern (fuzzpet_brain.c writes PET_ACTION)
# with the book:page events architecture (behavior.pdl + .asm scripts).
#
# Registers:
# r1: relay row parsed value (action string)
# r2: relay row parsed value (pet name)

start:
    # Check relay for new PET_ACTION rows
    read_relay_row PET_ACTION r1 r2

    # r1 = action string ("feed", "play", "sleep", etc.)
    # r2 = pet name (e.g. "fuzzball")

    # Compare against known actions
    # feed -> 1001
    strcmp r1, "feed", r3
    beq r3, x0, do_feed

    # play -> 1002
    strcmp r1, "play", r3
    beq r3, x0, do_play

    # sleep -> 1003
    strcmp r1, "sleep", r3
    beq r3, x0, do_sleep

    # explore -> 1004
    strcmp r1, "explore", r3
    beq r3, x0, do_dispatch_explore

    # heal -> 1005
    strcmp r1, "heal", r3
    beq r3, x0, do_dispatch_heal

    # evolve -> 1006
    strcmp r1, "evolve", r3
    beq r3, x0, do_dispatch_evolve

    # toggle_emoji -> 1007
    strcmp r1, "toggle_emoji", r3
    beq r3, x0, do_toggle_emoji

    # wait / unknown -> 1000
    strcmp r1, "wait", r3
    beq r3, x0, do_nothing

    # Unknown action — write error to relay
    relay_write ACTION_ERROR|unknown_action r1
    j finish

do_feed:
    call_method feed
    j finish

do_play:
    call_method play
    j finish

do_sleep:
    call_method sleep
    j finish

do_dispatch_explore:
    call_method explore
    j finish

do_dispatch_heal:
    call_method heal
    j finish

do_dispatch_evolve:
    call_method evolve
    j finish

do_toggle_emoji:
    call_method toggle_emoji
    j finish

do_nothing:
    relay_write ACTION_EXECUTED|wait|skipped
    j finish

finish:
    hit_frame
    halt
