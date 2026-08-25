# rpg-xyz Architecture & Design — RPG Maker MZ Parity, xyz-os Native

Written 2026-07-31, companion to `300.rtp-xyz/dox/rtp-xyz-architecture.md`
— same subject, same research, same decisions, this doc covers
rpg-xyz's own **widget-based, multi-process** variant. Read the rtp-xyz
doc first if you haven't — §0, §1, §3–§6, §8–§9 and both appendices
below are the SAME content (RPG-vs-ngn comparison, event system,
database editor, plugin system, camera system, testing, de-theming,
future notes) — this file only re-derives what's actually different:
§2 (GUI architecture) and §7 (save/load). Sections marked "— see
rtp-xyz doc" are intentionally not re-explained here to avoid drift
between two copies of the same reasoning.

================================================================================

## 0. Vision — see rtp-xyz-architecture.md §0

Same framing: rpg-xyz is a proving ground for xyz-os's own general
capabilities (dynamic menus, real PAL event scripts, a general plugin
system, per-project database content, file-mediated IPC), not an end
in itself. rpg-xyz's own specific contribution to that proof is
**multi-window live editing** — separate GL processes handing content
to each other in real time — which RPG Maker MZ genuinely cannot do
(single-window app) and rtp-xyz's own single-session design doesn't
demonstrate either. This is rpg-xyz's real reason to exist as a
distinct variant, not a redundant copy of rtp-xyz's own approach.

================================================================================

## 1. Feature comparison — see rtp-xyz-architecture.md §1

Same table applies. One rpg-xyz-specific row worth naming explicitly:

| Capability | RPG Maker MZ | ngn | rpg-xyz's own angle |
|---|---|---|---|
| Multi-window live editing | ❌ single app window | ✅ ngn's real advantage | **rpg-xyz is where this actually gets demonstrated** — separate widget processes (event-editor, map-picker, tile-picker, project-menu), each its own real GL window, live content handoff via `#.desktop/` |

================================================================================

## 2. GUI architecture (rpg-xyz specific)

**Separate widget processes, not one session with `<module>`
switching.** This is the entire architectural difference from
rtp-xyz — see that doc's own §2 for the contrasting single-session
design.

### 2a. The real precedent already proven end-to-end in this house

`&.widgits/file-menu` — the ONE widget with a real, passing K3
key-injection harness (`@.apps/text-editor-xyz/test-harn-ed-app`) —
is rpg-xyz's best model for `project-menu` (§2c below), not because
it's a file browser specifically, but because its own real bugs and
fixes (PITFALL 58: `fm_enqueue_cmd()` building its working directory
as bare `project_root` instead of `project_root + "/pieces/system"`,
caught only by a real, keypress-through-the-real-menu test, never by
an op-level harness that assumed the correct cwd) are the concrete,
already-paid-for lesson `project-menu`'s own `pm_enqueue_cmd`
equivalent must not repeat. Build `project-menu`'s own black-box
harness from day one, not after the fact.

### 2b. How widgets discover and hand off to each other

Two real, already-working mechanisms, not new ones:

1. **House runtime ledger** (`!.xyzos-standards+1.txt` §35.5) — a
   process registry at
   `<xyzfs>/home/runtime/ledger.txt` (pipe-delimited, `ledger_append`/
   `ledger_peers` ops). A widget announces itself ONLINE on launch,
   OFFLINE on exit; any other process can discover the latest live
   session for a given `project_id`. This is a different "ledger"
   than a project's own `data/master_ledger.txt` (which is a
   per-project, append-only AUDIT LOG of actions — auditability and
   crash-replay, same real TPMOS convention — not a discovery
   mechanism). Both matter; don't confuse them.
2. **`#.desktop/` tray** — real, already-exercised (`inbox/`,
   `events/`, `tiles/` subdirectories; `event_editor_open.request` +
   `.log` is a real, live example, not hypothetical). The mental
   model: (1) exporter writes a file into a shared, well-known tray
   path, (2) importer polls/watches that same path and consumes it,
   (3) both sides are separate OS processes/GL windows that never call
   each other directly. Same file-mediated-relay law as
   `interact_relay.txt`/cmd-bus inboxes everywhere else in this
   house, just with a tray (multiple named, discoverable items)
   instead of one hot-value relay file.

**XDND (real OS-level window drag-and-drop) was tried and explicitly
abandoned** — `rpg-xyz-plan.md`'s own decisions table: "WM bug, never
worked." Don't re-attempt it; the tray mechanism above is the real,
working answer to "drag content between GL windows."

### 2c. rpg-xyz's own widget set

