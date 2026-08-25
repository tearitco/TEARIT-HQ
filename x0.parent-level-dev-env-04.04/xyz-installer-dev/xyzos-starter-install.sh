#!/bin/bash
# xyzos-starter-install.sh - bootstrap a fresh ~/xyzos (install v1) from the
# dev tree. Reads FROM the dev tree, NEVER writes to it. Non-destructive:
# refuses if $dest-home/xyzos already exists.
#
# Lives in the xyz-installer-dev package (currently
# x0.parent-level-dev-env-04.04/xyz-installer-dev/, planned move:
# ~/xyz-installer-dev/). Not hardcoded - the dev-tree location is read from
# the sibling pointers.pdl unless overridden by args.
#
# Decisions it encodes (see #.haiku+/30.jul-30-handoff/3.harness-install-xyzos~/
# 4.install-v1-xyzfs-layout.md - APPROVED):
#   - scope: login-signup + avatar only (agent 045 deferred)
#   - installed login dir KEEPS the name 00.login-signup (avatar's button.sh
#     globs ../00.login-signup* and ensure_user_identity.c falls back to
#     $root/../00.login-signup - renaming breaks the identity link)
#   - Option B layout: xyzfs/users/<uuid>/home-🏠️ (physical dir "home"), with
#     per-app subdirs added later under it
#   - pointer file: $dest/xyzos/paths.pdl (logical name -> real path)
#   - fresh guest session.pdl + empty users/ on first boot
#
# Usage:
#   xyzos-starter-install.sh [dev-tree] [dest-home]
#     dev-tree   path to 44.xyz❤️‍🔥️00.10 (the house root); default from
#                pointers.pdl (relative to the package's parent dir)
#     dest-home  where ~/xyzos will be created (default: $HOME)
set -euo pipefail

PKG_DIR="$(cd "$(dirname "$0")" && pwd)"
META_ROOT="$(cd "$PKG_DIR/.." && pwd)"
POINTER_FILE="$PKG_DIR/pointers.pdl"

DEV_TREE="${1:-}"
DEST_HOME="${2:-${HOME:-}}"

if [ -z "$DEV_TREE" ] && [ -f "$POINTER_FILE" ]; then
    DEV_TREE=$(awk -F'|' '$1 ~ /POINTERS/ && $2 ~ /dev_tree/ { gsub(/[ \t]/, "", $3); print $3 }' "$POINTER_FILE" | head -1)
    if [ -n "$DEV_TREE" ] && [ "${DEV_TREE:0:1}" != "/" ]; then
        DEV_TREE="$META_ROOT/$DEV_TREE"
    fi
fi

if [ -z "$DEV_TREE" ] || [ -z "$DEST_HOME" ]; then
    echo "usage: $0 [dev-tree] [dest-home]" >&2
    echo "  dev-tree  path to 44.xyz❤️‍🔥️00.10 (the house root); default from $POINTER_FILE" >&2
    echo "  dest-home where ~/xyzos will be created (default: \$HOME)" >&2
    exit 1
fi

HOUSE="$(cd "$DEV_TREE" && pwd)"
LOGIN_SRC="$HOUSE/0.user-pal👤️/00.login-signup"
AVATAR_SRC="$HOUSE/0.user-pal👤️/01.avatar-creation👤️"

for p in "$LOGIN_SRC/button.sh" "$AVATAR_SRC/button.sh"; do
    if [ ! -f "$p" ]; then
        echo "ERROR: dev tree does not look like the house root - missing $p" >&2
        exit 1
    fi
done

XYZOS="$DEST_HOME/xyzos"
if [ -e "$XYZOS" ]; then
    echo "ERROR: $XYZOS already exists - refusing to touch it (non-destructive)." >&2
    echo "       Move/rename it, or pick a different dest-home." >&2
    exit 1
fi

echo "==> xyzos install v1"
echo "    source: $HOUSE"
echo "    dest:   $XYZOS"
echo

mkdir -p "$XYZOS/apps" "$XYZOS/app-store" "$XYZOS/xyzfs"

echo "==> copying apps (one-way; names preserved for the avatar identity link)"
cp -a "$LOGIN_SRC" "$XYZOS/apps/"
cp -a "$AVATAR_SRC" "$XYZOS/apps/"

echo "==> stripping dev-only state"
for app in "00.login-signup" "01.avatar-creation👤️"; do
    rm -rf "$XYZOS/apps/$app/pieces/sessions" \
           "$XYZOS/apps/$app/proof" \
           "$XYZOS/apps/$app/test-harn-same"
done

LOGIN="$XYZOS/apps/00.login-signup"
rm -rf "$LOGIN/users"/* "$LOGIN/xyzfs/users"/*
rm -f "$LOGIN/xyzfs/session.pdl" "$LOGIN/current_login.txt"
: > "$LOGIN/current_login.txt"

echo "==> compiling in place"
( cd "$LOGIN" && bash button.sh compile )
( cd "$XYZOS/apps/01.avatar-creation👤️" && bash button.sh compile )

echo "==> fresh guest identity (session.pdl)"
cat > "$LOGIN/xyzfs/session.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | xyzfs_session
META         | version            | 1.0

STATE        | mode                 | guest
STATE        | user_id              | 
STATE        | user_uuid            | 
STATE        | display_name         | Guest
STATE        | xyzfs_path           | 
STATE        | logged_in_at         | 0
STATE        | active_avatar_uuid   | 
STATE        | active_avatar_path   | 
EOF

echo "==> app-store ledger"
cat > "$XYZOS/app-store/catalog.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | xyzos_app_catalog
META         | version            | 1.0

APPS         | login-signup       | 00.login-signup
APPS         | avatar-creation    | 01.avatar-creation👤️
EOF
INSTALLED_AT="$(date +%F)"
cat > "$XYZOS/app-store/installed_apps.pdl" << EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | xyzos_installed_apps
META         | version            | 1.0

STATE        | installed_count    | 2
STATE        | login-signup       | 00.login-signup
STATE        | avatar-creation    | 01.avatar-creation👤️
STATE        | installed_at       | $INSTALLED_AT
EOF

echo "==> paths.pdl (the pointer: logical name -> real path)"
cat > "$XYZOS/paths.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | xyzos_paths
META         | version            | 1.0
META         | notes             | Logical name -> real path for this install.
META         | notes             | Edit HERE if the layout ever changes. Legacy
META         | notes             | ops that still hardcode paths are listed in the
META         | notes             | install design doc (§3). Going-forward label for
META         | notes             | a user's home dir is home-🏠️; the physical dir
META         | notes             | is still 'home' today (Option B, v1).

PATHS        | user_home_root     | xyzfs/users
PATHS        | user_home_dir      | home
PATHS        | account_registry   | apps/00.login-signup/users
PATHS        | identity_state     | apps/00.login-signup/xyzfs/session.pdl
PATHS        | installed_ledger   | app-store/installed_apps.pdl
PATHS        | catalog            | app-store/catalog.pdl
EOF

echo "==> top-level launcher"
cat > "$XYZOS/button.sh" << 'EOF'
#!/bin/bash
# xyzos launcher (install v1) - boots straight into login-signup.
cd "$(dirname "$0")"
exec apps/00.login-signup/button.sh run
EOF
chmod +x "$XYZOS/button.sh"

echo
echo "=== xyzos install v1 complete ==="
echo "    root:   $XYZOS"
echo "    boot:   $XYZOS/button.sh"
echo "    login:  $XYZOS/apps/00.login-signup"
echo "    avatar: $XYZOS/apps/01.avatar-creation👤️"
echo "    dev tree $HOUSE was NOT modified (read-only copy)"
