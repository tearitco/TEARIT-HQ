; look.asm - Examine the current tile and adjacent entities
; Reads player position from state.txt, checks map tile

start:
    read_pdl R1 "STATE" "x" R1
    read_pdl R1 "STATE" "y" R2

    console_print "You look around..."

    ; Check for monsters at player position
    read_pdl "projects/cataclysm-hq/pieces/monster/piece.pdl" "STATE" "x" R4
    read_pdl "projects/cataclysm-hq/pieces/monster/piece.pdl" "STATE" "y" R6
    ; If monster at same position, describe it
    beq R4, R1, monster_here_a
    beq R6, R2, monster_here_b

    console_print "You see: stone walls, dust, silence."
    j finish

monster_here_a:
    console_print "You see an Orc Warrior here!"
    j finish

monster_here_b:
    console_print "An Orc Warrior blocks your path!"
    j finish

finish:
    hit_frame
    halt