| Widget | Role | Status |
|---|---|---|
| `&.widgits/project-menu/` | list/new/delete/open projects under `xyzfs/users/<uuid>/home/projects/rpg-xyz/` | **new, doesn't exist yet** — build on file-menu's own pattern (§2a) |
| `&.widgits/event-editor/` | build/edit a piece's exported `.pal` event script | **exists, partially real** — UI chrome (`[x]`) done: digit-accum nav, multi-digit jump, Commands\|Scratch toggle, page cycle, GL text. Save/Load/Import/Export/Edit.pal are `[~]` status-stubs. Real package edit / event_run / muta auto-spawn are `[ ]` not done at all. **Finish this widget, don't build a second one.** |
| `&.widgits/map-picker/` | list/switch maps | referenced in `rpg-xyz-plan.md`'s own asset list — verify current real status before assuming done |
| `&.widgits/tile-picker/` | brush selection, desktop stamps | same — verify before assuming |
| A new database-editor widget | full RPG Maker category set (§4, rtp-xyz doc) | new, following the same widget shape as the others |
| A new plugin-manager widget | general plugin registry (§5, rtp-xyz doc) | new, same shape |

Each is its own real GL process, own `button.sh`, discovered via the
ledger (§2b), handing content to the running game via the `#.desktop/`
tray. "Open" in `project-menu` should (re)launch
`300.rpg-xyz/button.sh run` pointed at the chosen project directory —
reuse the existing `PRISC_PROJECT_ROOT` env var pattern already
confirmed working in `editor`/`file-menu` (see the emoji pipeline
pitfall write-ups this session referenced), don't invent a new
project-selection mechanism.

### 2d. Input model — see rtp-xyz-architecture.md §2c

Same decision: keyboard-nav only this pass (digit-accumulator +
Enter-commit), mouse support noted for later via the real
`wraith-alpha` precedent, not built now. `201.rpg-maker-clone` is a
helpful local reference only — defer to real RPG Maker MZ's actual
behavior/feel when judging emulation parity, and always bend toward
xyz-os's own native idioms (file-mediated flow, dynamic PDL menu
methods, the 2D/3D POV system) over a literal port.

================================================================================

## 3. Event system — see rtp-xyz-architecture.md §3

Identical data shape and PAL-as-real-VM reasoning applies unchanged
(a piece's `piece.pdl` `METHOD` row points at a real, exported
`.pal` script; RPG Maker's command vocabulary maps onto real PAL
primitives — `beq`, `j`, `jalr`, `ecall` — not a bespoke interpreter).
The only rpg-xyz-specific note: **finish `&.widgits/event-editor`'s
own real save/load/package/event_run logic** (§2c above) rather than
building `rtp_event_editor.c`'s equivalent from scratch — but keep
the exported package SHAPE (`ee_export_entity.c`'s own format)
compatible with whatever rtp-xyz's own event editor produces, so
events are at least data-portable between the two variants even
though the daemons/widgets themselves are separate code.

================================================================================

## 4. Database editor — see rtp-xyz-architecture.md §4

Identical: full RPG Maker category set (Actors/Classes/Skills/Items/
Weapons/Armors/Enemies/Troops/States/Animations/Tilesets/Common
Events/System), stored per-project, seeded from `world_template/`.
rpg-xyz's own version is a new standalone widget (§2c above) rather
than a CHTPM `<module>` layout inside the game session — same data
shape and category map, different process boundary.

================================================================================

## 5. Plugin system — see rtp-xyz-architecture.md §5

Identical: general-purpose `plugins/manifest.pdl` per project, a
Plugin Manager screen (here, its own widget rather than a
`<module>`-owned layout), battle as the first real demonstration —
`205.ttg-tactics` (grid tactics, real, harness-proven) and
`203.gb-pokemon` (turn-based menu, real) registered as two real
plugin flavors via the `#.desktop/battles/` request/result handoff,
mutaclsym's own ambient real-time combat staying outside that
handoff entirely (§5d of the rtp-xyz doc explains why). No hardcoded
default battle system, per direct instruction.

================================================================================

## 6. 2D/3D POV camera system — see rtp-xyz-architecture.md §6

Identical: already real (`ops/camera_control.c`, 4 camera modes),
inherited wholesale from the M1 mutaclsym copy, zero new work needed.
Genuine xyz-os differentiator vs RPG Maker MZ, worth keeping visible
in any player-facing UI/docs.

================================================================================

## 7. Save/Load (rpg-xyz specific — the project-menu pattern)

### 7a. Why this differs from rtp-xyz's approach

rtp-xyz save/load is a `cp -r` sweep triggered from inside the running
game session (§7, rtp-xyz doc). rpg-xyz has no equivalent in-session
save menu — **the widget you pick a project from IS the save/load
mechanism**: `project-menu` lists real project directories, "open"
launches the game pointed at one, and saving is simply the game's own
live state already living inside that same project directory (or, for
an explicit "Save As" style flow, a real directory copy exactly like
`slop-ed-dev`'s own `save_game_to_path()`/`load_game_from_path()`,
triggered from `project-menu` itself rather than from inside the
game).

### 7b. Real mechanism

`project-menu`'s own ops (`pm_list.c`, `pm_new.c`, `pm_delete.c`,
`pm_open.c`, per `rpg-xyz-plan.md`'s own naming):

```
list:   scan xyzfs/users/<uuid>/home/projects/rpg-xyz/*/
new:    mkdir + cp -r world_template/ into a new named project dir
delete: rm -rf the named project dir (with a real confirm step)
open:   (re)launch 300.rpg-xyz/button.sh run with PRISC_PROJECT_ROOT
        pointed at the chosen project directory (§2c)
```

Same file set as rtp-xyz's own save sweep (`pieces/world_01`,
`pieces/registry`, `project.pdl`, `switches.pdl`, `pal/events`,
`pal/common_events`, `plugins/`) — since it's the SAME per-project
data shape from §3–§5, just addressed by directory-open instead of an
in-session save menu.

