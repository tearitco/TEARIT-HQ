#!/usr/bin/env bash
# git-login-push.sh — one-time GitHub SSH login + push for this repo.
#
# Run it. The FIRST time it prints a public key: add it to GitHub
# (Settings -> SSH and GPG keys -> New SSH key -> paste, any title).
# Then re-run it — it switches origin to SSH (local .git/config only,
# nothing committed) and pushes the current branch.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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