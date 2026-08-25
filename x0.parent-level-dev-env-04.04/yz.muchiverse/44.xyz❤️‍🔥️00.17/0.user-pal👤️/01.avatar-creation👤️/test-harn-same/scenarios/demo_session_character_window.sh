#!/bin/bash
# demo_session_character_window.sh
# 1) login user -> xyzfs/session.pdl mode=logged_in
# 2) mint character (clone) via generate_clone
# 3) session.pdl active_avatar_* set
# 4) open GL desktop window (avatar_window) like muchi pets
# 5) prove process alive + window.pid
set -u
AV="$(cd "$(dirname "$0")/../.." && pwd)"
LOGIN="$(cd "$AV/../00.login-signup" 2>/dev/null && pwd || true)"
HOUSE="$(cd "$LOGIN/../.." 2>/dev/null && pwd || true)"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    # reap window without killing the whole machine
    if [ -n "${AVATAR_UUID:-}" ] && [ -f "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/window.pid" ]; then
        pid=$(tr -d ' \n' < "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/window.pid")
        kill -TERM "$pid" 2>/dev/null || true
        sleep 0.2
        kill -9 "$pid" 2>/dev/null || true
        rm -f "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/window.pid"
    fi
    bash "$AV/scripts/kill_avatar_windows.sh" 2>/dev/null || true
    if [ -n "${TEST_USER:-}" ] && [ -n "$LOGIN" ] && [ -d "$LOGIN/users/$TEST_USER" ]; then
        uuid=$(grep '^uuid=' "$LOGIN/users/$TEST_USER/profile.txt" 2>/dev/null | cut -d= -f2-)
        rm -rf "$LOGIN/users/$TEST_USER"
        [ -n "$uuid" ] && rm -rf "$HOUSE/xyzfs/users/$uuid"
        "$LOGIN/ops/+x/userpal_logout.+x" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

[ -n "$LOGIN" ] || { echo "no login-signup sibling"; exit 1; }

export PRISC_PROJECT_ROOT="$LOGIN"
export USERPAL_LOGIN_ROOT="$LOGIN"

echo "=== build login + avatar ops (targeted) ==="
# Avoid full rebuild spam/timeouts — only ops we need for this scenario.
gcc -Wall -Wextra -O2 -o "$LOGIN/ops/+x/userpal_login.+x" "$LOGIN/ops/userpal_login.c" 2>/dev/null
gcc -Wall -Wextra -O2 -o "$LOGIN/ops/+x/userpal_logout.+x" "$LOGIN/ops/userpal_logout.c" 2>/dev/null
gcc -Wall -Wextra -O2 -o "$LOGIN/ops/+x/userpal_create_account.+x" "$LOGIN/ops/userpal_create_account.c" 2>/dev/null
gcc -Wall -Wextra -O2 -o "$AV/ops/+x/generate_clone.+x" "$AV/ops/generate_clone.c" 2>/dev/null
gcc -Wall -Wextra -O2 -o "$AV/ops/+x/open_avatar_window.+x" "$AV/ops/open_avatar_window.c" 2>/dev/null

echo "=== logout -> guest session.pdl ==="
"$LOGIN/ops/+x/userpal_logout.+x"
grep -q 'mode.*guest' "$LOGIN/xyzfs/session.pdl" && pass "session.pdl guest after logout" \
  || fail "session.pdl not guest: $(grep mode "$LOGIN/xyzfs/session.pdl")"

TEST_USER="schar_$$"
echo "=== create + login $TEST_USER ==="
"$LOGIN/ops/+x/userpal_create_account.+x" "$TEST_USER" "SessionChar"
"$LOGIN/ops/+x/userpal_login.+x" "$TEST_USER"
cat "$LOGIN/xyzfs/session.pdl"
grep -q 'mode.*logged_in' "$LOGIN/xyzfs/session.pdl" && pass "session.pdl logged_in" || fail "not logged_in"
grep -q "user_id.*$TEST_USER" "$LOGIN/xyzfs/session.pdl" && pass "session.pdl user_id" || fail "user_id missing"
grep -q 'user_uuid' "$LOGIN/xyzfs/session.pdl" && pass "session.pdl has user_uuid" || fail "no user_uuid"
# Only the STATE row (not comments mentioning xyzfs_path)
XYZ=$(awk -F'|' '/^STATE[[:space:]]*\|[[:space:]]*xyzfs_path/ {gsub(/^ +| +$/,"",$3); print $3; exit}' "$LOGIN/xyzfs/session.pdl")
echo "xyzfs_path=[$XYZ]"
[ -n "$XYZ" ] && [ -d "$HOUSE/$XYZ" ] && pass "user xyzfs tree exists" || fail "xyzfs tree missing ($HOUSE/$XYZ)"

