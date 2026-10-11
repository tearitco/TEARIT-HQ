; combat.asm - Combat resolution (player vs monster)
; Uses rng_op for d6 damage, reads/writes HP from piece.pdl state

start:
    ; Read player STR and monster HP from state
    read_pdl R1 "STATE" "str" R2       ; player_str
    read_pdl R5 "STATE" "hp" R6        ; monster_hp (from monster piece)

    ; Check if monster already dead
    ble R6, 0 monster_dead

    ; Player attacks: damage = str - d6 roll
    exec ./ops/+x/rng_op.+x 6
    read_pdl R7 "STATE" "last_roll" R8
    sub R9, R2, R8                     ; damage = str - roll
    ble R9, 0 player_miss              ; if damage <= 0, minimum 1

    ; Apply damage to monster
    sub R6, R6, R9                     ; monster_hp -= damage
    write_pdl "pieces/monster/piece.pdl" "STATE" "hp" R6
    console_print "You hit the orc for some damage."

    ; Check monster death
    ble R6, 0 monster_dead

    ; Monster counterattacks
    read_pdl R10 "STATE" "str" R11     ; monster_str
    read_pdl R1 "STATE" "hp" R2        ; player_hp
    exec ./ops/+x/rng_op.+x 6
    read_pdl R7 "STATE" "last_roll" R8
    sub R12, R11, R8                   ; mon_damage = str - roll
    sub R2, R2, R12                    ; player_hp -= damage
    write_pdl "pieces/hero/piece.pdl" "STATE" "hp" R2
    console_print "The orc strikes back."

    ; Check player death
    ble R2, 0 player_dead
    hit_frame
    halt

player_miss:
    ; Monster still counterattacks on miss
    read_pdl R10 "STATE" "str" R11
    read_pdl R1 "STATE" "hp" R2
    exec ./ops/+x/rng_op.+x 6
    read_pdl R7 "STATE" "last_roll" R8
    sub R12, R11, R8
    sub R2, R2, R12
    write_pdl "pieces/hero/piece.pdl" "STATE" "hp" R2
    console_print "You missed! The orc hits you anyway."
    ble R2, 0 player_dead
    hit_frame
    halt

monster_dead:
    console_print "You killed the Orc Warrior! (+50 XP)"
    relay_write MONSTER_DEATH|orc_01|player=hero
    read_pdl R13 "STATE" "xp" R14
    addi R14, R14, 50
    write_pdl "pieces/hero/piece.pdl" "STATE" "xp" R14
    hit_frame
    halt

player_dead:
    console_print "You have been slain..."
    call_method die
    hit_frame
    halt