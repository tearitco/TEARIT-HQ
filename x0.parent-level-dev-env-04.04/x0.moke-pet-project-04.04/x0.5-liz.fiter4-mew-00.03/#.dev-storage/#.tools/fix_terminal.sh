#!/bin/bash
# fix_terminal.sh - Restore terminal if Ctrl+C breaks it
# Usage: ./fix_terminal.sh (or just run if terminal is messed up)

echo "=== Restoring Terminal ==="

# Reset terminal to sane defaults
reset

# Alternative: stty sane
stty sane 2>/dev/null

# Make sure echo works
echo ""
echo "✓ Terminal restored!"
echo ""
echo "If this didn't work, try:"
echo "  1. Type: reset (then press Enter)"
echo "  2. Or close and reopen the terminal"
