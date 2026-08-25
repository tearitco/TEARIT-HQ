#!/bin/bash
# size_audit.sh - Generate comprehensive size report

OUTPUT_FILE="#.tools/size_list.txt"

echo "=== TPMOS PROJECT SIZE AUDIT ===" > "$OUTPUT_FILE"
echo "Date: $(date)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== ROOT LEVEL DIRECTORIES ===" >> "$OUTPUT_FILE"
du -sh */ 2>/dev/null | sort -hr >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== HIDDEN DIRECTORIES ===" >> "$OUTPUT_FILE"
du -sh #.*/ 2>/dev/null | sort -hr >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== TOP 30 LARGEST DIRECTORIES ===" >> "$OUTPUT_FILE"
find . -type d -exec du -sh {} \; 2>/dev/null | sort -hr | head -30 >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== TOP 30 LARGEST FILES ===" >> "$OUTPUT_FILE"
find . -type f -exec du -h {} \; 2>/dev/null | sort -hr | head -30 >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== LEGACY ARCHIVE CONTENTS ===" >> "$OUTPUT_FILE"
du -sh projects/trunk/legacy_archive/* 2>/dev/null | sort -hr >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== INSTALLED APPS ===" >> "$OUTPUT_FILE"
du -sh pieces/apps/installed/* 2>/dev/null | sort -hr >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== BINARY FILES (.+x) ===" >> "$OUTPUT_FILE"
find . -name "*.+x" -exec du -h {} \; 2>/dev/null | sort -hr | head -20 >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "=== TOTAL PROJECT SIZE ===" >> "$OUTPUT_FILE"
du -sh . 2>/dev/null >> "$OUTPUT_FILE"

echo "Size audit complete: $OUTPUT_FILE"
