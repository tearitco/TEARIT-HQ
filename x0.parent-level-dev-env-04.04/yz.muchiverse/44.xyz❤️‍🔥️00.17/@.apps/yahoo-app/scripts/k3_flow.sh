#!/usr/bin/env bash
# k3_flow.sh - headless (no /dev/tty) end-to-end flow test for yahoo-app's
# in-app Yahoo Finance broker-sim trading screen.
#
# Follows _.0.aigent-testing-k3.txt: headless parser+orchestrator subset,
# key injection into pieces/keyboard/history.txt as
#   [YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <code>
# (Enter=13, ESC=27, DOWN=1003, printable=ASCII), frame + state assertions.
#
# Flow under test:
#   bank -> Enter -> broker_select -> Enter -> broker.chtpm
#   cli_io sym_input/amt_input typing + Enter (saves to gui_state target_id)
#   METHOD button Enter (KEY:n -> inject '0'+n; n>=10 -> ':' ';' '<' '=') -> dispatch
#   LOOKUP_STOCK (offline simulated quote), DEBUG_LEDGER, master-ledger rows
#
# Navigator (broker.chtpm, is_navigable): 0=sym_field 1=amt_field
#   2..14=METHOD1..13 15=Back (16 items). DOWN=1003, UP=1002.
# METHOD n (1-based) button -> KEY:n+1 -> '0'+(n+1): 1='2'..8='9', 9=':',
#   10=';', 11='<', 12='=', 13='>'.
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HOUSE_DIR="$(cd "$APP_DIR/../.." && pwd)"

SESSION="/tmp/.yahoo-app-k3-$(date +%s)-$$"
LOG="$SESSION/flow.log"
PASS=0
FAIL=0

mkdir -p "$SESSION/pieces/system/widget_cmds" "$SESSION/pieces/display" \
         "$SESSION/pieces/apps/player_app" "$SESSION/pieces/keyboard" \
         "$SESSION/pieces/os" "$SESSION/projects/yahoo-app/manager" \
         "$SESSION/data"

ln -sfn "$APP_DIR/system" "$SESSION/system"
ln -sfn "$APP_DIR/ops" "$SESSION/ops"
ln -sfn "$APP_DIR/pal" "$SESSION/pal"
ln -sfn "$APP_DIR/default_op.txt" "$SESSION/default_op.txt"
ln -sfn "$APP_DIR/pieces/chtpm" "$SESSION/pieces/chtpm"
ln -sfn "$APP_DIR/pieces/registry" "$SESSION/pieces/registry"
ln -sfn "$APP_DIR/projects/yahoo-app/pieces" "$SESSION/projects/yahoo-app/pieces"
# Ledger: per-run SCRATCH copy, NOT the real app data dir - keeps the
# append-only master ledger clean of test rows and makes the buy/sell
# phase assertions idempotent (the real file used to leak an old manual
# buy row into phase 11's `|buy$` check).
: > "$SESSION/data/master_ledger.txt"
printf 'timestamp|epoch|player|turn|word|action_type\n' > "$SESSION/data/master_ledger.txt"

cd "$SESSION"

: > pieces/apps/player_app/interact_relay.txt
: > pieces/keyboard/history.txt
: > pieces/display/frame_changed.txt
: > pieces/display/renderer_pulse.txt
: > pieces/display/yahoo_screen_changed.txt
: > pieces/display/broker_screen_changed.txt
: > pieces/system/widget_cmds/inbox.txt
: > pieces/system/widget_cmds/status.txt
: > projects/yahoo-app/manager/gui_state.txt

cat > pieces/system/config.txt << 'EOCONFIG'
user_hash=
bank_balance=5000.00
current_broker=
last_lookup_symbol=
last_lookup_price=0.00
last_lookup_time=
EOCONFIG

cat > pieces/system/brokers.txt << 'EOBROKERS'
yahoo_finance|Yahoo Finance|full
EOBROKERS

