#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

git add -A
git status --short

git commit -F - <<'COMMIT_MSG_EOF'
Build real managers for bookmarks/palettes, replacing bash XML generation

Per TPMOS-COMPLIANCE-DEBT.md's own standing rule (added earlier tonight
after a process-failure correction): the compliant manager+module
pattern was already proven for stats-hq, and the follow-up call was to
build it for bookmarks/palettes too instead of leaving them on bash
printf-XML composition.

- bookmarks_manager.c (new): reads bookmarks.pdl per pal, publishes
  bookmarks_state.txt. bookmarks.chtpm/.css are now static, provisioned
  once per pal from template files. The renderer's
  dbhq_inject_bookmark_items() builds real rows at runtime. Deletes the
  chtpm-live-reload workaround from the last commit - dead code the
  moment a real manager exists, exactly as predicted.
- palettes_manager.c (new): one binary serves both emojis/elements
  categories via a module args attribute (small generic renderer
  extension - module tags can carry static extra argv now). Real
  quote-aware CSV parsing (the old bash IFS split silently mis-split
  quoted fields with embedded commas). Stub categories stay fully
  static, no manager needed. Also populates open-hai's shared emoji
  registry with the ~46 chemistry-compound codepoints it was missing,
  fixing blank/tofu glyphs in the elements category for any consumer,
  not just palettes.
- bm_menu.sh / palettes_menu.sh: thin launchers only now, zero XML
  generation.
- Real bugs found and fixed along the way: a sed replacement and a
  shell postcmd both silently broke on this house's own literal
  ampersand in one of its own directory names (sed's match-reinsert
  special char; the shell's background-job operator) - fixed via
  escaping and single-quoting respectively; the onclick field (512
  bytes) truncated a real 913-byte postcmd, bumped to 1536; a
  pre-existing double nav-assignment bug desynced focus from the
  rendered highlight for bookmarks; the wide-tile CSS class relied on
  min-width/width-auto, never supported by the layout engine (only a
  real width is) - silently gave every wide element zero width,
  invisible background, and crowded overlapping labels.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
COMMIT_MSG_EOF

echo "--- git log ---"
git log --oneline -5

echo "--- pushing ---"
git push origin main

echo "--- done ---"
