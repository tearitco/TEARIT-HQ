#!/usr/bin/env bash
# =====================================================================
# macsync.sh — push/pull the T@Q (tomom@qroq) dir to/from the LAN Mac.
#
# YOU DON'T NEED TO KNOW rsync/ftp. Just:
#     ./macsync.sh push       # home -> Mac (use after editing code here)
#     ./macsync.sh pull       # Mac -> home (use after editing on the Mac)
#     ./macsync.sh check      # md5 compare, show any drift
#
# TL;DR FAQ (the part you "suck at ftp")
#   Q: Which one do I run to SEND my changes to the Mac?
#   A: `./macsync.sh push`. Home box is the source of truth; the Mac is
#      just the machine that trains (fast, big CPU).
#   Q: What if I edited files ON the Mac? 
#   A: `./macsync.sh pull` to bring them back here BEFORE you quit, so
#      nothing is lost. Then keep editing here.
#   Q: Do I need a password? Will it ask me?
#   A: No and no — auth is wired in with sshpass (password 1234).
#   Q: Does push DELETE stuff on the Mac?
#   A: Yes, push uses --delete: anything on the Mac not on this box is
#      removed. That keeps them identical. Don't push if you have edits
#      on the Mac you haven't pulled yet.
#   Q: What is NOT synced / protected?
#   A: Two things are ALWAYS protected:
#       1) curriculum/*_train/ — the trained matrices. They are generated
#          ONLY on the Mac by the trainer. push NEVER touches them (they are
#          excluded from --delete, so they can't be wiped). pull DOES bring
#          them back here, so this box always keeps a backup copy.
#       2) +x/*.+x compiled binaries — each machine keeps its own build
#          (Linux ELF here, macOS Mach-O on the Mac). push/pull both exclude
#          them; rebuild on the Mac after pushing code.
#      Everything else (source .c files, curricula, docs) syncs both ways.
#   Q: How do I make sure trained data is never lost again?
#   A: Run `./macsync.sh pull` after any training on the Mac. That copies the
#      tiny curriculum/*_train/ matrices back here (they're only ~100KB each,
#      nothing like a real LLM), so this box is always the backup.
#      push can never delete them (protected by exclude), so even a full
#      re-push after a clean checkout keeps the trained data safe.
#   Q: What if the Mac is off / host unreachable?
#   A: You'll see "ssh: connect to host ... refused/timeout". That means
#      the Mac is off or not on the LAN. Nothing was changed here.
#
# =====================================================================
set -euo pipefail

HOST=10.0.0.144
USER=lfs.master
PASS=1234
SSH_OPTS="-o PreferredAuthentications=password -o PubkeyAuthentication=no"

SRC="$(cd "$(dirname "$0")/3.stage.llm.tomom@qroq.fame]921🐋️" && pwd)/"
DST="$USER@$HOST:/Users/lfs.master/tomom-qroq/"

ssh_cmd() { sshpass -p "$PASS" ssh $SSH_OPTS "$USER@$HOST" "$@"; }

RSYNC_RSH="sshpass -p $PASS ssh $SSH_OPTS"

# Two things must be protected from push --delete:
#  1) curriculum/*_train/  - trained matrices, generated ONLY on the Mac.
#  2) +x/*.+x compiled binaries - platform-specific (Linux ELF here vs macOS on
#     the Mac). Pushing these overwrites the Mac's working binaries AND, worse,
#     --delete would strip them entirely. Rebuild on the Mac instead.
# NEVER remove these excludes.
PUSH_EXCLUDES=(
  --exclude='curriculum/*_train/'
  --exclude='+x/*.+x'
)

case "${1:-}" in
  push)
    echo ">> PUSH home -> Mac ($HOST:tomom-qroq)"
    echo ">> (curriculum/*_train/ is PROTECTED - never sent or deleted)"
    rsync -av --delete "${PUSH_EXCLUDES[@]}" -e "$RSYNC_RSH" "$SRC" "$DST"
    echo ">> done."
    ;;
  pull)
    echo ">> PULL Mac -> home (includes trained matrices from curriculum/*_train/)"
    echo ">> (+x/*.+x binaries are NOT pulled - each machine keeps its own build)"
    rsync -av --delete --exclude='+x/*.+x' -e "$RSYNC_RSH" "$DST" "$SRC"
    echo ">> done."
    ;;
  check)
    echo ">> md5 compare (SAME = good, DIFF = drift)"
    cd "$SRC"
    for f in optimizer.c backward_prop.c forward_prop.c trainer.c vocab_model.c chatbot_moe_v1.c; do
      l=$(md5sum "$f" 2>/dev/null | cut -d' ' -f1)
      r=$(ssh_cmd "md5 -q /Users/lfs.master/tomom-qroq/$f" 2>/dev/null)
      if [ -n "$l" ] && [ "$l" = "$r" ]; then
        echo "SAME  $f"
      else
        echo "DIFF  $f  (local=$l remote=$r)"
      fi
    done
    ;;
  *)
    echo "usage: $0 {push|pull|check}"
    echo "  push  -> copy HOME files to the Mac (run after editing here)"
    echo "  pull  -> copy Mac files back HOME (run after editing on Mac)"
    echo "  check -> compare the C sources both sides, show drift"
    ;;
esac
