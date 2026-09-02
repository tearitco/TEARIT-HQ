#!/bin/bash
# tick_all.sh <rounds> - ticks EVERY corp_*/gov_* piece under
# projects/wsr-pal/pieces/, dynamic discovery (scan the directory for
# every piece of a type, no hardcoded entity list) - this is
# mutaclsym's own cited Moke-Pet epoch-loop pattern, applied here
# because prisc+x's real read_state opcode resolves a piece's literal
# id at .pal PARSE time, not from a runtime register - true dynamic
# per-piece dispatch isn't structurally possible from .pal source
# alone (confirmed by reading system/prisc+x.c directly), so the
# "which entity's turn is it" loop lives here, in a driver, while each
# individual entity's own idle->deciding->trading FSM dispatch still
# goes through the exact same corp_*/gov_* ops the single-piece .pal
# scripts use - the FSM logic isn't reimplemented, just invoked in a
# loop instead of from a fixed-piece-id .pal file.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROUNDS="${1:-1}"
export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
cd "$SCRIPT_DIR"

tick_one() {
    local piece_id="$1"
    local prefix="$2"
    local action_verb="${3:-trade}" # corp_*/gov_* use "trade", realestate_* uses "act"
    local state_path="projects/wsr-pal/pieces/$piece_id/state.txt"
    [ -f "$state_path" ] || return
    local current_state
    current_state=$(grep '^current_state=' "$state_path" | head -1 | cut -d= -f2)
    case "$current_state" in
        0) "./ops/+x/${prefix}_tick_idle.+x" "$piece_id" > /dev/null ;;
        1) "./ops/+x/${prefix}_decide.+x" "$piece_id" ;;
        2) "./ops/+x/${prefix}_${action_verb}.+x" "$piece_id" ;;
    esac
}

# pop_*/weather_* are 2-state (idle/updating - see pop_update.c/
# weather_update.c's own header comments for why there's no separate
# "deciding" state) - a distinct tick_one shape, not the 3-state
# corp/gov/realestate one above.
tick_one_2state() {
    local piece_id="$1"
    local prefix="$2"
    local state_path="projects/wsr-pal/pieces/$piece_id/state.txt"
    [ -f "$state_path" ] || return
    local current_state
    current_state=$(grep '^current_state=' "$state_path" | head -1 | cut -d= -f2)
    case "$current_state" in
        0) "./ops/+x/${prefix}_tick_idle.+x" "$piece_id" > /dev/null ;;
        1) "./ops/+x/${prefix}_update.+x" "$piece_id" ;;
    esac
}

# Real analysis_loop.c mechanic, ported (had never been wired in
# before - corp_decide.c's fundamental_value() only ever picked buy/
# sell/hold, nothing wrote a new stock_price back so prices never
# actually moved). Runs ONCE per End Turn, BEFORE the idle->decide->
# trade round loop below - matches the real day_loop.c -> analysis_
# loop.c ordering (a day's price is set before anyone reacts to it
# that day). wsr_news_op.c then reports the day's biggest movers,
# same as the real news_loop.c writing data/news.txt.
for dir in projects/wsr-pal/pieces/corp_*/; do
    [ -d "$dir" ] || continue
    piece_id="$(basename "$dir")"
    # Real per-turn financial effects (bond/loan interest, R&D/
    # marketing/growth spend, dividend payout) BEFORE the price
    # formula, same ordering principle as this pass itself running
    # before the idle->decide->trade round loop - this turn's spend
    # should feed into this turn's valuation, not next turn's.
    "./ops/+x/corp_apply_finances.+x" "$piece_id"
    "./ops/+x/corp_update_price.+x" "$piece_id"
done
"./ops/+x/wsr_news_op.+x"
"./ops/+x/player_settle_futures.+x" > /dev/null

for round in $(seq 1 "$ROUNDS"); do
    echo "=== round $round/$ROUNDS ==="
    for dir in projects/wsr-pal/pieces/corp_*/; do
        [ -d "$dir" ] || continue
        piece_id="$(basename "$dir")"
        tick_one "$piece_id" "corp"
    done
    for dir in projects/wsr-pal/pieces/gov_*/; do
        [ -d "$dir" ] || continue
        piece_id="$(basename "$dir")"
        tick_one "$piece_id" "gov"
    done
    for dir in projects/wsr-pal/pieces/realestate_*/; do
        [ -d "$dir" ] || continue
        piece_id="$(basename "$dir")"
        tick_one "$piece_id" "realestate" "act"
    done
    for dir in projects/wsr-pal/pieces/pop_*/; do
        [ -d "$dir" ] || continue
        piece_id="$(basename "$dir")"
        tick_one_2state "$piece_id" "pop"
    done
    for dir in projects/wsr-pal/pieces/weather_*/; do
        [ -d "$dir" ] || continue
        piece_id="$(basename "$dir")"
        tick_one_2state "$piece_id" "weather"
    done
done
