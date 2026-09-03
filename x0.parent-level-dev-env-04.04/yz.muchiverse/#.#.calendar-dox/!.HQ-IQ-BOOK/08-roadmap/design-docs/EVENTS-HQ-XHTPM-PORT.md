# events-hq → static xhtpm + projector (no `evhq_*` layout C)

**Status:** design for review / implementation  
**Date:** 2026-09-03  
**Branch:** `chtpm-var-substitution` (do not land this on `main` until the parallel window is signed off)  
**Parent plan:** `DB-EVENTS-HQ-PORT-DESIGN.md` (this doc is the events-hq slice only)  
**Audience:** an agent that did not sit through the strip/db-hq-pal session

Read this whole file before editing `khtpm_core_render.c` or `khtpm_events_hq_manager.c`.

---

## 0. One-paragraph goal

Stop expressing the events-hq **window** as C (`g_is_events_hq` / `evhq_*` in `khtpm_core_render.c`). The window becomes a **static** `events-hq.xhtpm` + a projector that writes `key=value` state. The **compile chain stays** in `khtpm_events_hq_manager.c` (`event.ir.pdl → event.pal → cmd_N.sh`). Old `class="events-hq-window"` launcher stays until the parallel window dumps match. Then retarget `button.sh` and delete `evhq_*`.

This is the **standalone event editor**. db-hq’s Common Events pane is the **same editor** reused later — do not invent a second one. Convert once.

---

## 1. What is live today (do not guess past this)

| piece | path / fact |
|---|---|
| Launcher | `44.xyz.01.00/&.widgits/events-hq/button.sh` |
| Binary | `*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x` |
| Layout | `&.widgits/events-hq/pieces/dashboard.chtpm` — `<window class="events-hq-window">` **trips `g_is_events_hq`** |
| CSS | `&.widgits/events-hq/pieces/dashboard.css` |
| Picker layout (C-filled) | `&.widgits/events-hq/pieces/picker.chtpm` |
| Manager | `&.widgits/events-hq/ops/khtpm_events_hq_manager.c` → `ops/+x/khtpm_events_hq_manager.+x` |
| argv | `"$BIN" "$HOUSE" "$CHTPM" "$PKG_DIR" "$LABEL"` — `PKG_DIR` is the entity’s `event_pkg/` |
| Multi-instance | **required**. One process pair per `PKG_DIR`. `same_entity_pids()` kills only the same entity. Never `pkill` the binary name. |
| Manager args | `house_root`, `pkg_dir`, `entity_label` (same 3 as the old shell) |
| Manager state (all **inside pkg_dir**) | `.hq_manager/pages.state.txt`, `selected_page.txt` (shell writes), `page.state.txt` (trigger + commands), `action.txt` (`append:<type>\|<params>`) |
| Compile | `compile_page()` in the manager. **Do not move or rewrite.** |
| Play | `&.widgits/events-hq/ops/play_event.sh` + `mr_*.+x` ops |
| Registry | data-driven — `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`. **No hardcoded command-type arrays in the renderer.** |
| View tabs | Scripting (real command list) / Scratch / Blueprints (stubs). `g_evhq_view_mode` in C today. |
| Picker | overlay filled in C from `picker.chtpm` placeholders. Esc/cancel. |

`dashboard.chtpm` is a **skeleton**. Pages, command rows, picker rows are **injected in C** (`evhq_layout_pass` / `evhq_assign_nav_indices`). That is layout-as-C — the thing this house is killing.

Renderer budget already on this branch (do not reimplement): `${var}`, `<repeat>`, `show=`, `<tabbar>/<tab>`, `ACTIVATE` / `kh_elem_in_scope()`, content-hash reparse of `vars=` files, frame dump `entity_menu_frame_<pid>.txt`.

---

## 2. Target architecture

```
button.sh
  → khtpm_core_render.+x  HOUSE  events-hq.xhtpm  PKG_DIR  LABEL
       class="events-hq-pal"     ← MUST NOT be "events-hq-window"

events-hq.xhtpm  (STATIC — never rewritten)
  vars="state/ui.txt"     (or pkg_dir/.hq_manager/ui.txt — see §4)
  <module src="…/khtpm_events_hq_manager.+x"/>   compile/IR owner
  <module src="…/prisc+x.+x pal/evhq_projector.pal"/>  OR a small C projector
  <tabbar> Scripting | Scratch | Blueprints
  <tabbar> pages from <repeat>
  left: trigger
  right: command <repeat>
  footer: + Add Command | Play
  overlay: picker <repeat>  show="${picker_open}"

khtpm_events_hq_manager.+x   UNCHANGED compile_page / action.txt / page.state.txt
projector                    reads manager publishes; writes ui.txt for the template
ops/evhq_action.sh           tab / page / add / pick / play / cancel
                             writes selected_page.txt or action.txt
                             NEVER compiles IR itself
```

