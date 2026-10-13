; mine.asm - Mining action: extract ore veins with pickaxe
; Gemma can evolve this script via concept bank mining/trade weights

start:
    console_print "You swing your pickaxe at the stone..."
    exec ./ops/+x/rng_op.+x 20
    read_pdl R1 "STATE" "last_roll" R2
    read_pdl R1 "STATE" "skill_mining" R3
    sub R2, R2, R3
    ble R2, 0 mine_failed

    ; Check pickaxe quality (base 5)
    add R2, R2, 5
    read_pdl R1 "STATE" "xp" R4
    addi R4, R4, 10
    write_pdl "pieces/hero/piece.pdl" "STATE" "xp" R4
    console_print "You find a rich vein of ore!"

    ; Chance for precious metal (skill + roll > 25)
    bge R2, 25, found_precious
    hit_frame
    halt

mine_failed:
    console_print "The stone yields nothing useful."
    read_pdl R1 "STATE" "xp" R2
    addi R2, R2, 2
    write_pdl "pieces/hero/piece.pdl" "STATE" "xp" R2
    hit_frame
    halt

found_precious:
    console_print "You discover a vein of precious metal!"
    read_pdl R1 "STATE" "xp" R2
    addi R2, R2, 25
    write_pdl "pieces/hero/piece.pdl" "STATE" "xp" R2
    hit_frame
    halt