echo "=== mint character via generate_clone (reads session.pdl) ==="
export PRISC_PROJECT_ROOT="$AV"
export USERPAL_LOGIN_ROOT="$LOGIN"
AVATAR_UUID=$("$AV/ops/+x/generate_clone.+x" "HarnessHero")
AVATAR_UUID=$(echo "$AVATAR_UUID" | tr -d '\r\n' | tail -1)
echo "avatar=$AVATAR_UUID"
[ -n "$AVATAR_UUID" ] && [ ${#AVATAR_UUID} -ge 32 ] && pass "minted avatar uuid" || fail "bad uuid [$AVATAR_UUID]"
[ -f "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/state.txt" ] && pass "local lobby piece" || fail "no local piece"
if [ -f "$HOUSE/$XYZ/home/avatars/$AVATAR_UUID/state.txt" ]; then
    pass "xyzfs avatar state"
else
    AP=$(awk -F'|' '/^STATE[[:space:]]*\|[[:space:]]*active_avatar_path/ {gsub(/^ +| +$/,"",$3); print $3; exit}' "$LOGIN/xyzfs/session.pdl")
    if [ -n "$AP" ] && [ -f "$HOUSE/$AP/state.txt" ]; then
        pass "xyzfs avatar state via session path"
    else
        fail "no xyzfs avatar ($HOUSE/$XYZ/home/avatars/$AVATAR_UUID)"
    fi
fi

echo "=== session.pdl active_avatar after mint ==="
cat "$LOGIN/xyzfs/session.pdl"
grep -q "active_avatar_uuid.*$AVATAR_UUID" "$LOGIN/xyzfs/session.pdl" \
  && pass "session.pdl active_avatar_uuid set" || fail "active_avatar not set"
grep -q "active_avatar_path" "$LOGIN/xyzfs/session.pdl" \
  && pass "session.pdl active_avatar_path present" || fail "no active path"

echo "=== open desktop GL window (avatar_window) ==="
OUT=$("$AV/ops/+x/open_avatar_window.+x" "$AVATAR_UUID" 2>&1)
echo "$OUT"
echo "$OUT" | grep -qi 'Opened chara window' && pass "open_avatar_window reported open" || fail "open failed: $OUT"
sleep 0.5
WPID=""
if [ -f "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/window.pid" ]; then
    WPID=$(tr -d ' \n' < "$AV/pieces/world_01/map_lobby/$AVATAR_UUID/window.pid")
    pass "window.pid=$WPID"
else
    fail "no window.pid written"
fi
# process alive by exe basename
ALIVE=0
if [ -n "$WPID" ] && kill -0 "$WPID" 2>/dev/null; then
    ALIVE=1
    pass "window pid alive"
else
    # fallback scan
    for pid in /proc/[0-9]*; do
        pid=${pid#/proc/}
        exe=$(readlink -f "/proc/$pid/exe" 2>/dev/null) || continue
        base=${exe##*/}
        if [ "$base" = "avatar_window" ]; then ALIVE=1; WPID=$pid; break; fi
    done
    [ "$ALIVE" = "1" ] && pass "avatar_window process found pid=$WPID" || fail "no avatar_window process (DISPLAY?)"
fi

if [ "$ALIVE" = "1" ] && [ -n "$WPID" ]; then
    ps -p "$WPID" -o pid,pcpu,etime,cmd 2>/dev/null || true
fi

# session.pdl still points at active avatar after open
grep -q "active_avatar_uuid.*$AVATAR_UUID" "$LOGIN/xyzfs/session.pdl" \
  && pass "session.pdl still tracks active avatar after open" || fail "session lost active avatar"

echo
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
