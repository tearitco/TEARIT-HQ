; death.asm - Player death sequence

start:
    console_print "========================================"
    console_print "          GAME OVER - YOU DIED          "
    console_print "========================================"
    console_print ""
    console_print "You reached dungeon level:"
    read_pdl R1 "STATE" "turn" R2
    console_print R2
    console_print ""
    console_print "Final stats:"
    read_pdl R3 "STATE" "level" R4
    console_print "Level: R4"
    read_pdl R5 "STATE" "xp" R6
    console_print "XP: R6"
    console_print ""
    console_print "A true adventurer's death is never forgotten."
    relay_write GAME_OVER|reason=death|turn=R2

    ; Record death to concept bank (for Gemma learning)
    exec ./&.widgits/concept-bank/ops/concept_edit_validate.c nil
    hit_frame
    halt