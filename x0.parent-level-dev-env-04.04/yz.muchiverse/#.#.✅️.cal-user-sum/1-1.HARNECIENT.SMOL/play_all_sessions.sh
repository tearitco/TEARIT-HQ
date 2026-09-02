#!/bin/bash
# Play all HARNECIENT training sessions in order using mpg123

# REAL FIX 2026-09-01 (S1_HOUSE_PATH_MIGRATION.md) - was a hardcoded
# absolute path; derive from this script's own real location instead
# (sibling 1-1.HARNECIENT.AUBIO/, same real house-standard pattern).
AUDIO_DIR="$(cd "$(dirname "$0")/../1-1.HARNECIENT.AUBIO" && pwd)/audio-book"

# Check if directory exists
if [ ! -d "$AUDIO_DIR" ]; then
    echo "✗ Audio directory not found: $AUDIO_DIR"
    exit 1
fi

# Get all DAY_*.mp3 files in order
files=$(ls -1 "$AUDIO_DIR"/DAY_*.mp3 2>/dev/null | sort -V)

if [ -z "$files" ]; then
    echo "✗ No audio files found in $AUDIO_DIR"
    exit 1
fi

count=$(echo "$files" | wc -l)
echo "🎙️  HARNECIENT TRAINING SESSIONS — $count files"
echo "=================================================="
echo ""

# Play each file in order
current=1
for file in $files; do
    filename=$(basename "$file")
    echo "[$current/$count] Playing: $filename"
    mpg123 "$file"
    if [ $? -eq 0 ]; then
        echo "  ✓ Completed"
    else
        echo "  ✗ Error playing file"
    fi
    echo ""
    current=$((current + 1))
done

echo "=================================================="
echo "✓ All sessions completed"
