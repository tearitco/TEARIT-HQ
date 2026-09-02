#!/bin/bash
# dispatch.sh - real Show Choices orchestration for book-stack's own
# page_1. prisc+x's own real exec opcode only supports ONE literal arg
# (confirmed this session, !.HOUSE_STDS.md §H.5.1) and has no way to
# capture a command's own stdout into a PAL register - so the real
# "call khtpm_show_choices, branch on its result" logic lives here, in
# a real shell wrapper, not in event.pal itself. event.pal's own exec
# line just calls this ONE script with zero extra args.

SCRIPT_DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd)" || { echo "Error: can't find script dir"; exit 1; }
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../.." 2>/dev/null && pwd)" || { echo "Error: can't find house"; exit 1; }

# PACKAGE_DIR should be exported from meta.pdl, fallback to find
if [ -z "$PACKAGE_DIR" ]; then
PACKAGE_DIR=$(find "$HOUSE" -type d -name "book-stack" 2>/dev/null | grep -E "monads.*book-stack/entities" | head -1)
fi

# Find choices file
CHOICES_FILE=$(find "$HOUSE" -type f -name "choices.objects.pdl" 2>/dev/null | grep "book-stack" | head -1)

# Find khtpm_show_choices binary
CHOICES_BIN=$(find "$HOUSE" -type f -name "khtpm_show_choices.+x" 2>/dev/null | head -1)

# Validate paths exist
if [ ! -d "$PACKAGE_DIR" ]; then echo "Error: PACKAGE_DIR not found: $PACKAGE_DIR"; exit 1; fi
if [ ! -f "$CHOICES_FILE" ]; then echo "Error: CHOICES_FILE not found: $CHOICES_FILE"; exit 1; fi
if [ ! -x "$CHOICES_BIN" ]; then echo "Error: CHOICES_BIN not found: $CHOICES_BIN"; exit 1; fi

# Call the choices picker
PICKED=$("$CHOICES_BIN" "$PACKAGE_DIR" "$CHOICES_FILE")

case "$PICKED" in
    bible_text) bash "$SCRIPT_DIR/branches/bible_text/run.sh" ;;
    bible_tts)  bash "$SCRIPT_DIR/branches/bible_tts/run.sh" ;;
    tao)        bash "$SCRIPT_DIR/branches/tao/run.sh" ;;
esac
