; game_loop.asm - Main turn-based game loop for Cataclysm-HQ
; Uses events-hq pattern: read input from relay, dispatch to method

start:
    ; Increment turn counter
    read_pdl R0 "STATE" "turn" R1
    addi R1, R1, 1
    write_pdl PROJECT_PDL "STATE" "turn" R1

    ; Read player input from relay
    read_relay_row KEY_PRESSED R2 R3

    ; Check for game exit
    strcmp R2, "ESC", R4
    beq R4, x0, game_quit

    ; Dispatch to movement handler
    call_method move

    ; Process monster AI
    call_method monster_ai

    ; Check for combat (proximity)
    call_method combat_check

    ; Trigger render
    hit_frame

    ; Continue loop
    j start

game_quit:
    console_print "Game over. Thanks for playing Cataclysm-HQ."
    write_pdl PROJECT_PDL "STATE" "game_over" "true"
    hit_frame
    halt
