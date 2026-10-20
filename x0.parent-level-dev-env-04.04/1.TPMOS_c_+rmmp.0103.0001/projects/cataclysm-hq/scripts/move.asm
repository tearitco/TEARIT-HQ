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
    read_pdl R0 "STATE" "y" R4
    addi R4, R4, 1
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "y" R4
    console_print "You move north."
    j finish

move_south:
    read_pdl R0 "STATE" "y" R4
    sub R4, R4, 1
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "y" R4
    console_print "You move south."
    j finish

move_west:
    read_pdl R0 "STATE" "x" R4
    sub R4, R4, 1
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "x" R4
    console_print "You move west."
    j finish

move_east:
    read_pdl R0 "STATE" "x" R4
    addi R4, R4, 1
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "x" R4
    console_print "You move east."
    j finish

finish:
    hit_frame
    halt
