#!/bin/bash
# Real "Tao Te Ching (random chapter)" branch - real chapter-boundary
# scan (awk, a real standard tool) against the real source text, picks
# one real chapter at random, real Show Text displays it.
# 2026-08-07: Show Text binary + package dir used to point at 00.10
# (copied-tree drift) - now house-rooted like bible_text/run.sh.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOOK_STACK="$(cd "$SCRIPT_DIR/../../../../../.." && pwd)"
HOUSE="$(cd "$BOOK_STACK/../../.." && pwd)"
# The asset folder comes from ONE resolver (asset_root_op, rows in ../../data/asset_roots.pdl): $BIBLE_ASSET_ROOT / $BOOK_SHARE_ROOT first, then the per-OS rows, mounting the
# shared drive on Linux when needed. It fails loudly (every tried path on stderr) instead of silently showing nothing. Before 2026-11-02 this hunt was copied into three scripts.
ASSET_OP="$HOUSE/&.widgits/_shared-lib/ops/+x/asset_root_op.+x"
ROOTS="$SCRIPT_DIR/../../data/asset_roots.pdl"
MOUNT_SCRIPT="$BOOK_STACK/_shared/ensure_book_mount.sh"
[ -x "$ASSET_OP" ] || { echo "ERROR: $ASSET_OP is not built (sh &.widgits/_shared-lib/ops/build_asset_root_op.sh)" >&2; exit 1; }
SHARE="$("$ASSET_OP" "$ROOTS" share "$MOUNT_SCRIPT")" || exit 1
TAO_FILE="$SHARE/tao-te-ching-tty/tao-te-ching]a1.txt"
if [ ! -f "$TAO_FILE" ]; then
    echo "ERROR: tao text not found at $TAO_FILE" >&2
    exit 1
fi
N_CHAPTERS=$(grep -c "^Chapter " "$TAO_FILE")
PICK=$(( (RANDOM % N_CHAPTERS) + 1 ))
TMP=$(mktemp --suffix=.txt 2>/dev/null || echo "/tmp/tao_chapter_$$.txt")
awk -v n="$PICK" '
  /^Chapter / { c++; if (c==n) { p=1; next } else if (p) { exit } }
  p { print }
' "$TAO_FILE" | fold -s -w 70 > "$TMP"
# REAL FIX 2026-08-10 (same class as bible_text/run.sh): use the live
# PACKAGE_DIR exported by meta.pdl's "Read" method, not the dev-tree guess.
"$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x" "${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}" "$TMP"