**Layout file mtime does not move.** Data changes → `ui.txt` hash changes → `reparse_chtpm_if_changed()`. Same contract as db-hq-pal and the 2026-09-03 strip retarget (`PROGRESS-strip-static-layout.md`).

---

## 3. What you must NOT do

1. **Do not** set `class="events-hq-window"` on the new template. That re-enters `evhq_*`.
2. **Do not** rewrite `compile_page()` or the IR→pal chain into the renderer or into bash.
3. **Do not** hardcode command types in C (`EVHQ_PICKER_TYPES[]` was already a standing-rule incident). Picker rows come from the registry file the manager already reads.
4. **Do not** `git add -A`. No `#.desktop/*`, no `*.pid`, no xyzfs, no generated frames.
5. **Do not** compact `g_nav[]` for the picker. Overlay uses `kh_elem_in_scope()` / ACTIVATE scope. Read `HANDOFF-scope-nav-and-chtpm-port.md` **§9**.
6. **Do not** edit only `ops/khtpm_draw_core.c` — `build_core_render.sh` copies `_shared-lib/khtpm_draw_core.c` over it.
7. **Do not** delete `evhq_*` until the parallel window is signed off and `button.sh` points at the xhtpm.
8. **Do not** use bash as the projector. `.pal` via `prisc+x` or compiled `.+x` C.
9. **Do not** write `#.desktop/events_hq_*.chtpm` every tick (strip `publish_live_chtpm` was that bug).
10. **Do not** kill all `khtpm_core_render` PIDs. Scope by `PKG_DIR` like `same_entity_pids()`.

---

## 4. File layout (new)

All under `44.xyz.01.00/&.widgits/events-hq/` unless noted.

| file | role |
|---|---|
| `events-hq.xhtpm` | static window. `class="events-hq-pal"`. `vars=` → instance ui file |
| `events-hq.css` | copy/adapt `pieces/dashboard.css`; overlay class |
| `pal/evhq_projector.pal` **or** `ops/evhq_projector.c` | reads `.hq_manager/*.state.txt`, writes ui.txt |
| `ops/evhq_action.sh` | the only write path from UI clicks (then manager does IR) |
| `state/` | **not** house-global. Per entity: `$PKG_DIR/.hq_manager/ui.txt` |

**`vars=` path:** templates resolve `#.desktop/…` against `g_house_root` (strip fix). For per-entity state, prefer an **absolute** `vars=` baked at launch **or** a tiny argv/env hook:

Recommended: `button.sh` writes `$PKG_DIR/.hq_manager/instance.pdl` (`pkg=…`, `label=…`) and the xhtpm uses `vars="${PKG}/../…"` — **PKG in khtpm is the .xhtpm directory**, not event_pkg.

**Cleaner:** keep renderer argv[3]=`PKG_DIR`, argv[4]=`LABEL`. Before `parse_chtpm`, if argv[3] is a directory, set a built-in `${EVHQ_PKG}` / load `$PKG_DIR/.hq_manager/ui.txt` as an extra vars file. That is **generic-enough** (optional extra argv) and matches today’s launcher. Document it as `g_extra_vars_path` if you add it — **one** new generic hook, not `g_is_events_hq`.

Manager launch: `<module src="&.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x"/>`. Generic `launch_module()` must pass **`house_root`, `pkg_dir`, `label`** — today’s `evhq_launch_module()` already does. If you only call the 3-arg generic helper with `package_dir` = the xhtpm folder, the manager will look in the **wrong tree**. **Pass argv[3] as extra_arg and keep the manager’s argv contract.**

---

## 5. Template shape (normative sketch)

Not final pixels — regions and data bindings.

