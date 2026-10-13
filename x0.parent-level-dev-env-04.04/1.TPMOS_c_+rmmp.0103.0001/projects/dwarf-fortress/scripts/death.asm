; death.asm - Player death sequence

start:
    console_print "========================================"
    console_print "      YOUR DWARF HAS FALLEN!           "
    console_print "========================================"
    console_print ""
    console_print "Legends will sing of your deeds, "
    read_pdl R1 "STATE" "name" R2
    console_print R2
    console_print ""
    console_print "Mining skill: R3"
    console_print "Combat skill: R4"
    console_print ""
    console_print "The mountain claims another soul."
    relay_write DWARF_DEATH|name=R2|reason=combat
    hit_frame
    halt