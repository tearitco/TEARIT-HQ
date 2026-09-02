#!/bin/bash
# buy_batteries.sh - v1 buy-batteries op. Spends gold on battery packs.
# Each pack: 5 gold -> +20 battery.
# Usage: buy_batteries.sh <qty>
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

QTY="${1:-1}"
[ "$QTY" -lt 1 ] && QTY=1
gold="$(read_state gold)"
battery="$(read_state battery)"

cost=$((QTY * 5))
if [ "$gold" -lt "$cost" ]; then
    ledger_append "BuyBatteries" "wanted $QTY pack(s) but only $gold gold (need $cost)" "buy_batteries.sh"
    echo "BuyBatteries: not enough gold ($gold < $cost)"
    exit 0
fi
gain=$((QTY * 20))
write_state gold $((gold - cost))
write_state battery $((battery + gain))
ledger_append "BuyBatteries" "bought $QTY pack(s) for $cost gold, battery +$gain" "buy_batteries.sh"
echo "BuyBatteries: gold $gold->$((gold-cost)), battery $battery->$((battery+gain))"
