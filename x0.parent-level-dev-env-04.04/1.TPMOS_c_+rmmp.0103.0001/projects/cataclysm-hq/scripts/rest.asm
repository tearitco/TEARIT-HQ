; rest.asm - Rest to restore HP (costs hunger/thirst)
; Gemma can evolve this script via concept bank combat/survival weights

start:
    console_print "You rest and tend to your wounds..."

    ; Restore HP (10 + d10)
    exec projects/cataclysm-hq/ops/+x/rng_op.+x 10
    read_state hero last_roll R2
    addi R3, R2, 10                    ; heal = 10 + roll

    ; Get current HP
    read_pdl R4 "STATE" "hp" R5
    read_pdl R6 "STATE" "max_hp" R7
    add R5, R5, R3
    ble R5, R7, hp_ok
    mov R5, R7

hp_ok:
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "hp" R5

    ; Increase hunger
    read_pdl R8 "STATE" "hunger" R9
    addi R9, R9, 1
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "hunger" R9

    console_print "You recovered some HP. (hunger increased)"

    ; Check for hunger penalty
    bge R9, 5, hungry_warning

    hit_frame
    halt

hungry_warning:
    console_print "You feel hungry..."
    hit_frame
    halt