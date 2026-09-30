#!/bin/bash
# charge.sh - v1 charge op. Spends stored energy to refill battery.
# Usage: charge.sh <amount>
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

AMT="${1:-10}"
battery="$(read_state battery)"
energy="$(read_state energy)"

[ "$energy" -lt 1 ] && energy=0
if [ "$energy" -le 0 ]; then
    ledger_append "Charge" "cannot charge - out of energy (battery $battery)" "charge.sh"
    echo "Charge: no energy left"
    exit 0
fi
if [ "$AMT" -gt "$energy" ]; then AMT="$energy"; fi
if [ "$AMT" -lt 1 ]; then AMT=1; fi

write_state battery $((battery + AMT))
write_state energy $((energy - AMT))
ledger_append "Charge" "spent $AMT energy, battery $battery->$((battery+AMT))" "charge.sh"
echo "Charge: battery $battery->$((battery+AMT)) (-$AMT energy)"
