#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

git add -A
git status --short

git commit -m "Build real managers for bookmarks/palettes, replacing bash XML generation

Per TPMOS-COMPLIANCE-DEBT.md's own standing rule (added earlier tonight
after a process-failure correction): the compliant manager+<module>
pattern was already proven for stats-hq, and the follow-up call was to
build it for bookmarks/palettes too instead of leaving them on bash
printf-XML composition.

- bookmarks_manager.c (new): reads <pal>/bookmarks.pdl, publishes
  <pal>/bookmarks_state.txt. bookmarks.chtpm/.css are now static,
  provisioned once per pal from bookmarks.template.chtpm/.css. The
  renderer's dbhq_inject_bookmark_items() builds real rows at runtime.
  Deletes the chtpm-live-reload workaround from the last commit - dead
  code the moment a real manager exists, exactly as predicted.
- palettes_manager.c (new): one binary serves both emojis/elements
  categories via <module args="<category>"/> (small generic renderer
  extension - <module> tags can carry static extra argv now). Real
  quote-aware CSV parsing (the old bash IFS=, split silently mis-split
  quoted fields with embedded commas). Stub categories stay fully
  static, no manager needed.
- bm_menu.sh / palettes_menu.sh: thin launchers only now, zero XML
  generation.
- Real bugs found and fixed along the way: a sed replacement and a
  sh -c postcmd both silently broke on this house's own literal '&' in
  &.widgits (sed's match-reinsert special char; sh's background-job
  operator) - fixed via escaping and single-quoting respectively;
  Elem.onclick (512B) truncated a real 913B postcmd, bumped to 1536;
  a pre-existing double nav-assignment bug in dbhq_assign_nav_indices()
  desynced focus from the rendered highlight for bookmarks; .pal-wide's
  min-width/width:auto were never supported by css_layout_pass (only
  real width: is) - silently gave every wide element w=0, invisible
  background, and crowded overlapping labels.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"

echo "--- git log ---"
git log --oneline -5

echo "--- pushing ---"
git push origin main

echo "--- done ---"
