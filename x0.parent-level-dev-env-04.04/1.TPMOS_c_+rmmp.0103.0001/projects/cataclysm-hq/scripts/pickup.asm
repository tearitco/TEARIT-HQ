; pickup.asm - Pick up an item from the current tile
; Checks map for items, adds to hero inventory

start:
    read_state "x" R1
    read_state "y" R2

    ; Check if there's an item at this position
    ; In a real implementation, this would read from a map items section
    console_print "You search the ground..."

    ; Placeholder: no items to find yet
    console_print "Nothing of interest here."
    j finish

finish:
    hit_frame
    halt
