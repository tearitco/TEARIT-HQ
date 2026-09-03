#!/bin/bash
# make-payload.sh — assemble the curated "tearit-hq" minimal-desktop
# payload from the live house tree.
#
# Output: a self-contained tree that preserves the house-relative dir
# names the C code hardcodes (see
# dev-doc/04.harnecient-fresh-install-design.md §4), plus a fresh
# #.desktop/ config, a bootstrap.sh (compile-in-place) and a start.sh
# (per-root launcher). That tree is what gets pushed to the
# tearit-hq-payload repo and downloaded by tearit-install/install.sh.
#
# This is the ONLY place that knows which house subtrees the minimal
# desktop needs. Keep it in sync with the real build/runtime deps, not
# with a wishlist.
#
# Usage:
#   bash make-payload.sh [OUT_DIR]
# Default OUT_DIR: ./build/tearit-hq-payload  (gitignored)

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../yz.muchiverse/44.xyz.01.00" && pwd)"
OUT="${1:-$HERE/build/tearit-hq-payload}"

echo "house : $HOUSE"
echo "out   : $OUT"

[ -d "$HOUSE/*.monads/*.livedesk-taskbar" ] || { echo "FATAL: taskbar dir not found under \$HOUSE"; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"

# --- helper: copy a house-relative subtree into the payload, same rel path
copy_rel() {
  local rel="$1"
  local src="$HOUSE/$rel"
  local dst="$OUT/$rel"
  [ -e "$src" ] || { echo "FATAL: missing house subtree: $rel"; exit 1; }
  mkdir -p "$(dirname "$dst")"
  # -a preserves perms/exec bits; exclude VCS + heavy dev-only cruft
  rsync -a \
    --exclude '.git' \
    --exclude '*.7z' --exclude '*.zip' \
    --exclude 'LEGACY-ARCHIVE-*' \
    --exclude 'proof/' --exclude 'test-harn-same/' --exclude 'test-harn*/' \
    --exclude 'scenarios/' \
    "$src/" "$dst/"
  echo "  + $rel"
}

echo "--- copying code subtrees (house-relative paths preserved) ---"
copy_rel "*.monads/*.livedesk-taskbar"
copy_rel "*.monads/*.cursword"
copy_rel "&.widgits/_shared-lib"
copy_rel "&.widgits/livedesk-clock"
copy_rel "0.user-pal👤️/00.login-signup"
copy_rel "&.hq-apps/signup-hq"       # the real "New User" window (launcher_signup)

echo "--- stripping dev-only state from the login app ---"
LOGIN="$OUT/0.user-pal👤️/00.login-signup"
rm -rf "$LOGIN/proof" "$LOGIN/test-harn-same" "$LOGIN/pieces/sessions" 2>/dev/null || true
rm -rf "$LOGIN/users" && mkdir -p "$LOGIN/users"
: > "$LOGIN/current_login.txt" 2>/dev/null || true
mkdir -p "$LOGIN/xyzfs" && printf 'mode | guest\n' > "$LOGIN/xyzfs/session.pdl"
# drop any compiled artifacts — bootstrap.sh rebuilds everything in place
find "$OUT" -name '*.+x' -delete 2>/dev/null || true
find "$OUT" -name 'prisc+x' -delete 2>/dev/null || true
find "$OUT" -name '*.o' -delete 2>/dev/null || true

echo "--- prebuilt emoji helpers (Linux ELF, from wsr-pal toolchain) ---"
# Ship them at their REAL house-relative location. build_khtpm_strip.sh
# does `WSR="$(cd .../014.wsr-pal.../ && pwd)"` under `set -e` and then
# copies +x/emoji_*.+x into its own ops/+x — so this dir must exist
# (empty is not enough: the cd must succeed AND the binaries be there
# for the copy branch, else `set -e` aborts the whole build).
WSR_REL="014.wsr-pal💸️📌️+2/ops/+x"
mkdir -p "$OUT/$WSR_REL"
for t in emoji_gen_atlas emoji_xtract; do
  if [ -x "$HOUSE/$WSR_REL/$t.+x" ]; then
    cp "$HOUSE/$WSR_REL/$t.+x" "$OUT/$WSR_REL/$t.+x"
    chmod +x "$OUT/$WSR_REL/$t.+x"
    echo "  + $WSR_REL/$t.+x"
  else
    echo "  WARN: $HOUSE/$WSR_REL/$t.+x not found — entity emoji atlas may be degraded"
  fi
done

echo "--- fresh #.desktop/ config (regenerated, not copied) ---"
DESK="$OUT/#.desktop"
mkdir -p "$DESK/livedesk-nav-claims"
cp "$HERE/payload-src/desktop-config/"*.pdl "$DESK/"
: > "$DESK/livedesk_shortcuts.pdl"
# build_uid sprite (rendered by the header's ${build_uid} text cell)
if [ -d "$HOUSE/#.desktop/build_uid_sprite" ]; then
  cp -a "$HOUSE/#.desktop/build_uid_sprite" "$DESK/build_uid_sprite"
fi
# runtime files the parser/manager fopen(...,"w") anyway — pre-touch so a
# first launch on a read-only-ish fs still finds them
for f in livedesk_agent_relay.txt livedesk_open.txt strip_history.txt \
         strip_state.txt khtpm_strip_frame_history.txt strip_frame_changed.txt; do
  : > "$DESK/$f"
done

echo "--- fresh xyzfs/ skeleton ---"
mkdir -p "$OUT/xyzfs/users" "$OUT/xyzfs/bin"
printf 'mode | guest\n' > "$OUT/xyzfs/session.pdl"

echo "--- payload scripts ---"
cp "$HERE/payload-src/bootstrap.sh" "$OUT/bootstrap.sh"
cp "$HERE/payload-src/start.sh"     "$OUT/start.sh"
chmod +x "$OUT/bootstrap.sh" "$OUT/start.sh"
cp "$HERE/payload-src/PAYLOAD-README.md" "$OUT/README.md" 2>/dev/null || true

echo
echo "payload size: $(du -sh "$OUT" | cut -f1)"
echo "done: $OUT"
