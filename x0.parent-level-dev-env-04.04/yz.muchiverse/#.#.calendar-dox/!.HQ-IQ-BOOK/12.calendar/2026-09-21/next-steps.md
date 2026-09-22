# 2026-09-21 — what's next (agreed direction) + hardware checklist

State: `claude`, `main` and `origin/opencode` all at `860da9a5` (pushed 2026-09-21). Previous day's log and the long todo list: `../2026-09-20/2do.md`.

## Ordered plan

### 1. Hardware pass (user, ~5 min) + two old bugs that may now be closed
Built but **not yet confirmed on the real machine** (all were tested only in private Xephyr/relay, which bypass X grabs and Wayland focus - see `HOUSE_CODE_PITFALLS.md` #24). Restart the pals (Cursword first) and the taskbar/renderers before testing so the new binaries are running.

- [ ] **Pal icons in the Linux dock:** turn the taskbar `@` always-on-top **OFF**, look at the left dock - do pals show their own sprite? Does the dock group all pals under one entry? (`khtpm_entity.c` `set_net_wm_icon()`, class left as `MuchiverseLivedesk` on purpose.)
- [ ] **Space = context menu:** focus an item in File Explorer (digit/arrows), press Space -> right-click menu opens; on the dock, focus a pal cell, Space = Enter.
- [ ] **File Explorer Search:** click the `Search:` row above Back, type `app` - list filters; empty clears.
- [ ] **Labelled Place grid:** right-click a pal in an Inventory -> Place: labels A-Z / 1-N on the grid; type `c7`, Enter (green frame jumps), Enter again places; arrows move; Esc cancels; mouse click still places. Placed spot == where the labelled cell is (snap now uses the real 80-px cell, `c1ebf1b2`).
- [ ] **Inventory drop:** drag a desk pal over an open Inventory - green frame + `[ drop into inventory: name ]`; release moves it in.
- [ ] **UI scaling on the second computer** (taskbar not cut off, entities normal size); Windows entity twin still absolute px.
- [ ] **Re-test old bugs (likely same stuck-keyboard-grab class, fixed `2c1301ab`):**
  - text-edit-hq typing - `04-bugs/bug_bounty.md` OPEN 2026-09-14 entry (close it if it types).
  - network-browser address bar losing focus/backspace - `04-bugs/BUG-LOG.md` (recurring 2026-09-10/11) and its bug_bounty note.

If anything fails, note which line above and what happened; `kh_focus_debug.log` in the window's app dir now records `x_focus` + `_NET_ACTIVE_WINDOW` on every click.

### 2. Events from inside Inventory (user priority b)
Already true: right-clicking a pal in an Inventory shows that entity's own METHOD rows (`a1053ccb`). Missing bridge: robot / puzzle-piece entities whose **event pages become method rows**, run with the host entity in context (`$0`). Plan: one vertical slice - one robot entity, one event (e.g. a tax), dropped into Cursword's inventory, run from its right-click. Step 1 = short research of how a pal's `event_pkg` is authored/run (`event_pkg/pages/page_1/*`, `call_event_op`, events-hq / db-hq Common Events, `event_commands.registry.pdl`) and how a `meta.pdl` METHOD row could point at an event page; then the edits (small, done directly).

### 3. First IRL/AI slice
The event registry has **zero `ai_*` commands** today. First slice = `ai_describe` primitive (gemma DESCRIBE only - never classify) + a relay watcher that writes a description to a file and a Synonym Bank. Needs a small gameplay loop to watch, so it follows step 2 and depends on the **kilo unpause decision** (WSR-CIV/DSR still paused - `13.agent-coms/KILO/claude-2-kilo-9.17.md`, `LLMUD-HACK.md`, `DUSTOPIA-HACK.md`).

### 4. piececraft-hq ".main tab only" board bug
Open in `04-bugs/BUG-LOG.md`; blocks every game desk (WSR-CIV Step B). Diagnose next.

## Deferred / documented (do not start without a reason)
- Dock unfactor stages 3-5 - `DOCK-UNFACTOR-AUDIT.md` §5c (unified nav is a hard requirement; revisit when a second window needs a pager / shrink-to-fit / several X windows per process).
- In-memory DB - `INMEM-DB-STATE-LAYER-PLAN.md`: optional streaming/video demo only; files stay default; text includes are transitional (P0 freeze list there).
- Windows entity twin `tp_desktop_window_win.c` (absolute px); emoji-brush placer `tp_arm_placer.c`; Cli-io on all entity context menus (experimental); `db-hq-pal` toy only syntax-checked; `nav.sh` `row`/`type` not exercised on live rows.
- **Cancelled:** `.xhtpm` -> `.xhtm` rename (never).

## Working agreements (from this session)
- Hardware bugs: I gather evidence myself first (grab probe, passive KeyPress listener, logs); at most one small ask of the user at a time.
- Token cost: forks re-read this very long conversation on every tool call (400-740k tokens each). Small edits are done directly; independent work goes to fresh tight-prompt agents; big tasks get a scope budget; consider a fresh session + handoff for large jobs.
- Comments stay in code (fresh, value-adding, pointing to docs, mirrored in docs); LOC excludes comments.