### 7c. `pm_enqueue_cmd` — the PITFALL 58 lesson, again

Build `project-menu`'s own command-relay op (whatever the equivalent
of `fm_enqueue_cmd()`/`enqueue_cmd_with_path()` ends up being) with
its working directory resolved as the CONSUMER's own
`pieces/system` directory from the start, not bare `project_root` —
see §2a above. This is the one concrete, already-paid-for bug not to
repeat.

================================================================================

## 8. Testing methodology — see rtp-xyz-architecture.md §8

Same governing law (real key injection, real running processes, real
frame + on-disk assertions, PITFALL 58 discipline). The rpg-xyz-
specific version of the full loop (§5d of the scaffold doc) swaps step
6 ("module handoff, same PID") for the multi-process equivalent:
inject the key that spawns `project-menu` from inside a running game
(or vice versa), assert the SEPARATE process's own window/frame
actually renders real content, not just that a file got written.
Name new harnesses `%.harnesses/rpg-xyz-full-loop/`, matching the
existing `%.harnesses/event-editor+desktop/` convention — this harness
necessarily drives multiple independent processes, same tier as that
one.

================================================================================

## 9. De-theming — see rtp-xyz-architecture.md §9

Same steps and same 2026-07-31 corrections apply verbatim (read that
doc's own §9, not re-explained here to avoid drift): registries were
narrower to de-theme than expected (terrain/furniture/items were
already generic), the emoji-regeneration claim in an earlier draft was
wrong (no such pipeline exists for world-tile assets), and — verified
live in THIS project too, same `load_dynamic_methods()` +
`${piece_methods}` wiring, same hero `piece.pdl`, confirmed byte-
identical to rtp-xyz's own copy — **the action-bar was already real
and dynamic, nothing needed building there either.** Since rpg-xyz and
rtp-xyz are both M1 copies of the same mutaclsym source, **the
de-themed registry content itself can be built once and copied into
both** — no need to design it twice, just verify the emoji-regeneration proof
independently in each project (the two projects' own compiled
binaries/asset caches are separate, even if the source data is
shared).

================================================================================

## 10. Milestones / build order

Runs AFTER rtp-xyz's own M1–M7 (see that doc's §10) per the agreed
sequencing — rtp-xyz first because its architecture (single-session
`<module>` handoff) is the harder, less-proven pattern; rpg-xyz's own
widget approach leans on `file-menu`'s already-proven precedent, so
it's the faster follow-up once the shared pieces (de-themed
registries, event system data shape, database category map, plugin
envelope) are already real from rtp-xyz's own build.

| # | Milestone | Depends on |
|---|---|---|
| M1 | ✅ DONE — copy engine in, Phase 2 module split | — |
| M2 | De-theme (§9) — reuse rtp-xyz's own registry content | rtp-xyz M2 |
| M3 | `&.widgits/project-menu/` (§7) | M2, `file-menu` pattern (§2a) |
| M4 | Finish `&.widgits/event-editor`'s real save/load/package/event_run (§3) | M2 |
| M5 | Database editor widget, full category set (§4) | M2, reuse rtp-xyz's own category map (§4, that doc) |
| M6 | Plugin manager widget + battle-as-first-plugin (§5) | M4, M5, reuse rtp-xyz's own `#.desktop/battles/` contract |
| M7 | Full multi-process harness proof of M3–M6 (§8) | M3–M6 |
| M8 | Cross-project comparison pass (rtp-xyz vs rpg-xyz) — per `rpg-xyz-plan.md`'s own original M5 | after both variants' own M7 |

================================================================================

## Appendix A: Future-direction notes — see rtp-xyz-architecture.md
    Appendix A

Identical (voxel rendering parameter set — transparency, translation,
scale, "grain"/micro-voxel subdivision, extrude-vs-per-face texture
styles; character generator mirroring `01.avatar-creation`'s own real
flow). Not this pass, captured so it isn't lost.

## Appendix B: xyzos-app export vision — see rtp-xyz-architecture.md
    Appendix B

Identical framing: real xyzos-style app/executable export, not RPG
Maker's browser/NW.js/mobile deployment, because this whole stack is
meant to grow into a general application development environment.

================================================================================
*End rpg-xyz-architecture.md*
