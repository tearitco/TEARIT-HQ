#!/bin/bash

# count_loc.sh - Count lines of code in all .c files
# Uses dynamic path resolution

# DYNAMIC PATH RESOLUTION
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_FILE="${SCRIPT_DIR}/loc_count.txt"

echo "Lines of Code Count" > "$OUTPUT_FILE"
echo "===================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

TOTAL=0

# Find all .c files and count lines
find "$SCRIPT_DIR" -name "*.c" -type f | while read -r file; do
    COUNT=$(wc -l < "$file")
    echo "$file: $COUNT" >> "$OUTPUT_FILE"
done

# Calculate total
for file in $(find "$SCRIPT_DIR" -name "*.c" -type f); do
    COUNT=$(wc -l < "$file")
    TOTAL=$((TOTAL + COUNT))
done

echo "" >> "$OUTPUT_FILE"
echo "===================" >> "$OUTPUT_FILE"
echo "TOTAL: $TOTAL lines" >> "$OUTPUT_FILE"

echo "LOC count written to $OUTPUT_FILE"
echo "Total: $TOTAL lines in .c files"
