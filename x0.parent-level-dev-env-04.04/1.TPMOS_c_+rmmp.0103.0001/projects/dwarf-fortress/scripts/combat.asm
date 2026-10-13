; combat.asm - Combat resolution (dwarf vs goblin)
; Uses rng_op for d20 to-hit, reads/writes HP from piece.pdl state

start:
    read_pdl R1 "STATE" "str" R2       ; dwarf_str
    read_pdl R5 "STATE" "hp" R6        ; goblin_hp

    ble R6, 0 monster_dead

    ; Goblin dodge check
    exec ./ops/+x/rng_op.+x 20
    read_pdl R7 "STATE" "last_roll" R8
    add R8, R8, R6                     ; goblin evasion = hp + roll
    ble R8, R2, goblin_hit
    console_print "The goblin dodges!"

    ; Goblin counterattacks
    read_pdl R10 "STATE" "str" R11
    read_pdl R1 "STATE" "hp" R2
    exec ./ops/+x/rng_op.+x 20
    read_pdl R7 "STATE" "last_roll" R8
    sub R12, R11, R8
    ble R12, 0 goblin_miss
    sub R2, R2, R12
    write_pdl "pieces/hero/piece.pdl" "STATE" "hp" R2
    console_print "The goblin slashes at you!"
    ble R2, 0 player_dead
    hit_frame
    halt

goblin_hit:
    ; Dwarf attacks
    sub R8, R2, R8
    ble R8, 0, dwarf_miss
    sub R6, R6, R8
    write_pdl "pieces/monster/piece.pdl" "STATE" "hp" R6
    console_print "Your pickaxe connects!"
    ble R6, 0 monster_dead
    hit_frame
    halt

dwarf_miss:
    console_print "Your attack misses!"
    read_pdl R10 "STATE" "str" R11
    read_pdl R1 "STATE" "hp" R2
    exec ./ops/+x/rng_op.+x 20
    read_pdl R7 "STATE" "last_roll" R8
    sub R12, R11, R8
    sub R2, R2, R12
    write_pdl "pieces/hero/piece.pdl" "STATE" "hp" R2
    console_print "The goblin stabs you in return!"
    ble R2, 0 player_dead
    hit_frame
    halt

goblin_miss:
    console_print "The goblin's attack glances off your armor!"
    hit_frame
    halt

monster_dead:
    console_print "The goblin screeches and falls! (+30 XP)"
    relay_write GOBLIN_DEATH|goblin_01|player=hero
    read_pdl R13 "STATE" "xp" R14
    addi R14, R14, 30
    write_pdl "pieces/hero/piece.pdl" "STATE" "xp" R14
    hit_frame
    halt

player_dead:
    console_print "You have fallen in battle. Your beard will be mourned."
    call_method die
    hit_frame
    halt
