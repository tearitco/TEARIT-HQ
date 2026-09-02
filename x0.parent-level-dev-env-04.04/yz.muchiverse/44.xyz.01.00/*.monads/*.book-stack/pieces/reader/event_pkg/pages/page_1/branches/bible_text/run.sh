#!/bin/bash
# Real "Bible verse (text)" branch - portable asset root.
# Prefer: BIBLE_ASSET_ROOT, then assets_root_win.txt, then Win Desktop
# assets-4win, then legacy Linux SHARE] path.
# 2026-08-07: the legacy Linux fallback below used copied wrong dir
# names (SHARECOP^ / 🗂️.BIBLE - the real mount dir is SHARE]^ /
# 🫁️.BIBLE, confirmed against 00.10's working run.sh), so Linux "read
# now -> bible_text" hit `cd ... || exit 1` and silently showed nothing.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOOK_STACK="$(cd "$SCRIPT_DIR/../../../../../.." && pwd)"
HOUSE="$(cd "$BOOK_STACK/../../.." && pwd)"
BIBLE_ROOT="${BIBLE_ASSET_ROOT:-}"
if [ -z "$BIBLE_ROOT" ] && [ -f "$BOOK_STACK/assets_root_win.txt" ]; then
    BIBLE_ROOT="$(grep -v '^\s*#' "$BOOK_STACK/assets_root_win.txt" | head -1 | tr -d '\r')"
fi
# Git-bash / MSYS path for user Desktop assets
if [ -z "$BIBLE_ROOT" ] || [ ! -d "$BIBLE_ROOT/bible.ch2en.ran" ]; then
    if [ -d "/c/Users/jbro8/OneDrive/Desktop/assets-4win/bible-ench.twins+ai]b2/bible.ch2en.ran" ]; then
        BIBLE_ROOT="/c/Users/jbro8/OneDrive/Desktop/assets-4win/bible-ench.twins+ai]b2"
    elif [ -d "C:/Users/jbro8/OneDrive/Desktop/assets-4win/bible-ench.twins+ai]b2/bible.ch2en.ran" ]; then
        BIBLE_ROOT="C:/Users/jbro8/OneDrive/Desktop/assets-4win/bible-ench.twins+ai]b2"
    fi
fi
if [ -z "$BIBLE_ROOT" ] || [ ! -d "$BIBLE_ROOT/bible.ch2en.ran" ]; then
    # macOS leg (2026-08-22, direct report "mac local bookstack books are
    # in book-stack/ dir on this Desktop"): this Mac keeps its books at
    # ~/Desktop/bible]as.DeathNote]0000/book-stack (different on every
    # OS — win uses assets_root_win.txt above). Validate loudly per
    # HOUSE_STDS §I.25 rather than silently showing nothing.
    if [ -d "$HOME/Desktop/bible]as.DeathNote]0000/book-stack/bible-ench.twins+ai]b2/bible.ch2en.ran" ]; then
        BIBLE_ROOT="$HOME/Desktop/bible]as.DeathNote]0000/book-stack/bible-ench.twins+ai]b2"
    fi
fi
if [ -z "$BIBLE_ROOT" ] || [ ! -d "$BIBLE_ROOT/bible.ch2en.ran" ]; then
    BIBLE_ROOT="/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/!.🫁️.BIBLE.📔️]z3+/bible-ench.twins+ai]b2"
fi
if [ ! -d "$BIBLE_ROOT/bible.ch2en.ran" ]; then
    echo "ERROR: bible assets not found at $BIBLE_ROOT (set BIBLE_ASSET_ROOT?)" >&2
    exit 1
fi
cd "$BIBLE_ROOT/bible.ch2en.ran" || exit 1
if [ -x ./bible_verses ]; then
    OUT=$(./bible_verses --short bible.en_translation.txt "bible]ch.txt" 2>/dev/null)
elif [ -x ./bible_verses.exe ]; then
    OUT=$(./bible_verses.exe --short bible.en_translation.txt "bible]ch.txt" 2>/dev/null)
else
    OUT="(bible_verses binary missing in $BIBLE_ROOT/bible.ch2en.ran)"
fi
TMP=$(mktemp --suffix=.txt 2>/dev/null || echo /tmp/bible_verse_$$.txt)
echo "$OUT" | fold -s -w 70 > "$TMP" 2>/dev/null || echo "$OUT" > "$TMP"
SHOW="$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x"
# REAL FIX 2026-08-10 (same class as dispatch.sh, one dir up): hardcoded
# to the dev-tree entity location, wrong once book-stack runs from pals.
# PACKAGE_DIR is exported by meta.pdl's "Read" method and inherited
# through every exec (prisc+x -> dispatch.sh -> this script) - use it.
PKG="${PACKAGE_DIR:-$HOUSE/*.monads/*.book-stack/entities/book-stack}"
if [ -x "$SHOW" ]; then
    "$SHOW" "$PKG" "$TMP"
else
    cat "$TMP"
fi
