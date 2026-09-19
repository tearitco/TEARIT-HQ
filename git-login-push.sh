#!/usr/bin/env bash
# git-login-push.sh — one-time GitHub SSH login + push for TEARIT-HQ.
#
# Run it. The FIRST time it prints a public key: add it to GitHub
# (Settings -> SSH and GPG keys -> New SSH key -> paste, any title).
# Then re-run it — it switches origin to SSH (local .git/config only,
# nothing committed) and pushes the current branch.
#
# Usage: git-login-push.sh [REPO_DIR]
#   REPO_DIR defaults to the git worktree next to this script
#   (tearit-hq when the script lives in the 0.opencode-desk dir).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REPO_DIR=""
if [ -n "${1:-}" ]; then
  REPO_DIR="$1"
elif git rev-parse --git-dir >/dev/null 2>&1; then
  REPO_DIR="$PWD"
else
  for d in "$SCRIPT_DIR/tearit-hq" "$SCRIPT_DIR"; do
    if [ -d "$d/.git" ] && git -C "$d" rev-parse --git-dir >/dev/null 2>&1; then
      REPO_DIR="$d"; break
    fi
  done
fi
if [ -z "$REPO_DIR" ]; then
  echo "Cannot find the git worktree. Pass it as the first argument, e.g.:"
  echo "  $0 /path/to/TEARIT-HQ"
  exit 1
fi
cd "$REPO_DIR"

KEY="$HOME/.ssh/id_ed25519"
if [ ! -f "$KEY" ]; then
  echo "No SSH key found — generating one (ed25519, empty passphrase)..."
  ssh-keygen -t ed25519 -N "" -C "tearitco@github" -f "$KEY"
fi

OUT="$(ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -T git@github.com 2>&1 || true)"
if ! printf '%s\n' "$OUT" | grep -q "successfully authenticated"; then
  echo "GitHub SSH is not authenticated yet. One-time setup:"
  echo "  1. Copy this public key into GitHub:"
  echo "     Settings -> SSH and GPG keys -> New SSH key -> paste:"
  echo
  cat "$KEY.pub"
  echo
  echo "  (any title works, e.g. 'TEARIT-HQ opencode')."
  echo "  2. Re-run this script — it will verify and push."
  exit 1
fi

echo "GitHub SSH authenticated."
git remote set-url origin git@github.com:tearitco/TEARIT-HQ.git
echo "Origin switched to SSH (local config only, not committed)."
git push -u origin "$(git branch --show-current)"
echo "Pushed."