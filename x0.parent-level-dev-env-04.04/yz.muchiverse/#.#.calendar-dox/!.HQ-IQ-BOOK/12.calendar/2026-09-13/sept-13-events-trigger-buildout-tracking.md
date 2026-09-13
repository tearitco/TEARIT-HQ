# 🗓️ Sept 13 — Events-Trigger Build-Out: state check + tracking

**Purpose of this doc**: before starting the NIGHT_05 "trigger layer"
build-out (world event → dispatch → Common Event), verify the legacy
events-hq era still actually works after this week's refactors
(registry consolidation, PID-identity fixes, khtpm_core_render merge,
incremental reparse). Direct instruction: "before that i may want u 2
make sure the old legacy events (events-hq era) still work... i want
to know current state. then we will keep a document... tracking the
state and progress of this task."

Living doc - update as the build-out progresses, don't create a new
file per session unless this one gets unwieldy.

---

## 1. What "events trigger build-out" actually refers to

No file is literally titled "events trigger build-out." Two real
things exist:

- **`08-roadmap/design-docs/EVENTS-PAL-BUILDOUT-PLAN.md`** - the
  standing plan for the RPG-Maker-style event backend itself
  (event-ez → event.ir.pdl → event.pal → cmd_N.sh → generic `mr_
  <command>` C binaries), data-driven per `EVENT-COMMAND-REGISTRY-
  ARCHITECTURE.md`'s standing rule (no hardcoded C enums for command
  types).
- **`1-1.HARNECIENT.SMOL/NIGHT_05_THE_TRIGGER_LAYER.txt`** - a
  narrative-form design proposal (not yet promoted to a roadmap doc)
  for the actual missing piece: a real in-game *trigger* (e.g.
  `touched_npc:NAME` appended to `pieces/display/board_events.txt`) →
  a watcher → looks up a registered Common Event for that trigger
  string → fires it through events-hq's existing dispatcher. The
  effect side (Common Events themselves, db-hq read/write) already
  works; only the trigger side doesn't exist yet.

**Next step once this doc's Part 2 below is closed out**: promote
NIGHT_05's proposal into a real `EVENT-TRIGGER-LAYER-PLAN.md` in
`08-roadmap/design-docs/`, then start building.

---

## 2. Legacy events-hq state check (today's work, in order found)

### 2a. What's current vs. stale, before touching anything

Checked via real evidence (process list, `master_ledger.txt` logs,
binary mtimes) - no guessing:

| Piece | State | Evidence |
|---|---|---|
| `mr_show_text.+x` / `mr_change_gold.+x` (the real "Show Text" popup / "add gold" commands) | **Current** | Binaries rebuilt Sep 8, actively invoked by `greet_player`'s compiled `cmd_N.sh` |
| `common_events/greet_player/` | **Current** | `event.ir.pdl`/`event.pal` regenerated Sep 3; manager running live at time of check |
| `common_events/test_target/` | **Stale-ish** | Unchanged since Aug 27; own ledger shows "no event.pal for common event test_target" (page_2 incomplete) as of Sep 3 |
| book-stack's Bible-verse "Show Text" popup | **NOT events-hq** | No `event_pkg` exists anywhere under book-stack's pal dir - separate, bespoke mechanism, unrelated to Common Events |
| cursword's own `event_pkg` | Exists (page_1/page_2), no dedicated harness | The harnesses in `cursword/harnesses/` test the events-hq *system* generally, using cursword as the clicking actor - none test cursword's own page content specifically |
| Both `greet_player` and `test_target` ledgers | No successful "Play" trigger logged since **Sep 3** (10 days idle) despite managers running the whole time | `master_ledger.txt` tail, both dirs |

### 2b. Ran the real harness - found it broken, but usefully so

`run_visible_window_events_hq_demo.sh` (cursword's own disposable-
entity, end-to-end proof harness: launches a real events-hq window on
a throwaway `/tmp` package, drives it via the real `prisc+x` VM, reads
back a real PNG). Two real bugs found, both fixed and pushed
(`ac10a579`):

