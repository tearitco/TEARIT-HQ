#!/bin/bash
# demo_account_switch.sh — account list + Switch: METHOD on login screen.
# Direct ops + compose frames (reportable view.txt snapshots).
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$PROJECT_DIR/ops/+x"
PROOF="$PROJECT_DIR/proof/account-switch-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

export PRISC_PROJECT_ROOT="$PROJECT_DIR"
export PRISC_PROJECT_ID="user-pal"

USER_A="asw_a_$$"
USER_B="asw_b_$$"

cleanup() {
    for u in "$USER_A" "$USER_B"; do
        if [ -d "$PROJECT_DIR/users/$u" ]; then
            uuid=$(grep '^uuid=' "$PROJECT_DIR/users/$u/profile.txt" 2>/dev/null | cut -d= -f2-)
            rm -rf "$PROJECT_DIR/users/$u"
            [ -n "$uuid" ] && rm -rf "$PROJECT_DIR/xyzfs/users/$uuid"
        fi
    done
}
trap cleanup EXIT

snap() {
    local name="$1"
    mkdir -p "$PROJECT_DIR/pieces/system" "$PROJECT_DIR/pieces/apps/player_app" \
             "$PROJECT_DIR/pieces/display" "$PROJECT_DIR/projects/user-pal/manager"
    [ -f "$PROJECT_DIR/pieces/system/userpal_menu_state.txt" ] || \
        printf 'last_message=\n' > "$PROJECT_DIR/pieces/system/userpal_menu_state.txt"
    "$OPS/userpal_menu_input.+x" 0
    "$OPS/userpal_compose_frame.+x"
    cp "$PROJECT_DIR/pieces/apps/player_app/view.txt" "$PROOF/${name}.txt"
    echo "---- FRAME: $name ----"
    cat "$PROOF/${name}.txt"
    echo "---- END FRAME ----"
}

echo "=== build ==="
bash "$PROJECT_DIR/scripts/build.sh" >/dev/null

mkdir -p "$PROJECT_DIR/pieces/system" "$PROJECT_DIR/pieces/apps/player_app" \
         "$PROJECT_DIR/pieces/display" "$PROJECT_DIR/projects/user-pal/manager" \
         "$PROJECT_DIR/users" "$PROJECT_DIR/xyzfs/users"
printf 'last_message=Welcome to user-pal.\n' > "$PROJECT_DIR/pieces/system/userpal_menu_state.txt"
printf 'current_user_id=\ncurrent_user_uuid=\ncurrent_xyzfs=\n' > "$PROJECT_DIR/current_login.txt"
: > "$PROJECT_DIR/projects/user-pal/manager/gui_state.txt"

echo "=== frame 01: empty / guest ==="
snap "01_guest_start"

echo "=== create account A ==="
"$OPS/userpal_create_account.+x" "$USER_A" "Alice" | tee "$PROOF/create_a.txt"
"$OPS/userpal_login.+x" "$USER_A" | tee "$PROOF/login_a.txt"
printf 'last_message=Account created - logged in as %s.\n' "$USER_A" \
    > "$PROJECT_DIR/pieces/system/userpal_menu_state.txt"
snap "02_logged_in_A"

echo "=== create account B (no switch yet) ==="
"$OPS/userpal_create_account.+x" "$USER_B" "Bob" | tee "$PROOF/create_b.txt"
# stay as A for a moment
"$OPS/userpal_login.+x" "$USER_A" >/dev/null
printf 'last_message=Both accounts exist; still A.\n' \
    > "$PROJECT_DIR/pieces/system/userpal_menu_state.txt"
snap "03_both_accounts_still_A"

echo "=== regenerate methods / find SWITCH:B key ==="
"$OPS/userpal_menu_input.+x" 0
grep METHOD "$PROJECT_DIR/projects/user-pal/pieces/login/piece.pdl" | tee "$PROOF/methods.txt"
# method_idx starts at 2: Create=2, LogIn=3, LogOut=4, then Switch rows sorted by name
# KEY:n injects 'n' as char when n<=9, or raw n when n>9
# menu_input: key from KEY:n via send is '2'.. for 2-9
# resolved = key-'0'-1 for digit chars

# Count METHOD lines and find SWITCH:B index (1-based among METHOD rows)
IDX=0
KEY_FOR_B=""
while IFS= read -r line; do
    IDX=$((IDX + 1))
    if echo "$line" | grep -q "SWITCH:$USER_B"; then
        # method_idx in parser starts at 2 for first METHOD
        KEY_FOR_B=$((IDX + 1))
        break
    fi
done < <(grep '^METHOD' "$PROJECT_DIR/projects/user-pal/pieces/login/piece.pdl")

if [ -z "$KEY_FOR_B" ]; then
    fail "SWITCH:$USER_B not in piece.pdl methods"
else
    pass "SWITCH B is METHOD row -> KEY:$KEY_FOR_B"
fi

echo "=== switch to B via menu_input KEY:$KEY_FOR_B ==="
# For KEY:n with n>=10, inject raw n; menu uses key>9 -> resolved=key-1
# For n 2-9, inject char 'n' which is ASCII 48+n
if [ "$KEY_FOR_B" -le 9 ]; then
    KEY_CODE=$((48 + KEY_FOR_B))
else
    KEY_CODE=$KEY_FOR_B
fi
"$OPS/userpal_menu_input.+x" "$KEY_CODE"
snap "04_switched_to_B"

CUR=$(grep '^current_user_id=' "$PROJECT_DIR/current_login.txt" | cut -d= -f2-)
if [ "$CUR" = "$USER_B" ]; then
    pass "current_login is B ($CUR)"
else
    fail "expected B, got '$CUR'"
fi
grep -q "Switched account" "$PROJECT_DIR/pieces/system/userpal_menu_state.txt" \
    && pass "last_message says switched" || fail "no switch message"

echo "=== switch back to A ==="
IDX=0
KEY_FOR_A=""
while IFS= read -r line; do
    IDX=$((IDX + 1))
    if echo "$line" | grep -q "SWITCH:$USER_A"; then
        KEY_FOR_A=$((IDX + 1))
        break
    fi
done < <(grep '^METHOD' "$PROJECT_DIR/projects/user-pal/pieces/login/piece.pdl")
if [ -n "$KEY_FOR_A" ]; then
    if [ "$KEY_FOR_A" -le 9 ]; then KEY_CODE=$((48 + KEY_FOR_A)); else KEY_CODE=$KEY_FOR_A; fi
    "$OPS/userpal_menu_input.+x" "$KEY_CODE"
    snap "05_switched_to_A"
    CUR=$(grep '^current_user_id=' "$PROJECT_DIR/current_login.txt" | cut -d= -f2-)
    [ "$CUR" = "$USER_A" ] && pass "current_login is A" || fail "expected A got $CUR"
else
    fail "SWITCH A not found"
fi

echo "=== methods include * marker on current ==="
grep "SWITCH:$USER_A" "$PROJECT_DIR/projects/user-pal/pieces/login/piece.pdl" | grep -q '\*' \
    && pass "current account marked with *" || fail "no * marker on A"

echo
echo "Proof frames in: $PROOF"
if [ "$FAIL" -eq 0 ]; then echo "=== OVERALL: PASS ==="; exit 0; fi
echo "=== OVERALL: FAIL ==="; exit 1
