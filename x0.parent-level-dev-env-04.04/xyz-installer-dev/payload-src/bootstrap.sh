#!/bin/sh
# bootstrap.sh — compile the tearit-hq minimal desktop IN PLACE.
#
# Runs from the payload/install root. Each component is built with its
# OWN real build script (design doc 04 §5.4) so this stays correct as
# those evolve. Linux is the supported leg today; the component scripts
# already carry macOS guards for later.

set -eu
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "=== tearit-hq bootstrap ==="
echo "root: $ROOT"

# --- toolchain preflight --------------------------------------------------
miss=""
command -v gcc >/dev/null 2>&1 || miss="$miss gcc"
command -v pkg-config >/dev/null 2>&1 || miss="$miss pkg-config"
command -v make >/dev/null 2>&1 || true
if [ -n "$miss" ]; then
    echo "FATAL: missing build tools:$miss"
    echo "  Debian/Ubuntu: sudo apt install build-essential pkg-config"
    exit 1
fi
hdr_miss=""
pkg-config --exists x11   || hdr_miss="$hdr_miss libx11-dev"
pkg-config --exists xext  || hdr_miss="$hdr_miss libxext-dev"
pkg-config --exists xft   || hdr_miss="$hdr_miss libxft-dev"
if [ -n "$hdr_miss" ]; then
    echo "FATAL: missing X11 dev headers:$hdr_miss"
    echo "  Debian/Ubuntu: sudo apt install$hdr_miss"
    exit 1
fi
echo "toolchain OK: $(gcc --version | head -1)"

# --- 1. taskbar (parser/manager + shared core + helpers) ----------------
echo
echo "--- [1/3] taskbar ---"
( cd "$ROOT/*.monads/*.livedesk-taskbar/ops" && sh build_khtpm_strip.sh )

# --- 2. login / signup app --------------------------------------------------
echo
echo "--- [2/3] login-signup ---"
( cd "$ROOT/0.user-pal👤️/00.login-signup" && bash scripts/build.sh )

# --- 3. livedesk-clock (optional; header clock is manager-internal) -----
echo
echo "--- [3/3] livedesk-clock ---"
if [ -d "$ROOT/&.widgits/livedesk-clock/ops" ]; then
    for b in "$ROOT/&.widgits/livedesk-clock/ops/"build*.sh; do
        [ -f "$b" ] || continue
        echo "  running $(basename "$b")"
        ( cd "$ROOT/&.widgits/livedesk-clock/ops" && sh "$(basename "$b")" ) || echo "  WARN: $b failed (non-fatal)"
    done
else
    echo "  (no clock ops dir — skipping; header clock still works)"
fi

# --- verify --------------------------------------------------------------
echo
echo "--- verify ---"
ok=1
for f in \
  "$ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x" \
  "$ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_taskbar_manager_main.+x" \
  "$ROOT/0.user-pal👤️/00.login-signup/ops/+x/userpal_login.+x" \
  "$ROOT/0.user-pal👤️/00.login-signup/ops/+x/userpal_create_account.+x" ; do
    if [ -x "$f" ]; then echo "  OK  ${f#$ROOT/}"; else echo "  MISSING  ${f#$ROOT/}"; ok=0; fi
done
[ "$ok" = 1 ] || { echo "bootstrap: FAILED (missing binaries above)"; exit 1; }

echo
echo "bootstrap OK — launch with:  sh $ROOT/start.sh"