**Bug 1 - the harness itself was unsafe to run** (`run_visible_
window_events_hq_demo.sh`): its "kill stray render processes before/
after" steps matched `khtpm_core_render\.+x` by binary name alone -
against this house's *normal* state (a real, long-running desktop:
strip + every pal), that would have killed the entire live desktop as
"stray," not just the harness's own disposable window. **Fixed**:
scoped the kill pattern to the harness's own unique, disposable entity
name (`visproof-disposable`) - safe now, verified by running it twice
against a live desktop with zero disruption to the real strip/entities.

**Bug 2 - a real, CURRENT regression** (`khtpm_core_render.c`):
events-hq's real launch shape is `<house> <chtpm> <pkg_dir>
<entity_label>` - `khtpm_events_hq_manager.c`'s own `main()` hard-exits
with a usage error below `argc<4`. The generic `<module>` self-launch
consolidation (`launch_module()`/`kh_launch_window_modules()`, shared
with db-hq/chat-hai - neither of which need a 3rd manager argv) never
carried these two values through: it passed `g_package_dir` (dirname
of the `.chtpm` itself - the wrong directory) as the manager's
package_dir, and never passed an entity_label at all. Confirmed live:
a fresh events-hq launch's self-spawned manager exited immediately
with `"usage: <house_root> <pkg_dir> <entity_label>"` every time - the
long-lived `asa`/`ava`/`greet_player` managers already running predate
whatever refactor broke this, which is why nobody had noticed.
**Fixed**: captured `argv[4]` into a new `g_arg4_entity_label` global
alongside the existing `g_arg3_dir` capture, used both at both real
`kh_launch_window_modules()` call sites.

**Bug 3 - a genuinely STALE artifact**
(`visible_window_events_hq_demo.pal`): four literal, hardcoded
house-root path occurrences still pointed at the OLD pre-migration
tree (`NNEST-11.17`) - a real PAL-level limitation (no path
interpolation exists in this VM), never updated when the house moved
to `NNEST-12.00`. **Fixed**: repointed all four to the current house
root.

### 2c. Still open - NOT yet fixed

Even with all three fixes above, the harness's own PAL drive still
times out (60s) on the same step every time:

- The render **does** create `events_hq_view_mode.txt` (proof the
  mechanism partially works - this used to not exist at all before
  fix #2).
- The injected relay events (digit-jump `50` + Enter `13`) **do**
  reach the real relay file (`events_hq_history.txt`) - confirmed by
  reading it back after a run.
- But `view_mode` never flips from `0` to `1` - the view-tab switch the
  digit-jump is supposed to trigger doesn't appear to be landing.

**Not yet root-caused.** Candidate causes, unconfirmed:
- A relay-dispatch change since this harness was last proven (some
  other refactor this week touched `dispatch_relay_code()`-adjacent
  code for nav/scope state - see `bug_bounty.md`'s stuck-nav entries).
- The events-hq dashboard's own nav-tab count/order may have shifted
  (digit `2` may no longer land on the same tab it used to).
- Possibly unrelated to any of this week's work - could predate it.

**Next step**: drive the same disposable window manually (relay file,
one keypress at a time) and dump a real PNG after each step to see
exactly where the sequence diverges from what the PAL expects, instead
of guessing further. Not started yet.

---

## 3. Real files touched today

- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` - `g_arg4_entity_label` capture + wiring (commit `ac10a579`)
- `xyzfs/users/.../pals/cursword/harnesses/run_visible_window_events_hq_demo.sh` - safe stray-kill scoping (commit `ac10a579`)
- `xyzfs/users/.../pals/cursword/harnesses/pal/visible_window_events_hq_demo.pal` - house-root path fix (commit `ac10a579`)

Unrelated, same-day fixes also landed this session (taskbar
performance/reliability work, not events-hq) - see `bug_bounty.md` and
`03-pitfalls/HOUSE_CODE_PITFALLS.md` #19 for those; not repeated here
since this doc is scoped to the events-trigger build-out track.

---

*Update this doc as the open item in §2c gets root-caused, and again
once NIGHT_05's trigger-layer proposal is promoted to a real roadmap
doc and build-out actually starts.*
