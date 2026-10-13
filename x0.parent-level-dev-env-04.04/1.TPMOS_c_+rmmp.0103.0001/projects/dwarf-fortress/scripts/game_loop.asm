; game_loop.asm - Main turn-based game loop for Dwarf Fortress
; Uses events-hq pattern: read input from relay, dispatch to method

start:
    read_pdl R0 "STATE" "turn" R1
    addi R1, R1, 1
    write_pdl PROJECT_PDL "STATE" "turn" R1

    read_relay_row KEY_PRESSED R2 R3

    strcmp R2, "ESC", R4
    beq R4, x0, game_quit

    call_method on_move
    call_method on_mining

    hit_frame
    j start

game_quit:
    console_print "Your fortress stands eternal. Until next session."
    write_pdl PROJECT_PDL "STATE" "game_over" "true"
    hit_frame
    halt