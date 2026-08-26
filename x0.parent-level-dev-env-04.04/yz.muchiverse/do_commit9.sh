#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

git add -A
git restore --staged "x0.parent-level-dev-env-04.04/yz.muchiverse^v25.5.05.7z" 2>/dev/null || true
git status --short

git commit -F - <<'COMMIT_MSG_EOF'
Elevate keyboard-accessibility rule to a real house standard

Direct request: document tonight's "no UI element without a mirror
keyboard path" rule in the actual house standards, not just a
project-doc pitfall note.

- HOUSE_STDS.md: new section K.6 - the full rule plus the concrete
  reference pattern (arrow-key edge-autoscroll, real nav-dispatched
  Elems for mouse-only controls, disabled meaning dimmed+inert rather
  than unnumbered, the generic badge_align_left field) so future widgets
  have something to follow, not just a record of what happened tonight.
- HOUSE_STDS.md K.2/K.5 corrected: both still claimed the old standalone
  renderer was live for stats-hq/bookmarks, stale since the manager
  rebuild earlier tonight - now point at the real current state and the
  file's deletion.
- INDEX.md: new Standing Rule 8 mirroring K.6, a new changelog entry,
  and the HOUSE_STDS.md document-roles row updated to mention both
  changes.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
COMMIT_MSG_EOF

echo "--- git log ---"
git log --oneline -5

echo "--- pushing ---"
git push origin main

echo "--- done ---"
