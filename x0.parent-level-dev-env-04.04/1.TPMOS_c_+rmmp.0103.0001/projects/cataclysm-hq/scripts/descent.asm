; descent.asm - Descend to next dungeon level
; Generates new map via dungeon_gen.c, resets player position

start:
    console_print "You find a staircase down and descend..."

    ; Increment dungeon level
    read_pdl R1 "STATE" "turn" R2
    write_pdl "STATE" "turn" R2

    ; Generate new dungeon level
    exec ./ops/+x/dungeon_gen.+x 15 10

    ; Reset player position (spawn near top)
    write_pdl "pieces/hero/piece.pdl" "STATE" "x" 7
    write_pdl "pieces/hero/piece.pdl" "STATE" "y" 1
    write_pdl "pieces/hero/piece.pdl" "STATE" "z" R2

    console_print "New dungeon level generated. Survive deeper..."
    relay_write LEVEL_CHANGE|new_level=
    hit_frame
    halt