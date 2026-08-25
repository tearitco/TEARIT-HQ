; player_loop.asm - OP-ED Thin Engine Player
; Architecture: PAL Orchestrator

; 1. BOOTSTRAP
; Resolve current project context from system state
READ_STATE "project_id" R0
IF_NOT R0 BOOT_FAIL

; 2. LOAD PROJECT METADATA
; Read starting map and player piece from project.pdl
READ_PDL R0 "STATE" "starting_map" R1
READ_PDL R0 "STATE" "player_piece" R2

; 3. THEATER INIT
; Initial render call
CALL_OP "render_map" R1

; 4. MAIN GAME LOOP
LOOP:
    ; A. POLL INPUT
    READ_INPUT R3
    IF_NOT R3 TICK
    
    ; B. MODE 1: REALTIME MOVEMENT
    ; (WASD / Arrows)
    IF_KEY R3 "w" MOVE_UP
    IF_KEY R3 "s" MOVE_DOWN
    IF_KEY R3 "a" MOVE_LEFT
    IF_KEY R3 "d" MOVE_RIGHT
    
    ; C. MODE 2: INTERACTION
    IF_KEY R3 "ENTER" INTERACT
    
    ; D. SYSTEM NAVIGATION
    IF_KEY R3 "ESC" EXIT_PLAYER
    
    GOTO LOOP

MOVE_UP:
    CALL_OP "move_entity" R2 "w" R0
    GOTO LOOP

MOVE_DOWN:
    CALL_OP "move_entity" R2 "s" R0
    GOTO LOOP

MOVE_LEFT:
    CALL_OP "move_entity" R2 "a" R0
    GOTO LOOP

MOVE_RIGHT:
    CALL_OP "move_entity" R2 "d" R0
    GOTO LOOP

INTERACT:
    ; Find piece at player's facing position and execute its on_interact
    CALL_OP "interact" R2 R0
    GOTO LOOP

TICK:
    ; Background logic (Slice 3: Stat decay, NPC AI)
    ; usleep equivalent to throttle loop
    SLEEP 16
    GOTO LOOP

EXIT_PLAYER:
    ; Return to Editor
    TRANSITION "projects/op-ed/layouts/op-ed.chtpm"
    HALT

BOOT_FAIL:
    SET_RESPONSE "PLAYER ERROR: No project context found."
    HALT
