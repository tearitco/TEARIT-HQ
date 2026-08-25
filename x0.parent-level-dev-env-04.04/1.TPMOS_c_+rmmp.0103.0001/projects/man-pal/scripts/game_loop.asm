# game_loop.asm - Persistent PAL Orchestrator (demo project)
# Purpose: Orchestrate shared Ops binaries based on high-level input.

# Registers:
# r1: current key
# r2: scratch
# r3: history position
# r4: is_active focus
# r10: sleep time (16667 for 60 FPS)

li r10, 16667
li r3, 0

loop:
    # 1. Check if we are still the active layout (prevent zombie input)
    read_layout r4, "playrm/layouts/game.chtpm"
    beq r4, x0, sleep_loop

    # 2. Poll for new keys in history
    read_history r1, r3
    beq r1, x0, sleep_loop

    # 3. Process the key (High-level decision)
    # Movement: Direct flex of the 'move_entity' muscle
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

    # Back to polling for more keys in this frame
    j loop

move_up:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x hero w man-pal
    hit_frame
    j loop

move_down:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x hero s man-pal
    hit_frame
    j loop

move_left:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x hero a man-pal
    hit_frame
    j loop

move_right:
    exec ./pieces/apps/playrm/ops/+x/move_entity.+x hero d man-pal
    hit_frame
    j loop

sleep_loop:
    sleep r10
    j loop
