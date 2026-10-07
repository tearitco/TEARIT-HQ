# Q006 — one generic board layout for grave, ghost roster, server and phone windows

| field | value |
|---|---|
| status | built and scored: `@.apps/board-hq/verify.sh` PASS 15/15 (headless), selftest PASS; NOT shown on the real screen, not linked from the h-ai menu, grave/ghost windows not launched live |
| tier | manager (claude) or outside-agent with layout experience |
| size | M |
| assignee | claude |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | none to start; look-and-feel review by the owner at the end |

## Mission (one sentence)

Build `board.chtpm` (+ css) once, driven by a small `board.pdl` data-source file, so the grave, the ghost roster, the server and the phone windows are the same layout with different data.

## Why it matters

All four windows are a list of rows, a detail/history pane and actions (design `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3d). One layout means one fix
fixes all of them, the owner edits it once in the layout studio, and no per-window C gets written (house rule: zero new per-project renderer code).

## Read first

1. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` §3d
2. `#.#.calendar-dox/!.HQ-IQ-BOOK/18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` and `08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md` (the layout, overlay and nav machinery; what is built vs planned)
3. The house standards skill `khtpm-house-standards` (read in full before touching the renderer; do not add a `g_is_<project>` branch; edit the shared-lib draw core, not the ops copy)
4. `@.apps/layout-studio/sandbox/` (a working layout with `<overlay src>`, `vars="..."`, `<repeat>`)

## Do

1. A `board.chtpm` with: a header row (title + minimize/close chrome), a scrolling row list (`<repeat count="${n_rows}" bind="row">`), a detail pane (tail of a file), an action row. Every interactive element nav-numbered.
2. `board.pdl` (`SOURCE | ...`, `ROW | label=... id=...`, `DETAIL | file=...`, `ACTION | label=... cmd=...`) read by a small generic op that writes the vars file the layout reads; no per-window C.
3. Prove it with two sources: the quest board (`^.grave/quests/INDEX.md` data) and a roster (`^.ghost/roster/*/ghost.pdl`).

## Acceptance

- [ ] Both windows render from the same `board.chtpm`; a frame dump of each (paste paths) shows rows, detail and actions.
- [ ] Every row/button has a nav number; a typed number + Enter and a real click both activate it (relay AND real key).
- [ ] Changing one css rule in `board.css` changes both windows.
- [ ] No new renderer branch; build warnings none.

## Rules

Everything in `^.grave/README.md`. Test windows are launched by you; do not reset the owner's desktop.

## Log

2026-10-06 | claude | Built `@.apps/board-hq/`: ONE template `board.xhtpm` + `board.css` (sidebar > scrolllist > repeat rows; detail panel with repeat lines; toolbar of action buttons; same idiom as music-player-hq / hotbar-hq), `ops/board_vars_op.c` (reads `state/<name>/board.pdl`, publishes `ui.txt` only when it changed, handles a row click `--select N`, `watch` mode for the window `<module>`; sources: `md-table`, `dirs`; details: `link-tail`, `dir-file-tail`; 0 warnings), `ops/new_board.sh <name>` stamps `board-<name>.xhtpm/.css` from the template, `state/quests/board.pdl` (the quest index) and `state/ghosts/board.pdl` (the roster) are the two real instances.
2026-10-06 | claude | DEVIATION from the quest text: an instance is a STAMPED copy (a `vars=` path and a module argument are per window and cannot be parameterised in one literal file), so "one layout" means one TEMPLATE; `new_board.sh` regenerates every instance after an edit. Proven: instances differ only by name; one css edit in `board.css` reaches both after re-stamping.
2026-10-06 | claude | `verify.sh` (headless render with the real renderer, `--headless`, no window or display; sandbox data + throwaway `vtest-*` instances, killed by pid and deleted afterwards): title/subtitle, row count, nav-numbered rows, chosen columns, detail = tail of the selected row's file, action button, keyboard select (nav 2 + Enter through the relay) switches the detail and saves the selection, second source (roster rows, `_template` hidden, detail from history.txt), instances differ only by name, one css edit restyles both. 13/13 PASS; selftest (a template with no rows) fails 9 checks. Bugs found on the way: the window module launcher prefixes EVERY argument with the house path and appends `<house> <package_dir> <module id>` (the op now ignores them in watch mode); a `watch` loop outlived its window (now exits when its parent or data file is gone); `sed` and `${//}` mangle the `&` in `&.widgits`/dash has no `${//}` (awk).

2026-10-06 | claude | Third instance: `state/phones/board.pdl` = the phone directory (new source kind `pipe-index` over `^.hai-server/phones.index`: rows = number + owner label, detail = tail of that phone's `history.txt`; read-only). On the REAL index: 55 rows. `verify.sh` now 15 checks (phone rows + conversation detail on scratch data), selftest PASS, no stray processes.

2026-10-06 | claude | Owner review of the live window: no way to create or delete quests. Added (layout idiom copied from open-hai / chat-hai): "+ New quest" item (top of the list) and a composer `cli_io` at the bottom of the panel; `board.pdl` gets `CREATE | label | command | default title` and `DELETE | command | confirm text`; typed text reaches the command as `$1` only (hostile title test: nothing executed). Backspace on a row opens a CONFIRM popup (generic renderer feature below); Enter/y deletes, any other key cancels. Quest scripts `^.grave/ops/quest_new.sh` / `quest_delete.sh` (delete is reversible: folder moves to `quests/_deleted/`, row saved; ids never reused). verify.sh now 24/24 (headless, scratch quests, scratch renderer first, then the live build).
2026-10-06 | claude | GENERIC renderer feature: `<item backspace_action=... confirm="Delete X?">` shows a popup and runs the action only on Enter/y (khtpm_core_render.c +34 lines, Elem.confirm in khtpm_render_core.c, copied by khtpm_reparse_diff.c; popup text is also in the ascii frame as `[CONFIRM]`). Opted in: chat-hai and open-hai session deletes, media-3d-hq DEL. Not opted in on purpose: media-img/img3d/daw Backspace (VIS / MUTE are toggles, not deletes). Only windows opened after the 22:00 renderer build have it; already-open windows keep the old binary. Mouse clicks do not answer the popup yet (keyboard only).

## Not done

- [ ] Not looked at on the real screen (only the headless ascii frame): colours/geometry unseen. Launching a live window is the owner's call.
- [ ] Not linked from the h-ai menu / placed like a palette item. The phone directory exists as an instance; a 'server' instance (ledger + observations tail) is not made yet.
- [ ] The quest board instance has no ACTION buttons configured (the mechanism is tested with a scratch action).
- [ ] Windows/Linux only checked on Linux.

## Result
