# pet_explore.asm - Pet explores the map (random walk via zombie pathfinding)
# Used by: brain_dispatch.asm -> explore

start:
    console_print "Fuzzball: *explores the vicinity*"
    # Use zombie_ai for pathfinding toward pet (random target)
    exec ./pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x zombie_01 fuzzball
    hit_frame
    halt
