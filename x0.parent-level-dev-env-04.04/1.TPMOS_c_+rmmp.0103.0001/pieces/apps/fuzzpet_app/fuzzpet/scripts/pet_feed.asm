# pet_feed.asm - Feed the pet
# Reads PET_ACTION from relay, executes feed op
# Used by: brain_dispatch.asm -> feed

start:
    console_print "Fuzzball: *nom nom*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball feed
    hit_frame
    halt
