#!/bin/bash
# battle.sh - v1 battle op. Picks a muchi entity (target=argv), rolls a
# damage HP-exchange, gains xp. Writes to its own ledger (house format).
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

TARGET="$1"
[ -z "$TARGET" ] && TARGET="m6_golddeity"

ENTITY_DIR="$HOUSE_ROOT/*.monads/*.muchi-pet/entities/$TARGET"
if [ ! -f "$ENTITY_DIR/hp.txt" ]; then
    ledger_append "Battle" "no such muchi entity: $TARGET" "battle.sh"
    echo "Battle: no such entity $TARGET"
    exit 1
fi

hp="$(cat "$ENTITY_DIR/hp.txt" 2>/dev/null | tr -d ' \n')"
[ -z "$hp" ] && hp=0
hp_max="$(cat "$ENTITY_DIR/hp_max.txt" 2>/dev/null | tr -d ' \n')"
[ -z "$hp_max" ] || [ "$hp_max" -eq 0 ] 2>/dev/null && hp_max=100

dmg=$((RANDOM % 15 + 5))
new_hp=$((hp - dmg))
[ "$new_hp" -lt 0 ] && new_hp=0
printf '%s' "$new_hp" > "$ENTITY_DIR/hp.txt"

xp_gain=$((dmg * 2))
xp="$(read_state xp)"
write_state xp $((xp + xp_gain))

ledger_append "Battle" "fought $TARGET, dealt $dmg dmg (hp $hp->$new_hp), gained $xp_gain xp" "battle.sh"
echo "Battle: $TARGET hp $hp->$new_hp (-$dmg), +$xp_gain xp"
