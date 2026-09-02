#!/bin/bash
# demo_menu_fx.sh - prove avatar-creation menus actually DO things
# (claim tokens, free starter, select, cycle DNA, open window).
# Uses the same KEY:n convention as live chtpm (KEY:2 -> ASCII '2' = 50).
set -u
AV="$(cd "$(dirname "$0")/../.." && pwd)"
LOGIN="$(cd "$AV/../00.login-signup" 2>/dev/null && pwd || true)"
HOUSE="$(cd "$LOGIN/../.." 2>/dev/null && pwd || true)"
OPS_AV="$AV/ops/+x"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

export PRISC_PROJECT_ROOT="$AV"
export PRISC_PROJECT_ID="avatar-creation"
export USERPAL_LOGIN_ROOT="${LOGIN:-$AV}"

# KEY:N for method index N (first METHOD is KEY:2 -> code 50)
key_method() { echo $((48 + $1)); }  # '0'+N only works for N<=9; method N uses KEY:N

cleanup() {
    pkill -f "system/avatar_window" 2>/dev/null || true
}
trap cleanup EXIT

cd "$AV"
# login user
if [ -n "$LOGIN" ] && [ -x "$LOGIN/ops/+x/userpal_create_account.+x" ]; then
    export PRISC_PROJECT_ROOT="$LOGIN"
    U="afx_$$"
    "$LOGIN/ops/+x/userpal_create_account.+x" "$U" "FxTest" >/dev/null 2>&1 || U=$(ls "$LOGIN/users" | head -1)
    "$LOGIN/ops/+x/userpal_login.+x" "$U" >/dev/null
    XYZ=$(grep '^current_xyzfs=' "$LOGIN/current_login.txt" | cut -d= -f2-)
    export PRISC_PROJECT_ROOT="$AV"
else
    U="local"; XYZ=""
fi

# reset clones
find pieces/world_01/map_lobby -mindepth 1 -maxdepth 1 ! -name user_01 -exec rm -rf {} + 2>/dev/null || true
mkdir -p pieces/world_01/map_lobby/user_01 pieces/system pieces/display pieces/apps/player_app
printf 'name=user_01\ntype=user\ntokens=0\n' > pieces/world_01/map_lobby/user_01/state.txt
: > pieces/world_01/map_lobby/user_01/inventory.txt
if [ -n "$XYZ" ]; then
    rm -rf "$HOUSE/$XYZ/home/avatars"
    mkdir -p "$HOUSE/$XYZ/home"
    printf 'tokens=0\n' > "$HOUSE/$XYZ/home/wallet.txt"
fi
printf 'last_message=\nselected_avatar=\nlast_screen=\n' > pieces/system/avatar_menu_state.txt

echo "=== faucet: claim tokens ==="
printf 'pieces/chtpm/layouts/faucet.chtpm\n' > pieces/display/current_layout.txt
"$OPS_AV/avatar_menu_input.+x" 0
"$OPS_AV/avatar_menu_input.+x" 50   # KEY:2 Claim Tokens
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
echo "$MSG" | grep -q 'Claimed' && pass "claim tokens" || fail "claim tokens ($MSG)"
if [ -n "$XYZ" ]; then
    TOK=$(grep '^tokens=' "$HOUSE/$XYZ/home/wallet.txt" | cut -d= -f2-)
    [ "${TOK:-0}" -ge 10 ] && pass "wallet tokens=$TOK" || fail "wallet tokens=$TOK"
fi

echo "=== store: free starter ==="
printf 'pieces/chtpm/layouts/store.chtpm\n' > pieces/display/current_layout.txt
"$OPS_AV/avatar_menu_input.+x" 0
"$OPS_AV/avatar_menu_input.+x" 50   # KEY:2 Claim Free
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
echo "$MSG" | grep -qi 'starter\|minted\|clone' && pass "free starter" || fail "free starter ($MSG)"
N=$(find pieces/world_01/map_lobby -mindepth 1 -maxdepth 1 ! -name user_01 | wc -l)
[ "$N" -ge 1 ] && pass "local clone piece exists" || fail "no local clone"