mkdir -p "projects/yahoo-app/pieces/broker_select"
{
    printf 'SECTION      | KEY                | VALUE\n'
    printf '%s\n' '----------------------------------------'
    printf 'META         | piece_id           | broker_select\n'
    while IFS='|' read -r id name type rest; do
        id=$(echo "$id" | xargs)
        name=$(echo "$name" | xargs)
        type=$(echo "$type" | xargs)
        if [ -n "$id" ] && [ -n "$name" ]; then
            printf 'METHOD       | %s (%s)                | SELECT_BROKER:%s\n' "$name" "$type" "$id"
        fi
    done < pieces/system/brokers.txt
} > "projects/yahoo-app/pieces/broker_select/piece.pdl"

# Offline price seed: the yfin ops run with CWD = $SESSION (project root) and
# read yfin_master_list.txt / <SYM>.txt. fetch_stock's raw HTTP port-80 call
# to query2.finance.yahoo.com is NOT reachable from the headless k3 env, so
# seed a fresh (age 0) cached NVDA quote mirroring fetch_stock's JSON shape.
# Kept <= 3600s (lookup_stock CACHE_DURATION) for the whole run.
SEED_NOW=$(date +%Y-%m-%dT%H:%M:%S)
{
    printf '{"chart":{"result":[{"meta":{"regularMarketPrice":219.55,'
    printf '"chartPreviousClose":218.10}}],"error":null}}'
} > NVDA.txt
printf 'NVDA,219.55,%s\n' "$SEED_NOW" > yfin_master_list.txt

echo "$HOUSE_DIR" > pieces/system/house_root.txt

export PRISC_PROJECT_ROOT="$SESSION"
export PRISC_PROJECT_ID="yahoo-app"
export PAL_LAYOUT="pieces/chtpm/layouts/bank.chtpm"

setsid "$APP_DIR/system/orchestrator" </dev/null >> pieces/system/orchestrator.log 2>&1 &
ORCH_PID=$!

