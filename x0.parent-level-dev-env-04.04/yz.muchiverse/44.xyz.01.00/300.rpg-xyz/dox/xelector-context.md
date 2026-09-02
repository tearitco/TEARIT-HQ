# xelector-context.md — Xlector context menu + event editor

**House:** `44.xyz…`  
**Project:** `101.mutaclsym…`  
**Date:** 2026-07-28  
**Status:** investigation + design (no product code yet)  
**Audience:** humans + agents before implementing maker / RPG-Maker-style events  

**Spelling note:** code and state use **`xlector`** (`xlector_pos_x/y`). Product speech often says **xelector**. This doc uses **xlector** for paths/fields and **xelector** only in product-facing titles where it matches user wording.

---

## 0. What this document is

You asked for a **context menu on the xlector** (Spacebar) that leads into an **RPG Maker MV–style event editor**, with:

| Surface | Behavior |
|---------|----------|
| **Context menu** | `Event · Copy · Paste · Delete · Exit` — numbered, arrow-nav, **ASCII + GL**, inside the **interact** map window |
| **Event option** | Opens **separate event-editor widget** (not in-process view). Exports package to **house desktop** + open request |
| **Event editor** | Own dir: `&.widgits/event-editor/` — name, switches, sprite, script; also edits **desktop** pets/charas |
| **GL script pane** | Button / **scratch-block** style (widget GL later) |
| **ASCII script pane** | Readable **`.pal`**; optional raw pal edit |
| **Desktop tray** | `#.desktop/` — pickers place items here; editor works offline; **drop into mutaclysm** |
| **Persistence** | Desktop packages until import; once in `world_01/…/events/` they ride SAVE/LOAD |
| **Runtime** | Imported scripts → `event_run` / entity ops (later) |

This file is the **investigation of what already exists** and the **architecture to implement next**. It is not an implementation checklist forced into one PR; phases are explicit so agents do not boil the ocean.

**Related house notes (do not re-derive laws):**

| Doc | Why it matters |
|-----|----------------|
| `#.wussup…` in this project | User intent: xelector meta methods → event editor; RMMV events + scratch first; reuse ops |
| `dox/ctrl-legend.md` | Live key ownership map (Space free today) |
| `dox/01-cdda-architecture.md` | xlector active-target pattern; FSM / scriptable AI orbit |
| `&.widgits/WIDGETS_ROADMAP.txt` | tile/map pickers, PLACE_TILE by coords (maker spine) |
| `%.harnesses/muta-zoo.md` | Cross-project save slots + maker surfaces |
| `ARCHI_TEST_SUM-J28.txt` | What is already coded vs deferred |
| External: `#.ref/#.rmmv-events-explained/` | RMMV command catalog + PMO/TPM fit notes |

---

## 1. Product intent (one paragraph)

Mutaclysm is shifting from “a fixed zombie roguelike” toward a **project runtime** you can **author in-place**. The xlector is the maker cursor: walk it over a tile or entity, open a **context menu**, and either **clipboard ops** (copy/paste/delete) or open **Event** — a full event page for that cell/entity. Events are the RPG Maker “why does this tile do something” layer: dialogue, switches, moves, spawns, custom ops. **Scratch blocks (GL)** and **`.pal` (ASCII / power-user)** are two views of **one** event graph, not two languages.

---

## 2. Investigation — what exists today

### 2.1 Xlector / interact mode (LIVE)

| Field | Where | Role |
|-------|--------|------|
| `interact_mode` | `map_start/hero/state.txt` | `1` = cursor mode |
| `xlector_pos_x` / `xlector_pos_y` | same | free cursor on map |
| `possessed_id` / `last_possessed_id` | same | possession (separate from interact) |
| `active_panel` | same | overlay menus: `none` \| `craft` \| `inventory` |
| `panel_cursor` / `panel_digit_accum` | same | numbered panel nav |

**Input path (both ASCII and GL already share this):**

```text
key → keyboard_input.c | gl_mirror.c
   → pieces/apps/player_app/history.txt
   → pal main_loop: move_player → choice → camera_control
   → (chtpm path) interact_relay.txt + KEY:n / INTERACT
```

**Interact mode behavior (`ops/choice.c`, `ops/move_player.c`):**

