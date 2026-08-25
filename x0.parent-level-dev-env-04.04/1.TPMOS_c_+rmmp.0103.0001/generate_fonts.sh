#!/bin/bash

# generate_fonts.sh - TPM Font Orchestrator
# Follows the standard of emoji-studio/run.sh

echo "🔨 Compiling Font Generation Op..."
gcc -O2 projects/wraith-alpha/ops/font-gen-op.c -o projects/wraith-alpha/ops/+x/font-gen-op.+x -I/usr/include/freetype2 -lfreetype
if [ $? -ne 0 ]; then echo "❌ Failed to compile font-gen-op"; exit 1; fi

ASSETS_DIR="projects/wraith-alpha/assets/fonts/ascii"
FONT_PATH="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

# 0. NUCLEAR CLEANUP
echo "🧹 Cleaning stale assets..."
rm -rf "$ASSETS_DIR"
mkdir -p "$ASSETS_DIR"

echo "🚀 Generating ASCII set..."

# Generate ASCII 32 to 126
for i in {32..126}
do
    CHAR_DIR="$ASSETS_DIR/$i"
    ./projects/wraith-alpha/ops/+x/font-gen-op.+x "$FONT_PATH" "$i" "$CHAR_DIR" > /dev/null
    if [ $? -eq 0 ]; then
        echo -n "."
    else
        echo "❌ Failed at char $i"
    fi
done

echo ""
echo "✅ ASCII set generated in $ASSETS_DIR"