```xml
<window label="${entity_label}" class="events-hq-pal" vars="…/ui.txt">
  <module src="&.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x"/>
  <module src="&.widgits/_shared-lib/system/+x/prisc+x.+x pal/evhq_projector.pal"/>

  <page name="main">
    <tabbar class="view-tabs">
      <tab id="view-scripting"  label="Scripting"  action="'${PKG}/ops/evhq_action.sh' view 0"/>
      <tab id="view-scratch"    label="Scratch"    action="'${PKG}/ops/evhq_action.sh' view 1"/>
      <tab id="view-blueprints" label="Blueprints" action="'${PKG}/ops/evhq_action.sh' view 2"/>
    </tabbar>

    <tabbar class="page-tabs">
      <repeat count="${n_pages}" bind="pg">
        <tab id="page-${pg.#}" label="${pg.name}"
             action="'${PKG}/ops/evhq_action.sh' page ${pg.#}"/>
      </repeat>
    </tabbar>

    <panel class="left-panel" show="${is_scripting}">
      <text class="block-title" label="Trigger"/>
      <text class="prop-value" label="${trigger}"/>
    </panel>

    <panel class="command-list" show="${is_scripting}">
      <text class="block-title" label="Commands"/>
      <scrolllist>
        <repeat count="${n_cmds}" bind="cmd">
          <item class="cmd-row" label="${cmd.text}"
                action="'${PKG}/ops/evhq_action.sh' edit ${cmd.#}"/>
        </repeat>
      </scrolllist>
    </panel>

    <panel class="footer" show="${is_scripting}">
      <item id="add-command" label="+ Add Command"
            action="'${PKG}/ops/evhq_action.sh' picker open"/>
      <item id="play-test" label="Play"
            action="'${PKG}/ops/evhq_action.sh' play"/>
    </panel>

    <panel class="stub-panel" show="${is_scratch}">
      <text label="Scratch (stub)"/>
    </panel>
    <panel class="stub-panel" show="${is_blueprints}">
      <text label="Blueprints (stub)"/>
    </panel>

    <panel class="overlay picker-overlay" show="${picker_open}">
      <repeat count="${n_picker}" bind="pk">
        <item class="picker-row" label="${pk.label}"
              action="'${PKG}/ops/evhq_action.sh' pick ${pk.id}"/>
      </repeat>
      <item label="Cancel" action="'${PKG}/ops/evhq_action.sh' picker close"/>
    </panel>
  </page>
</window>
```

**Heterogeneous command rows** (Show Text vs Change Gold vs …): first cut may use a **single `cmd.text` line** (what the manager already publishes). `<repeat>` v2 (`show="${cmd.is_text}"` etc.) is **only** if the first-cut dump is not accepted. Do not block the port on v2.

**Picker overlay:** `show="${picker_open}"`. On open, set `g_default_active_scope_root` to that panel (same as `<tab>` → sidebar). Esc already pops scope if you wire the overlay as an ACTIVATE/`target_id` panel. Prefer `onclick="ACTIVATE"` + `target_id="picker"` on Add Command so you reuse scoped nav instead of a new modal C path.

**`onclick` and `action` are the same Elem field.** Emit **one**.

---

## 6. Projector + action.sh

### Projector (poll)

1. Read `$PKG_DIR/.hq_manager/pages.state.txt` and `page.state.txt` (manager already publishes).
2. Read `state/active.pdl` or equivalent: `view=`, `picker=`, `picker_type=` (action.sh writes these).
3. Write `$PKG_DIR/.hq_manager/ui.txt` **only when the content hash changes** (strip/db-hq-pal lesson: identical every-tick writes are ok if the renderer hashes; still cheaper to skip).
4. Map manager command lines → `n_cmds`, `cmd_0_text`, …  
   Map registry → `n_picker`, `pk_0_id`, `pk_0_label` when picker open.
5. Idle no-op if files unchanged.

Language: **`.pal` if string-ops.md is enough; else a small `evhq_projector.c`**. Not bash.

### `evhq_action.sh` verbs

| verb | writes | manager does |
|---|---|---|
| `view N` | view mode in active.pdl | nothing |
| `page N` | `.hq_manager/selected_page.txt` | republishes page.state.txt |
| `picker open` | `picker_open=1` | nothing (registry already on disk) |
| `pick ID` | `action.txt` = `append:ID\|…` then close picker | `compile_page` + republish |
| `picker close` | `picker_open=0` | nothing |
| `play` | exec `play_event.sh` with pkg_dir | compile already done |
| `edit N` | optional: open picker in edit mode | later; first cut can skip edit-in-place |

Keep param collection for multi-field commands (Show Text, etc.) as **follow-up** unless the current C picker is trivial to mirror. First cut: types that append with empty/default params, **or** one `cli_io` for the first param. Be honest in the PR if the picker is not at parity.

---

## 7. Renderer work allowed (generic only)

Only if dumps prove it is missing:

