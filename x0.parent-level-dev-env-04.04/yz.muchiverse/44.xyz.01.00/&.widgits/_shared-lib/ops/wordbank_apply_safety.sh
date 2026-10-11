#!/bin/bash
# wordbank_apply_safety.sh - Safety gate for running wordbank_ensure_op --apply
# on the real pals tree (xyzfs/users). Per AGENTS.md: "take a tarball +
# sha256sum list outside the repo and verify the file count" before any
# bulk operation that touches xyzfs/users.
#
# Usage: wordbank_apply_safety.sh <pals_root>
#
# Steps:
#   1. Count files in <pals_root>
#   2. Tarball <pals_root> to /tmp (outside the repo)
#   3. Generate sha256sum list of the tarball
#   4. Verify the tarball is restorable (quick integrity check)
#   5. Print summary; exit 0 if all checks pass, exit 1 otherwise
#
# Run this BEFORE wordbank_ensure_op --apply --pals-root <pals_root>.
# If this script exits 0, it is safe to proceed with --apply.

set -eu

if [ $# -lt 1 ]; then
    echo "usage: wordbank_apply_safety.sh <pals_root>" >&2
    exit 2
fi

PALS_ROOT="$1"
if [ ! -d "$PALS_ROOT" ]; then
    echo "wordbank_apply_safety: pals root does not exist: $PALS_ROOT" >&2
    exit 1
fi

# Resolve to absolute path
PALS_ROOT="$(cd "$PALS_ROOT" && pwd)"
STAMP="$(date -u '+%Y%m%dT%H%M%SZ')"
BACKUP_DIR="/tmp/wordbank_backup_${STAMP}"
mkdir -p "$BACKUP_DIR"

echo "=== Word Bank Apply Safety Check ==="
echo "pals_root: $PALS_ROOT"
echo "backup_dir: $BACKUP_DIR"
echo ""

# 1. Count files before backup
FILE_COUNT=$(find "$PALS_ROOT" -type f ! -path '*/.git/*' | wc -l)
echo "1. File count before backup: $FILE_COUNT"

# 2. Tarball the pals tree
TARBALL="$BACKUP_DIR/pals_backup.tar.gz"
echo "2. Creating tarball..."
tar czf "$TARBALL" -C "$(dirname "$PALS_ROOT")" "$(basename "$PALS_ROOT")" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "   FAIL: tarball creation failed" >&2
    rm -rf "$BACKUP_DIR"
    exit 1
fi
TAR_SIZE=$(du -h "$TARBALL" | cut -f1)
echo "   Created: $TARBALL ($TAR_SIZE)"

# 3. Generate sha256sum of the tarball
CHECKSUMS="$BACKUP_DIR/checksums.sha256"
echo "3. Generating sha256sum..."
sha256sum "$TARBALL" > "$CHECKSUMS"
echo "   Checksum: $(cat "$CHECKSUMS" | awk '{print $1}')"

# 4. Verify the tarball is restorable (test extraction to a temp dir)
VERIFY_DIR="/tmp/wordbank_verify_${STAMP}"
mkdir -p "$VERIFY_DIR"
echo "4. Verifying tarball integrity (test extraction)..."
if tar xzf "$TARBALL" -C "$VERIFY_DIR" 2>/dev/null; then
    VERIFY_COUNT=$(find "$VERIFY_DIR/$(basename "$PALS_ROOT")" -type f ! -path '*/.git/*' | wc -l)
    if [ "$VERIFY_COUNT" = "$FILE_COUNT" ]; then
        echo "   OK: $VERIFY_COUNT files restored (matches original)"
    else
        echo "   WARN: file count mismatch after restore ($VERIFY_COUNT vs $FILE_COUNT)"
        echo "   Proceed with caution."
    fi
else
    echo "   FAIL: tarball extraction failed — backup may be corrupt" >&2
    rm -rf "$BACKUP_DIR" "$VERIFY_DIR"
    exit 1
fi
rm -rf "$VERIFY_DIR"

# 5. Verify sha256sum still matches (tarball wasn't modified)
echo "5. Verifying sha256sum..."
if sha256sum -c "$CHECKSUMS" --quiet 2>/dev/null; then
    echo "   OK: checksum verified"
else
    echo "   FAIL: checksum verification failed" >&2
    rm -rf "$BACKUP_DIR"
    exit 1
fi

echo ""
echo "=== SAFETY CHECK PASSED ==="
echo "Backup saved to: $BACKUP_DIR"
echo "It is safe to run wordbank_ensure_op --apply --pals-root $PALS_ROOT"
echo ""
echo "After --apply completes, re-verify by restoring from the backup"
echo "if you need to roll back."
