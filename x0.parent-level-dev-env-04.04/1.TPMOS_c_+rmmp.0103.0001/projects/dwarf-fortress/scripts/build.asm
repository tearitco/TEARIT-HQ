; build.asm - Construct fortifications and furniture
; Uses mining skill as a proxy for crafting ability

start:
    console_print "You begin constructing fortifications..."
    exec projects/dwarf-fortress/ops/+x/rng_op.+x 10
    read_state hero last_roll R2
    read_pdl R1 "STATE" "skill_crafting" R3
    add R2, R2, R3

    ble R2, 8, build_failed
    console_print "You build a sturdy stone wall section."
    read_pdl R1 "STATE" "xp" R4
    addi R4, R4, 15
    write_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "xp" R4
    hit_frame
    halt

build_failed:
    console_print "The construction collapses. Try again."
    hit_frame
    halt