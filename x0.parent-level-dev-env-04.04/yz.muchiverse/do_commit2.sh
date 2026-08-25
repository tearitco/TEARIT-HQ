#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

git add -A
git status --short

git commit -m "Migrate stats-hq/palettes/bookmarks off deprecated khtpm_hq_render.c, delete it

- stats-hq: full TPMOS-compliant rebuild (real stats_hq_manager.c,
  sidebar+panel UI replacing the old broken tabbar)
- palettes: ported sprite/badge/nav onto khtpm_entity_menu_render.c,
  fixed a nav-badge font double-close bug, dark theme, real Up/Down
  grid-row stepping, click-to-place dispatch that was silently missing
- bookmarks: ported open:/exec:/input: generic dispatch,
  chtpm-live-reload, dark theme, its own real window size (was
  inheriting db-hq's 900x600 default and exactly overlapping palettes),
  fixed a nav-index double-assignment bug that desynced focus from the
  rendered ring/badge, ported open-hai's inline emoji-in-text rendering
- khtpm_hq_render.c + build_db_hq.sh deleted: confirmed via grep no
  remaining launch sites after all three consumers migrated
- docs updated: khtpm-merge-how2.md, TPMOS-COMPLIANCE-DEBT.md,
  CREATOR_AGENT.md (new debugging-pitfall section)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"

echo "--- git log ---"
git log --oneline -5

echo "--- pushing ---"
git push origin main

echo "--- done ---"