| cap | when |
|---|---|
| Pass argv[3]/argv[4] into module launch + extra vars path | **likely required** (pkg_dir) |
| Overlay: `class="overlay"` center + dim, scope nav into it | if ACTIVATE+`show=` is not enough |
| `<repeat>` v2 / nested repeat | only if command/page lists cannot be flat text rows |
| Fork **every** `<module>` | if you need manager **and** projector as two `<module>` tags. Today generic launch often takes the **first** `<module>` only — **verify** `launch_module` call sites before assuming both run. If only one `<module>` is launched, start the projector from `button.sh` (like db-hq-pal starts `prisc+x`) and keep a single `<module>` for the IR manager. |

**Preferred first cut:** `button.sh` starts projector the way db-hq-pal starts `prisc+x`; xhtpm has **one** `<module>` for `khtpm_events_hq_manager.+x` with pkg_dir extra_arg. Fewer renderer changes.

No new `g_is_events_hq_pal`.

---

## 8. Migration procedure

1. Work on `chtpm-var-substitution`. Progress notes in  
   `#.#.calendar-dox/!.HQ-IQ-BOOK/09-appendix/PROGRESS-events-hq-xhtpm.md` (create it; update often; push).
2. Verify current `launch_module` / `<module>` count. Decide button.sh-started projector vs second `<module>`.
3. Author xhtpm + css + ui.txt schema. Empty lists must render (trigger unknown, no commands).
4. Projector: **read-only** first (pages + commands + trigger). No picker yet.
5. Parallel launch: copy `button.sh` to `button-pal.sh` (or argv) pointing at xhtpm, **class events-hq-pal**. Old window stays.
6. Headless: `entity_menu_frame_<pid>.txt` vs old events-hq dump. Match regions and nav_index order, not pixel-perfect CSS on day one.
7. Add picker + Play.
8. Owner click-through on a real entity `event_pkg`.
9. Point `button.sh` at xhtpm. Keep `dashboard.chtpm` as rollback for one commit.
10. Delete `evhq_*` + `g_is_events_hq` **in a follow-up commit**. Rebuild. Confirm db-hq still opens (it still uses `g_is_db_hq` until its own port).

db-hq Common Events: **after** this window works, include the same template fragment / projector with `pkg_dir` = that event’s folder. Do not fork the editor.

---

## 9. Pass criteria

- New window does **not** log or take the `g_is_events_hq` branch (grep a debug dump / `g_is_` in a one-line fprintf if needed, then remove).
- `events-hq.xhtpm` mtime does not change while using the app.
- `$PKG_DIR/.hq_manager/ui.txt` **does** change on page switch / append.
- Two entities can be open at once (two PKG_DIRs).
- Page tabs and command list match manager `page.state.txt`.
- Play still compiles via `compile_page` (no second compiler).
- Esc closes picker before quitting the window (scope pop).
- Chrome `_` `!` `X` reachable (do not exclude `chrome-*` from arrow wrap).
- `git diff` is explicit paths only.

---

## 10. Pitfalls (from 2026-09-03 live work)

Full list: `09-appendix/HANDOFF-scope-nav-and-chtpm-port.md` §9 and `PROGRESS-strip-static-layout.md`.

- Default-mode **paints from a frame file**; copies have **no parent**. Do not use `kh_elem_in_scope()` to blank `[>]` / `[^]`.
- Rebuild ≠ live. `/proc/pid/exe` → `(deleted)` until relaunch.
- `KH_VAR_VALUE` is 2048. Do **not** stuff the whole command list into one `${var}`; use `<repeat>` + `cmd_0_text`.
- Strip used to regenerate chtpm every tick — do not copy `publish_live_chtpm`.
- `button.sh` `set -e` + `pgrep` — copy `same_entity_pids`, not a new regex.

---

## 11. Effort / order

1. Read-only xhtpm + projector + parallel launcher (1 focused pass).  
2. Picker + Play (1 pass).  
3. Parity click-through.  
4. Retarget `button.sh`.  
5. Delete `evhq_*` (separate commit).

Scratch/Blueprints stay stubs (`show=`). Visual scripting is out of scope.

---

## 12. Review checklist (for the reviewing agent)

- [ ] Does this keep `compile_page` in the existing manager binary?
- [ ] Is the new class distinct from `events-hq-window`?
- [ ] Is pkg_dir isolation preserved?
- [ ] Is there a forbidden `g_is_events_hq_pal`?
- [ ] Picker types from registry data, not C arrays?
- [ ] Layout file static; ui.txt is the only churn?
- [ ] Shared with db-hq CE later, not a second editor?
- [ ] Explicit git paths; progress doc on the branch?

If any box fails, reject the implementation, not this constraint list.
