#!/bin/bash
# ensure_entities.sh - AUTO-GENERATES corp_*/gov_* pieces from wsr-pal's
# real source financial data, called automatically from button.sh's
# run/tick actions (NOT a manual pre-step) - per direct correction:
# "dont migrate the corps u have to auto gen them on start."
#
# IDEMPOTENT: only creates a piece if its state.txt doesn't already
# exist - never overwrites a piece that's already accumulated real
# trading state (cash/shares_held changes). This is "ensure the world
# is populated," not "reset the world every launch."
#
# Freshly-generated pieces default to decision_mode=1 (weighted) - the
# REAL ported fundamental-value/fiscal logic (see corp_decide.c/
# gov_decide.c), not decision_mode=0 (preset) - per direct correction:
# "were supposed to be using ai fsm when possible." preset stays
# available and settable per-entity, it's just no longer the default
# for auto-generated entities.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CORP_SRC="$SCRIPT_DIR/Mar\$.\$treetRace.wsr]Q]k32/corporations/generated"
GOV_SRC="$SCRIPT_DIR/Mar\$.\$treetRace.wsr]Q]k32/governments/generated"
DEST="$SCRIPT_DIR/projects/wsr-pal/pieces"

corp_created=0
corp_skipped=0
for dir in "$CORP_SRC"/*/; do
    ticker="$(basename "$dir")"
    profile="$dir/$ticker.txt"
    weights="$dir/weights.txt"
    [ -f "$profile" ] || continue

    piece_dir="$DEST/corp_$ticker"
    if [ -f "$piece_dir/state.txt" ]; then
        corp_skipped=$((corp_skipped + 1))
        continue
    fi

    cash=$(grep "Free Cash and Equivalents:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    stock_price=$(grep "Current Stock Price:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    book_value=$(grep "Equity (Net Worth):" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    shares_outstanding=$(grep "Shares of Stock Outstanding:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    market_cap=$(grep "Total Stock Capitalization:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    debt_to_equity=$(grep "Debt to Equity Ratio:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    risk_bias=50
    if [ -f "$weights" ]; then
        risk_bias=$(grep "risk" "$weights" | head -1 | grep -oE '[0-9]+' | head -1)
    fi
    if [ -z "$cash" ] || [ -z "$stock_price" ] || [ -z "$book_value" ] || \
       [ -z "$shares_outstanding" ] || [ -z "$market_cap" ] || [ -z "$debt_to_equity" ]; then
        continue
    fi

    mkdir -p "$piece_dir"
    cat > "$piece_dir/state.txt" <<EOF
current_state=0
decision_mode=1
cash=$cash
stock_price=$stock_price
book_value=$book_value
shares_outstanding=$shares_outstanding
market_cap=$market_cap
debt_to_equity=$debt_to_equity
risk_bias=$risk_bias
shares_held=0
pending_action=
last_action=
human_decision=
owned_by=
EOF
    corp_created=$((corp_created + 1))
done

gov_created=0
gov_skipped=0
for dir in "$GOV_SRC"/*/; do
    name="$(basename "$dir")"
    profile="$dir/financial_profile.txt"
    [ -f "$profile" ] || continue

    safe_name="$(echo "$name" | tr ' ' '_')"
    piece_dir="$DEST/gov_$safe_name"
    if [ -f "$piece_dir/state.txt" ]; then
        gov_skipped=$((gov_skipped + 1))
        continue
    fi

    cash=$(grep "Cash and Cash Equivalents:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    revenue=$(grep "Total Revenue:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    spending=$(grep "Net Cost of Operations:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    net_operating=$(grep "Net Operating (Cost)/Revenue:" "$profile" | head -1 | grep -oE '\-?[0-9]+\.[0-9]+' | head -1)
    gdp=$(grep "GDP (Nominal" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    debt_to_gdp=$(grep "Debt-to-GDP Ratio:" "$profile" | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    if [ -z "$cash" ] || [ -z "$revenue" ] || [ -z "$spending" ] || \
       [ -z "$net_operating" ] || [ -z "$gdp" ] || [ -z "$debt_to_gdp" ]; then
        continue
    fi

    mkdir -p "$piece_dir"
    cat > "$piece_dir/state.txt" <<EOF
current_state=0
decision_mode=1
cash=$cash
revenue=$revenue
spending=$spending
net_operating=$net_operating
gdp=$gdp
debt_to_gdp=$debt_to_gdp
tax_rate_adj=0.0
pending_action=
last_action=
human_decision=
EOF
    gov_created=$((gov_created + 1))
done

echo "corporations: $corp_created created, $corp_skipped already existed"
echo "governments:  $gov_created created, $gov_skipped already existed"
