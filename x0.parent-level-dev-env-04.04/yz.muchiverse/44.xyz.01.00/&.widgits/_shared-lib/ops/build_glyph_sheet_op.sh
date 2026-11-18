#!/bin/sh
# build_glyph_sheet_op.sh - build the sheet-to-Chinese-glyph lookup op (glyph_sheet_op.c over ../glyph_sheet.c)
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/glyph_sheet_op.+x glyph_sheet_op.c && echo "OK +x/glyph_sheet_op.+x"
