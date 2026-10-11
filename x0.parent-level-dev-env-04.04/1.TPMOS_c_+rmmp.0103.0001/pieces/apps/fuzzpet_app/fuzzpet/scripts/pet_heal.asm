# pet_heal.asm - Pet heals (combined sleep + play)
# Used by: brain_dispatch.asm -> heal

start:
    console_print "Fuzzball: *grooms self*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball sleep
    console_print "Fuzzball: *purrs with renewed energy*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball play
    relay_write ACTION_EXECUTED|heal|combined_sleep_play
    hit_frame
    halt
