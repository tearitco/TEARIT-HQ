; rest.asm - Rest to restore HP (costs hunger/thirst/alcohol)
; In Dwarf Fortress, proper rest requires a bed

start:
    console_print "You rest in your bed, tending to wounds..."
    exec projects/dwarf-fortress/ops/+x/rng_op.+x 10
    read_state hero last_roll R2
    add R3, R2, 10                     ; heal = 10 + roll

    read_pdl R1 "STATE" "hp" R5
    read_pdl R1 "STATE" "max_hp" R7
    add R5, R5, R3
    ble R5, R7, hp_ok
    mov R5, R7

hp_ok:
    write_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "hp" R5

    read_pdl R1 "STATE" "hunger" R9
    addi R9, R9, 1
    write_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "hunger" R9

    read_pdl R1 "STATE" "thirst" R10
    addi R10, R10, 1
    write_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "thirst" R10

    console_print "You recovered some HP. (hunger and thirst increased)"

    bge R9, 5, hungry_warning
    hit_frame
    halt

hungry_warning:
    console_print "You feel the pangs of hunger..."
    hit_frame
    halt
