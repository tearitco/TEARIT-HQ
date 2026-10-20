; monster_death.asm - Death sequence for the Orc warrior

start:
    console_print "========================================"
    console_print "      THE ORC WARRIOR FALLS!          "
    console_print "========================================"
    read_pdl R1 "STATE" "name" R2
    console_print R2
    console_print ""
    read_pdl R1 "STATE" "xp" R3
    console_print "Your foe gained:"

    ; Award XP to player
    read_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "xp" R6
    addi R6, R6, 30
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "xp" R6
    console_print "+30 XP"

    relay_write "MONSTER_DEATH|orc_01|name=R2|xp=R3"
    hit_frame
    halt
