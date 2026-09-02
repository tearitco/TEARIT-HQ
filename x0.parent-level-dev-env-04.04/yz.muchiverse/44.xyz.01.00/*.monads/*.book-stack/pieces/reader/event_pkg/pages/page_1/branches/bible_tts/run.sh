#!/bin/bash
# Bible verse + TTS — same asset root as bible_text/run.sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../bible_text/run.sh
# Reuse text path first
bash "$SCRIPT_DIR/../bible_text/run.sh"
# TTS remains Linux-oriented (edge-tts); Win uses run.ps1
if command -v edge-tts >/dev/null 2>&1 && command -v mpg123 >/dev/null 2>&1; then
    : # full TTS path still in git history if needed later
fi
