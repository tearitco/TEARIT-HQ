#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <file.txt|file.md> | <dir/>"
    echo "  <file>   - Process a single document file (.txt or .md)"
    echo "  <dir/>   - Process all documents in a directory"
    exit 1
fi

TARGET="$1"
TEMP_DIR=$(mktemp -d)
echo "Creating TTS files in: $TEMP_DIR"
echo ""

process_file() {
    local FILE="$1"

    if [ ! -f "$FILE" ]; then
        echo "Skipping: $FILE (not found)"
        return
    fi

    # Only process .txt and .md files, skip code files
    case "$FILE" in
        *.txt|*.md)
            ;;
        *)
            return
            ;;
    esac

    echo "Processing: $FILE"

    # Extract text, skip code blocks (lines between ``` or indented code)
    TEXT=$(cat "$FILE" | sed '/^```/,/^```/d' | sed '/^    /d' | sed '/^\t/d' | grep -v '^$' | tr '\n' ' ')

    # Trim whitespace
    TEXT=$(echo "$TEXT" | xargs)

    if [ -n "$TEXT" ]; then
        # Create output mp3 filename based on input file
        FILENAME=$(basename "$FILE" | sed 's/\.[^.]*$//')
        TEMP_MP3="$TEMP_DIR/${FILENAME}.mp3"

        # Generate TTS using edge-tts (Female Chinese voice)
        edge-tts --voice zh-CN-XiaoxiaoNeural --text "$TEXT" --write-media "$TEMP_MP3" > /dev/null 2>&1

        if [ -f "$TEMP_MP3" ]; then
            echo "  ✓ Created: $TEMP_MP3"
            mpg123 -q "$TEMP_MP3"
        else
            echo "  ✗ Failed to create TTS for $FILE"
        fi
    else
        echo "  ⊘ No text extracted from $FILE"
    fi
    echo ""
}

# Check if target is a directory
if [[ "$TARGET" == */ ]]; then
    TARGET="${TARGET%/}"  # Remove trailing slash
    if [ -d "$TARGET" ]; then
        echo "Processing all documents in directory: $TARGET"
        echo ""
        for FILE in "$TARGET"/*.txt "$TARGET"/*.md; do
            [ -f "$FILE" ] && process_file "$FILE"
        done
    else
        echo "Error: Directory not found: $TARGET"
        exit 1
    fi
else
    # Single file
    process_file "$TARGET"
fi

echo ""
echo "Done! TTS files are in: $TEMP_DIR"
echo "To play: mpg123 $TEMP_DIR/*.mp3"
echo "To delete: rm -rf $TEMP_DIR"