cleanup() {
    # Orchestrator runs in its own session (setsid): kill its whole process
    # group so prisc/renderer/keyboard_input children die with it instead of
    # lingering and holding the test runner's stdio open.
    kill -9 -- -"$ORCH_PID" 2>/dev/null || true
    kill "$ORCH_PID" 2>/dev/null || true
    sleep 0.2
    if [ "${KEEP_SESSION:-0}" = "1" ]; then
        say "session kept at $SESSION"
    else
        rm -rf "$SESSION" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

say()  { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG"; }
pass() { PASS=$((PASS+1)); say "PASS: $*"; }
fail() { FAIL=$((FAIL+1)); say "FAIL: $*"; }

inject() { echo "[2026-08-05 08:08:15] KEY_PRESSED: $1" >> pieces/keyboard/history.txt; }

get_layout() { tr -d '\r\n' < pieces/display/current_layout.txt 2>/dev/null; }

wait_for_layout() {
    local want="$1" tries="${2:-80}"
    while [ "$tries" -gt 0 ]; do
        [ "$(get_layout)" = "$want" ] && return 0
        sleep 0.25
        tries=$((tries-1))
    done
    return 1
}

wait_for_grep() {
    local file="$1" pat="$2" tries="${3:-80}"
    while [ "$tries" -gt 0 ]; do
        grep -q "$pat" "$file" 2>/dev/null && return 0
        sleep 0.25
        tries=$((tries-1))
    done
    return 1
}

# ---------------------------------------------------------------- phase 1: bank
say "=== PHASE 1: bank screen ==="
if wait_for_layout "pieces/chtpm/layouts/bank.chtpm" 120; then
    pass "booted into bank layout"
else
    fail "boot into bank layout (current_layout=$(get_layout))"
fi
if wait_for_grep pieces/apps/player_app/view.txt "Bank Balance" 40; then
    pass "bank frame shows Bank Balance"
else
    fail "bank frame missing Bank Balance"
fi

# ---------------------------------------------------------------- phase 2: broker_select
say "=== PHASE 2: broker select ==="
inject 13
if wait_for_layout "pieces/chtpm/layouts/broker_select.chtpm"; then
    pass "Enter -> broker_select layout"
else
    fail "Enter -> broker_select layout (current_layout=$(get_layout))"
fi

# ---------------------------------------------------------------- phase 3: broker screen
say "=== PHASE 3: broker trading screen ==="
inject 13
if wait_for_layout "pieces/chtpm/layouts/broker.chtpm"; then
    pass "Enter on Yahoo Finance -> broker layout"
else
    fail "Enter on Yahoo Finance -> broker layout (current_layout=$(get_layout))"
fi
sleep 1
if wait_for_grep pieces/apps/player_app/view.txt "Broker Balance" 40; then
    pass "broker frame rendered (game_map)"
else
    fail "broker frame not rendered"
fi
if grep -q "selected_broker=yahoo_finance" pieces/system/broker_state.txt 2>/dev/null; then
    pass "broker_state seeded selected_broker=yahoo_finance"
else
    fail "broker_state missing selected_broker"
fi

# ---------------------------------------------------------------- phase 4: ADD_CREDIT via cli_io + METHOD
say "=== PHASE 4: cli_io amt_input + ADD_CREDIT METHOD ==="
# Navigator: 0=sym_field 1=amt_field 2..13=METHOD1..12 14=Back (15 items).
# Initial focus 0. DOWN once -> amt_field; Enter to activate; type 100;
# Enter saves amt_input=100 (relayed 13 is ignored by the op); ESC deactivates.
inject 1003
inject 13
inject 49; inject 48; inject 48
inject 13
inject 27
# focus on amt_field(1); DOWN x3 -> 4 (METHOD3 Add Credit); Enter -> KEY:4 -> '4'.
inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Added \$100 credit" 60; then
    pass "ADD_CREDIT ran: message landed in gui_state"
else
    fail "ADD_CREDIT message missing in gui_state"
fi
if ls usr_acc.*.txt >/dev/null 2>&1; then
    pass "account file created (usr_acc.<hash>.txt)"
else
    fail "no account file created"
fi

# ---------------------------------------------------------------- phase 5: WATCHLIST_ADD
say "=== PHASE 5: WATCHLIST_ADD via sym_input cli_io + METHOD ==="
# focus on 4 (ADD_CREDIT). UP x4 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# focus on 0. DOWN x10 -> 10 (METHOD9 Add to Watchlist); Enter -> KEY:10 -> ':'.
for _ in 1 2 3 4 5 6 7 8 9 10; do inject 1003; done
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Added NVDA to watchlist" 60; then
    pass "WATCHLIST_ADD ran: message landed in gui_state"
else
    fail "WATCHLIST_ADD message missing in gui_state"
fi

# ---------------------------------------------------------------- phase 6: CHECK_BALANCE
say "=== PHASE 6: CHECK_BALANCE ==="
# focus on 10. UP x7 -> 3 (wrap) = METHOD2 Check Balance; Enter -> KEY:3 -> '3'.
for _ in 1 2 3 4 5 6 7; do inject 1002; done
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Bank: \$5000" 60; then
    pass "CHECK_BALANCE ran: bank balance shown"
else
    fail "CHECK_BALANCE message missing"
fi

# ---------------------------------------------------------------- phase 7: marker discipline
say "=== PHASE 7: marker discipline ==="
sleep 2
c1=$(wc -c < pieces/display/broker_screen_changed.txt)
sleep 2
c2=$(wc -c < pieces/display/broker_screen_changed.txt)
if [ "$c1" = "$c2" ]; then
    pass "broker_screen_changed stable between idle cycles ($c1 bytes)"
else
    fail "broker_screen_changed churned idle ($c1 -> $c2 bytes)"
fi

# ---------------------------------------------------------------- phase 8: LOOKUP_STOCK (simulated quote)
say "=== PHASE 8: LOOKUP_STOCK (offline simulated quote) ==="
# focus on 3. UP x3 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# focus on 0. DOWN x2 -> 2 (METHOD1 Lookup); Enter -> KEY:2 -> '2'.
inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "NVDA current price" 60; then
    pass "LOOKUP_STOCK ran: NVDA price fetched"
else
    fail "LOOKUP_STOCK did not produce a price"
fi

# ---------------------------------------------------------------- phase 9: DEBUG_LEDGER (METHOD 12)
say "=== PHASE 9: DEBUG_LEDGER (--debug--) ==="
# focus on 2 (LOOKUP). DOWN x11 -> 13 (METHOD12 --debug--); Enter -> KEY:13 -> '='.
for _ in 1 2 3 4 5 6 7 8 9 10 11; do inject 1003; done
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "master_ledger" 60; then
    pass "DEBUG_LEDGER ran: ledger path reported"
else
    fail "DEBUG_LEDGER message missing in gui_state"
fi

# ---------------------------------------------------------------- phase 10: master ledger rows
say "=== PHASE 10: master ledger recorded transactions ==="
if [ -f data/master_ledger.txt ]; then
    if grep -q "action_type" data/master_ledger.txt; then
        pass "ledger header present"
    else
        fail "ledger missing header"
    fi
    if grep -q "add_credit" data/master_ledger.txt; then
        pass "ledger has add_credit row from ADD_CREDIT"
    else
        fail "ledger missing add_credit row"
    fi
else
    fail "data/master_ledger.txt not created"
fi

# ---------------------------------------------------------------- phase 11: BUY_STOCK
say "=== PHASE 11: BUY_STOCK (top-up then buy NVDA) ==="
# focus on 13 (DEBUG_LEDGER). DOWN x3 -> wrap (13->14->15->0) to sym_field;
# Enter; type NVDA; Enter; ESC. (13 METHODS now push Back to index 15, so the
# wrap needs one extra DOWN vs the pre-BUY_OPTIONS 15-item navigator.)
inject 1003; inject 1003; inject 1003
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type 1000; Enter; ESC.
inject 1003
inject 13
inject 49; inject 48; inject 48; inject 48
inject 13
inject 27
# DOWN x3 -> 4 (METHOD3 Add Credit); Enter -> credit $1000 (balance must cover 1x NVDA ~$219).
inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Added \$1000 credit" 60; then
    pass "top-up credit ran before buy"
else
    fail "top-up credit message missing"
fi
# focus on 4. UP x4 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type 1 share; Enter; ESC.
inject 1003
inject 13
inject 49
inject 13
inject 27
# DOWN x4 -> 5 (METHOD4 Buy Stock); Enter -> KEY:5 -> '5'.
inject 1003; inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Bought" 60; then
    pass "BUY_STOCK ran: bought message landed"
else
    fail "BUY_STOCK message missing"
fi
if grep -q "|buy$" data/master_ledger.txt; then
    pass "ledger has buy row"
else
    fail "ledger missing buy row"
fi
if grep -q "stocks,NVDA,1.00" usr_acc.*.txt; then
    pass "NVDA holding recorded in account file"
else
    fail "account file missing NVDA holding"
fi

# ---------------------------------------------------------------- phase 12: SELL_STOCK
say "=== PHASE 12: SELL_STOCK (sell the NVDA share) ==="
# focus on 5 (BUY_STOCK). UP x5 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type 1 share; Enter; ESC.
inject 1003
inject 13
inject 49
inject 13
inject 27
# DOWN x5 -> 6 (METHOD5 Sell Stock); Enter -> KEY:6 -> '6'.
inject 1003; inject 1003; inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Sold" 60; then
    pass "SELL_STOCK ran: sold message landed"
else
    fail "SELL_STOCK message missing"
fi
if grep -q "|sell$" data/master_ledger.txt; then
    pass "ledger has sell row"
else
    fail "ledger missing sell row"
fi
if ! grep -q "stocks,NVDA" usr_acc.*.txt; then
    pass "NVDA holding removed after full sell"
else
    fail "NVDA still held after sell"
fi

# ---------------------------------------------------------------- phase 13: OPTIONS_PRICING
say "=== PHASE 13: OPTIONS_PRICING (Black-Scholes -> option_prices.NVDA.csv) ==="
# focus on 6 (SELL_STOCK). UP x6 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type strike 200; Enter; ESC.
inject 1003
inject 13
inject 50; inject 48; inject 48
inject 13
inject 27
# DOWN x10 -> 11 (METHOD10 Options Pricing); Enter -> KEY:11 -> ';'.
inject 1003; inject 1003; inject 1003; inject 1003; inject 1003; inject 1003
inject 1003; inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "option_prices.NVDA.csv" 60; then
    pass "OPTIONS_PRICING ran: csv confirmed"
else
    fail "OPTIONS_PRICING message missing"
fi
if [ -f option_prices.NVDA.csv ]; then
    pass "option_prices.NVDA.csv written (10 expirations)"
    if [ "$(wc -l < option_prices.NVDA.csv)" -ge 11 ]; then
        pass "csv has header + 10 rows"
    else
        fail "csv too short"
    fi
else
    fail "option_prices.NVDA.csv missing"
fi

# ---------------------------------------------------------------- phase 14: BUY_OPTIONS (METHOD 13)
say "=== PHASE 14: BUY_OPTIONS (METHOD 13, key '>') ==="
# focus on 11 (OPTIONS_PRICING). UP x11 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type 10000 top-up; Enter; ESC.
inject 1003
inject 13
inject 49; inject 48; inject 48; inject 48; inject 48
inject 13
inject 27
# DOWN x3 -> 4 (METHOD3 Add Credit); Enter -> credit so the option buy clears.
inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Added \$10000 credit" 60; then
    pass "top-up credit ran before option buy"
else
    fail "top-up credit message missing"
fi
# focus on 4. UP x4 -> 0 (sym_field); Enter; type NVDA; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002
inject 13
inject 78; inject 86; inject 68; inject 65
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type "1,1" (index 1, 1 contract); Enter; ESC.
inject 1003
inject 13
inject 49; inject 44; inject 49
inject 13
inject 27
# DOWN x13 -> 14 (METHOD13 Buy Options); Enter -> KEY:14 -> '>'.
inject 1003; inject 1003; inject 1003; inject 1003; inject 1003; inject 1003
inject 1003; inject 1003; inject 1003; inject 1003; inject 1003; inject 1003
inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Bought" 60; then
    pass "BUY_OPTIONS ran: bought message landed"
else
    fail "BUY_OPTIONS message missing"
fi
if grep -q "|buy_option" data/master_ledger.txt; then
    pass "ledger has buy_option row"
else
    fail "ledger missing buy_option row"
fi
if grep -q "Call,NVDA" usr_acc.*.txt; then
    pass "option holding recorded in account file"
else
    fail "account file missing option holding"
fi

# ---------------------------------------------------------------- phase 15: SELL_OPTIONS (METHOD 6)
say "=== PHASE 15: SELL_OPTIONS (sell the option back) ==="
# focus on 14 (BUY_OPTIONS). UP x14 -> 0 (sym_field); Enter; type option index 1; Enter; ESC.
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 1002; inject 1002; inject 1002; inject 1002; inject 1002; inject 1002
inject 1002; inject 1002
inject 13
inject 49
inject 13
inject 27
# DOWN x1 -> 1 (amt_field); Enter; type 1 contract; Enter; ESC.
inject 1003
inject 13
inject 49
inject 13
inject 27
# DOWN x6 -> 7 (METHOD6 Sell Options); Enter -> KEY:7 -> '7'.
inject 1003; inject 1003; inject 1003; inject 1003; inject 1003; inject 1003
inject 13
if wait_for_grep projects/yahoo-app/manager/gui_state.txt "Sold" 60; then
    pass "SELL_OPTIONS ran: sold message landed"
else
    fail "SELL_OPTIONS message missing"
fi
if grep -q "|sell_option" data/master_ledger.txt; then
    pass "ledger has sell_option row"
else
    fail "ledger missing sell_option row"
fi

# ---------------------------------------------------------------- summary
say ""
say "=== SUMMARY: $PASS PASS, $FAIL FAIL ==="
cp "$LOG" "$APP_DIR/FRAME_REPORT_k3_flow.txt"
if [ "$FAIL" -gt 0 ]; then
    say "VERDICT: FAIL - do not proceed"
    exit 1
fi
say "VERDICT: PASS - safe to proceed"
exit 0
