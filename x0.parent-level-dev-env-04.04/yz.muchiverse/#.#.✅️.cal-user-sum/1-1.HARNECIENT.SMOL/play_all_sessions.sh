#!/bin/bash
# Play all HARNECIENT training sessions in order using mpg123

AUDIO_DIR="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.✅️.cal-user-sum/1-1.HARNECIENT.AUBIO/audio-book"

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
