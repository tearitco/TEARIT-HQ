# pc-hq — entity context menu, entities taskbar, real dropdowns

**Status: DESIGN — decisions resolved 2026-09-10, nothing built yet.**
Written from a direct brainstorm request. Read `PIECECRAFT_XYZ_DESIGN.md`
§3a (the xelector-context-menu precedent this supersedes in ambition) and
`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md`
first.

**Resolved decisions (2026-09-10):**
1. Context-menu verb list — the §2 draft is accepted as a starting
   point; it lives in `pieces/system/keybinds.pdl` / a menu descriptor
   so it can be refined later without a rebuild.
2. **The board window is reframed as a normal sidebar+panel HQ window**
   with the 2D/3D view as a `<canvas>` inside the panel (was "option
   2.2"). Chrome, nav, dropdowns, minimize, the taskbar entry, and the
   footer entities bar then all come from the generic path with no
   board-specific code. This is its own milestone (A) — the board's
   existing fixes (fullscreen, drag zones, `managed` mode, `!`/`_`
   chrome, canvas scaling) get a full regression pass.
3. **The entities bar is an in-window bottom region**, not a second
   strip process. With decision 2 it is just a generic bottom-dock
   region in the same Elem tree — no proc-ledger entry, no edge-glue,
   no leak risk. Needs one generic addition: a bottom/footer dock
   region in `layout_sidebar_panel` (milestone B).
4. Trigger: right-click (`Button3`) + `KEY | ctx_menu | 16` (Shift),
   pdl-changeable.

---

## 0. Fundamental design direction (future agents MUST honor this)

> **Refactor and reuse. Never rewrite.**
>
> When pc-hq needs a capability the desktop (livedesk) already has — a
> context menu, a taskbar cell, a dropdown, click routing, an entity
> record — the answer is **always** one of:
>   1. call the existing binary/mode as-is (fork+exec + file IPC), or
>   2. **refactor** the existing code so pc-hq and the desktop share the
>      *same* implementation, then call that.
>
> It is **never** "write a pc-hq copy of it." A parallel second
> implementation of a menu/taskbar/dropdown is a defect on sight,
> regardless of how small it looks or how quickly it can be typed. If
> sharing needs a refactor first, do the refactor — that is the task,
> not a detour from it. The pc-hq board's current hand-built File/Desk
> "dropdown" (§4) is exactly the anti-pattern this rule exists to stop
> from spreading.
>
> This mirrors the house-wide `khtpm-house-standards` rule ("no new
> per-project C in the shared renderer; route through the generic
> vocabulary") and extends it: reuse isn't just for the renderer file,
> it's for every desktop primitive pc-hq is tempted to re-grow.

---

## 1. Are player / chicken / trees "entities" yet? — No (mostly)

| thing | how pc-hq stores it today | entity-like? |
|---|---|---|
| **hero** | `pieces/hero_01/` dir with `state.txt` + `piece.pdl` (`possessable`, `de_possessible`, hp, pos, owner_id) | **semi** — has its own dir + pdl, but nothing renders a menu for it |
| **xelector** (cursor) | `pieces/xelector_01/state.txt` | semi — own dir, no menu |
| **chicken / animals** | rows in `pieces/world_01/animals.txt` + `phymoji_entities.txt` (pos + template id) | **no** — a line in a manifest, not a dir |
| **trees** | phymoji template instances, same manifest files, or baked into the chunk | **no** |
| **base voxels / tiles** | `board3d[z][y][x]` cells from `load_voxel_chunk` (a CSV chunk) | **no** — pure grid data |

**Contrast with the desktop.** On livedesk a tile/sprite *is* a real
file-backed entity (its own package dir; the taskbar-manager spawns one
window/PID per entity from the desk PDL; right-click → `khtpm_core_render`
in entity-menu mode acts on that dir). RMMV's rule ("a tile isn't an
entity until an event is on it") is the same idea from the other side —
plmain-ness is the default, entity-ness is opt-in.

**pc-hq wants the desktop's treatment for a different reason:** so the
player can *select / delete / copy / cut / paste* world content (later:
a rectangle mouse selector in `7.edit`, house-wide — see §7). To get
there, the menu comes first; full entity-dir promotion can be lazy
(only when something acts on a cell).

### 1a. Minimum shape to make a cell "menu-able"

The menu doesn't need a real entity dir. It needs a **descriptor** the
entity-menu renderer can read. Proposal: on right-click / Shift, pc-hq
(`pc_menu_input.c` / a small new op) writes a transient package dir:

```
#.desktop/pchq_ctx_<pid>/
  entity.txt        kind=chicken | tree | voxel | hero | xelector
                    world=world_01  x=..  y=..  z=..
                    template=phymoji:chicken_01   (if any)
                    label="Chicken"
  menu.chtpm        the row set (see §2) — OR let the renderer synthesise
                    it from entity.txt kind=, same as the desktop does
```

then `fork+exec khtpm_core_render.+x <pchq_ctx_dir> <house_root>` in
entity-menu mode, positioned at the cursor. Its action rows write back
to `#.desktop/pchq_ctx_<pid>/action.txt` (`seq=/cmd=`), which
`pc_menu_input.c` polls and applies to the real world files
(`animals.txt`, the chunk CSV, `hero_01/`…).

**Refactor needed:** the desktop's entity-menu launch path
(`tp_desktop_window*.c`) hard-codes reading a livedesk entity dir. Pull
the "given a descriptor dir, show a menu, emit actions" core out so both
the desktop and pc-hq call the *same* launch helper with different
descriptor dirs. That extraction **is** the first work item.

---

## 2. The context menu itself — reuse the desktop's, don't fork it

- **Renderer:** `khtpm_core_render.+x` already has an entity-menu mode
  (the file's original identity is `khtpm_entity_menu_render.c`). Same
  binary, same `<package_dir> <house_root>` argv. Nothing new to build
  on the render side.
- **Row vocabulary:** keep the house's fixed-numbered convention
  (`xelector-context.md`: `1 Event · 2 Copy · 3 Paste · 4 Delete · 5
  Exit`). pc-hq's world verbs slot into the same numbering — **needs a
  decision on the verb list**, draft:

  | row | voxel / tile | chicken / tree | hero |
  |---|---|---|---|
  | 1 | Inspect | Inspect | Possess / Inspect |
  | 2 | Copy | Copy | — |
  | 3 | Paste | Paste | — |
  | 4 | Delete | Delete | Send home |
  | 5 | Mine / Place… | Convert to entity | — |
  | 6 | Exit | Exit | Exit |

- **Trigger:** right-click (`Button3`) on the canvas, hit-tested to a
  world cell via the existing raycast/`xelector` projection; **or** a
  key (default `Shift`, `KEY | ctx_menu | 16` in
  `pieces/system/keybinds.pdl`, changeable). Both routes end at the same
  "write descriptor + launch" helper.
- **Hit-testing a click to a cell:** the GPU/CPU raymarcher already
  computes a first-hit cell per ray for the whole frame; expose the
  centre-ray (or click-ray) hit as `pieces/display/pick.txt`
  (`x y z face kind`) each frame — cheap, and the selector/`7.edit`
  rectangle tool needs the exact same primitive later.

---

## 3. Entities bar along the bottom of the pc-hq window (opt-in)

Mirror the livedesk bottom strip: one cell per world entity (chicken,
tree-cluster, hero, xelector…), click → focus/inspect, right-click →
the §2 menu — **inside** the pc-hq window's own bottom edge, not a
separate process.

**Resolved (decision 3):** once the board is a normal sidebar+panel
window (decision 2 / milestone A), the entities bar is simply a
**bottom-dock region in the same Elem tree**, laid out by the generic
engine, published by the projector. No second process → nothing to
spawn, leak, glue to a window edge, or register in
`livedesk_proc_list.txt`. It moves/resizes/minimizes with the window
because it *is* the window.

**The one generic addition (milestone B):** `layout_sidebar_panel`
today lays out `<tabbar>` (top) + `<sidebar>` (west) + `<panel>`
(center). It has **no bottom region**. Add a generic bottom-dock zone —
`<footer>` tag (or `<panel dock="bottom">`) — reserved height along the
window's bottom edge, laid out after sidebar/panel. House-wide win: any
HQ window can then carry a status/footer bar.

**Cell rendering is reused, not re-grown.** The `<footer>` contains a
`<repeat>` of dock-style cells using the SAME cell markup/CSS the
livedesk strip's cells use (`class="dock-cell"` …). If that rendering
isn't cleanly separable from `khtpm_taskbar_manager`, extracting it is
part of milestone C — never a hand-rolled parallel row (§0).

**Default OFF.** `pieces/system/pchq.pdl`:
`OPT | entities_bar | 0`. A real `Menu ▸ View ▸ Entities bar` dropdown
item (see §4) flips it; the projector emits `entities_bar_on` and the
`<footer show="${entities_bar_on}">` appears/hides. Ship it false.

---

## 4. Real dropdowns (fix "6.Desk", and Menu)

**Today:** the board toolbar's `Desk` "dropdown" is fake — the projector
flips `desk_menu_open`, and `<item show="${desk_menu_open}">` rows
appear inline in the toolbar area. `Menu` / `Player` are `action="void"`
stubs. This is hand-built, mode-specific, predates the gold standard —
the exact thing §0 forbids growing further.

**The primitive already exists.** `khtpm_core_render.c` (~line 1633,
comment dated 2026-09-03, written *specifically* because "piececraft-hq's
own File/Desk dropdown is hand-built, mode-specific C"): any default-mode
app gets a real dropdown for free with

```
onclick="ACTIVATE"  class="dropdown-child"  target_id="<trigger id>"
```

one active "open" trigger at a time, nav-scoped, Esc-to-close — zero new
C.

**The gap, and the resolution (decision 2).** That primitive was wired
for **default / sidebar+panel** mode; the board today renders in the
`has_canvas` flat-toolbar layout, which the scope logic doesn't cover.
Rather than extend the `has_canvas` special case, **the board is
reframed as a normal sidebar+panel window with the view as a `<canvas>`
in the panel** (milestone A). Then dropdowns "just work" with zero
board-specific code — along with chrome, minimize, nav, the taskbar
entry and the §3 footer. `has_canvas` stays for genuinely chrome-less
fullscreen canvases; the board is not one of those.

The toolbar becomes:

```
<item id="tb-desk" onclick="ACTIVATE" target_id="tb-desk">Desk ▾</item>
<panel class="dropdown-child" target_id="tb-desk">
  <repeat count="${n_desk_opts}" bind="d">
    <item ... action="pchq_board_action.sh ... desk ${d.#}"/>
  </repeat>
</panel>
```

and `Menu` becomes a real dropdown (`View ▸ Entities bar`, later
`Db`, `Settings`…) instead of a stub.

---

## 5. Shared-primitive inventory (what to reuse vs refactor)

| primitive | exists as | reuse path for pc-hq |
|---|---|---|
| entity context menu | `khtpm_core_render` entity-menu mode | **call as-is** once §1a descriptor-dir refactor lands |
| dropdown menu | `ACTIVATE` + `dropdown-child` + `target_id` (default mode) | **free** once the board is a default-mode window (milestone A) |
| board window frame | `layout_sidebar_panel` (chrome/nav/minimize/taskbar entry) | **reframe** the board onto it; `<canvas>` in the `<panel>` (milestone A) |
| bottom-dock / footer region | *does not exist* in `layout_sidebar_panel` | **new, generic**: `<footer>` zone, house-wide (milestone B) |
| entity-cell rendering | livedesk strip's `dock-cell` markup/CSS | **reuse** in the `<footer>` `<repeat>`; extract from `khtpm_taskbar_manager` if needed (milestone C) |
| toolbar-item click routing | generic `action=` → shell verb, `onclick=` verbs | already used by pchq-board.xhtpm — keep |
| entity record | livedesk = package dir; pc-hq = manifest row | **refactor**: a descriptor-dir shim so the menu doesn't care which |
| click → world cell | per-frame raymarch first-hit | **new, tiny**: publish `pick.txt`; selector + `7.edit` reuse it |

The real work: **(A)** reframe the board as a default-mode window,
**(B)** add a generic `<footer>` dock region, **(C)** the descriptor-dir
extraction so the entity menu is world-agnostic + reuse the dock-cell
rendering. Everything else falls out.

---

## 6. Milestones (all pc-hq flags ship false)

`pieces/system/pchq.pdl` (new, or fold into existing config):

```
OPT | ctx_menu_enabled   | 1        # right-click / Shift → entity menu
KEY | ctx_menu           | 16       # Shift; changeable
OPT | entities_bar       | 0        # bottom entities bar (ship OFF)
```

| # | milestone | delivers / unblocks |
|---|---|---|
| **A** | **Reframe the board** onto `layout_sidebar_panel`: `<sidebar>` (thin/collapsible) + `<panel>` holding `<canvas id="view">`. Full regression pass on fullscreen, drag zones, `managed` mode, `!`/`_` chrome, and canvas scaling inside a panel (the real unknown). | generic chrome, nav, minimize, taskbar entry; **real `Desk`/`Menu` dropdowns for free** (drop the fake `show=` rows) |
| **B** | **Generic `<footer>` dock region** in `layout_sidebar_panel` — reserved bottom-edge height, laid out after sidebar/panel, `show=`-able. | any HQ window can carry a status/footer bar |
| **C** | **Entities bar**: projector emits the entity-cell list + `entities_bar_on`; `<footer show="${entities_bar_on}">` of `<repeat>` **`dock-cell`** cells (extract that rendering from `khtpm_taskbar_manager` if it isn't cleanly reusable). `Menu ▸ View ▸ Entities bar` toggles it. | familiar bottom entity bar, in-window, no new process |
| **D** | **`pick.txt`** per-frame cell pick (raymarch first-hit → `pieces/display/pick.txt`: `x y z face kind`). | click→cell for the menu, the selector, and `7.edit` |
| **E** | **Descriptor-dir refactor**: pull the entity-menu launch out of the livedesk-specific path so it takes a `kind=`/`x y z`/`template=` descriptor dir. `pc_menu_input.c` right-click / `Shift` → write descriptor → `fork+exec khtpm_core_render.+x` entity-menu mode → poll `action.txt` → apply to world files (`animals.txt`, chunk CSV, `hero_01/`). **Proof: delete a voxel and a chicken end-to-end.** | the actual feature |
| **F** | *(later)* copy/cut/paste buffer shared with `7.edit`'s rectangle selector (§7). | — |

A and D are independent and can run in parallel; B needs A; C needs B;
E needs D (and benefits from A for positioning the popup).

---

## 7. Related / future

- **`7.edit` rectangle mouse selector** (house-wide, not pc-hq-only):
  drag a rectangle, select/copy/cut/paste tiles+entities. pc-hq's
  `pick.txt` + descriptor-dir + the shared clipboard are the same
  primitives; design that alongside so they aren't built twice. Track
  in `11.brainstorm/`.
- `PIECECRAFT_XYZ_DESIGN.md` §3a — the earlier "port the pattern, own
  copy" stance for the xelector menu. **This doc supersedes it**: the
  direction is now maximum literal reuse (§0), pattern-copy only where a
  genuine refactor is impossible.
- `02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md` — the sidebar/panel,
  dropdown, `<module>` mechanics.
- `03-pitfalls/00-INDEX.md` → CPU safety — the board engine is already
  the house's worst leak source. Decision 3 (in-window footer, no second
  process) keeps it that way; if milestone C ever needs a helper
  process, it must register in `livedesk_proc_list.txt` or it becomes
  another `mon-hq` BAD row.

## 8. Decisions — RESOLVED 2026-09-10

1. **Verb list** — §2 draft accepted as a start; pdl/descriptor-driven,
   refine later without a rebuild.
2. **Board window** — reframed onto `layout_sidebar_panel` with the view
   as an in-panel `<canvas>` (milestone A). Not the `has_canvas`
   special case.
3. **Entities bar** — in-window bottom `<footer>` region (milestones
   B+C), no second process.
4. **Trigger** — right-click + `KEY | ctx_menu | 16` (Shift), changeable.

Still open (implementation-time, not blocking): the canvas-scaling-in-a-
panel unknown in milestone A; whether `dock-cell` rendering is cleanly
extractable in C.
