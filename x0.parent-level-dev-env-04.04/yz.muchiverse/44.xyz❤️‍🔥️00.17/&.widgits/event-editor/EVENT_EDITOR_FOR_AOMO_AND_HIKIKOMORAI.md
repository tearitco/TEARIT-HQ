# 🎬 event-editor — how this fits aomorai-editor AND hikikomorai

**Status:** design doc, written 2026-08-04 alongside `walk-off-aug4.md` and `&.widgits/tile-picker/TILE_PICKER_DESIGN.md`. No new event-editor code written yet — this is the plan.
**Read first:** `201.rpg-maker-clone/ARCHITECTURE.md` + `README.md` (the real reference mock), `&.widgits/tile-picker/TILE_PICKER_DESIGN.md` §4.5 (the context-menu design this feeds).

---

## 0. 🎯 Why event-editor is next, in one paragraph

Direct instruction: the real end goal is letting creators build/modify game entities **on the desktop or in a game view**, and letting end-users create/customize their own **desktop pets/items** — the same mechanism, one unified idea. Tile-picker's desktop-placer (a bare emoji, live on your desktop) is the first working slice of that. The very next real capability every one of those "things" needs is a **context menu with real methods** — and the very first method worth having is **"open the event editor for this thing."** That means event-editor has to be real before the context-menu work can mean anything.

---

## 0.5 📂 Where desk-created events live, and the "every event gets its own button.sh" convention

Direct instruction, 2026-08-04: **every event created on the desktop should be independently openable via its own `button.sh`** — a real, house-wide convention, stated explicitly now. This is not specific to any one consumer project (see `@.apps/asa-&-ava/asa&ava-design-plan.md` §2.3 for the first concrete consumer, its own conversational-event packages) — it's a real requirement on event-editor's own package-export step.

**Storage location:** desk-created event packages live in the house-wide `#.desktop/events/` tray — already provisioned in `#.desktop/README.txt`'s own real layout (`events/`, `entities/`, `tiles/`, `inbox/`), just unused by any real event-editor output yet. One package per event:

```
#.desktop/events/<event_id>/
    event.pal              # the real, compiled event script (this house's real event-runtime format)
    event_readable.txt      # human-readable mirror (same ASCII+real-format duality every project here keeps)
    meta.pdl                # METHOD rows - same data-driven table tp_desktop_window.c's own
                             # context menu already reads (see tile-picker's own
                             # TILE_PICKER_DESIGN.md §4.5) - "Open in Event Editor", "Replay", etc.
    button.sh                # <- the real ask: opens THIS event independently
```

