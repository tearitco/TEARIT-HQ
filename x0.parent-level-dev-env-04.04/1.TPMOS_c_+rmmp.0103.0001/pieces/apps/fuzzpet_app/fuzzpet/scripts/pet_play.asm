# pet_play.asm - Play with the pet
# Used by: brain_dispatch.asm -> play

start:
    console_print "Fuzzball: *plays excitedly*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball play
    hit_frame
    halt