- Enter interact: historically `'i'` / CHTPM “Control Hero”; possession via Enter on possessable tile; `'9'` release/reverse-jump.
- While `interact_mode=1`: arrows move **xlector**, not hero.
- Enter at cursor: possess if possessable, else **examine**.
- Escape: exit interact mode (does **not** clear possession).
- **Digits in interact_mode are currently a pure no-op** (no action-bar while cursor is live). That is the natural slot for a **context menu** that only appears on demand (Space), so digits do not fight camera POV keys until the menu is open.

**ASCII + GL:** compose path already duals (`compose_frame` + `compose_rgb_frame` / gl_mirror). CHTPM `game.chtpm` embeds the map as `${game_map}` and methods as buttons. **Both surfaces already read the same hero/state and history** — a new panel type that only writes state + is drawn by compose will show in both, same as craft/inventory.

### 2.2 Overlay panel pattern (template for context menu)

Craft / inventory already prove the UX shape we need:

| Concern | Existing solution |
|---------|-------------------|
| Mode flag | `active_panel=craft` \| `inventory` |
| Numbered rows + Cancel | `panel_cursor`, digits, Enter commits, Esc closes |
| Suspend movement | `move_player` returns early if panel open |
| ASCII draw | `compose_frame.c` overwrites viewport sub-rectangle |
| CHTPM / GL | `write_panel_gui_state()` → `craft_panel_items` / ACTIVATE submenus in `game.chtpm` |

**Implication:** context menu is a **new `active_panel` value** (`xlector_ctx`), same as craft/inventory. Event editor is **not** a mutaclysm view_mode — it is a **separate widget** (`&.widgits/event-editor/`) talking through `#.desktop/` packages + open requests (file-menu / drag-drop kinship).

### 2.3 Spacebar ownership (LIVE)

`ctrl-legend.md` and `choice.c` key map: **Space (`32`) is unused** by mutaclysm gameplay keys today.  
Safe default: **Space opens / closes xlector context menu only when `interact_mode=1`** (and optionally when cursor is “maker-focused”). Outside interact mode, Space remains free or reserved (do not steal from future message-advance without deciding).

### 2.4 Digit / camera conflict (design constraint)

In 3D (`render_mode=1`), keys `1`–`4` set **POV camera modes**. Context menu numbering **must only consume digits while `active_panel=xlector_ctx` (or event editor nav)**. Same pattern as craft panel already gates digits when open. **Do not** global-remap 1–5 forever.

Recommended menu numbers (fixed product order):

```text
1  Event
2  Copy
3  Paste
4  Delete
5  Exit
```

(5 = Exit also matches “last row / cancel-ish” mental model; Esc always = Exit/close without action.)

### 2.5 World / entity file layout (LIVE)

```text
pieces/world_01/<map_id>/
  map.txt
  furniture.txt
  state.txt
  hero/                 # hero always under map_start/hero (map_id field teleports)
  items/<item_id>/state.txt
  monsters/<id>/state.txt
  <entity_id>/          # pets/eggs: piece.pdl + state.txt
    piece.pdl
    state.txt
```

**piece.pdl** already has METHOD rows → ops binaries (hero: pickup/drop/…; pets: feed/pet/play — some ops stubs).  
**No first-class “event page” tree** exists yet. No global switches file for RMMV-style game progression.

**SAVE/LOAD:**

| Path | Mechanism |
|------|-----------|
| Legacy in-game | `ops/save_game.c` → `pieces/saves/save_N/world_01/` |
| Product slots | `muta_world_io` → `<saves_root>/<slot>/world_01/` full tree copy |
| Implication | Anything under `world_01/` is automatically save/load durable if we place events there |

### 2.6 What is NOT live (gaps this feature fills)

| Gap | Today |
|-----|--------|
| Context menu on xlector | No |
| Copy/paste/delete cell or entity | No (tile-picker can PLACE_TILE by cmd only) |
| Event page / switches / event sprite | No |
| Scratch-block UI | No |
| Entity-driven `.pal` event runtime | Main loop is fixed `pal/main_loop.pal`; entities do not run custom event graphs |
| Click-to-paint | Deferred (coords/cmd only) — **context Event is independent** of mouse paint |
| Second GL window for editor | Not required for v1 if event editor is a **view mode inside** mutaclysm interact |

### 2.7 External RMMV / PMO research (available on disk)

`/home/no/Desktop/🧩️Piecemark-IT/#.ref/#.rmmv-events-explained/` catalogs RMMV event commands with **PMO/TPM FIT** notes. High-value first wave for mutaclysm (not the whole catalog):

