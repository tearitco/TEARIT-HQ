#!/bin/bash
# dryrun.sh - READ-ONLY survey for Q007 step 3. Changes nothing. Usage: bash dryrun.sh   (run from anywhere; finds the house from its own path)
HOUSE="$(cd "$(dirname "$0")/../../.." && pwd)"; cd "$HOUSE" || exit 1
echo "== tracked files under xyzfs/users: $(git ls-files xyzfs/users | wc -l)   untracked-but-present: $(git ls-files --others --exclude-standard xyzfs/users | wc -l)   modified now: $(git status --porcelain xyzfs/users | grep -c '^ M\|^M')"
echo "== tracked per user (uuid prefix / guest):"; git ls-files xyzfs/users | sed -E 's#xyzfs/users/([^/]{1,8})[^/]*/.*#\1#' | sort | uniq -c | sort -rn | head -12
echo "== tracked by kind (second path level under home/):"; git ls-files xyzfs/users | sed -E 's#xyzfs/users/[^/]+/home/([^/]+)(/.*)?#\1#' | sort | uniq -c | sort -rn | head -8
echo "== SENSITIVE tracked (wallet/key/secret names):"; git ls-files xyzfs/users | grep -iE 'wallet|api_key|secret|password|\.pem' | sed -E 's#xyzfs/users/([^/]{1,8})[^/]*#users/\1..#' | head -12
echo "== code/scripts that READ xyzfs/users paths (potential seed dependencies; first 25):"
git grep -nI -E 'xyzfs/users' -- ':!xyzfs' ':!*.md' ':!*.txt' ':!#.#.calendar-dox' 2>/dev/null | sed -E 's#^([^:]{0,80})[^:]*:([0-9]+):.*#\1:\2#' | head -25
echo "== other tracked per-desk runtime files modified now (outside xyzfs):"; git status --porcelain | grep -E '^ M' | grep -vE 'xyzfs/users' | sed -E 's#^...(.*)#\1#; s#.*/yz.muchiverse/##' | head -10
