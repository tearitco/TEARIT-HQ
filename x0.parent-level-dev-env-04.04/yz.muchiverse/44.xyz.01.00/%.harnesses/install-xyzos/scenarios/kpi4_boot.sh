#!/bin/bash

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# kpi4_boot.sh - KPI#4: a clean throwaway $HOME gets a full install v1, and the
# INSTALLED login app boots to the signup screen (real key-injection-ready).
# Also proves the dev tree was NOT modified (read-only copy).
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="${1:?house root arg required}"
INSTALLER="${2:?installer path arg required}"
OPS="$HARNESS_DIR/ops/+x"

DEST="/tmp/xyzos-test-user"
PROOF_DIR="$HARNESS_DIR/proof/kpi4-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3" >/dev/null; [ $? -ne 0 ] && FAIL=1; }

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

echo "=== KPI#4: clean-HOME install v1 boots to signup screen ==="
rm -rf "$DEST"
mkdir -p "$DEST"

echo "--- install (dest=$DEST, source=$HOUSE) ---"
INSTALL_LOG="$PROOF_DIR/install.log"
if bash "$INSTALLER" "$HOUSE" "$DEST" >"$INSTALL_LOG" 2>&1; then
    pass "installer ran (exit 0)"
else
    fail "installer exited nonzero"
    tail -40 "$INSTALL_LOG"
    exit 1
fi

XYZ="$DEST/xyzos"

echo "--- structural asserts ---"
for p in "$XYZ/apps" "$XYZ/app-store" "$XYZ/xyzfs" "$XYZ/paths.pdl" "$XYZ/button.sh" \
         "$XYZ/apps/00.login-signup/button.sh" "$XYZ/apps/01.avatar-creation👤️/button.sh" \
         "$XYZ/apps/00.login-signup/ops/+x/userpal_create_account.+x" \
         "$XYZ/apps/00.login-signup/ops/+x/userpal_whoami.+x" \
         "$XYZ/apps/01.avatar-creation👤️/ops/+x/generate_clone.+x" \
         "$XYZ/apps/01.avatar-creation👤️/ops/+x/ensure_user_identity.+x" \
         "$XYZ/app-store/catalog.pdl" "$XYZ/app-store/installed_apps.pdl"; do
    if [ -e "$p" ]; then
        pass "exists: ${p#$XYZ/}"
    else
        fail "missing: ${p#$XYZ/}"
    fi
done

if [ -z "$(ls -A "$XYZ/apps/00.login-signup/users" 2>/dev/null)" ]; then
    pass "login users/ is empty (fresh registry)"
else
    fail "login users/ not empty"
fi
if [ -z "$(ls -A "$XYZ/apps/00.login-signup/xyzfs/users" 2>/dev/null)" ]; then
    pass "xyzfs/users/ is empty (zero users)"
else
    fail "xyzfs/users/ not empty"
fi
for s in pieces/sessions proof test-harn-same; do
    if [ -e "$XYZ/apps/00.login-signup/$s" ]; then
        fail "dev-only state NOT stripped: $s"
    else
        pass "dev-only state stripped: $s"
    fi
done
if grep -q "PATHS        | user_home_dir      | home" "$XYZ/paths.pdl"; then
    pass "paths.pdl points user_home_dir -> home (home-🏠️ label)"
else
    fail "paths.pdl missing user_home_dir"
fi
cp "$XYZ/paths.pdl" "$PROOF_DIR/paths.pdl"
cp "$XYZ/app-store/catalog.pdl" "$PROOF_DIR/catalog.pdl"
cp "$XYZ/app-store/installed_apps.pdl" "$PROOF_DIR/installed_apps.pdl"

echo "--- dev tree not modified (read-only copy) ---"
if grep -q "afx_3265291" "$HOUSE/0.user-pal👤️/00.login-signup/xyzfs/session.pdl"; then
    pass "dev-tree session sentinel intact (afx_3265291 still logged in)"
else
    fail "DEV TREE WAS MODIFIED (sentinel gone)"
fi

echo "--- boot the INSTALLED os (top-level button.sh) ---"
cd "$XYZ"
$SETSID bash button.sh run > /tmp/th_install_kpi4.log 2>&1 </dev/null & disown
sleep 3
SESS=$(ls -dt "$XYZ/apps/00.login-signup/pieces/sessions/"*/ 2>/dev/null | head -1)
SESS="${SESS%/}"
if [ -n "$SESS" ] && [ -d "$SESS" ]; then
    pass "installed login created a live session"
else
    fail "no session dir under installed login"
    cat /tmp/th_install_kpi4.log 2>/dev/null | tail -40
    exit 1
fi
FRAME="$SESS/pieces/display/current_frame.txt"
READY=0
for i in $(seq 1 40); do
    if [ -s "$FRAME" ] && grep -qE "U S E R - P A L|Not logged in" "$FRAME" 2>/dev/null; then
        READY=1
        break
    fi
    sleep 0.2
done
cp "$FRAME" "$PROOF_DIR/00_boot_frame.txt" 2>/dev/null || true
if [ "$READY" = "1" ]; then
    pass "signup screen composed (U S E R - P A L frame)"
else
    fail "no signup screen"
    tail -40 /tmp/th_install_kpi4.log 2>/dev/null
fi
check "$FRAME" "Not logged in." "starts logged out"
check "$FRAME" "Create Account" "signup action present"
check "$FRAME" "User ID" "user-id field present"

echo
echo "=== KPI#4 proof saved to: $PROOF_DIR ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
