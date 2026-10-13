# pet_toggle_emoji.asm - Toggle pet display emoji
# Used by: brain_dispatch.asm -> toggle_emoji

start:
    console_print "Fuzzball: *emoji shift*"
    exec ./pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzball toggle_emoji
    hit_frame
    halt