| RMMV family | Fit for muta v1 |
|-------------|-----------------|
| Show Text / Choices | Message log + panel already exist |
| Control Switch / Self Switch / Variables | File-based keys |
| Conditional Branch / Loop / Label / Jump | `.pal` control flow |
| Transfer / Set Move Route (later) | Map switch ops already partial (`muta_map_io`) |
| Common Event | Shared `events/common/` |
| Battle / Party / Equipment | Defer until combat/party product |

User `#.wussup` direction: **use existing RMMV event vocabulary + scratch blocks first**, then refactor mutaclysm/AI to **consume** those scripts rather than invent a private language — then later import/export real RMMV.

---

## 3. UX specification

### 3.1 Preconditions

1. User is in **interact mode** (`interact_mode=1`), xlector visible on map.  
2. Cursor sits on a **target** (see §4 resolution order).  
3. User presses **Space**.

**Decided (2026-07-28):** Space opens the context menu **only in interact mode**, for **wherever the xlector currently is** (any map cell the cursor occupies — empty, entity, event, painted emoji, etc.). Outside interact mode: **Space is a no-op** (never auto-enter interact; never surprise pure hero play).

**Decided (2026-07-28, architecture):** Event editor is a **separate widget** (`&.widgits/event-editor/`), launchable **independently**. It edits packages on the house **desktop** (`#.desktop/`) including pets/charas; pickers place onto desktop; packages are **dropped/imported into mutaclysm**. Mutaclysm does **not** host the full editor as an in-process map view_mode.

### 3.2 Context menu chrome (ASCII + GL)

**ASCII (overlay panel, craft-style box over map):**

```text
┌─ XLECTOR ─────────────┐
│ cell (5,4) map_start  │
│ target: empty / hero  │
│ [>] 1. Event          │
│ [ ] 2. Copy           │
│ [ ] 3. Paste          │
│ [ ] 4. Delete         │
│ [ ] 5. Exit           │
└───────────────────────┘
```

**GL / CHTPM:** same rows as numbered buttons under an ACTIVATE “xlector context” (or dynamic `${xlector_ctx_items}`), still driven by `active_panel=xlector_ctx` + `panel_cursor`. **Interact window** = the existing mutaclysm map + chtpm shell, not a second process.

**Nav:**

| Input | Action |
|-------|--------|
| ↑ / ↓ | Move `[>]` |
| `1`–`5` | Jump / fire (pick one policy: immediate like outer action bar, **or** jump+Enter; **recommend immediate when menu open**, matching live digit dispatch) |
| Enter | Commit focused row |
| Esc / `5` Exit | Close menu, stay in interact mode |
| Space (toggle) | Close menu if open |

While menu open: arrows do **not** move xlector (same as craft panel freezes movement).

### 3.3 Option semantics

| # | Option | Behavior |
|---|--------|----------|
| 1 | **Event** | Resolve target → export/create package under `#.desktop/events/` → write **event-editor open request** (widget). Close context menu. User may already have event-editor open independently. |
| 2 | **Copy** | Snapshot target into **clipboard** (entity dir and/or event stub and/or tile glyph — see §4). Message log: “Copied …”. Close menu. |
| 3 | **Paste** | Apply clipboard at **current xlector cell** (create/overwrite rules). Fail loudly if empty clipboard. Close menu. |
| 4 | **Delete** | Remove **whatever occupies that cell under the xlector** — emoji / glyph / entity / event / furniture / item as resolved by target order (§4). Not “event-only.” Terrain base tile may clear to a default empty glyph when the cell’s content is the paint layer; exact wipe order = same as resolve stack (top content first). v1: hard-delete + message log; later optional confirm. |
| 5 | **Exit** | Close menu only. |

**These five rows are the default context methods**, not a closed API. Later we may append more methods (e.g. Paint, Possess, Examine, custom piece METHODs) the same way piece.pdl METHOD tables grow — menu is a **method list at the cursor**, with Event/Copy/Paste/Delete/Exit as the shipped defaults.

**Copy/Paste/Delete** are useful **without** full event editor; implementable in phase B before scratch UI.

### 3.4 Event editor = separate widget + house desktop

