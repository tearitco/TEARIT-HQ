# pc-hq — entity context menu, entities taskbar, real dropdowns

**Status: BRAINSTORM / DESIGN — nothing built.** Written 2026-09-10 from
a direct brainstorm request. Read `PIECECRAFT_XYZ_DESIGN.md` §3a (the
xelector-context-menu precedent this supersedes in ambition) and
`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md`
first.

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

## 3. Entities taskbar on the pc-hq board (opt-in)

Mirror the livedesk **bottom** strip: one cell per world entity
(chicken, tree-cluster, hero, xelector…), click → focus/inspect,
right-click → the §2 menu.

- **Reuse:** `khtpm_taskbar_manager*.c` already builds a bottom
  entity-strip from a live PID/entity set and renders it via
  `khtpm_core_render` dock mode (`class="dock-bottom"`). The pc-hq board
  window can host a **second dock-bottom strip** the same way the
  desktop does — the manager already separates "which entity set" from
  "how to draw a strip." Feed it pc-hq's world-entity list instead of
  the livedesk desk's.
- **Default OFF.** `pieces/system/pchq.pdl`:
  `OPT | entities_taskbar | 0`. A `Menu ▸ View ▸ Entities bar` toggle
  (see §4) flips it. Ship it false; let the user turn it on.
- **Open question:** the board window is one X window; a child
  dock-bottom strip is a second window glued to its bottom edge (the
  desktop strip is its own top-level). Simplest first cut: the strip is
  a row *inside* the board window's own Elem tree (a `<row
  class="entities-bar" show="${entities_bar_on}">` of `<repeat>`
  cells) — no second process, projector publishes the cell list. Only
  promote to a real separate strip window if the in-window row proves
  too cramped.

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

**The gap:** that primitive was wired for **default / sidebar+panel**
mode. The board window renders in the **`has_canvas` flat-toolbar**
layout, which the dropdown scope logic may not cover. Two options,
pick one (decision needed):

1. **Extend the `ACTIVATE`/dropdown-child scope handling to the
   `has_canvas` layout** — small, generic, benefits any future
   canvas+toolbar app. Preferred.
2. Give the board window a real sidebar/panel chrome frame around the
   canvas so it's a default-mode window that happens to contain a
   `<canvas>` — bigger change, but then *everything* generic (dropdowns,
   chrome, taskbar entry, minimize) works with no special-casing.

Either way the toolbar becomes:

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
| dropdown menu | `ACTIVATE` + `dropdown-child` + `target_id` (default mode) | **refactor**: extend scope handling to `has_canvas` (§4.1) |
| bottom entity strip | `khtpm_taskbar_manager` dock-bottom + `khtpm_core_render` dock mode | **reuse** the draw path; feed pc-hq's entity list (§3) |
| toolbar-item click routing | generic `action=` → shell verb, `onclick=` verbs | already used by pchq-board.xhtpm — keep |
| entity record | livedesk = package dir; pc-hq = manifest row | **refactor**: a descriptor-dir shim so the menu doesn't care which |
| click → world cell | per-frame raymarch first-hit | **new, tiny**: publish `pick.txt`; selector + `7.edit` reuse it |

Nothing in this list is a from-scratch build. The two real refactors
are: **(a)** the descriptor-dir extraction so the entity menu is
world-agnostic, **(b)** dropdown scope in `has_canvas`.

---

## 6. Rollout (all flags default false)

`pieces/system/pchq.pdl` (new, or fold into existing config):

```
OPT | ctx_menu_enabled   | 1        # right-click / Shift → entity menu
KEY | ctx_menu           | 16       # Shift; changeable
OPT | entities_taskbar   | 0        # bottom entity strip (ship OFF)
```

Order:
1. `pick.txt` per-frame cell pick (unlocks everything else).
2. Descriptor-dir refactor + `pc_menu_input.c` right-click → launch the
   shared entity menu → apply `action.txt` to world files. Delete a
   voxel / a chicken end-to-end = the proof.
3. `has_canvas` dropdown scope; convert `Desk` + `Menu` to real
   dropdowns; `Menu ▸ View ▸ Entities bar` toggle.
4. In-window entities bar row, projector-fed, default off.
5. (Later) copy/cut/paste buffer shared with `7.edit`.

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
  the house's worst leak source; a second strip process (§3) must
  register in `livedesk_proc_list.txt` or it becomes another `mon-hq`
  BAD row.

## 8. Decisions needed from the user

1. Verb list for the context menu (§2 draft table) — confirm / edit.
2. Dropdown approach (§4): extend `has_canvas` scope (rec.) vs reframe
   the board as a default-mode window.
3. Entities bar (§3): in-window row first (rec.) vs real second strip
   window.
4. Default trigger key for the menu (`Shift` = `16` assumed).
