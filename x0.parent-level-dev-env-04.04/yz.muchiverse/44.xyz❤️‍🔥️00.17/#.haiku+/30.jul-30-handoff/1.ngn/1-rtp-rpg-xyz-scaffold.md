# 🏗️ Topic 1 — Scaffolding rtp-xyz & rpg-xyz fast, and proving it's real

> Companion deep-dive to `#.haiku+/30.jul-30-handoff.md`. This file assumes
> you've read that index first. Highly technical, verbose on purpose — the
> goal is that you can read this and start editing real files within the hour,
> not just nod along.

## 🧭 0. The one-sentence version

**`101.mutaclsym🧟‍♂️️+18.01` is already a generic RPG engine wearing a zombie
costume.** The costume (monster/item/terrain/furniture registries, ~110
lines total, plus the `world_01`/`world_01_template` map content) is
separable from the engine (27 `ops/*.c` files, all of them mechanics —
movement, camera, turn system, inventory, crafting, combat, save/load,
world I/O, map generation, tile placement, widget-cmd bridge — **zero**
zombie-specific `.c` files exist). Confirmed by direct listing, not
assumed:

```
$ ls 101.mutaclsym🧟‍♂️️+18.01/ops/*.c
camera_control.c   craft.c        emoji_xtract.c   move_player.c      pickup.c
choice.c           drop.c         end_turn.c       muta_map_io.c      save_game.c
compose_frame.c    dump_rgb_png.c examine.c         muta_place_tile.c  tick_monsters.c
compose_rgb_frame.c eat.c         game_dispatch.c   muta_widget_cmds.c title_input.c
compose_title_frame.c emoji_gen_atlas.c generate_map.c muta_world_io.c toggle_emoji.c
                                   gen_voxels8.c    pdl_reader.c
                                                     pet_import.c
```

None of those files know what a "zombie" is. Grep confirms it — the string
`"zombie"` only appears in `pieces/registry/monsters/monster_types.txt`
(a data file) and in map/world save data, never in a `.c` file's own
logic. This is the load-bearing fact for everything below: **"scaffold a
new RPG fast" is a data-swap problem, not a code-writing problem**, for
at least the first pass.

## 📐 1. What already exists — read this before touching anything

There is already a real plan document, written by a prior session, sitting
at `300.rpg-xyz/rpg-xyz-plan.md` (byte-identical copy at
`300.rtp-xyz/rpg-xyz-plan.md`). **Read it in full before you do anything
else** — this file doesn't repeat it, it goes one layer deeper into the
actual code/trace-flow that plan assumes you already understand. The plan
defines two variants:

