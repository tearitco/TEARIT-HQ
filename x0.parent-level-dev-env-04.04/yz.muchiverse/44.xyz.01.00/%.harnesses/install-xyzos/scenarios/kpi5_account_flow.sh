#!/bin/bash

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# kpi5_account_flow.sh - KPI#5: full identity flow on the INSTALLED apps via
# real key injection: create account -> auto-login -> logout -> login (same
# uuid) -> whoami -> assert the xyzfs/users/<uuid>/home-🏠️ tree appears.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="${1:?house root arg required}"
INSTALLER="${2:?installer path arg required}"
OPS="$HARNESS_DIR/ops/+x"

DEST="/tmp/xyzos-test-user"
LOGIN_ROOT="$DEST/xyzos/apps/00.login-signup"
PROOF_DIR="$HARNESS_DIR/proof/kpi5-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

UUID_A=""

cleanup() {
    echo; echo "--- cleanup ---"
    ps aux | grep -E "xyzos-test-user/xyzos/apps/00.login-signup" | grep -v grep \
        | awk '{print $2}' | xargs -r kill -9 2>/dev/null
    ps aux | grep -E "system/(chtpm_parser_pal|renderer|keyboard_input)|prisc\+x" | grep -v grep \
        | awk '{print $2}' | xargs -r kill -9 2>/dev/null
    sleep 1
    rm -rf "$DEST"
}
trap cleanup EXIT

