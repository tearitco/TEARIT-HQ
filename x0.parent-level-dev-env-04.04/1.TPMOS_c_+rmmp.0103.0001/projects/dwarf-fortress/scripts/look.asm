; look.asm - Examine surroundings for threats and resources

start:
    console_print "You look around the underground fortress..."
    read_pdl R1 "STATE" "x" R2
    read_pdl R1 "STATE" "y" R3

    ; Check for goblin proximity (simple distance check)
    read_pdl R5 "STATE" "x" R6
    read_pdl R5 "STATE" "y" R7
    sub R8, R2, R6
    sub R9, R3, R7
    add R8, R8, R9

    ble R8, 3, see_goblin
    ble R8, 0, see_goblin

    console_print "You see stone walls and dusty corridors."
    hit_frame
    halt

see_goblin:
    console_print "You spot a goblin lurking nearby!"
    hit_frame
    halt