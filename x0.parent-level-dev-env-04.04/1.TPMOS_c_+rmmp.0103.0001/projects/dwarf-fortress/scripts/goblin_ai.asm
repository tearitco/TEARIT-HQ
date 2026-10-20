; goblin_ai.asm - Goblin AI behavior during combat
; Goblins are aggressive but cowardly - attack if healthy, flee if weak

start:
    read_pdl R1 "STATE" "hp" R2
    read_pdl R1 "STATE" "max_hp" R3

    ; Flee if below 30% HP (approx: hp * 10 <= max_hp * 3)
    add R4, R2, R2
    add R4, R4, R2
    add R4, R4, R4
    add R4, R4, R2          ; R4 = hp * 10
    add R5, R3, R3
    add R5, R5, R3          ; R5 = max_hp * 3
    ble R4, R5, goblin_flees

    ; Attack: deal 8-15 damage
    exec projects/dwarf-fortress/ops/+x/rng_op.+x 8
    read_state hero last_roll R6
    add R6, R6, 7

    read_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "hp" R8
    sub R8, R8, R6
    write_pdl "projects/dwarf-fortress/pieces/hero/piece.pdl" "STATE" "hp" R8
    relay_write "GOBLIN_ATTACK|target=hero"
    console_print "The goblin stabs you for some damage."

    ble R8, 0 player_survives
    console_print "You have been slain by the goblin!"
    hit_frame
    halt

player_survives:
    hit_frame
    halt

goblin_flees:
    console_print "The goblin cowers and flees!"
    write_pdl "projects/dwarf-fortress/pieces/monster/piece.pdl" "STATE" "hp" R0
    relay_write "GOBLIN_FLED|goblin_01"
    hit_frame
    halt
