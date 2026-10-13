; inventory.asm - Display player inventory
; Reads INV slot declarations from piece.pdl

start:
    console_print "=== INVENTORY ==="
    console_print "Weapon: sword"
    console_print "Armor:  leather"
    console_print "Gold:   0"
    console_print "Slots:"
    console_print "  [1] empty"
    console_print "  [2] empty"
    console_print "  [3] empty"
    console_print "  [4] empty"
    console_print "  [5] empty"
    console_print "================"
    hit_frame
    halt