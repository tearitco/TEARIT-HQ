#!/bin/bash
# Real "Tao Te Ching (random chapter)" branch - real chapter-boundary
# scan (awk, a real standard tool) against the real source text, picks
# one real chapter at random, real Show Text displays it.
# 2026-08-07: Show Text binary + package dir used to point at 00.10
# (copied-tree drift) - now house-rooted like bible_text/run.sh.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOOK_STACK="$(cd "$SCRIPT_DIR/../../../../../.." && pwd)"
HOUSE="$(cd "$BOOK_STACK/../../.." && pwd)"
# Asset root is per-machine (HOUSE_STDS §I.25): env wins, then this
# Mac's local books dir, then the legacy Linux SHARE] mount. Validate
# loudly rather than silently showing nothing.
TAO_FILE="${BIBLE_ASSET_ROOT:-}"
if [ -z "$TAO_FILE" ] || [ ! -f "$TAO_FILE/tao-te-ching-tty/tao-te-ching]a1.txt" ]; then
    TAO_FILE="$HOME/Desktop/bible]as.DeathNote]0000/book-stack"
fi
if [ -f "$TAO_FILE/tao-te-ching-tty/tao-te-ching]a1.txt" ]; then
    TAO_FILE="$TAO_FILE/tao-te-ching-tty/tao-te-ching]a1.txt"
else
    # Linux leg: ensure the shared asset drive is up before falling back
    # to the Linux SHARE] mount path below. No-op on macOS/Windows.
    if [ "$(uname)" = "Linux" ] && command -v udisksctl >/dev/null 2>&1; then
        MNT_SHARED="$BOOK_STACK/_shared/ensure_book_mount.sh"
        if [ -f "$MNT_SHARED" ]; then
            # shellcheck disable=SC1090  # sourced path is computed
            . "$MNT_SHARED"
            ensure_book_mount >/dev/null 2>&1 || true
        fi
    fi
    TAO_FILE="/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/!.🫁️.BIBLE.📔️]z3+/tao-te-ching-tty/tao-te-ching]a1.txt"
fi
if [ ! -f "$TAO_FILE" ]; then
    echo "ERROR: tao text not found at $TAO_FILE (set BIBLE_ASSET_ROOT?)" >&2
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
"$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x" "${PACKAGE_DIR:-$HOUSE/*.monads/*.book-stack/entities/book-stack}" "$TMP"
