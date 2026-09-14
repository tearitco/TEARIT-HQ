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

## 0. Orientation (2026-09-14) - what "trigger" means, what "a game" means here, and what already works

Written after a direct request to be walked through this from
scratch, catching up on what's done/missing and why. Grounded in real
code search (`tp_main()`'s click handling, `dashboard.chtpm`'s Play
button, `khtpm_events_hq_manager.c`'s dispatch, `play_event.sh`,
piececraft-hq's `events.pdl`) - not guessed.

**The surprising part: nothing automatic fires a Common Event today.**
The only real way one runs is a human opening the events-hq editor and
pressing the **"▶ Play"** button (`dashboard.chtpm`'s
`<button id="play-test">`). That calls `khtpm_events_hq_manager.c`'s
`play` action, which shells out to `&.widgits/events-hq/ops/
play_event.sh`, which scans `event_pkg/pages/page_*`, picks whichever
page's `condition.pdl` trigger label matches the (default `"on-click"`)
filter argument, and runs its compiled `event.pal`.

The `trigger=on-click` / `trigger=Autorun` / `trigger=player-touch`
labels seen in event data are **not live listeners** - confirmed no
code path anywhere compares them against a real mouse click, a page
load, or player movement. They're inert filter tags `play_event.sh`
reads only when a human has already pressed Play. Real footnote:
`common_events/greet_player/`'s own trigger is actually `Autorun`, but
`play_event.sh`'s default filter is `"on-click"` - pressing plain Play
on it today may not even select the matching page. Small, separate,
real gap, not currently blocking anything.

**The trigger layer (NIGHT_05) is building the FIRST real, automatic
listener** - something that watches for a genuine in-game action and
fires the matching Common Event on its own, no human touching the
editor.

**"A game" here means two different real things, not one:**
1. **The desktop itself** (pals - cursword, asa, ava, book-stack) -
   desktop companions, not a game board. They can have Common Events
   attached, but there's no "walking into" one; only ever fired via
   manual Play. Confirmed: `tp_main()`'s real ButtonPress handling has
   no code that looks up an entity's `event_pkg` or fires anything on
   click - a scripting sandbox, not a game.
2. **piececraft-hq** - a genuinely separate engine (its own renderer,
   `chtpm_parser_pal`/`prisc+x`, unrelated to the khtpm desktop family)
   with a real tile board, x/y coordinates, NPC-like glyphs. Real event
   data already exists there (`@.apps/piececraft-hq/pieces/system/maps/
   cdda_sample/events.pdl`: `EVENT | x=6 y=5 glyph=t |
   trigger=player-touch cmds=change_hp,change_state`) - but zero
   movement code anywhere reads it during play. Data exists, nothing
   acts on it. **This is the real target of the trigger layer.**

**How a human tests it, once built**: open piececraft-hq, move the
player piece onto the tile at `x=6, y=5` (or wherever a test NPC gets
registered), and watch something fire on its own - a real Show Text
popup, a real gold change - without ever touching the events-hq editor
or pressing Play. That's the whole proof: walk into it, something
happens, no manual step.

**What already works, and how it's unrelated to "trigger"**: Change
Gold / Show Text (seen working live) are the real, current **effect**
side - what an event DOES once running (`mr_change_gold.+x`/
`mr_show_text.+x`, both proven). Trigger is a separate concern
entirely - the **cause** side, why/when an event starts. Today the only
cause that exists is "a human pressed Play." The trigger layer doesn't
touch Change Gold/Show Text at all - it only gives events a SECOND, real
way to start (automatic), alongside the manual Play button, which
stays for editing/testing.

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

### 2c. CLOSED (2026-09-14) - root-caused, not one bug but five

Root-caused by hand, one injection at a time, not guessed. What
looked like a single "view_mode never flips" symptom was actually
**five separate, real, independent drift bugs** stacked on top of each
other - fixed in commit `3e621a4d`:

1. **Dead relay target**: this whole harness family wrote to a fixed
   `events_hq_history.txt` - confirmed via direct code search, zero
   references anywhere in `khtpm_core_render.c`. Every window now
   polls a generic per-process relay (`history_path()` ->
   `#.desktop/entity_menu_history/<pid>.txt`). Nothing the harness ever
   injected was reaching the window, at any point.
2. **The feature itself doesn't exist**: `g_evhq_view_mode`/
   `evhq_layout_pass()`/`events_hq_view_mode.txt` (all three named in
   `dashboard.chtpm`'s own header comment as implementing the
   Scripting/Scratch/Blueprints view-tab switch) - zero matches
   anywhere in the current renderer. Never carried over when events-hq
   got merged into this shared binary. Not drift - dead code. The
   harness's only real assertion (`view_mode == 1`) was unprovable by
   construction.
3. **Global nav numbering**: this house's own already-documented rule
   (`_.0.aigent-testing-k9.txt`'s "Rule 7") - nav numbers are shared
   across every open khtpm window, never reset to 1 per window. A
   blind digit-jump in a harness that always runs alongside the real
   live desktop could land anywhere, including a text field, arming it
   and swallowing the later PNG-dump key as literal text.
4. **Wrong PNG path**: `dump_frame_png()`'s real, current output is
   the generic `/tmp/entity-menu-frame.png` (shared by every mode) -
   never a per-mode `/tmp/events-hq-frame.png`. The dump mechanism was
   never broken; the harness was checking a file it never writes.
5. **Stale VM binary**: the deployed `prisc+x` binary itself predated
   its own source - `strings` on the binary showed zero matches for
   `sgetenv`/`slit`/`sappend`/`sfopen`/etc, all real opcodes already in
   `prisc+x.c` since 2026-09-03. Rebuilt via `101.mutaclsym`'s own
   `scripts/build.sh`.

**Also hardened, not just fixed**, per direct instruction ("is
retarget enough? can we harden it"):
- House root and target PID are now looked up dynamically at runtime
  (`sgetenv "KHTPM_HOUSE"`/`"KHTPM_TARGET_PID"`, prisc+x's own existing
  string-opcode family - no VM language change needed) instead of a
  literal baked into the `.pal` - survives any future house move.
- Dropped the digit-jump+Enter step outright (a real assumption
  removed, not routed around) since it depended on a feature that no
  longer exists and a nav-number that can't be predicted.
- Replaced the PNG size-threshold guess (`>1000 bytes`, wrong for this
  harness's deliberately-empty disposable entity) with a real PNG
  magic-byte signature check - a content-independent, hard assertion.

**Verified**: full harness run now genuinely PASSes all three
assertions (real PNG produced + signature verified, verdict
`done=1`/`pass=1`, zero stray processes after cleanup) in ~7 seconds,
zero disruption to the live desktop.

---

## 3. Real files touched

- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` - `g_arg4_entity_label` capture + wiring (commit `ac10a579`)
- `xyzfs/users/.../pals/cursword/harnesses/run_visible_window_events_hq_demo.sh` - safe stray-kill scoping (`ac10a579`), real relay path + PNG signature check (`3e621a4d`)
- `xyzfs/users/.../pals/cursword/harnesses/pal/visible_window_events_hq_demo.pal` - dynamic path lookup, dead-feature/dead-relay/wrong-nav/wrong-PNG-path fixes (`3e621a4d`)
- `101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x` - rebuilt from current source (`3e621a4d`)

Unrelated, same-day fixes also landed this session (taskbar
performance/reliability work, not events-hq) - see `bug_bounty.md` and
`03-pitfalls/HOUSE_CODE_PITFALLS.md` #19 for those; not repeated here
since this doc is scoped to the events-trigger build-out track.

---

*§2's legacy-events-hq state check is now fully closed. Next real step
on this track: promote NIGHT_05's trigger-layer proposal into
`08-roadmap/design-docs/EVENT-TRIGGER-LAYER-PLAN.md` and start
building - not yet started.*
