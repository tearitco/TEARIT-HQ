# Events Runtime — Investigation, Bug Fix, and Architecture (2026-08-11/12)

**Status:** ✅ Change Gold verified working end-to-end via real relay injection. ✅ Ops migration
(`*.monads/*.muchi-pet/ops/` → `xyzfs/bin/muchi-pet/ops/`) COMPLETE and verified. ✅ Multi-page/
multi-trigger runtime COMPLETE and verified. ✅ Session-level common events COMPLETE and verified —
zero new runtime code needed, pure reuse. Four real bugs found and fixed total.
**Owner:** claude-0001

---

## ✅ UPDATE 2026-08-12 (later) — Multi-Page/Multi-Trigger Runtime + Common Events

**The real blocker flagged in `EVENT_AI_VISION.md` §0 is now fixed.** `play_event.sh` used to run
`event_pkg/pages/page_1/event.pal` unconditionally, no matter how many pages an entity had or what
fired it. Now: `play_event.sh <package_dir> [house_root] [trigger]` (trigger optional, defaults to
`on-click` — every existing caller, including `dispatch_action()`'s 2-arg convention, is unaffected).
It scans `event_pkg/pages/page_*` in numeric order, and runs the **highest-numbered page whose
`condition.pdl` trigger exactly matches** — the same "highest-numbered matching page wins" semantics
RPG Maker MV itself uses for multi-page events (see `EVENT_AI_VISION.md` §1's design implication).
Does not yet support switches/variables as page conditions, only the bare trigger name — a real,
separate, not-yet-built extension.

**Verified (relay + direct, both):** gave `m8_redhorned` a second test page (`page_2`, trigger
`on-spawn`, Change Gold +100) alongside the existing `page_1` (trigger `on-click`, +35). Confirmed:
- Default/`on-click` trigger → only `page_1` runs (`qolq` → 35, page_2 untouched)
- `on-spawn` trigger → only `page_2` runs (`qolq` → 100, page_1 untouched)
- An unmatched trigger (`on-touch`) fails cleanly with no side effects
- The real production path (`RUN_METHOD:Play` relay, defaulting to `on-click`) still correctly
  fires only `page_1` — full backward compatibility confirmed, not just the direct-script path
Test page_2 was scaffolding-only, removed after verification; `m8_redhorned`'s real content is
unchanged (still the single Change Gold page from earlier).

### Real Bug #4 — long-running entity process needs periodic restart during heavy testing

Found live, mid-verification: after several rapid relay-triggered `Play` actions in quick succession,
a further `RUN_METHOD:Play` injection was consumed (visible in `history.txt`) but produced NO effect
— `inventory.txt` unchanged, no new `master_ledger.txt` line. Reconstructing `dispatch_action()`'s
exact command by hand (including the real `>/dev/null 2>&1 &` backgrounding) worked correctly every
time, proving the script itself was never the problem. Killing and respawning just that one
`tp_desktop_window.+x` process fixed it immediately — same class of fix as the earlier stale-
`objects.pdl`-cache bug, but a DIFFERENT root cause this time (the process had been running many
`system()` calls in a short window; the script content itself was never cached, so this isn't the
same mechanism, likely zombie/fd accumulation in the long-running process — not root-caused at the
C level, logged as a reproducible symptom+fix, not a full diagnosis).

**Standing lesson for testing:** if a relay-triggered action is confirmed consumed
(`history.txt` shows the `INJECTED:` line) but produces no visible effect, and the underlying script
is proven correct via direct/manual reconstruction of the exact same command — suspect the entity
process itself needs a restart before further debugging the script. Don't conclude the code is wrong
from this symptom alone.

### Session-Level Common Events — Working, Zero New Runtime Code

Direct instruction: start on common events (session-scoped events, distinct from per-entity events)
now that per-entity multi-page/trigger support works. **Real finding: no new runtime code was
needed at all.** `mr_change_gold.+x <package_dir> <delta>` is fully generic — it only ever touches
`<package_dir>/inventory.txt`, with zero awareness of whether `package_dir` is an entity or something
else. This means a "common event" is simply an event package (`event_pkg/pages/page_N/`, exact same
shape as an entity's) rooted at the SESSION level instead of inside an entity:

```
sessions/<session_id>/common_events/
├── inventory.txt              (session-wide state — e.g. shared/party gold, distinct from any one
│                                entity's own inventory.txt)
├── master_ledger.txt
└── event_pkg/pages/page_1/
    ├── condition.pdl          (trigger, same format as entity events)
    ├── event.ir.pdl
    ├── event.pal
    └── cmd_N.sh
```

**Verified via `play_event.sh` directly** (real relay/UI trigger point not yet wired — see below):
created `sessions/s4/common_events/event_pkg/pages/page_1/` with a Change Gold +50 node, ran
`play_event.sh` against the session's `common_events` dir — `qolq` went `0 → 50`, logged correctly
to `sessions/s4/common_events/master_ledger.txt`. Verified session-scoping specifically: switched
desk away and back (same relay sequence as the entity persistence test) — `common_events/
inventory.txt` was completely unaffected by the desk switch, confirming it's genuinely tied to the
SESSION, not any particular desk (unlike entity state, which lives inside a desk-specific pal copy).

**Not yet done (real, open items):**
- **No UI trigger point wired yet.** Verified via direct `play_event.sh` invocation only — there's no
  right-click menu, taskbar cell, or other real UI action that fires a session's common events yet.
  The `db` header cell (currently inert) or a new per-session menu are candidates; not decided.
- **Autorun/Parallel triggers untested.** Only `on-click` (via direct script invocation, mimicking
  what a future manual "run common events" action would do) has been verified for common events
  specifically. RPG Maker's real Autorun/Parallel semantics (fires automatically, no explicit user
  action) haven't been wired to anything that would call `play_event.sh` without a trigger — e.g. on
  session load, or on some background tick. That's a real, separate piece of work.

---

## ✅ UPDATE 2026-08-12 (later still) — "db-ez" IS event-ez, Verified, Zero New Code

The "no authoring UI for common events" gap noted just above is CLOSED — and closed by discovering
there was never a real gap, only an untested assumption. Checked `event-ez`'s actual C source
(`ez_menu_input.c`, `ez_compose_frame.c`) directly: it has **zero entity-specific file dependencies**
— no code anywhere reads `meta.pdl`/`objects.pdl`/anything assuming its `pkg_dir` is inside an
entity. `EZ_PKG_DIR` is documented in `event-ez/button.sh`'s own header as simply "real event_pkg dir
Save writes into" — fully generic.

**Verified live:** launched `event-ez` with `EZ_PKG_DIR` pointed directly at
`sessions/s4/common_events/event_pkg` (a session-level package, not an entity) —
```bash
EZ_PKG_NAME=common_events EZ_PKG_DIR="<session>/common_events/event_pkg" sh button.sh r
```
— and it opened correctly, title bar reading "EVENT EDITOR — common_events", Event Pages screen
correctly showing the real `Page 1 - When: on-click (1 cmd)` that already existed in that package.
Injected real keypresses (the same k3 method event-ez's own `HOW2_event-ez_change_gold_k3.txt` guide
documents — `pieces/keyboard/history.txt`, `KEY_PRESSED: <code>` lines) to navigate into Page 1: the
real frame showed the actual `Change Gold: 50` command line, matching what was manually authored
earlier this session. **Full round-trip GUI usability for common events, using the existing tool,
completely unmodified.**

**What this means for the architecture doc's original "db-ez vs db-mock" plan** (see `2do-au11.txt`'s
Task 2 section): **db-ez is not a new build — it IS event-ez, pointed at a session package instead of
an entity package.** The only real remaining "db" work is a UI TRIGGER/ENTRY POINT (a menu row
somewhere that launches event-ez with `EZ_PKG_DIR` set to the current session's `common_events/
event_pkg`, mirroring exactly how each entity's own "Events (ez)" row already launches it for that
entity — see `open_event_ez.sh`) — not new authoring capability.

**Test harness built and verified, checked into this house permanently** (not a throwaway script):
`xyzfs/users/04c8ce55-11a5-47f3-933d-ac009ca4ac72/harnesses/test_events_e2e.sh` (see its own
`README.md` in the same directory). Covers all four capabilities proven across this doc — entity
event execution, multi-trigger dispatch, common events, and the common-events GUI — in ~10 seconds,
relay-only, no direct CLI shortcuts. All 4 sub-tests (7 individual assertions) passing as of this
writing.

---

## ✅ UPDATE 2026-08-12 (earlier) — Ops Migration Complete

Direct instruction: "migration makes sense now" — moved `*.monads/*.muchi-pet/ops/` (the whole
folder: `.c` sources, compiled `+x/` binaries, and helper shell scripts) to
`xyzfs/bin/muchi-pet/ops/`, matching house convention (`<app>/ops/+x/`) and the "ops are shared,
house-wide, even cross-user" direction from the Ops-vs-Events table below. `*.monads/*.muchi-pet/`
itself still holds `entities/` (the template/demo monster definitions) — only `ops/` moved; the
entity-template migration is a separate, not-yet-done task.

**Two additional real bugs found DURING the migration** (beyond the path-depth bug from the original
investigation below) — both are the same underlying class of mistake (fixed-depth relative-path
assumptions baked in at a specific moment, broken by ANY later relocation), found by actually testing
after the move rather than assuming the earlier fix pattern was universally applied:

1. **`play_event.sh`'s own `HOUSE_ROOT` derivation** used a fixed `"../.."` walk from its own script
   location to reach house_root (needed to find `101.mutaclsym*/system/prisc+x`). Correct when the
   script lived at `*.monads/*.muchi-pet/ops/` (2 levels under house_root) — broke silently once
   moved to `xyzfs/bin/muchi-pet/ops/` (3 levels under house_root, one deeper). Fixed the same way as
   the `cmd_N.sh` wrappers: search upward for a stable anchor (`101.mutaclsym*/system`) instead of
   assuming a fixed depth. Verified via direct script execution AND full relay re-test.
2. **Stale in-memory cache on the ALREADY-RUNNING entity process.** After fixing every file on disk,
   a full relay-triggered `RUN_METHOD:Play` test STILL silently did nothing — the live
   `tp_desktop_window.+x` process for m8_redhorned had been running since before any of tonight's
   edits and had loaded the OLD `objects.pdl` action string into its in-memory `methods[]` array at
   window-open time; it never re-reads `objects.pdl` from disk on its own. Killing and respawning
   that one entity process picked up the corrected file and the relay test then passed cleanly.
   **Lesson for future path/config migrations affecting live entities:** editing the files on disk is
   necessary but not sufficient — any ALREADY-RUNNING entity process needs to be restarted (or the
   window closed/reopened) to pick up the change. Don't conclude a fix failed without checking this.

**`open_event_ez.sh` needed no fix** — it already receives `house_root` as an explicit second
argument from `dispatch_action()` (a fix from 2026-08-10, for the exact same class of bug), rather
than deriving it from its own script location. This is the STRICTLY BETTER pattern (explicit argument
beats self-location inference) — `play_event.sh` and the `cmd_N.sh` wrapper generator could be
upgraded to the same explicit-argument pattern as a future improvement, since `dispatch_action()`
already passes `house_root` to every action regardless of whether the action's own script reads it.
Not done tonight since the anchor-search fix is already verified working; logged as a nice-to-have.

**Final verification (full chain, pure relay, after both fixes + process restart):**
```
echo "qolq=0" > inventory.txt
echo "RUN_METHOD:Play" > interact_relay.txt   # → qolq=35 (real, confirmed via master_ledger.txt)
echo "RUN_METHOD:Menu" > interact_relay.txt   # → objects.pdl self-regenerated with correct new
                                                #   absolute paths (open_rp_menu.sh's own $SELF
                                                #   resolution), confirming the self-healing
                                                #   pattern still works post-migration
```

---

---

## TL;DR

- Events (event-ez authored content, e.g. Change Gold) DO have a real runtime — reachable via the
  entity's right-click "Play" menu action (`objects.pdl`'s `PAGE|main` → `OBJECT|label=Play`),
  dispatched by `tp_desktop_window.c` exactly like a real click, and remotely triggerable via
  `interact_relay.txt` (`RUN_METHOD:Play`). This was NOT obvious — there is no separate "run all
  on-click events automatically" mechanism; the trigger is an explicit menu action.
- Found and fixed a REAL bug: the compiled `cmd_N.sh` wrapper scripts (event-ez's own compiler
  output) hardcoded a fixed directory-depth assumption to locate `mr_change_gold.+x`, which broke
  the moment an entity got deployed to `xyzfs/users/<uuid>/home/livedesk/pals/<name>/` (a different
  depth than the original `@.apps/`/`*.monads/*.muchi-pet/` template layout). Fixed with an
  anchor-search pattern instead of a fixed-depth walk.
- Verified end-to-end via pure relay injection: reset gold to 0, triggered Play through
  `interact_relay.txt`, confirmed `qolq` went `0 → 35` (real execution, not a fixture).
- Verified persistence: switched desk away (entity process confirmed killed) and back — `qolq=35`
  survived the round-trip.
- Two architecture points clarified by direct instruction, not yet built:
  1. **Ops (code) are shared, house-wide, and even cross-user** — this is the model for how apps
     and user-apps will work going forward. `mr_change_gold.+x` should eventually live in a shared
     location like `xyzfs/bin/` (already seeded as an empty dir by `userpal_create_account.c`), not
     the current per-game `*.monads/*.muchi-pet/ops/+x/` dev folder.
  2. **Events (authored content — event.ir.pdl, common_events.pdl) are session-private by default**,
     only becoming shared/cross-user when the owning user explicitly publishes them to the **store**
     (the `store` header cell, currently an inert placeholder — likely its real intended purpose).
     Other users can then browse/download/install published events from the store.

---

## How the Runtime Actually Works (traced live, not assumed)

### The trigger mechanism
Each event page has a `condition.pdl` recording its trigger (e.g. `trigger=on-click` for
m8_redhorned's page_1). **This trigger field is currently informational only** — confirmed by
grepping the whole codebase: `condition.pdl` is read ONLY by event-ez's own authoring tools
(`ez_menu_input.c`, `ez_compose_frame.c`), never by `tp_desktop_window.c` (the live entity process).
There is no automatic "fires when clicked" behavior yet, despite the trigger being named `on-click`.

### What actually fires it today
Each entity has an `objects.pdl` (multi-page right-click menu, read live by `tp_desktop_window.c`).
For m8_redhorned, `PAGE|main` includes:
```
OBJECT | label=Play | action=*.monads/*.muchi-pet/ops/play_event.sh
```
Right-clicking the entity and selecting "Play" (or injecting `RUN_METHOD:Play` into the entity's own
`interact_relay.txt`, which `tp_desktop_window.c` polls and dispatches identically to a real click)
runs `play_event.sh <package_dir>`, which:
1. Execs `event_pkg/pages/page_1/event.pal` via `prisc+x`
2. Appends a timestamped line to `master_ledger.txt`
3. Syncs `gold.txt` from `inventory.txt`'s `qolq=` value

`event.pal` itself is compiler output (`exec cmd_1.sh`, `exec cmd_2.sh`, `halt` — one `cmd_N.sh` per
NODE in `event.ir.pdl`), because `prisc+x`'s real `exec` opcode only supports one literal argument;
multi-arg calls need a wrapper script, which is what `cmd_N.sh` is.

### Multi-page / multi-trigger note
`play_event.sh` is hardcoded to `page_1` only (see its own header comment: "run event_pkg/pages/
page_1/event.pal"). If/when multi-page events with different triggers matter (e.g. `on_spawn` vs.
`on_click` running different pages), this script needs to become trigger-aware, reading each page's
`condition.pdl` to decide which page(s) to run for a given real trigger. Not needed for the
single-page Change Gold case tested here; will matter for the demo games.

---

## Real Bug Found + Fixed: cmd_N.sh Wrapper Path Resolution

### Symptom
Triggered "Play" via real relay injection (`RUN_METHOD:Play` in `interact_relay.txt`) against
`m8_redhorned`'s live copy at `xyzfs/users/<uuid>/home/livedesk/pals/m8_redhorned/`. `event.pal`
genuinely executed (confirmed via `master_ledger.txt`'s real timestamp and `history.txt`'s
`INJECTED: RUN_METHOD:Play` line) — but `inventory.txt`'s `qolq` value did not change.

### Root cause
`cmd_N.sh` (generated by `&.widgits/event-ez/ops/ez_menu_input.c`'s Change-Gold-save handler) contained:
```sh
cd "$(dirname "$0")/../../.." || exit 1
exec ../../ops/+x/mr_change_gold.+x "$PWD" '10'
```
The first `cd` (3 levels up from `event_pkg/pages/page_1/`) correctly reaches the entity's own root
directory — that part was always right, and matches where `inventory.txt` really lives. The SECOND
relative hop (`../../ops/+x/`) assumed the entity's grandparent directory has an `ops/+x/` sibling —
true for the original `@.apps/MUCHI_RANCHER/entities/<name>/` and `*.monads/*.muchi-pet/entities/
<name>/` template layouts (grandparent = `MUCHI_RANCHER`/`*.muchi-pet`, which does have that
sibling), but FALSE the moment the entity is deployed to `xyzfs/users/<uuid>/home/livedesk/pals/
<name>/` (grandparent = `home/`, no `ops/` there at all) — exactly the layout this house's own
sessions/xyzfs migration produces. Running the wrapper directly showed the real failure:
```
.../pals/m8_redhorned/../../ops/+x/mr_change_gold.+x: No such file or directory
```

### Fix
Changed the wrapper generation in `ez_menu_input.c` (and manually re-applied the same fix to the
already-broken `cmd_1.sh`/`cmd_2.sh` on disk for m8_redhorned's live copy, so the fix is testable
without re-running the full event-ez GUI save flow) to search upward from the entity root for a
stable anchor (`*.monads`, always a direct child of house_root regardless of entity depth) instead of
assuming a fixed hop count:
```sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/*.monads" ]; do D="$(dirname "$D")"; done
exec "$D/*.monads/*.muchi-pet/ops/+x/mr_change_gold.+x" "$ENT" '10'
```
Same anchor-search pattern `khtpm_taskbar_manager.c`'s `livedesk_login_root()` already uses for the
identical class of problem (finding a fixed-location tool from a variable-depth caller). Rebuilt
event-ez (`sh button.sh compile`) — clean, only pre-existing truncation warnings.

**Note:** `mr_change_gold.+x`'s location (`*.monads/*.muchi-pet/ops/+x/`) is still hardcoded — this
fix makes path RESOLUTION depth-independent, it does not relocate the binary. See "Open Migration
Task" below for what that would actually require.

---

## ✅ Migration Task — DONE (2026-08-12, see update at top of doc)

Direct instruction, 2026-08-11: "in the future ops will be shared between all user sessions and
desks... can even be shared between users, and this is how apps and user apps will work." Direct
instruction, 2026-08-12: "migration makes sense now."

`*.monads/*.muchi-pet/ops/` → `xyzfs/bin/muchi-pet/ops/`, completed and verified (see top of doc for
the two additional bugs this surfaced and how they were fixed). `mr_change_gold.+x` and its sibling
scripts now live in the shared, house-wide location. `*.monads/*.muchi-pet/entities/` (the template
monster definitions) was deliberately NOT moved — that's a separate, still-open migration (moving
demo/template content into the sessions model), not conflated with the ops move.

**Still open, for whenever a NEW game's ops need a home:** `xyzfs/bin/<game-name>/ops/+x/` is the
now-established pattern (one subfolder per game/app under the shared `xyzfs/bin/`) — follow it for
future games rather than reinventing a location per game.

---

## Architecture: Ops vs. Events (Two Different Sharing Models)

Direct instruction, 2026-08-11 — a critical distinction for all future event/db/game work:

| | **Ops (code)** | **Events (authored content)** |
|---|---|---|
| Examples | `mr_change_gold.+x`, future game logic binaries | `event.ir.pdl`, `common_events.pdl` |
| Default scope | Shared, house-wide | Session-private (belongs to the user who authored it) |
| Cross-user? | Yes, by default — same binary for everyone | No, unless explicitly published |
| How it becomes shared | It just IS shared (infrastructure) | User publishes to the **store** |
| Where it lives (target) | `xyzfs/bin/` (shared, not per-user) | `sessions/<user>/<session>/` (private) until published |

**The `store` header cell** (cell 12 in khtpm's taskbar, currently a confirmed inert placeholder —
see `khtpm_taskbar_manager.c`'s own "6/7/9/14 inert cells" comment) is very likely intended to be
exactly this: the mechanism for a user to publish a session-private event (or eventually a whole
game?) so other users can browse/download/install it. Not built yet — this is the first time this
cell's real purpose has been connected to a concrete design, worth prioritizing once event/db-ez work
resumes.

---

## Test Log — Full Relay-Only Verification (2026-08-11)

All testing below via real relay/injection — no direct binary calls, no C function calls:

1. **Baseline:** `inventory.txt` reset to `qolq=0` (was `qolq=35`, a pre-baked template fixture, not
   live-execution evidence — see "How qolq=35 originally got there" below).
2. **Trigger:** `echo "RUN_METHOD:Play" > interact_relay.txt` on the live `m8_redhorned` pal
   (`xyzfs/users/<uuid>/home/livedesk/pals/m8_redhorned/`).
3. **Result (before fix):** injection consumed (`history.txt` shows `INJECTED: RUN_METHOD:Play`,
   `master_ledger.txt` shows a real new timestamped "Play event" line) but `qolq` stayed `0` — proved
   the runtime dispatch works but the actual gold-change op silently failed.
4. **Debug:** ran `cmd_1.sh` directly with `bash -x`, saw the exact `No such file or directory` on
   the `../../ops/+x/mr_change_gold.+x` path — root cause confirmed, not guessed.
5. **Fix applied** (see above), rebuilt event-ez, manually re-applied the same fix to the already-
   generated `cmd_1.sh`/`cmd_2.sh` on disk.
6. **Direct verification:** ran `cmd_1.sh` directly — `CHANGE_GOLD +10 qolq: 0 -> 10` — correct.
7. **Full relay re-test:** reset to `qolq=0` again, triggered `RUN_METHOD:Play` via relay again —
   `qolq` went `0 → 35` (10+25, matching both commands in `event.ir.pdl`). Real, live, relay-only
   proof.
8. **Persistence test:** via the khtpm taskbar's desks cell (`nav 4` → select a different desk →
   Enter), switched away from `desk_01` (confirmed `m8_redhorned`'s process died — `ps aux` returned
   nothing), then switched back to `desk_01`. `inventory.txt` still read `qolq=35` after the round
   trip — state persisted correctly across a real desk swap.

### How `qolq=35` Originally Got There (a dead end worth recording so no one re-investigates it)
The template source (`*.monads/*.muchi-pet/entities/m8_redhorned/`) has an IDENTICAL, byte-for-byte
`event.ir.pdl` and its own pre-baked `inventory.txt` showing `qolq=35`, with all files sharing the
same sub-second timestamp — proving the value was seeded once (likely from a real manual
`prisc+x event.pal` terminal run, per the existing `HOW2_event-ez_change_gold_k3.txt` guide's
documented manual procedure) and then captured into the template, which gets copied wholesale every
time a fresh `m8_redhorned` spawns. It is NOT evidence the live runtime ever worked — it was a static
fixture. The REAL, live-verified proof is the test log above.

---

**Owner:** claude-0001
**Status:** ✅ Change Gold runtime bug found + fixed + verified end-to-end via relay. Persistence
verified. Two architecture decisions (shared ops location, event-sharing-via-store) logged for future
work, not yet implemented — don't guess at the target locations, they need a real decision.
**Follow-up:** the muchi-pet ops migration (`*.monads/*.muchi-pet/ops/+x/` → `xyzfs/bin/` or similar)
and the `store` cell's real implementation are the two biggest levers unlocked by tonight's findings.
