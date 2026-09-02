#!/bin/bash
# %.harnesses/install-xyzos/button.sh - KPI harness for install v1.
# Drives the packaged installer (x0.parent-level-dev-env-04.04/xyz-installer-dev/
# xyzos-starter-install.sh, planned move ~/xyz-installer-dev/) against a clean
# throwaway $HOME, then boots the INSTALLED login app and proves it with real
# key injection (same tk_* ops as the login app's own test-harn-same).
#
#   ./button.sh compile   build tk_* ops from the login dev tree
#   ./button.sh kpi4      KPI#4: clean install -> signup screen boots
#   ./button.sh kpi5      KPI#5: signup -> logout -> login -> whoami -> xyzfs tree
#   ./button.sh all       compile + kpi4 + kpi5
#   ./button.sh kill      reap installed login processes
set -u
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HARNESS_DIR/../.." && pwd)"
META_DIR="$(cd "$HARNESS_DIR/../../../.." && pwd)"
INSTALLER="${XYZ_INSTALLER:-$META_DIR/xyz-installer-dev/xyzos-starter-install.sh}"
OPS_X="$HARNESS_DIR/ops/+x"
TK_SRC="$HOUSE/0.user-pal👤️/00.login-signup/test-harn-same/ops"
ACTION="${1:-help}"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$TK_SRC/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;
    kpi4|kpi4_boot)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then echo "tk ops not compiled - run compile first" >&2; exit 1; fi
        bash "$HARNESS_DIR/scenarios/kpi4_boot.sh" "$HOUSE" "$INSTALLER"
        ;;
    kpi5|kpi5_flow)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then echo "tk ops not compiled - run compile first" >&2; exit 1; fi
        bash "$HARNESS_DIR/scenarios/kpi5_account_flow.sh" "$HOUSE" "$INSTALLER"
        ;;
    all)
        bash "$0" compile || exit 1
        bash "$0" kpi4 || exit 1
        bash "$0" kpi5
        ;;
    kill|k|stop)
        ps aux | grep -E "xyzos-test-user/xyzos/apps/00.login-signup" | grep -v grep \
            | awk '{print $2}' | xargs -r kill -9 2>/dev/null
        ps aux | grep -E "system/(chtpm_parser_pal|renderer|keyboard_input)|prisc\+x" | grep -v grep \
            | awk '{print $2}' | xargs -r kill -9 2>/dev/null
        sleep 1
        echo "cleaned"
        ;;
    help|h|-h|--help|*)
        echo "%.harnesses/install-xyzos/button.sh - install v1 KPI harness"
        echo "Usage: ./button.sh <compile|kpi4|kpi5|all|kill|help>"
        echo "  installer: $INSTALLER"
        ;;
esac
