#!/bin/bash
set -e
# REAL FIX 2026-09-01 (S1_HOUSE_PATH_MIGRATION.md) - was a hardcoded
# absolute path, broke the moment the house moved. Real house standard:
# resolve dynamically. This script always lives a fixed 2 levels under
# the real git root (yz.muchiverse's own parent's parent) - climb from
# \$0 instead of hand-editing a literal path every time the house moves.
cd "$(cd "$(dirname "$0")/../.." && pwd)"

NAME="tearitco"
EMAIL="88365268+tearitco@users.noreply.github.com"

# Amend the ONE existing commit with the correct author + committer
# (git config for THIS repo only, not global - doesn't touch your
# real identity elsewhere).
git config user.name "$NAME"
git config user.email "$EMAIL"
GIT_AUTHOR_NAME="$NAME" GIT_AUTHOR_EMAIL="$EMAIL" \
GIT_COMMITTER_NAME="$NAME" GIT_COMMITTER_EMAIL="$EMAIL" \
  git commit --amend --reset-author --no-edit

echo "--- new commit identity ---"
git log -1 --format="Author: %an <%ae>%nCommitter: %cn <%ce>"

echo "--- force-pushing corrected commit ---"
git push --force-with-lease origin main

echo "--- done ---"
gh repo view tearitco/TEARIT-HQ --json url -q .url