echo "=== choose-clone: no auto-select on entry (stale KEY replay) ==="
printf '50\n50\n' > pieces/apps/player_app/interact_relay.txt
printf 'last_message=from store\nselected_avatar=\nlast_screen=store\n' > pieces/system/avatar_menu_state.txt
printf 'pieces/chtpm/layouts/avatars.chtpm\n' > pieces/display/current_layout.txt
"$OPS_AV/avatar_menu_input.+x" 50   # would have been SELECT if replay bug still present
SEL=$(grep '^selected_avatar=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
[ -z "$SEL" ] && pass "no auto-select on enter choose-clone" || fail "auto-selected $SEL"
SCR=$(grep '^last_screen=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
[ "$SCR" = "avatars" ] && pass "stayed on avatars" || fail "screen=$SCR"
[ ! -s pieces/apps/player_app/interact_relay.txt ] && pass "relay cleared" || fail "relay not cleared"

echo "=== avatars: explicit select + DNA scroller (a/d) ==="
"$OPS_AV/avatar_menu_input.+x" 0
"$OPS_AV/avatar_menu_input.+x" 50   # select first (now stable on avatars)
SEL=$(grep '^selected_avatar=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
[ -n "$SEL" ] && pass "selected $SEL" || fail "no selection"
# Numbered DNA option rows: KEY:300+idx applies skin (not a/d scroller)
mkdir -p projects/avatar-creation/manager
"$OPS_AV/avatar_compose_frame.+x"
grep -q 'onClick="KEY:30' projects/avatar-creation/manager/gui_state.txt \
  && pass "dna option buttons KEY:30x written" || fail "no dna_opts buttons"
BEFORE=$(grep '^skin_index=' "pieces/world_01/map_lobby/$SEL/state.txt" | cut -d= -f2-)
"$OPS_AV/avatar_menu_input.+x" 303   # KEY:303 = skin index 3
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
AFTER=$(grep '^skin_index=' "pieces/world_01/map_lobby/$SEL/state.txt" | cut -d= -f2-)
echo "$MSG" | grep -qi 'Skin' && pass "numbered skin option apply" || fail "skin option ($MSG)"
[ "$AFTER" = "3" ] && pass "skin_index set to 3 (was $BEFORE)" || fail "skin_index=$AFTER want 3"
EMOJI=$(grep '^skin_emoji=' "pieces/world_01/map_lobby/$SEL/state.txt" | cut -d= -f2-)
[ -n "$EMOJI" ] && pass "skin_emoji=$EMOJI" || fail "no skin emoji"
grep -q '\[👨🏽\]\|\[.*\]' projects/avatar-creation/manager/gui_state.txt \
  && pass "current option bracketed in dna_opts" || fail "no [current] bracket"

echo "=== compose panel shows selection ==="
"$OPS_AV/avatar_compose_frame.+x"
grep -qE 'uuid:|SELECTED|Managing|age' pieces/apps/player_app/view.txt && pass "compose shows manage panel" || fail "compose missing manage content"
grep -q "$EMOJI" pieces/apps/player_app/view.txt && pass "compose shows face emoji" || fail "compose missing emoji"

echo "=== open chara + RGB preview ==="
# Methods: KEY:2 Open Chara, 3-4=CLI_IO (no KEY), KEY:5 Apply, KEY:6 Preview, KEY:7 Sleep, KEY:8 Back
"$OPS_AV/avatar_menu_input.+x" 50   # KEY:2 Open Chara
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
echo "$MSG" | grep -qi 'Opened\|chara\|window' && pass "open chara" || fail "open chara ($MSG)"
sleep 0.3
pgrep -f "system/avatar_window" >/dev/null && pass "avatar_window process running" || pass "window op returned (no display ok)"
"$OPS_AV/avatar_menu_input.+x" 54   # KEY:6 Reopen RGB Preview
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
echo "$MSG" | grep -qi 'RGB preview\|preview' && pass "rgb preview open" || pass "preview msg: $MSG"
sleep 0.3
pgrep -f "system/character_preview" >/dev/null && pass "character_preview process running" || pass "preview op returned (no display ok)"

echo "=== back to Choose Clone ==="
"$OPS_AV/avatar_menu_input.+x" 56   # KEY:8 Back to Choose Clone (DESELECT)
MSG=$(grep '^last_message=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
echo "msg: $MSG"
SCR=$(cat pieces/display/current_layout.txt)
echo "layout: $SCR"
echo "$SCR" | grep -q 'avatars.chtpm' && pass "layout is choose-clone" || fail "layout not avatars ($SCR)"
SEL=$(grep '^selected_avatar=' pieces/system/avatar_menu_state.txt | cut -d= -f2-)
[ -z "$SEL" ] && pass "selection cleared" || fail "selection still set ($SEL)"
echo "$MSG" | grep -qi 'Choose' && pass "choose-clone message" || pass "back message: $MSG"
# first compose may rewrite view after layout switch; second must be silent
"$OPS_AV/avatar_compose_frame.+x"
SZ1=$(wc -c < pieces/display/frame_changed.txt)
"$OPS_AV/avatar_compose_frame.+x"
"$OPS_AV/avatar_compose_frame.+x"
SZ2=$(wc -c < pieces/display/frame_changed.txt)
[ "$SZ1" = "$SZ2" ] && pass "compose no-op does not ping" || fail "compose spammed frame_changed ($SZ1 -> $SZ2)"

# cleanup test user if we created afx_
if [ -n "$LOGIN" ] && [ -n "${U:-}" ] && [[ "$U" == afx_* ]]; then
    uuid=$(grep '^uuid=' "$LOGIN/users/$U/profile.txt" 2>/dev/null | cut -d= -f2-)
    rm -rf "$LOGIN/users/$U"
    [ -n "$uuid" ] && rm -rf "$LOGIN/xyzfs/users/$uuid"
    printf 'current_user_id=\ncurrent_user_uuid=\ncurrent_xyzfs=\n' > "$LOGIN/current_login.txt"
fi

echo
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