```text
  mutaclysm (Space → Event)          tile-picker / other pickers
           │                                    │
           │ package + open.request             │ place stamp
           v                                    v
                    #.desktop/   ← house tray
                 events/ entities/ tiles/ inbox/
                           │
                           │ open / edit / save
                           v
              &.widgits/event-editor/   (own process; run-widget later)
                           │
                           │ ee_import_to_world / drag-drop later
                           v
              mutaclysm world_01/<map>/events/…
```

**Independent launch:** event-editor can run with **no mutaclysm**. Edit pets/charas/events sitting on desktop; later drop into mutaclysm (or zoo).

**Widget UI (GL later, ops now):** same RMMV-ish fields (name, trigger, sprite, switches, script blocks / `.pal`). Scratch in GL; `.pal` in ASCII path.

**Ops shipped (scaffold):**

| Op | Role |
|----|------|
| `ee_resolve_desktop` | Find `#.desktop` / `XYZ_DESKTOP_ROOT` |
| `ee_package_init` | Create empty event package |
| `ee_open_request_write/read` | Muta → widget handoff |
| `ee_import_to_world` | Drop package into `world_01` |
| `ee_export_entity` | Entity tree → desktop |
| `tp_place_desktop` | Tile brush → `#.desktop/tiles/` |

**Harness:** `%.harnesses/event-editor+desktop/` (`./button.sh demo`)

---

## 4. Target resolution (what is under the xlector)

When Space or Event/Copy/Delete runs, resolve **one primary target**:

```text
1. Entity with state at (x,y) on current map_id
     (pet / egg / monster / item — first match by scan order; later: pick list if stack)
2. Else: event page bound to this cell (events/… see §6)
3. Else: empty cell → Event creates a new map event; Copy may copy terrain glyph;
         Paste can place clipboard entity/event; Delete may clear terrain if policy allows
```

**Hero:** if xlector on hero tile, context menu still valid; Event on hero is optional (hero script vs map event) — **v1 recommendation:** treat hero as entity with `piece.pdl` methods only; **map events** are separate objects under `events/` so we do not overload `hero/`.

**Possession vs maker:** context menu is a **maker tool** while interact_mode is on. Possession can remain; menu does not require unpossess. If that is confusing later, gate menu when `possessed_id != none` — **default: allow**.

---

## 5. Dual representation: scratch blocks ↔ `.pal`

### 5.1 Goal

One event graph, two authoring UIs:

| View | Audience | Feel |
|------|----------|------|
| Scratch / blocks (GL) | makers, RMMV users | drag/add buttons, nested if/then |
| `.pal` (ASCII + raw edit) | power users, agents, debugging | linear assembly of the same flow |

`.pal` is **not** a different language invented ad hoc — it is the **lowered IR** that prisc/ops can already understand (or a restricted dialect that `event_run` interprets).

### 5.2 Proposed IR (file: `event.ir.pdl` or JSON-lines — prefer house PDL)

Example (conceptual):

```text
SECTION      | KEY                | VALUE
META         | piece_id           | ev_map_start_5_4
STATE        | name                 | door_guard
STATE        | trigger              | action
STATE        | sprite               | 🎨️
STATE        | self_A               | 0
# nodes listed in execution order; children via indent id
NODE         | id=1 type=show_text    | text=Halt!
NODE         | id=2 type=if_switch    | switch=door_open op=eq value=0
NODE         | id=3 type=set_switch   | switch=door_open value=1 parent=2 branch=then
```

### 5.3 Lowered `.pal` (file: `event.pal`)

Illustrative dialect (final opcodes depend on `event_run` op, not main_loop):

```text
# event.pal — lowered from event.ir; hand-edit allowed
# maps 1:1 to blocks when well-formed
show_text "Halt!"
beq_switch door_open, 0, .then_open
j .end
.then_open:
set_switch door_open, 1
.end:
ret
```

**Rules:**

1. **Canonical save:** write **both** `event.ir.*` and `event.pal` when possible.  
2. **Raw pal edit:** if user saves pal that fails raise → keep pal, mark `ir_stale=1`, show ASCII warning; runtime prefers pal via `event_run` interpreter.  
3. **Block edit:** always re-lower pal so agents/ASCII stay honest.  
4. **Agent/CI:** harnesses assert IR or pal text, not pixels.

### 5.4 Scratch block vocabulary (v1 palette)

Ship a **small** palette that maps cleanly to ops/files:

