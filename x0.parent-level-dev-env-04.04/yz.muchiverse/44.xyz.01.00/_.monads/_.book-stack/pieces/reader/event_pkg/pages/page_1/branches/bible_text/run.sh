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
# The asset folder comes from ONE resolver (asset_root_op, rows in ../../data/asset_roots.pdl): $BIBLE_ASSET_ROOT / $BOOK_SHARE_ROOT first, then the per-OS rows, mounting the
# shared drive on Linux when needed. It fails loudly (every tried path on stderr) instead of silently showing nothing. Before 2026-11-02 this hunt was copied into three scripts.
ASSET_OP="$HOUSE/&.widgits/_shared-lib/ops/+x/asset_root_op.+x"
ROOTS="$SCRIPT_DIR/../../data/asset_roots.pdl"
MOUNT_SCRIPT="$BOOK_STACK/_shared/ensure_book_mount.sh"
[ -x "$ASSET_OP" ] || { echo "ERROR: $ASSET_OP is not built (sh &.widgits/_shared-lib/ops/build_asset_root_op.sh)" >&2; exit 1; }
BIBLE_ROOT="$("$ASSET_OP" "$ROOTS" bible "$MOUNT_SCRIPT")" || exit 1
cd "$BIBLE_ROOT/bible.ch2en.ran" || exit 1
if [ -x ./bible_verses ]; then
    OUT=$(./bible_verses --short bible.en_translation.txt "bible]ch.txt" 2>/dev/null)
elif [ -x ./bible_verses.exe ]; then
    OUT=$(./bible_verses.exe --short bible.en_translation.txt "bible]ch.txt" 2>/dev/null)
else
    OUT="(bible_verses binary missing in $BIBLE_ROOT/bible.ch2en.ran)"
fi
# BIBLE_SPEAK=1 (set by the bible_tts branch): speak the English line, then the Chinese line, of THIS verse with the local Kokoro voices (offline; edge-tts has been 403 since 2026-10-12),
# in the background so the text window still opens at once. Niced: the machine is shared with a browser.
if [ -n "$BIBLE_SPEAK" ]; then
    SPEAK_EN=$(printf '%s\n' "$OUT" | sed -n 's/^English: *//p' | head -1)
    SPEAK_ZH=$(printf '%s\n' "$OUT" | sed -n 's/^Chinese: *//p' | head -1 | sed 's/ //g')      # the tool spaces every character; Kokoro wants the plain string
    TTS_PY="$HOUSE/@.apps/pet-trainer/ops/pet_tts_local.py"
    if { [ -n "$SPEAK_EN" ] || [ -n "$SPEAK_ZH" ]; } && [ -f "$TTS_PY" ] && [ -f "$HOME/.local/share/kokoro/kokoro-v1.0.int8.onnx" ]; then
        # English then Chinese, one after the other (one synthesis at a time is the TTS script's own rule); voices are the KMAP rows in pet-trainer/voices.pdl
        ( for pair in "en-US-AnaNeural|$SPEAK_EN" "zh-CN-XiaoyiNeural|$SPEAK_ZH"; do
              V="${pair%%|*}"; TXT="${pair#*|}"; [ -n "$TXT" ] || continue
              WAV=$(mktemp --suffix=.wav 2>/dev/null || echo /tmp/bible_tts_$$.wav)
              nice -n 15 python3 "$TTS_PY" --voice "$V" --rate=+0% --text "$TXT" --write-media "$WAV" >/dev/null 2>&1 \
                  && { command -v paplay >/dev/null 2>&1 && paplay "$WAV" || aplay -q "$WAV"; } >/dev/null 2>&1
              rm -f "$WAV"
          done ) >/dev/null 2>&1 < /dev/null &
    else
        echo "bible_tts: no speech (verse lines, pet_tts_local.py or the kokoro model is missing)" >&2
    fi
fi
TMP=$(mktemp --suffix=.txt 2>/dev/null || echo /tmp/bible_verse_$$.txt)
echo "$OUT" | fold -s -w 70 > "$TMP" 2>/dev/null || echo "$OUT" > "$TMP"
SHOW="$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x"
# REAL FIX 2026-08-10 (same class as dispatch.sh, one dir up): hardcoded
# to the dev-tree entity location, wrong once book-stack runs from pals.
# PACKAGE_DIR is exported by meta.pdl's "Read" method and inherited
# through every exec (prisc+x -> dispatch.sh -> this script) - use it.
PKG="${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}"
if [ -x "$SHOW" ]; then
    "$SHOW" "$PKG" "$TMP"
else
    cat "$TMP"
fi
