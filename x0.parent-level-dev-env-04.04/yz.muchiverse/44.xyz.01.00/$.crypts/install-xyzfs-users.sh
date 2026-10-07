#!/bin/sh
# install-xyzfs-users.sh - install a house's xyzfs/users data (made by tar on the Linux house) into a house checkout on this machine.
# Works on macOS and Linux (POSIX sh). Dry run by default; NOTHING is started; it never overwrites existing users data.
#
#   sh install-xyzfs-users.sh <xyzfs-users-....tar.gz> <house_root> [--apply] [--replace]
#
#   house_root  the folder that contains $.crypts, xyzfs ... e.g. ~/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
#   --apply     really unpack (without it: only checks and prints the plan)
#   --replace   if <house_root>/xyzfs/users already has files, MOVE it aside to users.bak-<time> first (never deleted)
# Checks: archive sha256 (against <archive>.archive.sha256 when present), safe paths only (all under users/, no .., no absolute),
# case-collisions (a case-insensitive Mac would merge them), file count after unpacking equals the archive's count.
set -u
ARC="${1:-}"; HOUSE="${2:-}"; APPLY=0; REPLACE=0
[ -n "$ARC" ] && [ -n "$HOUSE" ] || { sed -n 2,14p "$0"; exit 2; }
shift 2; for a in "$@"; do case "$a" in --apply) APPLY=1;; --replace) REPLACE=1;; *) echo "unknown option $a"; exit 2;; esac; done
[ -f "$ARC" ] || { echo "no such archive: $ARC"; exit 2; }
[ -d "$HOUSE" ] || { echo "no such house folder: $HOUSE"; exit 2; }
[ -d "$HOUSE/\$.crypts" ] || { echo "REFUSED: $HOUSE has no \$.crypts - not a house root"; exit 2; }
sha() { if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1; else sha256sum "$1" | cut -d' ' -f1; fi; }
GOT=$(sha "$ARC"); echo "archive sha256: $GOT"
if [ -f "$ARC.archive.sha256" ]; then
  WANT=$(cut -d' ' -f1 "$ARC.archive.sha256"); [ "$GOT" = "$WANT" ] && echo "checksum OK (matches $ARC.archive.sha256)" || { echo "CHECKSUM MISMATCH: expected $WANT"; exit 3; }
else echo "WARNING: no $ARC.archive.sha256 next to the archive; checksum not compared"; fi
LIST=$(mktemp); tar -tzf "$ARC" > "$LIST" || { echo "cannot read archive"; rm -f "$LIST"; exit 3; }
BAD=$(grep -v '^users/' "$LIST" | grep -v '^users$' | head -3); [ -z "$BAD" ] || { echo "REFUSED: paths outside users/: $BAD"; rm -f "$LIST"; exit 3; }
grep -E '(^|/)\.\.(/|$)|^/' "$LIST" >/dev/null && { echo "REFUSED: unsafe path (.. or absolute) in archive"; rm -f "$LIST"; exit 3; }
N=$(grep -vc '/$' "$LIST"); echo "files in archive: $N"
DUP=$(awk '{print tolower($0)}' "$LIST" | sort | uniq -d | head -3)
[ -z "$DUP" ] || { echo "REFUSED: names that differ only by case (would collide on a case-insensitive disk): $DUP"; rm -f "$LIST"; exit 3; }
rm -f "$LIST"
DEST="$HOUSE/xyzfs"; echo "target: $DEST/users"
if [ -d "$DEST/users" ] && [ -n "$(ls -A "$DEST/users" 2>/dev/null)" ]; then
  if [ "$REPLACE" = 1 ]; then echo "existing users data will be MOVED to users.bak-<time> (not deleted)"; else echo "REFUSED: $DEST/users already has files; use --replace to move it aside"; exit 3; fi
fi
[ "$APPLY" = 1 ] || { echo "DRY RUN OK - nothing written. Re-run with --apply to install."; exit 0; }
mkdir -p "$DEST" || exit 1
if [ -d "$DEST/users" ] && [ -n "$(ls -A "$DEST/users" 2>/dev/null)" ]; then mv "$DEST/users" "$DEST/users.bak-$(date +%Y%m%d-%H%M%S)" || exit 1; fi
tar -xzf "$ARC" -C "$DEST" || { echo "UNPACK FAILED"; exit 1; }
GOTN=$(find "$DEST/users" -type f | wc -l | tr -d ' ')
[ "$GOTN" = "$N" ] && echo "INSTALLED $GOTN files = archive count" || { echo "COUNT MISMATCH: installed $GOTN, archive has $N (case/Unicode name merging on this disk?)"; exit 4; }
echo "Next (by hand): cd '$HOUSE' && sh '\$.crypts/button.sh' build   # compiled programs are not in git"
echo "Then start the desktop only after you have looked at $DEST/users."