| Block | Lowers to | Runtime |
|-------|-----------|---------|
| Show Text | `show_text` | append `message_log.txt` + optional modal later |
| Show Choices | `show_choices` | panel / wait for digit (harder — phase B) |
| Control Switch | `set_switch` | write `pieces/world_01/switches.pdl` |
| Control Self Switch | `set_self` | event `state.txt` |
| Conditional Branch (switch) | `beq_switch` | flow in interpreter |
| Comment | `#` | no-op |
| Call Op | `call_op path args` | `system()` or prisc invoke of existing `.+x` |
| Transfer Player | `switch_map` | reuse `muta_map_io` / move hero |

Defer full RMMV list; grow from the external catalog.

---

## 6. On-disk layout (events ride SAVE/LOAD)

### 6.1 Map-bound events (primary)

```text
pieces/world_01/<map_id>/
  events/
    ev_<x>_<y>/                 # or stable id ev_0001 + pos in state
      piece.pdl                 # META + optional METHODs
      state.txt                 # pos_x, pos_y, name, trigger, sprite, self_A..
      event.ir.pdl              # structured graph
      event.pal                 # lowered / hand-editable
      (optional) page_2/ …      # multi-page later
  switches.pdl                  # per-map or promote to world_01/switches.pdl
```

**World-global switches** (RMMV “game progression”):

```text
pieces/world_01/switches.pdl
pieces/world_01/variables.pdl
```

Because `muta_world_io` / `save_game` copy whole `world_01/`, **events and switches save/load for free**.

### 6.2 Entity-bound scripts (optional second attach point)

Pets already have `piece.pdl` METHOD → ops. Longer term:

```text
METHOD | on_interact | ops/+x/event_run.+x
# event_run reads this entity's event.pal
```

Or `METHOD | on_turn | …` for AI — aligns with architecture doc’s FSM layer and user “ops for entities”.

**v1:** focus **map events** under `events/`; entity attach is phase C once `event_run` exists.

### 6.3 Clipboard (session, not necessarily saved)

```text
pieces/system/xlector_clipboard/
  meta.pdl          # kind=entity|event|tile, source map/x/y
  payload/          # copied tree (event dir or entity dir or tile glyph file)
```

Session-local is fine; survives until quit. Optional: include clipboard in save (usually no).

---

## 7. Runtime — how events “run ops”

### 7.1 Trigger points (hook into existing loop)

| Trigger | When to fire | Hook candidate |
|---------|--------------|----------------|
| `action` | Player/xlector “activate” on cell (Enter examine path, or dedicated key later) | extend `examine_at` / new `event_try_trigger` after Enter in interact |
| `player_touch` | Hero steps onto cell | end of `move_player` when hero moves (not xlector-only) |
| `autorun` | Map load / every frame while conditions true | `game_dispatch` / post-compose (careful: idle spam pitfall 48) |
| `parallel` | background | deferred |

**v1 recommendation:** implement **`action` only** (Enter on event tile runs page), plus **editor test-run** method. Touch/autorun next.

### 7.2 Interpreter

New op: **`ops/event_run.c` → `event_run.+x`**

```text
event_run.+x <event_dir> [--step N]
```

- Reads `event.pal` (or IR if pal missing).  
- Executes until wait (choices/text) or end.  
- Side effects only via **file writes + existing ops** (no secret C game state).  
- Logging: append master-style line to `message_log.txt` / optional ledger.

**Call Op block** is how events reuse pickup/map_io/place_tile without reimplementing:

```text
call_op ops/+x/muta_place_tile.+x map_start 5 4 '#'
```

### 7.3 Relation to main_loop.pal

`pal/main_loop.pal` stays the **engine tick**. Event pages are **data**, interpreted by `event_run`, not spliced into main_loop. That keeps projects portable and saveable.

---

## 8. ASCII + GL “within interact window” — implementation mapping

| Layer | Work |
|-------|------|
| State | `active_panel=xlector_ctx` in muta; packages + request under `#.desktop/` |
| Input | `choice.c`: Space → ctx; Event → desktop + open request |
| ASCII draw | `compose_frame.c`: draw XLECTOR menu overlay |
| Widget | `&.widgits/event-editor/` separate process (GL run-widget later) |
| Desktop | `#.desktop/{events,entities,tiles,inbox}` |
| Drop | `ee_import_to_world` → `world_01/<map>/events/` |

**GL layout + nav (locked design probe):**

