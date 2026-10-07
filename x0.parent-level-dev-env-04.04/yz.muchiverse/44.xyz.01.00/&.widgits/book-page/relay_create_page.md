# Relay driver case (DOCUMENTATION ONLY, NOT RUN): create `pre-design:eden-test` through the REAL taskbar on a BETA copy

Why: `book_page_op` + `harness/book_page.pal` prove the on-disk shape on scratch roots. This case proves the taskbar's own menus produce that same
shape (`desks/eden-test.pdl` in the `pre-design` book). It is a rehearsal for beta, never for the live tree. The owner's go-ahead is needed for live.

## Preconditions
- A BETA copy of the house (`/home/no/staging/beta`), with a user whose `home/livedesk/sessions/` has a book named `pre-design` (`session.pdl`: `STATE | name | pre-design`). Compiled programs are in no branch: build first (`sh '$.crypts/button.sh' build` inside beta).
- Taskbar running from beta. Find its renderer PID by argv + chtpm path, not by a bare name (`ps -eo pid,args | grep khtpm_core_render` and pick the one whose path is under beta and is the taskbar strip). Never use `pkill -f <pattern>` in a tool shell (it kills that shell).
- Never touch the live tree or its relay files. The relay is per process: `<beta>/.../yz.muchiverse/#.desktop/entity_menu_history/<pid>.txt` (append-only, cursor-based, never truncate).

## How the taskbar reaches the rows (khtpm_taskbar_manager.c)
- `file` cell submenu rows come from `#.desktop/livedesk_taskbar.pdl` `file_menu_N_label/_cmd`: row 1 `new-desk` = `livedesk:new-desk`.
  `livedesk_new_desk()` creates an EMPTY `desks/desk_NN.pdl` (next free NN) in the CURRENT book, snapshots + closes the open windows, and makes it the ACTIVE page.
- `desks` cell submenu: one row per page, then the actions `edit` (`livedesk:edit-desk`), `+new-desk` (`livedesk:new-desk`), `cancel`.
  `edit` opens the cli-io rename modal (`ktb_cliio_open_rename_desk`), buffer seeded with the ACTIVE page name; Enter arms typing, characters type, Enter submits
  (`livedesk_rename_desk`: refuses an existing name, only `cliio_key_allowed` characters; renames the file and moves `active_desk` along); Esc backs out one level.
- The book is the current session: pick `pre-design` first (hq/session cell -> `livedesk:open-session:<id>`), otherwise `new-desk` lands in whatever book is active.

## Steps (each is `echo "KEY_PRESSED: <n>" >> <relay>`; a `# why` line is a no-op audit note)
0. Record before: `ls desks/` of the `pre-design` book dir, its `session.pdl`, `livedesk_open.txt` (window list is closed by new-desk, so note what was open).
1. Dump a frame first (`dump_frame_png_op.+x <window-id> out.png`) and read the REAL nav numbers; nav numbering is global across open windows. Never hardcode digits.
2. Open the session cell, choose `pre-design` (confirm via `session.pdl`'s `active_session` in the sessions root).
3. Open the `file` cell, choose `new-desk` (nav digit from step 1, then `KEY_PRESSED: 13`). Proof: a new `desk_NN.pdl` appears in `pre-design/desks/`, `session.pdl` `active_desk` = `desk_NN`.
4. Open the `desks` cell, move to `edit`, Enter. Proof: modal armed (frame dump shows the text field with `desk_NN`).
5. Enter (arms typing), Backspace (`8`) once per character of the seeded name, then type `eden-test` as one `KEY_PRESSED: <ascii>` per character (e=101 d=100 e=101 n=110 `-`=45 t=116 e=101 s=115 t=116), then Enter (13).
6. Proof (state files, cheapest first): `desks/eden-test.pdl` exists, `desks/desk_NN.pdl` is gone, `session.pdl` = `STATE | name | pre-design` + `STATE | active_desk | eden-test`; the taskbar label reads `pre-design:eden-test` (dump frame / `strip_history.txt` growth, never mtime).
7. Compare with the op: on a scratch copy of the same book, `book_page_op --root <scratch> new-page pre-design:eden-test --activate` must give an identical `session.pdl` and an empty page file (`SUMTREE`-style compare).
8. Restore beta to the recorded state (delete the test page via file ops on beta only) if the rehearsal is to be repeated.

## Warning
Relay-driven input goes through the same dispatch as real keys but cannot reproduce real X focus/grab behaviour: a relay pass has already masked a real cli_io focus bug that hardware still showed. A relay pass is NOT proof the modal works by hand. Have the owner (or a hardware session) type the rename once on beta before calling it done. Do not run this against the live tree; creation of `pre-design:eden-test` live needs the owner's go-ahead.
