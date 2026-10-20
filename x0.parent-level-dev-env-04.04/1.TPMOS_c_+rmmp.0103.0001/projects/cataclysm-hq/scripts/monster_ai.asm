; monster_ai.asm - Monster AI behavior (Orc warrior)
; Simple proximity + aggression check

start:
    read_pdl R1 "STATE" "aggression" R2
    read_pdl R1 "STATE" "ai_state" R3

    ; Check if in combat (player in proximity based on last combat result)
    read_pdl R1 "STATE" "hp" R4
    ble R4, 0 monster_dead

    ; Aggressive monsters attack if aggression > 40
    bge R2, 40, attack_player

    ; Otherwise stay idle
    console_print "The orc eyes you warily."
    hit_frame
    halt

attack_player:
    ; Attack: base damage 6-10
    exec projects/cataclysm-hq/ops/+x/rng_op.+x 6
    read_state hero last_roll R5
    add R5, R5, 5

    ; Player takes damage
    read_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "hp" R6
    sub R6, R6, R5
    ble R6, 0 player_death

    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "hp" R6
    console_print "The orc swings its club at you!"
    hit_frame
    halt

monster_dead:
    console_print "The orc crumples! (+20 XP)"
    relay_write "ORC_DEATH|orc_01|player=hero"
    read_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "xp" R7
    addi R7, R7, 20
    write_pdl "projects/cataclysm-hq/pieces/hero/piece.pdl" "STATE" "xp" R7
    hit_frame
    halt

player_death:
    console_print "The orc's club crushes you!"
    hit_frame
    halt
