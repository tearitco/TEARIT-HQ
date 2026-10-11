; move.asm - Player movement with collision detection
; Reads direction from relay history, checks map collisions

start:
    read_history R1
    beq R1, x0, finish      ; No key, skip

    ; Check movement keys (W/A/S/D + arrows)
    strcmp R1, "119", R2    ; 'w'
    beq R2, x0, move_north
    strcmp R1, "87", R2     ; 'W'
    beq R2, x0, move_north
    strcmp R1, "115", R2    ; 's'
    beq R2, x0, move_south
    strcmp R1, "97", R2     ; 'a'
    beq R2, x0, move_west
    strcmp R1, "100", R2    ; 'd'
    beq R2, x0, move_east
    j finish

move_north:
    call_method move_check_north
    j finish

move_south:
    call_method move_check_south
    j finish

move_west:
    call_method move_check_west
    j finish

move_east:
    call_method move_check_east
    j finish

finish:
    hit_frame
    halt
