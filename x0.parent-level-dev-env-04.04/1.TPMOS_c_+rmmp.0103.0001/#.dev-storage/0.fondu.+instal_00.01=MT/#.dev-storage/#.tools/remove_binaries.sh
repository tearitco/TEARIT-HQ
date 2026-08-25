#!/bin/sh
# remove_binaries.sh - Remove all compiled .+x binaries to reduce project size

cd "$(dirname "$0")/../.."

echo "=== REMOVING COMPILED BINARIES (.+x) ==="

# Count files before removal
count=$(find . -name "*.+x" 2>/dev/null | wc -l)
echo "Found $count binary file(s)"
echo ""

# Remove them with feedback
for file in $(find . -name "*.+x" 2>/dev/null); do
    echo "  - Deleting: $file"
    rm -f "$file"
done

echo ""
echo "=== DONE: REMOVED $count BINARY FILE(S) ==="
echo "Note: Run ./compile_all.sh to rebuild binaries"
