#!/bin/sh
# remove_baks.sh - Remove all .bak* backup files to reduce project size

cd "$(dirname "$0")/.."

echo "=== REMOVING BACKUP FILES (.bak*) ==="

# Count files before removal
count=$(find . -name "*.bak*" 2>/dev/null | wc -l)
echo "Found $count backup file(s)"
echo ""

# Remove them with feedback
for file in $(find . -name "*.bak*" 2>/dev/null); do
    echo "  - Deleting: $file"
    rm -f "$file"
done

echo ""
echo "=== DONE: REMOVED $count BACKUP FILE(S) ==="