- **RMMV chrome:** left Event props + right Contents + footer buttons  
- **CHTPM nav even in GL:** `[>]` focus, numbered rows, `KEY:n` digits, Enter commit, Esc BACK, `Nav>_` / `Active>_` status  
- **Commands | Scratch toggle** (Tab or method 5) — same dual-source graph  
- **REAL:** `&.widgits/event-editor/` — CHTPM layout + `ee_compose_frame` + rgb + `gl_mirror`
  - Continuous nav 1..34 (parser-owned; multi-digit OK)
  - Commands|Scratch toggle via KEY:5
  - `./button.sh run-widget`
- Design probe only: `gl_mock/ee_gl_mock` (freeglut; numbers fixed to continuous model)

**Scratch fidelity levels:**

1. **L0:** Numbered command list (RMMV Contents) — mock done  
2. **L1:** Scratch block rows + toggle — mock done  
3. **L2:** Freeform node graph (later)

Do not block product on L2.

---

## 9. Phased build plan (suggested)

### Phase A — Context menu shell (small, harnessable)

- [ ] `active_panel=xlector_ctx`  
- [ ] Space open/close in `interact_mode`  
- [ ] Nav 1–5 / arrows / Enter / Esc  
- [ ] Draw ASCII overlay + chtpm items  
- [ ] Exit / stub handlers for Event/Copy/Paste/Delete (log only)  
- [ ] Harness: inject keys → `active_panel` + cursor assertions  

### Phase B — Clipboard + Delete

- [ ] Resolve target under xlector (stack: entity → event → painted glyph/emoji → …)  
- [ ] Copy entity/event/glyph payload to clipboard  
- [ ] Paste at cursor (collision rules)  
- [ ] **Delete = clear content at that space** (emoji/glyph and/or entity/event as resolved — not event-only)  
- [ ] Harness: paint or place → Delete → cell empty; copy → move → paste → files/glyph exist  
- [ ] Menu rows remain **defaults**; leave hook for extra methods later

### Phase C — Event data + desktop + SAVE/LOAD proof

- [x] `#.desktop/` tray + event-editor ops scaffold  
- [x] Event menu → desktop package + open request  
- [x] `ee_import_to_world` into `world_01/.../events/`  
- [x] Harness `%.harnesses/event-editor+desktop/`  
- [ ] GL run-widget chrome for event-editor  
- [ ] Prove user save slot includes imported events  
- [ ] Desktop entity edit (pets/charas) end-to-end

### Phase D — event_run + action trigger

- [ ] `event_run.+x` minimal: show_text, set_switch, ret  
- [ ] Enter on event tile runs page  
- [ ] Wire Call Op to one existing op  

### Phase E — Scratch blocks (GL L1) + pal raw edit

- [ ] Block palette → IR → pal lower  
- [ ] ASCII raw pal edit path  
- [ ] Round-trip harness: block add → pal contains opcode  

### Phase F — RMMV breadth + multi-page + touch/autorun

- [ ] Grow command set from rmmv-events-explained  
- [ ] Multi-page conditions  
- [ ] Import/export research (later)

**Do not start Phase E/F until A–D green.** That matches the house pattern: cmd/file truth first, chrome second.

---

## 10. Key conflict matrix (update ctrl-legend when implementing)

| Key | Today | With this feature |
|-----|-------|-------------------|
| Space | free | Open/close **xlector_ctx** **only if interact_mode** (at current xlector cell) |
| 1–5 | POV (3D) / methods | **Only if panel open:** default menu rows (extensible method list later); else unchanged |
| Esc | close panel / exit interact | close ctx → exit editor → exit interact (layered) |
| Enter | examine / possess / panel commit | ctx commit / editor field activate |
| i | (legacy interact toggle notes) | keep; menu does not replace interact entry |

Layered Esc (recommended):

```text
if event_editor open → close editor (to map+interact)
else if xlector_ctx open → close menu
else if other panel → close panel
else if interact_mode → exit interact
```

---

## 11. Testing strategy

| Level | What |
|-------|------|
| Unit harness | key inject → state fields (panel, view_mode, clipboard meta) |
| File harness | events/ tree after Event create; pal after block lower |
| Save harness | save slot contains `events/` + switches |
| Runtime harness | set switch via event_run; message_log line |
| Live human | Space menu visible ASCII + GL; Event opens editor; Esc back |

