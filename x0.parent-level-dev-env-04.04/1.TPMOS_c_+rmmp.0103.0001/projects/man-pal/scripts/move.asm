# move.asm - Standardized PAL Brain (Track 2)
# Purpose: Orchestrate movement via Shared Traits (Optimized Batch Mode)

# Registers:
# r1: current key (from history)
# r2: scratch

start:
    # Use the VM's native history reader (Standard Buffer)
    read_history r1
    
    # If no key (0), we are done with the current batch
    beq r1, x0, finish

    # UP: 'w' (119), 'W' (87), 1002
    addi r2, r1, -119
    beq r2, x0, move_up
    addi r2, r1, -87
    beq r2, x0, move_up
    addi r2, r1, -1002
    beq r2, x0, move_up

    # DOWN: 's' (115), 'S' (83), 1003
    addi r2, r1, -115
    beq r2, x0, move_down
    addi r2, r1, -83
    beq r2, x0, move_down
    addi r2, r1, -1003
    beq r2, x0, move_down

    # LEFT: 'a' (97), 'A' (65), 1000
    addi r2, r1, -97
    beq r2, x0, move_left
    addi r2, r1, -65
    beq r2, x0, move_left
    addi r2, r1, -1000
    beq r2, x0, move_left

    # RIGHT: 'd' (100), 'D' (68), 1001
    addi r2, r1, -100
    beq r2, x0, move_right
    addi r2, r1, -68
    beq r2, x0, move_right
    addi r2, r1, -1001
    beq r2, x0, move_right

    # Unknown key? Just continue the loop to consume it and move on
    j start

move_up:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x selector w man-pal
    j start

move_down:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x selector s man-pal
    j start

move_left:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x selector a man-pal
    j start

move_right:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x selector d man-pal
    j start

finish:
    # After batch is complete, hit frame to update renderer
    hit_frame
    halt