**What that per-package `button.sh` actually needs to do:** resolve event-editor's own real session-launch binary/path, and launch it pre-focused on this specific package — same "cross-link via a small state file, never shared process infrastructure" rule every widget in this house already follows (file-menu's `focus.txt`, board-viewer's `bv_state.txt`/`focused_project_root`, tile-picker's own `tp_place_desktop.c` package convention). Concretely, this is a small, mostly-mechanical template (a handful of lines resolving event-editor's own `button.sh run-widget <this_package_dir>`-equivalent), generated once per package at export time — not something a human hand-writes per event.

This is real, necessary infrastructure for event-editor's own package-export step (§4 below), not optional polish — without it, a desk-created event is just an inert file, not a "thing" that can be reopened and interacted with the way every other live entity in this house already can be.

---

## 1. 📎 The real reference: `201.rpg-maker-clone`

A full, working freeglut binary — both a map/event editor AND a player — already exists at `201.rpg-maker-clone/`, deliberately shaped to match house data conventions **so it can be wired in later without a format rewrite**. This is not a CHTPM widget (it's the "pure GLUT product path," per its own `ARCHITECTURE.md`), but its **data model and UX flow are the real reference** for what event-editor needs to become.

### 1.1 Real command IR (mirror this, don't invent a new one)

```c
enum CmdType {
  CMD_SHOW_TEXT, CMD_SET_SWITCH, CMD_IF_SWITCH,
  CMD_END, CMD_TRANSFER, CMD_RET, CMD_COMMENT, CMD_EMPTY
};
```

Real, minimal, and already proven to run in a real play interpreter (`pc = 0; loop: dispatch on cmd type; pc++` — with `if_switch` skipping to its matching `end`). This is the shape to mirror, not reinvent.

### 1.2 Real file layout (already house-aligned)

| Path | Format |
|---|---|
| `project.pdl` | name, start_map, start_x/y |
| `switches.pdl` | `SWITCH \| key \| value` rows |
| `maps/<id>/map.txt` | grid of single-byte terrain |
| `maps/<id>/events/ev_X_Y/state.txt` | `key=value` (name, trigger, x, y, sprite) |
| `.../event.pal` | one command per line — the real interpreter source |
| `.../event.ir.pdl` | optional metadata stub |

Events are discovered by scanning `events/ev_*` directories — no central registry file.

### 1.3 Real UI/nav model (already matches CHTPM conventions)

Same continuous digit-accum model as every CHTPM screen already uses in this house: global focus index `0..N-1` shown as `#1..N`, never resets per panel; multi-digit types (e.g. `1` then `7` → jump to #17); Tab toggles Commands/Scratch views. **This means the mock's own UX already speaks this house's native nav language** — porting it to a real CHTPM widget is a data/rendering-layer change, not a UX redesign.

### 1.4 The mock already anticipated this exact handoff

Its own `ARCHITECTURE.md`, "Extension points," item 5:

> "Optional export into `#.desktop/events/` package shape for muta import."

This is the same `#.desktop/` convention tile-picker's own desktop-placer already writes into (`#.desktop/tiles/`) and hikikomorai's design doc formalizes as the house-wide living-desktop tray. The mock was built anticipating this exact convergence — building real event-editor support for `#.desktop/events/` packages is completing a connection the mock already left a door open for, not inventing a new one.

---

## 2. 🏗️ What event-editor needs to actually become

**Not** a port of the mock's raw-GLUT rendering. Event-editor should be rebuilt as a real house-standard CHTPM widget — same pattern tile-picker now correctly uses (own session, own `system/` pipeline copy, own `.chtpm` layout, own `*_menu_input`/`*_compose_frame` op pair, real digit-jump nav via `chtpm_parser_pal`). The mock is the reference for **data shape and UX flow**, not for **how to build a house widget** — see `&.widgits/tile-picker/TILE_PICKER_DESIGN.md` §1 for exactly why a bespoke rendering path (even one with a working, sensible-looking UI) fails in this house if it doesn't route through the real input pipeline.

### 2.1 For aomorai-editor

Aomorai-editor's own event editor (referenced throughout `aomorai-editor-blueprint.md` §3, "Event Editor — Buttons AND Script, Both Real") is the in-house-canon design: a command-list UI (Show Text, Show Choices, Control Variables, Conditional Branch, Move Route, Common Event Call) plus a real `RAW_PAL "..."` escape hatch, both compiling to real `.pal` scripts. The mock's own `CmdType` enum is a real, smaller, already-working subset of that same idea — build the real thing as a superset, informed by both.

### 2.15 🔍 Confirmed: the mock does NOT use CHTPM nav at all

Direct question, 2026-08-04: "does the mock use chtpm nav? cuz we should?" — checked directly against the real source rather than assumed. **No, it doesn't, and this needs to be said plainly so nobody builds on top of the mock's own input handling by mistake.**

`ee_gl_mock.c`'s own header calls itself "pure freeglut" and "the pretty GLUT build target... vs chtpm→rgb→gl_mirror" — its own words already flag it as a deliberately *separate*, non-CHTPM path. Confirmed via direct grep: zero occurrences of `chtpm_parser_pal`, `interact_relay.txt`, or `history.txt` anywhere in the file. Its nav (`digit_key()`, `do_jump()`, `keyboard()`/`special()` via `glutKeyboardFunc`/`glutSpecialFunc`) is a hand-rolled digit-accum/focus system, built directly against GLUT's own keyboard callbacks — the exact same category of mistake tile-picker's own first draft made (see `TILE_PICKER_DESIGN.md` §1: a bespoke window that "looked plausible... but the user could not interact with it at all," because it never wrote into `history.txt`, so `chtpm_parser_pal` — the real, sole owner of all nav/focus/digit-jump logic in this house — never saw a keystroke from it).

**What this means concretely for the real build:** none of `ee_gl_mock.c`'s own `keyboard()`/`special()`/`digit_key()`/`activate()` functions get ported. What DOES get reused is the **zone layout and data** — mirrored into real CHTPM markup, with `chtpm_parser_pal` (via a real `.chtpm` layout + `<button onClick="KEY:n">` elements, same as every other house widget) owning ALL real navigation instead:

| Mock zone (raw GLUT, own nav) | Real CHTPM equivalent |
|---|---|
| Methods strip (1-8: Save/Load/Import/Export/Toggle View/Edit .pal/New Page/Help) | Real `<button onClick="KEY:n">` row, generated by `ee_compose_frame.c` from a real METHOD-style table — same `${game_map}`-substitution pattern `fm_compose_frame.c`/`tp_compose_frame.c` already prove out. |
| Pages tabs (9-12) | More real buttons in the same dynamic markup block, or (if page switching is a real navigation, not just a same-screen state flip) a real `href` to a separate `.chtpm` layout per page — matching file-menu's own hard-learned "always use `href` to go to another page, never an internal mode switch" rule (`fm_compose_frame.c`'s own header comment, cited in `TILE_PICKER_DESIGN.md`). |
| Fields (13-16: Trigger/Priority/Options/Walk) | Real buttons or `<cli_io>` fields (if free-text editing is needed) declared statically in the layout, same convention file-menu's own browser screens already use for typed input. |
| Contents list (17-28, Commands/Scratch toggle) | The real dynamic list — one real button per command row, rebuilt every `ee_compose_frame.c` call, same "walk live data, emit one `<button>` per row" pattern `fm_compose_frame.c`'s own `build_file_browser_markup()` already proves. |
| Footer (29-34: OK/Cancel/Apply/Add Cmd/Palette/Desktop) | More real buttons, same dynamic block. |

Every one of these becomes a **real, `chtpm_parser_pal`-owned, digit-jump-navigable button** — the mock's own continuous 1-34 global focus index is a real, good UX target to preserve, but achieved through real CHTPM nav, not reimplemented in a bespoke keyboard handler.

### 2.2 For hikikomorai

Per `hikikomorai-design.md` §3 ("do not build hikikomorai's editor UI before aomorai-editor's event editor's command format is settled"): hikikomorai's own desk-session entities (tile-picker's live emoji windows, egg-pals, future desktop pets/items) need to consume the **same compiled `.pal` event output** as aomorai-editor's game entities do — one event format, two hosts (desk session vs. map session), not two divergent ones. This is why event-editor must be built once, correctly, before either consumer gets deep context-menu wiring.

---

## 3. 🖱️ The concrete next feature this unlocks: real context menus

Per `TILE_PICKER_DESIGN.md` §4.5 (already designed, not yet built): every living desk-session entity should get a real, data-driven context menu — a `METHOD`-row table living in the entity's own package dir (e.g. `#.desktop/tiles/<name>/meta.pdl`), not a hardcoded switch. The very first real method, for every kind of entity, is:

```
METHOD | Open Event Editor | OPEN_EVENT_EDITOR
```

Concretely, once event-editor is real: a right-click on a desktop-placed tile-picker window (or an egg-pal, later) reads its own package dir's method table, and "Open Event Editor" spawns event-editor's own widget session, focused on that specific entity's own event package (same `focus.txt`/`board_widget_bridge.txt`-style cross-link convention every other widget pairing in this house already uses — see `aomorai-editor-blueprint.md` §1.5 for the real, still-open "which convention" question that also applies here).

---

## 4. 🗺️ Suggested build order

1. Read `201.rpg-maker-clone/src/*.c` in full (not just the docs) — confirm the real `CmdType` dispatch loop and event-directory-scanning convention by reading the actual code, same "read real precedent before writing anything" discipline `TILE_PICKER_DESIGN.md` §1 already had to learn the hard way.
2. Rebuild event-editor's own `.chtpm`/`*_menu_input`/`*_compose_frame` triple (currently just `gl_mock/` + a plan file — no real CHTPM implementation exists yet), modeled on file-menu/tile-picker's own now-proven pattern, not the mock's raw-GLUT rendering.
3. Wire aomorai-editor's own event editor screen (per its own blueprint §3) as the first real consumer.
4. Add the `#.desktop/events/` package export/import path the mock's own extension-point 5 anticipated — this is what lets a desktop-placed entity (tile-picker, future hikikomorai pets) actually carry a real event script.
5. Only then: add the real context-menu method table (§3 above) to `tp_desktop_window.c` and egg_window.c-family windows, with "Open Event Editor" as the first working method.