key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.2; }
check()  { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3" >/dev/null; [ $? -ne 0 ] && FAIL=1; }

fill_field() {
    local sess="$1" frame="$2" label="$3" value="$4"
    focus "$sess" "$frame" "$label"; key "$sess" 13
    for _ in $(seq 1 24); do "$OPS/tk_inject_key.+x" "$sess" 127; done
    type_ "$sess" "$value"; key "$sess" 27
}

# Prints ONLY the uuid on stdout. Logs go to stderr.
assert_user_xyzfs() {
    local user="$1" label="$2"
    local profile="$LOGIN_ROOT/users/$user/profile.txt"
    if [ ! -f "$profile" ]; then
        fail "$label: missing profile $profile" >&2
        return 1
    fi
    local uuid xyzfs_path
    uuid=$(grep '^uuid=' "$profile" | cut -d= -f2-)
    xyzfs_path=$(grep '^xyzfs_path=' "$profile" | cut -d= -f2-)
    if [ -z "$uuid" ]; then fail "$label: profile has no uuid=" >&2; return 1; fi
    if [ -z "$xyzfs_path" ]; then fail "$label: profile has no xyzfs_path=" >&2; return 1; fi
    if [ "$xyzfs_path" != "xyzfs/users/$uuid" ]; then
        fail "$label: xyzfs_path='$xyzfs_path' expected 'xyzfs/users/$uuid'" >&2
        return 1
    fi
    for d in "home" "projects"; do
        if [ -d "$LOGIN_ROOT/$xyzfs_path/$d" ]; then
            pass "$label: $xyzfs_path/$d exists" >&2
        else
            fail "$label: missing $LOGIN_ROOT/$xyzfs_path/$d" >&2
        fi
    done
    if [ -f "$LOGIN_ROOT/$xyzfs_path/meta.txt" ]; then
        pass "$label: xyzfs meta.txt exists" >&2
        cp "$LOGIN_ROOT/$xyzfs_path/meta.txt" "$PROOF_DIR/${label}_xyzfs_meta.txt"
    else
        fail "$label: missing xyzfs meta.txt" >&2
    fi
    if [ -d "$LOGIN_ROOT/$xyzfs_path/home" ]; then
        : > "$PROOF_DIR/${label}_home_dir.txt"
        pass "$label: home-🏠️ tree present ($xyzfs_path/home)" >&2
    fi
    pass "$label: uuid=$uuid xyzfs=$xyzfs_path" >&2
    cp "$profile" "$PROOF_DIR/${label}_profile.txt"
    printf '%s\n' "$uuid"
    return 0
}

echo "=== KPI#5: identity flow on the installed apps ==="
rm -rf "$DEST"
mkdir -p "$DEST"

echo "--- install ---"
if ! bash "$INSTALLER" "$HOUSE" "$DEST" >"$PROOF_DIR/install.log" 2>&1; then
    fail "installer exited nonzero"
    tail -40 "$PROOF_DIR/install.log"
    exit 1
fi
pass "installer ran (exit 0)"

echo "--- boot installed login ---"
cd "$DEST/xyzos"
$SETSID bash button.sh run > /tmp/th_install_kpi5.log 2>&1 </dev/null & disown
sleep 3
SESS=$(ls -dt "$LOGIN_ROOT/pieces/sessions/"*/ 2>/dev/null | head -1)
SESS="${SESS%/}"
if [ -z "$SESS" ] || [ ! -d "$SESS" ]; then
    fail "no session dir under installed login"
    tail -40 /tmp/th_install_kpi5.log 2>/dev/null
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
READY=0
for i in $(seq 1 40); do
    if [ -s "$FRAME" ] && grep -qE "Not logged in|U S E R - P A L" "$FRAME" 2>/dev/null; then READY=1; break; fi
    sleep 0.2
done
cp "$FRAME" "$PROOF_DIR/00_initial_frame.txt" 2>/dev/null || true
if [ "$READY" = "1" ]; then
    pass "installed login composed its frame"
else
    fail "installed login never composed a frame"
    exit 1
fi
check "$FRAME" "Not logged in." "starts logged out"

USER="installUser_$$"
DISP="InstallHarness"
echo "session: $SESS"
echo "user:    $USER"

echo "--- create account $USER (uuid + xyzfs tree) ---"
fill_field "$SESS" "$FRAME" "User ID" "$USER"
fill_field "$SESS" "$FRAME" "Display Name" "$DISP"
focus "$SESS" "$FRAME" "Create Account"; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/01_after_signup.txt"
check "$FRAME" "Logged in as: $USER" "frame shows logged in as $USER"
check "$FRAME" "Account created" "last_message reports account created"
check "$FRAME" "uuid:" "frame shows uuid status line"
check "$FRAME" "xyzfs:" "frame shows xyzfs status line"
UUID_A=$(assert_user_xyzfs "$USER" "userA") || UUID_A=""
if grep -q "current_user_id=$USER" "$LOGIN_ROOT/current_login.txt" \
   && grep -q "current_user_uuid=" "$LOGIN_ROOT/current_login.txt" \
   && grep -q "current_xyzfs=xyzfs/users/" "$LOGIN_ROOT/current_login.txt"; then
    pass "current_login.txt has id + uuid + xyzfs"
    cp "$LOGIN_ROOT/current_login.txt" "$PROOF_DIR/01_current_login.txt"
else
    fail "current_login.txt incomplete after signup"
    cp "$LOGIN_ROOT/current_login.txt" "$PROOF_DIR/01_current_login.txt" 2>/dev/null || true
fi

echo "--- log out ---"
focus "$SESS" "$FRAME" "Log Out"; key "$SESS" 13
sleep 0.8
cp "$FRAME" "$PROOF_DIR/02_after_logout.txt"
check "$FRAME" "Not logged in." "logged out"

echo "--- log in as $USER again (same uuid) ---"
fill_field "$SESS" "$FRAME" "User ID" "$USER"
focus "$SESS" "$FRAME" "Log In"; key "$SESS" 13
sleep 0.8
cp "$FRAME" "$PROOF_DIR/03_after_login.txt"
check "$FRAME" "Logged in as: $USER" "re-login as $USER"
if [ -n "$UUID_A" ] && grep -q "current_user_uuid=$UUID_A" "$LOGIN_ROOT/current_login.txt"; then
    pass "re-login restored same uuid $UUID_A"
else
    fail "re-login did not restore uuid $UUID_A"
fi

echo "--- whoami op ---"
WHOAMI_OUT=$( (cd "$LOGIN_ROOT" && ./ops/+x/userpal_whoami.+x) )
echo "$WHOAMI_OUT" > "$PROOF_DIR/04_whoami.out"
if [ -n "$UUID_A" ] && [ "$WHOAMI_OUT" = "$USER uuid=$UUID_A xyzfs=xyzfs/users/$UUID_A" ]; then
    pass "whoami: $WHOAMI_OUT"
else
    fail "whoami output unexpected: '$WHOAMI_OUT'"
fi

cp "$LOGIN_ROOT/current_login.txt" "$PROOF_DIR/03_current_login.txt"
cp "$DEST/xyzos/paths.pdl" "$PROOF_DIR/paths.pdl"
: > "$PROOF_DIR/user_home_note.txt"
echo "home-🏠️ = $LOGIN_ROOT/xyzfs/users/$UUID_A/home (physical dir 'home' in v1)" \
    >> "$PROOF_DIR/user_home_note.txt"

echo
echo "=== KPI#5 proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
