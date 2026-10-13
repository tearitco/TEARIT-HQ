# pet_sleep.asm - Pet sleeps to restore energy
# Used by: brain_dispatch.asm -> sleep

start:
    console_print "Fuzzball: *snore* Zzz..."
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball sleep
    hit_frame
    halt
