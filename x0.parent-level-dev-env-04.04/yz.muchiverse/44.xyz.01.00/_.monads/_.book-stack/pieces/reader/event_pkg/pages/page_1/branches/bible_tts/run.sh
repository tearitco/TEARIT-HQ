#!/bin/bash
# Bible verse + speech: the bible_text branch with BIBLE_SPEAK=1 (it shows the verse and speaks the English line of that same verse, offline Kokoro).
# 2026-11-02: the old body derived HOUSE one level short (so the TTS block never ran), took line 1 of the verse tool's output ("ID: ...") instead of the English line,
# and drew a second random verse; all three are gone because the verse is now fetched once, in bible_text.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIBLE_SPEAK=1 exec bash "$SCRIPT_DIR/../bible_text/run.sh"
