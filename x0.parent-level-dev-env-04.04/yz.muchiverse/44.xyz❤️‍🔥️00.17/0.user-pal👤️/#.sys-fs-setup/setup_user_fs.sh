#!/bin/bash
# setup_user_fs.sh - ensures a user's xyzfs home tree has the real,
# durable directory shape every project's own save/load logic now
# depends on (see @.apps/text-editor-xyz/save-bug.txt for the full
# incident this closes). Non-destructive by design - every operation
# is `mkdir -p`, never touches or deletes anything that already exists.
#
# Usage:
#   setup_user_fs.sh <user_uuid>   # explicit UUID (e.g. called right
#                                   # after signup with the new user's
#                                   # own uuid)
#   setup_user_fs.sh                # no arg - resolves the CURRENTLY
#                                   # LOGGED IN user via current_login.txt
#                                   # (same resolution chain ledger_
#                                   # append.c's own resolve_ledger_path()
#                                   # already uses)
#
# Shape created under <house>/xyzfs/users/<uuid>/home/:
#   runtime/    - already used by the process ledger (§35.5), ensured
#                 here too so a brand-new user has it before their
#                 first program ever registers.
#   projects/   - per-project data (rpg-xyz/rtp-xyz already use this
#                 shape for their own project state - unrelated to
#                 saved documents).
#   documents/  - NEW (this fix): the real, durable DEFAULT save
#                 target for user-generated files (text editor
#                 documents, etc.) - the directory every project's own
#                 SAVE_AS now resolves bare filenames against.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"

UUID="${1:-}"
if [ -z "$UUID" ]; then
    LOGIN_FILE="$HOUSE/0.user-pal👤️/00.login-signup/current_login.txt"
    if [ ! -f "$LOGIN_FILE" ]; then
        echo "setup_user_fs: no UUID given and no current_login.txt found at $LOGIN_FILE" >&2
        exit 1
    fi
    UUID="$(grep '^current_user_uuid=' "$LOGIN_FILE" | head -1 | cut -d= -f2-)"
    if [ -z "$UUID" ]; then
        echo "setup_user_fs: current_login.txt has no current_user_uuid= line" >&2
        exit 1
    fi
fi

HOME_DIR="$HOUSE/xyzfs/users/$UUID/home"

mkdir -p "$HOME_DIR/runtime"
mkdir -p "$HOME_DIR/projects"
mkdir -p "$HOME_DIR/documents"

echo "setup_user_fs: ensured xyzfs home shape for $UUID"
echo "  $HOME_DIR/runtime"
echo "  $HOME_DIR/projects"
echo "  $HOME_DIR/documents"
