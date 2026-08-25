#!/bin/bash
# demo_login_signup.sh - reference SCENARIO for user-pal login GUI.
# Real keystrokes only through chtpm + cli_io (see
# #.haiku+/!.local-ux-testing-ai.txt).
#
# Proves:
#   create account (auto-login + UUID + xyzfs/users/<uuid>/ tree)
#   logout / login existing user (current_login carries uuid+xyzfs)
#   second distinct user gets a DIFFERENT uuid and xyzfs path
#   unknown-user login refused
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
# Per-user xyzfs homes are HOUSE-level state (<house>/xyzfs/users/<uuid>,
# matching ops' resolve_house_root() + button.sh's own HOUSE_ROOT export) -
# NOT project-level, and NOT session-local.
HOUSE_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"

PROOF_DIR="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

UUID_A=""
UUID_B=""

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$HARNESS_DIR/button.sh" kill
    # Remove throwaway test accounts + their uuid-tagged xyzfs trees.
    for u in ${USER_A:-} ${USER_B:-}; do
        if [ -n "$u" ] && [ -d "$PROJECT_DIR/users/$u" ]; then
            uuid=$(grep '^uuid=' "$PROJECT_DIR/users/$u/profile.txt" 2>/dev/null | cut -d= -f2-)
            rm -rf "$PROJECT_DIR/users/$u"
            echo "removed test user dir users/$u"
            if [ -n "$uuid" ] && [ -d "$HOUSE_DIR/xyzfs/users/$uuid" ]; then
                rm -rf "$HOUSE_DIR/xyzfs/users/$uuid"
                echo "removed xyzfs/users/$uuid"
            fi
        fi
    done
}
trap cleanup EXIT

key()    { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.2; }
type_()  { "$OPS/tk_type_text.+x" "$1" "$2"; sleep 0.2; }
focus()  { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.2; }
check()  { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }

fill_field() {
    local sess="$1" frame="$2" label="$3" value="$4"
    focus "$sess" "$frame" "$label"; key "$sess" 13
    for _ in $(seq 1 24); do "$OPS/tk_inject_key.+x" "$sess" 127; done
    type_ "$sess" "$value"; key "$sess" 27
}

# Prints ONLY the uuid on stdout (for capture). Logs go to stderr.
assert_user_xyzfs() {
    local user="$1" label="$2"
    # Mid-session: accounts live in the SESSION copy (copy-based strategy;
    # button.sh's persist_session_state() only fans them back out to
    # $PROJECT_DIR after the session ends - verified separately below).
    local root="$SESS"
    local profile="$root/users/$user/profile.txt"
    if [ ! -f "$profile" ]; then
        fail "$label: missing profile $profile" >&2
        return 1
    fi
    local uuid xyzfs_path
    uuid=$(grep '^uuid=' "$profile" | cut -d= -f2-)
    xyzfs_path=$(grep '^xyzfs_path=' "$profile" | cut -d= -f2-)
    if [ -z "$uuid" ]; then
        fail "$label: profile has no uuid=" >&2
        return 1
    fi
    if [ -z "$xyzfs_path" ]; then
        fail "$label: profile has no xyzfs_path=" >&2
        return 1
    fi
    if [ "$xyzfs_path" != "xyzfs/users/$uuid" ]; then
        fail "$label: xyzfs_path='$xyzfs_path' expected 'xyzfs/users/$uuid'" >&2
        return 1
    fi
    if [ ! -d "$HOUSE_DIR/xyzfs/users/$uuid/home" ]; then
        fail "$label: missing $HOUSE_DIR/xyzfs/users/$uuid/home" >&2
        return 1
    fi
    if [ ! -d "$HOUSE_DIR/xyzfs/users/$uuid/projects" ]; then
        fail "$label: missing $HOUSE_DIR/xyzfs/users/$uuid/projects" >&2
        return 1
    fi
    if [ ! -f "$HOUSE_DIR/xyzfs/users/$uuid/meta.txt" ]; then
        fail "$label: missing xyzfs meta.txt" >&2
        return 1
    fi
    pass "$label: uuid=$uuid xyzfs=$xyzfs_path" >&2
    cp "$profile" "$PROOF_DIR/${label}_profile.txt"
    cp "$HOUSE_DIR/xyzfs/users/$uuid/meta.txt" "$PROOF_DIR/${label}_xyzfs_meta.txt"
    printf '%s\n' "$uuid"
    return 0
}

