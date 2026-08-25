#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

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
