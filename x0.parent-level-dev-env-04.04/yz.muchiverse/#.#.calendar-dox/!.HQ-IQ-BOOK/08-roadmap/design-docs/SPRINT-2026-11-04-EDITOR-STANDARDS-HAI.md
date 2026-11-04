# Sprint 2026-11-04: editor, standards, h-ai (brainstorm + plan + test guide)

Owner brief (2026-11-04): drive REAL editor functionality; h-ai and palettes are the default sidebars (in a layout `.pdl`); mouse comes later; test the APIs through edit-hai (the right-bar chat pane in h-ai mode) and through the top-toolbar h-ai, and see whether h-ai can **locate the game session we are working on** (context-injected pid or similar). Focus: editor, standards, h-ai, plus the leftover tests from the CPU night (handoff `09-appendix/handoff-2026-11-03-editor-palettes-cpu-night.md`).

## 0. Done at sprint start
- `@.apps/rpg-edit-hq/layout.pdl` (new): `LAYOUT | left | palettes`, `LAYOUT | right | hai`. `pal_side restore` reads it when the preference `sidebar_start=layout` (new default, first value; Settings tab shows "layout.pdl (h-ai + palettes)"); `last|pets|palettes|none` still override. The right value sets the chat pane mode (`state/flag_chatmode.txt`) and shows the bar. Tested on the real files: default -> left=palettes, right=h-ai; edited layout (pets / none) -> pets left, right hidden; reverted -> default. (State only; window not yet relaunched to look at it.)

## 1. What exists today (verified in code, 2026-11-04)
- Right bar = chat pane with modes `edit` (pet chat), `hai`, `network` (`flag_chatmode.txt`, verb `chatmode`). Typed text in hai mode becomes verb `haisay`, which currently answers with a **canned line** ("not connected to the API yet"). Log `state/hai_chat.txt` (capped 24 KB -> 8 KB).
- Toolbar Chat menu row `h-ai (API, edits this book)` sets the mode. The standalone `&.widgits/open-hai` app has the real API client (README, `haiku-hai-api-testing.md`, sessions with `transcript.txt`).
- Session identity the editor already has: window pid (`board/module_parent.pid`, per-pid relay `#.desktop/entity_menu_history/<pid>.txt`), engine session folder (`board/state/ui.txt` `bv_session=`), process registry rows (`#.desktop/proc_registry.pdl`: role, app, owner, name; rule `@file:...ui.txt#bv_session`), book (`last_book.txt`, `book_label`), selection (xelector `pos_x/pos_y`, armed brush `board/state/brush.txt`, palette nav `pal_nav.txt`).

## 2. Brainstorm: how does h-ai find the session it is working on?
| option | how | good | bad |
|---|---|---|---|
| A. context file (truth) | the editor daemon publishes `state/hai_context.txt` (app, book, room, window pid, bv_session, xelector cell, armed brush, mode flags, `updated` marker) rewritten whole on change; h-ai reads it by path | one fact, one writer ("if it's not a file it's a lie"); works for every caller; cheap (change only) | h-ai must know the app dir (see B/C) |
| B. injected pid | the editor's `haisay` / toolbar action passes `--session-pid <window pid>` (or env `LYFEDESK_SESSION_PID`) to the h-ai call | explicit, no guessing; matches the owner's idea | a bare pid goes stale / is reused; useless for a later call from another tool |
| C. registry lookup | h-ai asks `proc_reg_op <house> list` for rows with role window/app rpg-edit-hq, takes owner pid, finds the session | no editor change; verifies B's pid is alive and ours | several editors open = ambiguity; registry coverage incomplete (125 unregistered sites) |
| D. active-session pointer | one file `#.desktop/active_session.pdl` rows `<app> \| <pid> \| <book> \| <bv_session> \| <epoch>` appended on focus, last row wins | tool-independent discovery for the top-toolbar h-ai that has no editor around it | needs focus events (the window knows its own focus) and a cap |
Recommendation: **B + C + A**: the call carries the pid hint, h-ai verifies it against the registry (alive, app = rpg-edit-hq, owner), then reads that app's `hai_context.txt` (A). For the top-toolbar h-ai use **D** to choose among several editors, verified the same way. Never send the pid blindly: refuse (say "no live session") when the registry check fails.

Open decisions for the owner: (1) top-toolbar h-ai = a different window than edit-hai or the same pane docked? (2) what may h-ai change: propose-then-apply only (default) or direct edits? (3) which provider/key (free models only; keys stay out of logs and out of `hai_context.txt`).

## 3. Sprint plan (editor, standards, h-ai)
1. **Look at it for real:** relaunch the editor through `cpu_gate run`, screenshot the left palettes + right h-ai default, relay-drive the Settings tab choice `layout`.
2. **Context file (A):** daemon writes `hai_context.txt` on change only (marker size growth, not mtime); cap + row in RUNTIME-FILE-GROWTH-STANDARD.md; contents listed above, no secrets, no user paths outside the house.
3. **Session locate verb:** a small C op `hai_session` (`hai_session <house> [--pid N]`) printing the verified context (or an error line); unit cases: pid alive/dead/reused, wrong app, two editors, none.
4. **Wire edit-hai:** `haisay` calls the op, injects the context summary into the API request (via the existing open-hai client, forked op, no linking); replace the canned reply; propose-then-apply for book edits (events); log every request/response row with a cap.
5. **Wire top-toolbar h-ai:** D pointer file + same verification; test with two editors open (one decoy).
6. **Mouse (later):** click targets for the sidebars (palette rows, tiles, prev/next, chat input); relay `MOUSE_EVENT` first, real pointer after.
7. **Standards pass:** every new file capped + registered; new processes through `kh_spawn.sh`; `cpu_lint` / `spawn_lint` stay at `new=0`; comments mirrored in docs; footprint line in every commit (idle + active, measured with a browser running).
8. **Leftovers from the CPU night:** drive df / doom / ohrrpgce / my-palettes tile buttons in the window; rebuild other petslike boards (gated, one at a time) for the registered ctx-menu launch; palettes list opens scrolled; engine GPU phase (31 ms) investigation; optional tmpfs display folder.

## 4. Test guide for this sprint (evidence for each)
- Layout: `cat board/state/sides.txt state/flag_chatmode.txt` after `pal_side restore`; screenshot of the real window; edit `layout.pdl`, restore, expect the change (done on files; screenshot pending).
- Session locate: run `hai_session` with the right pid -> context; with a dead pid -> refusal; with a pid of another app -> refusal; two editors -> the one asked for. Negative-test every assertion once.
- h-ai round trip: ask "which room am I in and what is armed?" in the right bar; the reply must quote the values in `hai_context.txt`; change the armed brush, ask again, the answer must change.
- Footprint: `cpu_gate check` before every drive; sample the house for 20 s idle and during a chat call; a call must not leave processes (list survivors after closing the window).
- Safety: API key never appears in `hai_chat.txt`, `hai_context.txt`, logs or commits (grep for it); free models only; never loop to a rate limit.

## 5. Risks
Two editors at once (ambiguous locate); pid reuse; a context file that lies when the daemon dies (use the registry alive check + `updated` marker age); a chat that blocks the window while the API call runs (must be a forked, niced, registered, time-limited op).
