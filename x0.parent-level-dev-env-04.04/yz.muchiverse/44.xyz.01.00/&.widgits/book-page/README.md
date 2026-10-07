# book-page

`ops/book_page_op.c` creates, lists, checks and renames BOOKS (`<sessions_root>/s<N>/` with `session.pdl` + `desks/`) and PAGES (`desks/<page>.pdl`) in the exact on-disk shape the
livedesk taskbar writes (`livedesk_next_id`, `livedesk_ensure_session`, `livedesk_new_desk`, `livedesk_write_active_desk`, `livedesk_rename_desk`). `pre-design:eden-test` = page `eden-test` in book `pre-design`.

- Build: `sh ops/build_book_page_op.sh` (-> `ops/+x/book_page_op.+x`, git-ignored; picked up by the compile runner).
- Usage and exit codes (0 ok, 1 not found, 2 usage/bad name/missing root, 3 exists, 4 I/O): header of `ops/book_page_op.c`. `--root <sessions_root>` is REQUIRED so a test cannot hit live data by accident.
- Verify: `&.widgits/_shared-lib/harness/book_page.pal` (scratch roots only; byte-compare of a created book against a hand-built fixture, duplicates, bad names, ledger, no write outside root).
- `relay_create_page.md`: how to create the same page through the real taskbar on a BETA copy via the relay. Documentation only, not run.
- Not covered: the real taskbar GUI path, live data, and any cleanup of the page in a running desk.