Avoid long `sleep` / blanket `pkill` (ARCHI_TEST_SUM cleanup rules).

---

## 12. Open questions (product)

| ID | Question | Status |
|----|----------|--------|
| Q1 | Space only in interact_mode? | **DECIDED: Yes.** Only when `interact_mode=1`; applies at current xlector cell, wherever it is. |
| Q2 | Multi-entity stack on one cell? | First match; later pick list (or multi-delete) |
| Q3 | What does Delete remove? | **DECIDED:** content **at that space** (emoji / glyph / entity / event / etc. under xlector) — not “event-only.” Defaults menu may grow more methods later. |
| Q4 | Event on hero tile? | Map event at cell **or** skip; don’t overwrite hero/ |
| Q5 | Auto-save editor on Esc? | **Yes** flush files |
| Q6 | Scratch L2 free graph needed for v1? | **No** — L0/L1 |
| Q7 | Interpreter = subset of prisc pal or custom event_run? | **Custom event_run** first; optional prisc later |
| Q8 | Widget vs in-process editor? | **DECIDED: separate widget** `&.widgits/event-editor/`; desktop tray; drop into muta |
| Q9 | Relation to tile-picker? | **Desktop-first:** pickers place onto `#.desktop/`; optional muta PLACE_TILE; Event is logic packages |
| Q10 | Common events shared across maps? | `world_01/events_common/` later |
| Q11 | Are Event/Copy/Paste/Delete/Exit the only methods forever? | **DECIDED: No.** They are **defaults**; more context methods may be added later. |

---

## 13. Non-goals (this doc)

- Full RMMV plugin API / JS  
- Real MV project import on day one  
- Second terminal for the editor  
- Replacing mutaclysm combat with event-only  
- Auto-wiring every entity AI to scratch before event_run exists  
- Perfect pixel scratch like commercial Scratch IDE  

---

## 14. Suggested first code touchpoints (when coding starts)

| File | Change |
|------|--------|
| `ops/choice.c` | Space → `xlector_ctx`; Event/Copy/Paste/Delete |
| `ops/move_player.c` | Freeze move when `xlector_ctx` |
| `ops/compose_frame.c` | Draw XLECTOR menu |
| `&.widgits/event-editor/ops/*` | desktop packages + import/export |
| `&.widgits/tile-picker/ops/tp_place_desktop.c` | stamps → desktop |
| `#.desktop/` | house tray |
| `%.harnesses/event-editor+desktop/` | multi-project proof |
| `ops/event_run.c` (later) | interpreter |
| `dox/ctrl-legend.md` | Space + layers |

---

## 15. One-page architecture diagram

```text
                    ┌─────────────────────┐
   Space            │  interact_mode=1    │
 ─────────────────► │  xlector @ (x,y)    │
                    └──────────┬──────────┘
                               │ active_panel=xlector_ctx
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
           Event            Copy/Paste       Delete/Exit
              │             clipboard/         files
              ▼
     Event → #.desktop/events/ + open.request
              │
              v
     &.widgits/event-editor (widget; independent launch OK)
         GL blocks / ASCII .pal  ◄──► package on desktop
              │
              │ ee_import_to_world / drop
              v
   world_01/<map>/events/ev_…/   ──SAVE/LOAD──► user slot world_01/
              │
              │ action trigger (Enter) later
              ▼
         event_run.+x ──► message_log / switches / call_op → existing .+x
```

---

## 16. Status stamp

| Item | State |
|------|--------|
| Investigation of live xlector / panels / save | **Done** (this doc) |
| Context menu product code | **Not started** |
| Event editor / scratch / event_run | **Not started** |
| RMMV ref available on disk | **Yes** (`#.rmmv-events-explained`) |
| User direction | Context menu → Event editor; dual pal/blocks; project save; entity ops |
| Q1 Space | **Locked:** interact mode only; wherever xlector is |
| Q3 Delete | **Locked:** delete content at cell (emoji/w/e); not event-only |
| Q11 Menu | **Locked:** Event/Copy/Paste/Delete/Exit are **defaults**; more methods later |

**Implemented (2026-07-28):** Phase A menu + desktop/widget scaffold + harness
`%.harnesses/event-editor+desktop/` **ALL PASS**. Next: GL run-widget for
event-editor, richer Delete stack, entity desktop edit, event_run.

---

*End xelector-context.md*
