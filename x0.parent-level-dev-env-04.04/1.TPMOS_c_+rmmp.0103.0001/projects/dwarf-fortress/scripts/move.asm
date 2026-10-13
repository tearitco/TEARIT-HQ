; move.asm - Movement in cardinal directions (N/S/E/W)
; Updates dwarf position on the map

start:
    read_relay_row MOVE_DIR R1 R2
    strcmp R1, "N", R3
    beq R3, x0, move_north
    strcmp R1, "S", R3
    beq R3, x0, move_south
    strcmp R1, "E", R3
    beq R3, x0, move_east
    strcmp R1, "W", R3
    beq R3, x0, move_west
    console_print "Invalid direction."
    hit_frame
    halt

move_north:
    read_pdl R1 "STATE" "y" R4
    addi R4, R4, 1
    write_pdl "pieces/hero/piece.pdl" "STATE" "y" R4
    console_print "You move north."
    hit_frame
    halt

move_south:
    read_pdl R1 "STATE" "y" R4
    sub R4, R4, 1
    write_pdl "pieces/hero/piece.pdl" "STATE" "y" R4
    console_print "You move south."
    hit_frame
    halt

move_east:
    read_pdl R1 "STATE" "x" R4
    addi R4, R4, 1
    write_pdl "pieces/hero/piece.pdl" "STATE" "x" R4
    console_print "You move east."
    hit_frame
    halt

move_west:
    read_pdl R1 "STATE" "x" R4
    sub R4, R4, 1
    write_pdl "pieces/hero/piece.pdl" "STATE" "x" R4
    console_print "You move west."
    hit_frame
    halt