| | rpg-xyz | rtp-xyz |
|---|---|---|
| Editing tools live... | as separate `&.widgits/*` processes | as CHTPM `<module>`-loaded daemons inside the game session itself |
| Pattern to copy | `&.widgits/file-menu` (fixed, proven — see §3) | `slop-ed-dev`'s `manager` + `pal_editor_module` (TPMOS reference, described but not yet ported here) |
| New thing that needs building | `&.widgits/project-menu/` (doesn't exist yet) | `manager/rtp_manager.c` + `rtp_map_editor.c` + `rtp_event_editor.c` + `rtp_palette.c` (none exist yet) |
| Layout switching | separate `button.sh run-widget` processes, never a layout swap | `<module>` tags + href/F-key layout switching, same session |

Both directories (`300.rpg-xyz/`, `300.rtp-xyz/`) already exist as
scaffolds — `ops/`, `system/`, `pal/`, `pieces/`, `scripts/`, `button.sh`,
a `project.pdl`. **Check their current state before assuming M1 (the
"copy mutaclsym in" step) hasn't happened** — as of this write-up they
have their own `ops/`, `system/`, `pieces/registry/` etc. already
populated (verify with `diff -rq 101.mutaclsym🧟‍♂️️+18.01/ops
300.rpg-xyz/ops` — if it's clean, M1 is done and you're picking up at M2).

## 🎨 2. The visual references, read correctly

You were pointed at two things:

**`&.widgits/event-editor/gl_mock/`** — a **separate, standalone freeglut
binary** (`ee_gl_mock.c`, compiled via its own `gl_mock/button.sh`) whose
entire purpose is "look and feel like RPG Maker MV's event editor,
`event_editor_rmmv_look.jpg` / `VISUAL_REF_rmmv_early.jpg` /
`VISUAL_REF_rmmv_target.jpg` are the literal target screenshots it's
chasing." **This has NO runtime link to the real event-editor widget.**
It is not "the event editor with a mock backend" — it's a completely
separate binary built to nail the UI feel first, in isolation, before the
real thing has to also worry about CHTPM/PAL wiring. Read
`gl_mock/RMMV_EVENT_EDITOR_GUIDE.md` for what it's chasing visually.

**`201.rpg-maker-clone/`** — also a **separate, standalone freeglut
binary** (`src/main.c` + `draw.c` + `project.c`, ~one gcc line to build,
see `ARCHITECTURE.md`), but this one is NOT a UI mock — it has a genuinely
**working Play + edit loop**: Title → Map Editor → Event Editor → Play,
real event interpreter (`CMD_SHOW_TEXT`/`CMD_SET_SWITCH`/`CMD_IF_SWITCH`/
`CMD_TRANSFER`/etc.), real save (`project.pdl` + `map.txt` + `switches.pdl`
+ per-event `event.pal`), a real door-that-needs-a-switch demo quest. Its
own `ARCHITECTURE.md` states directly: **"Not the CHTPM → rgb → gl_mirror
path. This is the pure GLUT product path."** — meaning this is a
DIFFERENT rendering pipeline than the rest of this house (chtpm_parser_pal
→ chtpm_rgb_render → gl_mirror). It proves the DATA MODEL and GAME LOOP
work; it does not prove the CHTPM pipeline works, because it deliberately
bypasses it.

So: **two separate reference points, for two separate purposes.**
`gl_mock` = "does the UI feel right." `rpg-maker-clone` = "does the actual
event/switch/save logic work, end to end, in isolation from CHTPM." Your
job in rtp-xyz/rpg-xyz is to get BOTH qualities into the REAL CHTPM-native
pipeline — neither reference does that today, and `ARCHITECTURE.md` says
so explicitly under "Relation to house": *"CHTPM event-editor widget:
Parallel product path... this binary is independent."*

## 🔌 3. The proven pattern to copy, and the one bug you must not repeat

`&.widgits/file-menu` is your best real precedent for `project-menu` (the
rpg-xyz variant's missing widget) — not because it's a file browser, but
because it is the ONE widget in this house that has been end-to-end
proven via real key injection (`@.apps/text-editor-xyz/test-harn-ed-app`,
see topic 3 companion doc and `!.xyzos-pitfalls+1.txt` PITFALL 58).

**Read PITFALL 58 before writing `project-menu`'s own `pm_enqueue_cmd`
equivalent.** The bug: `fm_enqueue_cmd()`/`enqueue_cmd_with_path()` were
building their working directory as bare `project_root` instead of
`project_root + "/pieces/system"`, which meant `fm_enqueue_cmd.+x` was
running the wrong binary from the wrong cwd — and **the op-level test
harness (which invoked `fm_enqueue_cmd.+x` directly, correct cwd assumed)
never caught it.** Only a real, level-2, actual-keypress-through-the-real-
menu test caught it, because that's the only kind of test that exercises
the REAL cwd the running widget process actually uses. `project-menu`
will have the identical shape (a widget that enqueues commands to a
consumer daemon over a relay/cmd-bus file) — build its own black-box
harness from day one, not after the fact.

## 🖱️ 4. "Dragging GL windows into each other" — what that actually is

This is not X11 window-manager drag-and-drop. That was tried and killed:
`rpg-xyz-plan.md`'s own decisions table says it outright — *"XDND
abandoned (WM bug, never worked)."* What you actually have, and what
already works, is **file-mediated handoff through a shared tray
directory**: `#.desktop/` (house root), with subdirectories `inbox/`,
`events/`, `tiles/`. The real, working example:

- `&.widgits/event-editor/ops/ee_export_entity.c` writes a package into
  `#.desktop/events/`.
- `&.widgits/event-editor/ops/ee_import_to_world.c` is muta's own consumer
  — it watches/reads from that same tray and pulls the package into the
  live world.
- `&.widgits/event-editor/ops/ee_open_request_write.c` /
  `ee_open_request_read.c` are the OTHER direction: mutaclsym (or anything
  else) drops an "open request" file into `#.desktop/inbox/` (confirmed on
  disk: `#.desktop/inbox/event_editor_open.request` +
  `event_editor_open.log`, a real, already-exercised example, not
  hypothetical), and event-editor's own launcher watches for it and spawns
  itself in response — this is the real mechanism behind the README's own
  line *"mutaclysm Space → Event → export package + open request →
  `&.widgits/event-editor/button.sh run-widget`, not a second human
  terminal."*
- There is already a real harness proving one leg of this end to end:
  `%.harnesses/event-editor+desktop/` (its own `button.sh`, `proof/`,
  `workdir/`) — **read this harness's own script before writing a new
  one**, it is the closest existing precedent for the "drag a GL window's
  content into another GL window" proof you're being asked to build for
  rtp-xyz/rpg-xyz (e.g. tile-picker → map editor, or event-editor →
  running game).

So the real mental model: every "drag" is (1) exporter writes a file into
a shared, well-known tray path, (2) importer polls/watches that same tray
path and consumes it, (3) both sides are separate OS processes/GL windows
that never talk to each other directly. This is exactly the same
file-mediated relay pattern as `interact_relay.txt`/cmd-bus inboxes
everywhere else in this house (see the main index doc's architecture
recap) — it is NOT a new mechanism, it's the SAME mechanism, just with a
tray directory instead of a single relay file, because a tray needs to
hold multiple named, discoverable items instead of one hot value.

## 🧵 5. Concrete near-term plan, in the order to actually do it

This section is deliberately more prescriptive than `rpg-xyz-plan.md`,
because that file already told you WHAT the milestones are (M1–M5) — this
tells you HOW to attack them given everything above.

### 5a. Separate the zombie costume from the engine (do this FIRST, it's small)

Since the split is ~110 lines of registry data + `world_01`/
`world_01_template`, not code: create a real, minimal, non-zombie starter
registry set (a handful of terrain/furniture/item/monster rows with
INTENTIONALLY generic content — "grass/wall/door", "stick/potion",
"slime" — enough to be playable, not a full re-theme) and a blank
`world_template/` (matches the plan doc's own naming). This is the actual
concrete version of the plan's own M1 step 3 ("Populate default project
with terrain, NPC, switch, items, monsters") — do it with GENERIC content,
not by literally reusing mutaclsym's zombie set, or rpg-xyz/rtp-xyz will
just be "mutaclsym with a different directory name," which defeats the
point of having two variants to compare.

**Prove this step, don't just do it**: after swapping the registries,
run `pieces/registry/emoji_assets/` generation cold (delete the folder,
launch, confirm real voxel art gets generated for your NEW registry's
`unicode` fields via the `ensure_emoji_asset_ready()` pipeline already
wired into `ops/compose_rgb_frame.c` — see `!.xyzos-pitfalls+1.txt`, the
mutaclsym port entry, 2026-07-30). If that works cleanly, your data split
was real and complete; if some old zombie asset_id is still referenced
anywhere, it'll show up as a broken/missing lookup immediately.

### 5b. rpg-xyz: build `project-menu` on the file-menu pattern

Follow `&.widgits/file-menu`'s own shape file-for-file: `pm_set_focus.c`,
`pm_enqueue_cmd.c` (apply the PITFALL 58 lesson from the start — cwd must
be the CONSUMER's own `pieces/system`, not bare `project_root`),
`pm_compose_frame.c`, `pm_menu_input.c`, `pm_scan_dir.c` (list projects
under `xyzfs/users/<uuid>/home/projects/`, per `rpg-xyz-plan.md`'s own
decision table), `ledger_append.c`/`ledger_peers.c` (copy verbatim, this
is boilerplate, not custom logic). Ops needed per the plan: list/new/
delete/open. "Open" should enqueue a command that (re)launches
`300.rpg-xyz/button.sh run` pointed at the chosen project directory —
check how `editor`/`file-menu`'s own `PRISC_PROJECT_ROOT` env var pattern
works (you've already seen this exact pattern if you've read the emoji
pipeline pitfall write-ups) and reuse it rather than inventing a new
project-selection mechanism.

### 5c. rtp-xyz: build the manager + module daemons on the slop-ed-dev pattern

This is the bigger lift of the two variants, because nothing here exists
yet (no `manager/` directory in `300.rtp-xyz/` as of this write-up).
`rpg-xyz-plan.md` names the real reference:
`slop-ed-dev` (TPMOS) — **go read its actual `manager/` daemon source
before writing `rtp_manager.c`**, don't design this from the plan doc's
one-paragraph description alone. The plan doc's own summary: *"Editor
tools loaded as CHTPM `<module>` daemons... Two daemons: slop-ed-dev_manager
+ pal_editor_module... File-as-database, daemon-as-controller
architecture... `piece.pdl` uses METHOD sections to map events to
handlers."* Your `rtp_manager.c` is the `slop-ed-dev_manager` analogue;
`rtp_map_editor.c`/`rtp_event_editor.c`/`rtp_palette.c` are each a
`pal_editor_module` analogue, one per tool. Layout switching between
`game.chtpm` / `map_editor.chtpm` / `event_editor.chtpm` /
`project_menu.chtpm` / `palette.chtpm` happens via `<module>` tags per
layout, same session, no second process spawn — this is the entire
architectural difference from rpg-xyz's separate-widget approach, and
it's the harder one to get right because module lifecycle (which module
owns focus, how a module signals "I'm done, hand back to the game layout")
isn't a solved problem yet in THIS house (it IS solved in slop-ed-dev,
which is why you're told to copy its pattern rather than invent one).

### 5d. Prove it's real, not superficial — build the harness BEFORE you declare done

This is the part `test-harn-ed-app` already taught you how to do — reuse
that exact shape, don't reinvent it. Concretely, for EITHER variant, a
real level-2 harness (see `!.xyzos-standards+1.txt` §36.6) needs to prove,
via real key injection through real running processes, not op-level
shortcuts:

1. **Create**: project-menu (rpg-xyz) or the title-screen "New" flow
   (rtp-xyz) creates a real new project directory under
   `xyzfs/users/<uuid>/home/projects/<name>/`, seeded from
   `world_template/` — assert the directory and its files exist on disk
   with the RIGHT starter content (not empty, not a stale copy of a
   PREVIOUS test run's project — use a unique marker string the same way
   `demo_load.sh`/`demo_save.sh` do).
2. **Edit**: inject real keys into the map editor (place a tile) and the
   event editor (create one event with one `CMD_SHOW_TEXT` command) —
   assert the RENDERED FRAME shows the change (not just the state file —
   this is the exact "state-file-only checks would have missed PITFALL 58"
   lesson, applied here).
3. **Save**: trigger a real save through the real menu flow — assert the
   actual files on disk (`map.txt`, `events/ev_X_Y/event.pal`,
   `switches.pdl`) contain what was actually typed/placed.
4. **Reload**: kill everything (`EMERGENCY_KILL.sh`), relaunch fresh,
   reopen the SAME project via project-menu/title-screen — assert the
   frame shows the SAME content that was saved, proving load isn't a
   no-op or reading stale cached state.
5. **Play**: inject real movement + real action-trigger keys — assert the
   event you built in step 2 actually fires (message box appears in the
   frame, or a switch flips in `switches.pdl`, matching whatever you
   built) — this is the step that proves the event INTERPRETER is real,
   not just that data got written to disk.
6. **(rtp-xyz only) Module handoff**: inject the key/href that switches
   `game.chtpm` → `map_editor.chtpm` mid-session — assert the SAME process
   (not a respawned one — check the PID) is now rendering the editor
   layout, proving `<module>` switching actually works rather than each
   layout secretly being its own relaunch.

Name it `%.harnesses/rpg-xyz-full-loop/` or
`%.harnesses/rtp-xyz-full-loop/` (matching the existing
`%.harnesses/event-editor+desktop/` convention for cross-project harnesses
that don't belong inside a single project's own `test-harn-*` dir), not
inside either project directly — this harness necessarily drives MULTIPLE
processes (project-menu/manager + the game engine + possibly event-editor),
so it belongs at the same tier as `event-editor+desktop`'s own harness,
per that convention.

## ⚠️ 6. Known trap, stated plainly

Every "quickly spin up X" temptation in this house has, historically,
turned into "a colored block instead of the real thing" (emoji pipeline),
"the harness passed but the feature never worked" (PITFALL 58), or "the
UI looks right but the backend is a stub" (event-editor's own current
`[~]`/`[ ]` status lines, quoted verbatim in §2 above). Treat "looks
right in gl_mock" and "compiles and the op-level test passes" as **zero
evidence** that rtp-xyz/rpg-xyz actually work. The only evidence that
counts is §5d's harness, run against the real running app, checking the
real rendered frame and the real files on disk.