echo "=== user-pal login + uuid/xyzfs multi-user scenario ==="
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
sleep 1

printf 'current_user_id=\ncurrent_user_uuid=\ncurrent_xyzfs=\n' > "$PROJECT_DIR/current_login.txt"
mkdir -p "$PROJECT_DIR/xyzfs/bin" "$PROJECT_DIR/xyzfs/users"

cd "$PROJECT_DIR"
setsid bash button.sh run > /tmp/th_userpal_login.log 2>&1 < /dev/null & disown
sleep 3

SESS=$(ls -dt pieces/sessions/*/ 2>/dev/null | head -1)
SESS="${SESS%/}"
if [ -z "$SESS" ] || [ ! -d "$SESS" ]; then
    fail "no session dir created under pieces/sessions/"
    cat /tmp/th_userpal_login.log 2>/dev/null | tail -40
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
USER_A="uloginA_$$"
USER_B="uloginB_$$"
DISP_A="LoginHarnessA"
DISP_B="LoginHarnessB"

echo "session: $SESS"
echo "user A:  $USER_A"
echo "user B:  $USER_B"

READY=0
for i in $(seq 1 40); do
    if [ -s "$FRAME" ] && grep -qE "Not logged in|Logged in as:|U S E R - P A L" "$FRAME" 2>/dev/null; then
        READY=1
        break
    fi
    sleep 0.2
done
cp "$FRAME" "$PROOF_DIR/00_initial_frame.txt" 2>/dev/null || true
if [ "$READY" != "1" ]; then
    fail "composed login frame never appeared"
else
    check "$FRAME" "Not logged in" "starts logged out"
fi

echo "--- create account $USER_A (uuid + xyzfs) ---"
fill_field "$SESS" "$FRAME" "User ID" "$USER_A"
fill_field "$SESS" "$FRAME" "Display Name" "$DISP_A"
focus "$SESS" "$FRAME" "Create Account"; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/01_after_signup_A.txt"
check "$FRAME" "Logged in as: $USER_A" "frame shows logged in as $USER_A"
check "$FRAME" "Account created" "last_message reports account created"
check "$FRAME" "uuid:" "frame shows uuid status line after signup"
check "$FRAME" "xyzfs:" "frame shows xyzfs status line after signup"
UUID_A=$(assert_user_xyzfs "$USER_A" "userA") || UUID_A=""
if grep -q "current_user_id=$USER_A" "$SESS/current_login.txt" \
   && grep -q "current_user_uuid=" "$SESS/current_login.txt" \
   && grep -q "current_xyzfs=xyzfs/users/" "$SESS/current_login.txt"; then
    pass "current_login.txt has user_id + uuid + xyzfs for A"
    cp "$SESS/current_login.txt" "$PROOF_DIR/01_current_login_A.txt"
else
    fail "current_login.txt incomplete after signup A"
    cp "$SESS/current_login.txt" "$PROOF_DIR/01_current_login_A.txt" 2>/dev/null || true
fi

echo "--- log out ---"
focus "$SESS" "$FRAME" "Log Out"; key "$SESS" 13
sleep 0.8
cp "$FRAME" "$PROOF_DIR/02_after_logout.txt"
check "$FRAME" "Not logged in" "logged out"

echo "--- log in as existing $USER_A (same uuid/xyzfs) ---"
fill_field "$SESS" "$FRAME" "User ID" "$USER_A"
focus "$SESS" "$FRAME" "Log In"; key "$SESS" 13
sleep 0.8
cp "$FRAME" "$PROOF_DIR/03_after_login_A.txt"
check "$FRAME" "Logged in as: $USER_A" "re-login as $USER_A"
if [ -n "$UUID_A" ] && grep -q "current_user_uuid=$UUID_A" "$SESS/current_login.txt"; then
    pass "re-login restored same uuid $UUID_A"
else
    fail "re-login did not restore uuid $UUID_A"
fi

echo "--- log out, create second user $USER_B ---"
focus "$SESS" "$FRAME" "Log Out"; key "$SESS" 13
sleep 0.6
fill_field "$SESS" "$FRAME" "User ID" "$USER_B"
fill_field "$SESS" "$FRAME" "Display Name" "$DISP_B"
focus "$SESS" "$FRAME" "Create Account"; key "$SESS" 13
sleep 1
cp "$FRAME" "$PROOF_DIR/04_after_signup_B.txt"
check "$FRAME" "Logged in as: $USER_B" "frame shows logged in as $USER_B"
UUID_B=$(assert_user_xyzfs "$USER_B" "userB") || UUID_B=""

if [ -n "$UUID_A" ] && [ -n "$UUID_B" ] && [ "$UUID_A" != "$UUID_B" ]; then
    pass "two users have distinct uuids ($UUID_A vs $UUID_B)"
else
    fail "uuids not distinct (A='$UUID_A' B='$UUID_B')"
fi
if [ -n "$UUID_A" ] && [ -n "$UUID_B" ] \
   && [ -d "$HOUSE_DIR/xyzfs/users/$UUID_A" ] \
   && [ -d "$HOUSE_DIR/xyzfs/users/$UUID_B" ]; then
    pass "both xyzfs/users/<uuid>/ trees coexist"
else
    fail "multi-user xyzfs trees missing (A='$UUID_A' B='$UUID_B')"
fi
echo "--- log out, refuse unknown user ---"
focus "$SESS" "$FRAME" "Log Out"; key "$SESS" 13
sleep 0.6
GHOST="uloginGhost_$$"
fill_field "$SESS" "$FRAME" "User ID" "$GHOST"
focus "$SESS" "$FRAME" "Log In"; key "$SESS" 13
sleep 0.8
cp "$FRAME" "$PROOF_DIR/05_unknown_user.txt"
check "$FRAME" "No such user" "unknown user refused"
check "$FRAME" "Not logged in" "still not logged in"

echo "--- session end: verify persistent state survived at the real roots ---"
# Killing the session lets its own EXIT trap run persist_session_state(),
# which fans users/ + current_login.txt + xyzfs/session.pdl back out to
# $PROJECT_DIR. The xyzfs user trees never needed persisting - with
# HOUSE_ROOT exported they were written straight to $HOUSE_DIR mid-session.
# This is THE check this migration exists for: real save data must survive
# the session dir being deleted. Poll briefly - the trap runs right after
# keyboard_input dies, but it is a separate process, not synchronous.
bash "$HARNESS_DIR/button.sh" kill >/dev/null 2>&1
PERSISTED=0
for i in $(seq 1 20); do
    if [ -n "$UUID_A" ] && [ -n "$UUID_B" ] \
       && [ -f "$PROJECT_DIR/users/$USER_A/profile.txt" ] \
       && [ -f "$PROJECT_DIR/users/$USER_B/profile.txt" ] \
       && [ -d "$HOUSE_DIR/xyzfs/users/$UUID_A" ] \
       && [ -d "$HOUSE_DIR/xyzfs/users/$UUID_B" ]; then
        PERSISTED=1
        break
    fi
    sleep 0.5
done
if [ "$PERSISTED" = "1" ] \
   && grep -q "^uuid=$UUID_A$" "$PROJECT_DIR/users/$USER_A/profile.txt" \
   && grep -q "^uuid=$UUID_B$" "$PROJECT_DIR/users/$USER_B/profile.txt"; then
    pass "users/ (persisted) + house xyzfs trees survived session end"
else
    fail "persistent state did NOT survive session end (save-data loss)"
fi

echo
echo "=== proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
