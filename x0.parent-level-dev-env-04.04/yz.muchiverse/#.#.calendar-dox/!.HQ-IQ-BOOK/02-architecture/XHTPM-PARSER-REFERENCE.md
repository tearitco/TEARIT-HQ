# XHTPM Parser + Renderer Reference

**Cross-reference:** xhtpm/khtpm (the X11 pixel renderer) vs. tpmos chtpm (ASCII terminal frame parser)

**Last revised:** 2026-09-04  
**Canonical sources:**  
- xhtpm: `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (parse, vars, actions, dispatch)
- xhtpm: `44.xyz.01.00/&.widgits/_shared-lib/khtpm_*.c` (Elem struct, CSS, draw)
- tpmos: `44.xyz.01.00/101.ledger-player-npc-simple+3/system/chtpm_parser.c` (reference impl)

---

## 1. Architecture Overview

### Comparison: tpmos vs. xhtpm

| aspect | tpmos chtpm | xhtpm khtpm |
|---|---|---|
| **Rendering** | ASCII text → `current_frame.txt` (one writer, marker-driven) | X11/Xft pixel rendering → `entity_menu_frame_<pid>.txt` (pipe serialization) |
| **Template engine** | `parse_chtm()` builds UIElement tree from markup at startup | `parse_chtpm()` parses Elem tree; repeats + var-subst pipeline runs BEFORE parse |
| **Data model** | tpmos manager regenerates entire markup on state change | Static `.xhtpm` template + separate projector writes `state/ui.txt` state file |
| **Variable resolution** | `get_var()` (lookup on-demand in loaded vars table) | `${var}` substituted in markup buffer before parse; builtins `${HOUSE}` `${PKG}` `${PID}` `${ARG3}` |
| **Repeats** | Not in tpmos chtpm (one-shot template) | `<repeat count="${n}" bind="x">…${x.field}…</repeat>` with nesting (5 passes max) and heterogeneous bodies |
| **Navigation** | `active_index` / `focus_index` / `do_jump()` digit accumulation | `nav_index` (1-based sequential), `g_nav[]` array, `g_focus_nav`, digit-jump, arrow skip-scan, scope confinement |
| **CSS** | None; color/style via display expressions (`fg_expr`, `bg_expr`) | Minimal CSS subset: selectors, flex layout, colors, fonts, borders, positioning |
| **Multi-page** | Page stacking (`g_page_stack`) with GOTO:/BACK | Same (xhtpm ported shape directly) |
| **Module fork/exec** | tpmos `launch_module()` | xhtpm `kh_launch_window_modules()` spawns every `<module>` with env export (`KHTPM_HOUSE` / `KHTPM_PKG` / `KHTPM_ARG3`) |

---

## 2. Tags

| tag | attributes | layout | rendering | tpmos equiv | notes |
|---|---|---|---|---|---|
| **`<window>`** | `class`, `drop_action` | — | X11 window container | UIElement type="window" | top-level; holds pages |
| **`<page>`** | `id="name"` | — | page container (tab/sidebar nav stacks pages) | UIElement type="page" | switch via `GOTO:name` / `BACK` |
| **`<sidebar>`** | class (css only) | sidebar panel flex column | sidebar child container | N/A | left column in sidebar+panel layout |
| **`<panel>`** | flex-grow (css) | panel child container | flex-grow remainder in sidebar+panel | N/A | right column; grows to fill |
| **`<tabbar>`** | class | horizontal flex row | tab strip (1st child of page in sidebar+panel) | N/A | children are `<tab>` elements |
| **`<tab>`** | `id`, `class`, `label`, `action` | — | tab label (left-click activates, runs action, marks active) | similar to UIElement type="item" | `g_default_active_tab_id` survives reparse; nav-numbered first |
| **`<scrolllist>`** | — | vertical flex column | scrollable list container (in sidebar after tabbar) | N/A | children are `<row>` / `<item>` elements; scroll tracked |
| **`<row>`** | — | horizontal flex row | row container (fixed height) | N/A | direct child of scrolllist; siblings form one list |
| **`<item>`** | `id`, `class`, `label`, `sprite`, `action`, `onclick`, `backspace_action` | — | clickable list item | UIElement type="item" | `nav_index` assigned; sprite grid wraps on class="swatch" |
| **`<text>`** | `id`, `class`, `label` | — | read-only text (no interaction) | UIElement type="text" | drawn as-is; nav_index=0 |
| **`<title>`** | `class`, `label` | position:absolute (CSS) | draw-only element (never interactive) | N/A | positioned at parent+top/left offset |
| **`<cli_io>`** | `id`, `target_id`, `label`, `action`, `backspace_action`, `rows` | — | armed text-input field | UIElement type="cli_io" | keyboard input synced to state file (target_id-keyed); multi-line when `rows > 1` |
| **`<module>`** | `src`, `args` | — | (not rendered; fork/exec only) | UIElement type="module" | `src` split on whitespace: first=interp, rest=script; env export to child |
| **`<repeat>`** | `count`, `bind`, (v2: per-element `show`) | — | (template expansion; not a real tag) | N/A | expanded at parse time before Elem tree build; up to 5 nesting levels; `${x.#}` = index, `${x.field}` = row field |

---

## 3. Attributes

| attribute | type | what it does | tpmos equiv | notes |
|---|---|---|---|---|
| **`id`** | string | unique identifier within window; target for nav confinement / `target_id` lookup | UIElement.id | 64 bytes |
| **`class`** | space-separated strings | CSS class list; affects styling + layout mode detection (`class="swatch"` → grid mode) | N/A | up to 8 classes; 32 bytes each |
| **`label`** | string | display text (drawn, passed to shell commands) | UIElement.label | 256 bytes; `&amp;` / `&quot;` decoded at parse time (2026-08-31 FIX) |
| **`sprite`** | string | path to sprite dir (`.csv` + texture atlas); empty → plain text | N/A | 256 bytes; ported from khtpm_hq_render.c (2026-08-25 FIX) |
| **`action` / `onclick`** | string | shell command (background, `&` added automatically) + special verbs (ACTIVATE, CLOSE, GOTO:, BACK, PICK:, OPACITY_±, etc.) | UIElement.onClick | 1536 bytes (2026-08-25 FIX: bumped 512→1536 for emoji paths); `&quot;`/`&amp;` decoded; also called `onClick` (2026-08-25) |
| **`target_id`** | string | `<cli_io>` only: target state file key (falls back to `id` if unset); multiple `<cli_io>` per window without collision | UIElement.target_id (ported from wraith-alpha.c) | 64 bytes; generic capability #2 (2026-08-31) |
| **`backspace_action`** | string | `<item>` / `<cli_io>` only: action run on Backspace (delete row), does NOT close window | N/A | 1536 bytes; decoded like `action=` (2026-09-01) |
| **`rows`** | int | `<cli_io>` only: text-input box height in lines; default 1 | N/A | multi-line composer support (2026-09-01) |
| **`src`** | string | `<module>` only: space-separated `<interp> <script>` (wraith-alpha convention); relative tokens resolved vs. `package_dir` then `house_root` | similar to href | tokens absolute or relative-with-fallback |
| **`args`** | string | `<module>` only: static argv (e.g., category name); reused in Elem.id field (safe: modules never drawn) | N/A | 64 bytes (Elem.id) |
| **`drop_action`** | string | `<window>` only: action run on X11 file drop (XDND); `&quot;`/`&amp;` decoded | N/A | 1536 bytes; drag-drop support (2026-08-24) |
| **`show`** | `""`, `"0"`, `"false"` | gate element: if true, drop from tree at parse time | similar to visibility_expr | parsed in `parse_element()` BEFORE `apply_attr()` |
| **`vars`** | space-separated paths | state file(s) for `${var}` substitution (resolved vs. `g_package_dir` or `g_house_root`; absolute or `#.desktop/...` OK) | N/A | found by `kh_find_vars_attr()` at parse time; content-hashed for change detection |

---

## 4. `${var}` Substitution

### Syntax and escapes

```
${name}         → value of variable named `name`
\$              → literal `$` (not var start)
\{              → literal `{`
\\              → literal `\`
\n (in value)   → newline
${UNKNOWN}      → empty string (no error)
```

### Resolution order (`kh_get_var()`)

1. Built-in constants:
   - `${HOUSE}` → `g_house_root` (absolute path to house)
   - `${PKG}` → `g_package_dir` (dir containing the `.xhtpm` file)
   - `${PID}` → process pid (frame dump key)
   - `${ARG3}` → argv[3] if it's a directory (instance-dir hook, 2026-09-03)
2. User variables (from state files):
   - Loaded by `kh_load_vars_multi()` from space-separated `vars="..."` paths
   - key=value format; `#` comments and blank lines ignored
   - `vars` attribute resolved relative to template's own dir or house_root (2026-09-03: also `#.desktop/...` prefix to house_root)
   - Multi-file support: later files add/override earlier ones
3. Unknown → `""`

### State file resolution (`kh_find_vars_attr()`)

- Attribute: `vars="path1 path2 path3"` (space-separated)
- Each token resolved: absolute wins; relative tries `g_package_dir` then `g_house_root`; prefix `#.desktop/` resolves to `house_root/#.desktop/`
- Appended as one space-separated path to `g_vars_path`
- Instance-dir hook (argv[3]): if argv[3] is an existing directory, `<instance>/.hq_manager/ui.txt` is appended to `g_vars_path` automatically (2026-09-03)
- Content change detected by FNV-1a hash (not mtime) via `kh_files_hash()`; debounced to avoid partial-write flips (`g_vars_hash_pending`)

### Pipeline timing

In `parse_chtpm()`:
1. Load state files (if `vars=` exists or argv[3] appended one)
2. Expand `<repeat>` blocks (if any `<repeat>` tags present)
3. Substitute `${var}` in entire buffer
4. Parse Elem tree from result

**Backward compatible:** if template has no `${` and no `<repeat>`, all three steps skipped and tree is parsed byte-for-byte as before.

---

## 5. `<repeat>` Expansion

### v1 (current)

```xml
<repeat count="${n}" bind="row">
  <item label="${row.field}" action="${row.action}"/>
</repeat>
```

- `count`: bare int (e.g., `"5"`) or one `${var}` (e.g., `"${item_count}"`)
- `bind`: variable name prefix (default `"item"`)
- Body expansion: `${row.field}` → `${row_<i>_field}` (0-indexed); `${row.#}` → literal index
- Nesting: depth-matched closing finds nested `<repeat>` without consuming them; outer expansion first, then inner passes

### v2 (sketch, not yet live)

Per-candidate `show=` gating for heterogeneous bodies:

```xml
<repeat count="${content_count}" bind="c">
  <text label="${c.text}" show="${c.is_title}"/>
  <text label="${c.text}" show="${c.is_text}"/>
  <item label="${c.label}" action="${c.action}" show="${c.is_link}"/>
  <item label="${c.label}" sprite="${c.sprite}" show="${c.is_media}"/>
</repeat>
```

Projector sets exactly one `c_<i>_is_*` to `"1"` per row; others drop via `show=` gating. Blocked on network-browser (heterogeneous page content). Documented in `CHTPM-ARCHITECTURE-FIX.md` §8.

### Caps and buffer sizes

- `KH_REPEAT_MAX` = 4096 (max expansions per `<repeat>` tag)
- Repeat expansion buffer: `rcap = sz * 48 + 512KB` (pre-allocated for a 256-row tile grid, ~600B post-sub per row)
- Var-substitution buffer: `cap = strlen(buf) * 4 + 65536`
- Nesting: up to 5 passes (unbounded nesting impossible by design; 5 catches most real trees, including multi-level object.pdl hierarchies)

---

## 6. Show Gating (`show=`)

```xml
<element show="${value}"/>
```

- Parsed in `parse_element()` BEFORE attributes applied
- Drop condition: `value == ""` OR `value == "0"` OR `value == "false"` (case-sensitive)
- Anything else: element kept
- Missing `show=`: element always kept (backward compatible)
- Detached parse: if dropped, children are still parsed (tree is built but detached from parent)

---

## 7. `<module>` Projectors

### Semantics

`<module>` elements are never rendered; instead, `kh_launch_window_modules()` spawns each one as a separate process:

```xml
<module src="manager.+x extra.sh" args="optional_argv"/>
```

### `src` token resolution

1. Split on whitespace: first token = interpreter/script, rest = args
2. Each token resolved: absolute path wins; relative tried against `g_package_dir` then `g_house_root`
3. Failed lookup: path left unchanged (execv will error visibly)
4. Single-token `src` (e.g., compiled manager): no split, launched directly

### Environment exported to child

- `KHTPM_HOUSE=<house_root>`
- `KHTPM_PKG=<package_dir>`
- `KHTPM_ARG3=<argv[3]>` (if argv[3] is a directory; 2026-09-03)
- `PRISC_PROJECT_ROOT=<house_root>` (legacy, for PAL interpreters)
- All parent environment inherited

### State file contract

Projector reads from manager state files (e.g., `state/active.pdl`, `db_hq_<x>.state.txt`) and writes to `state/ui.txt`:

- Format: `key=value` (per-line)
- One projector per `<module>` tag
- Manager/renderer pick up changes via content hash check (not mtime)
- File I/O: write to temp + rename (atomic, cross-process safe)
- Each `<module>` in the tree gets its own vars namespace (from its own `state/ui.txt`)

---

## 8. Action / Onclick Verbs

Handled by `dispatch()` / `activate_focused()` / `hq_dispatch_xevent()` in khtpm_core_render.c

### Reserved verbs (xhtpm-specific or generic)

| verb | what | scope |
|---|---|---|
| **`ACTIVATE`** | Open a dropdown or menu; confine nav to `target_id` subtree if set | db-hq mode + generic (2026-09-03) |
| **`DEACTIVATE`** | Close an active dropdown (called on Escape) | db-hq |
| **`CLOSE`** | Close window (`g_quit = 1`) | all modes |
| **`void`** | No-op; does NOT close (differs from tpmos which closes either way) | all |
| **`GOTO:<page>`** | Switch to named page; push current to stack | all |
| **`BACK`** | Pop page stack and switch | all |
| **`SCROLLUP:<i>` / `SCROLLDOWN:<i>`** | Step generic scrollbar slot `<i>` (`g_generic_sbars[i]`, clamped to its own `max_scroll`). Emitted by the auto-built `^`/`v` arrow Elems in `generic_sbar_register()` — never authored in a template. | generic (swatch grid, sidebar+panel scrolllist) |
| **`PICK:<n>`** | Write to swatch-picker action file (`taskbar_settings_action.txt`) | swatch-picker only |
| **`OPACITY_MINUS` / `OPACITY_PLUS`** | Adjust opacity by ±0.05, write theme, apply to window (2026-08-29) | all modes with theme support |
| **`ZORDER_TOGGLE`** | Toggle between override_redirect and WM-managed; kills + relaunches window | taskbar |
| **`TOGGLE_FULLSCREEN`** | Maximize (x/y = 0), toggle back | all (not gated) |
| **`MINIMIZE`** | Unmap window; write registry with `minimized=1`; other apps can focus via `FOCUSWIN:0x<id>:<pid>` | HQ windows |
| **`FOCUSWIN:0x<win>:<pid>`** | `kh_raise_and_focus()` (XRaiseWindow + `_NET_ACTIVE_WINDOW` ClientMessage + focus — works with always-on-top=false; see §16); restore if minimized. | taskbar/strip only |
| **`PAGEROW:±1`** | Increment/decrement visible dock rows (taskbar internal) | taskbar |
| **`<shell-command>`** | Any other string: executed as `sh -c "<action> '<pkg_dir>' '<house_root>' >/dev/null 2>&1 &"` | all; manager writes state file |

### Menu vs. persistent windows

- If `g_default_has_sidebar_panel` = 1 (persistent): action runs, window stays open
- If `g_default_has_sidebar_panel` = 0 (modal menu/dropdown): action runs, then `g_quit = 1` (window closes)
- Exception: `backspace_action` never closes (2026-09-01)

### dbhq_* path (being deleted)

Verbs `ACTIVATE:`, `SCROLLUP:`, etc. with colon-suffix were `dbhq_`-only dispatch branches; deprecated. Generic forms exist without colon.

---

## 9. Layout Modes

Modes are auto-detected at parse time based on tag/class presence and are mutually exclusive. Each mode runs its own `layout_pass()` equivalent.

### 1. Dock bar layout (`layout_dock_bar()`)

**Trigger:** `class="dock"` on `<window>`  
**Geometry:**
- Horizontal flex row, fixed height
- Children: typically `<item>` elements with sprite (icon) + label
- Height: taskbar uses dock-bar height from theme config

**Example:** livedesk taskbar (bottom-of-screen dock, always-on-top)

### 2. Sidebar + panel layout (`layout_sidebar_panel()`)

**Trigger:** `<window>` contains both `<sidebar>` and `<panel>` children  
**Geometry:**
- `<sidebar>`: flex column, fixed width (e.g., 200px), left edge
- `<panel>`: flex column, flex-grow:1, grows to fill remainder
- Children of `<sidebar>`:
  - `<tabbar>` (if present): horizontal strip, tab `<item>`s
  - `<scrolllist>`: vertical column below tabbar, `<row>` / `<item>` children, scrollbar when content > viewport
  - Fixed `<row>` elements (footer buttons, etc.)
  - `<cli_io>` pinned at bottom (with `rows` multi-line support)
- Children of `<panel>`:
  - Typically `<scrolllist>` + child `<row>` for record details
- Navigation:
  - `<tab>`: nav-numbered first; click activates tab, runs `action=`, marks `g_default_active_tab_id`
  - Click on a tab scopes nav to that page `<sidebar>` subtree: `[^]` on tab, arrows stay in subtree, Esc pops scope
  - Later nav-numbered: `<scrolllist>` items, then `<panel>` content, then chrome (minimize/fullscreen)

**Example:** db-hq-pal (15-tab selector + record grid + details panel)

### 3. Swatch grid layout

**Trigger:** `class="swatch"` on `<item>` elements  
**Geometry:**
- Wrapping grid; **column count is derived from window width** when the
  window is `g_default_persistent` (see §16) — the pre-port rmmv-picker
  shape (git `94d12680` `dbhq_layout_pass`). Otherwise a fixed 6 wide.
  12 rows visible; `generic_sbar` (draggable thumb + nav-numbered `^`/`v`
  arrows) beyond that.
- A persistent window is widened to `5/8` screen (cap 1180) and kept on
  screen from its current x (this path has no other screen clamp).
- Each `<item>` is square, drawn with sprite texture (+ a
  PNG-transparency checkerboard, unless `g_is_swatch_picker`).
- **Chooser chips** (non-`swatch` `<item>`s) are laid out by class family:
  a class-family change starts a new chip row (10px inter-row gap). By
  default the family carrying `class="pal-dir"` is pinned to a **footer
  below the grid**, every other family sits in a **strip above** it.
  Chip width = `max(CSS width, measured label + 46)` — `draw_elem`
  prepends a `[ ]NN. ` nav badge, so a text-width box clips short labels.
- `<item id="close" class="chrome-btn" onclick="CLOSE">` in the template
  is laid into the top chrome strip right-to-left and hard-clamped
  on-window (§16). Same for `class="chrome-btn"` `!` / `_`.

**Example:** palettes (rmmv/emojis/elements/…), taskbar Settings color
picker (`g_is_swatch_picker` — fixed 6-wide, no widen, solid swatches).

### 4. Flat item list (fallback)

**Trigger:** None of above; any `<window>` with `<item>` children  
**Geometry:**
- Vertical list, no sidebar
- Each `<item>` nav-numbered, clickable
- Scroll if window height insufficient

**Example:** popup context menus, entity-menu lists

---

## 10. Navigation / Focus Model

### Model (ported from wraith-alpha, standardized in CENTROID_GOLD_STD.md)

Each element has a `nav_index` (1-based, assigned once per redraw to every interactive element in document order). Interactive = has `onclick[0]` or tag ∈ {tab, item, cli_io}. Non-interactive (text, title, module) = nav_index = 0.

- `g_focus_nav`: current focus index (which `nav_index` is highlighted)
- `g_nav[]`: array of `Elem*` pointers for quick index → element lookup
- `g_n_nav`: count of navigable elements

### Navigation modes

| mode | trigger | behavior |
|---|---|---|
| **Digit jump** | key `'1'-'9'` | Accumulate digit value; on timeout or ESC, jump to that nav_index (wraith-alpha convention) |
| **Arrow nav** | LEFT/RIGHT/UP/DOWN | Skip-scan to next navigable (arrow logic is per-layout; sidebar scans different axis than scrolllist) |
| **Page nav** | PgUp/PgDn | Scroll page in scrolllist |
| **Return** | RETURN key | Activate focused element (run `onclick=` / `action=`) |
| **Escape** | ESC key | Close menu or pop scope; in persistent sidebar+panel, unused (handled by layout, not dispatch) |

### Scope confinement (`g_default_scope_confine`)

**Trigger:** `onclick="ACTIVATE" target_id="<id>"` (generic capability, 2026-09-03)  
When activated:
- Find `<element id="<id>">` (the target container)
- If a tab: scope nav to that tab + its page `<sidebar>` subtree + chrome (minimize/fullscreen buttons)
- If a container: scope nav to that subtree + chrome
- `[^]` displayed on focused element (cursor prefix computed by `elem_cursor_prefix()`)
- Arrow keys confined to subtree
- Esc: pop scope, return nav to full tree

**Tab activation** (sidebar+panel specific):
- Click `<tab id="...">`: run `action=`, mark `g_default_active_tab_id` (survives reparse)
- Activate that tab's scope (nav confined to page sidebar)
- Esc: restore focus to tab row

### Cursor prefixes

| prefix | meaning |
|---|---|
| `[^]` | Element is focused AND is the root of an active scope (2026-09-03) |
| `[>]` | Element is focused (nav_index == g_focus_nav) |
| `[ ]` | Element is navigable but not focused |

Generated on-the-fly by `elem_cursor_prefix()`; never stored in label.

---

## 11. CSS Support

Minimal subset (inventory confirmed on db-hq/events-hq/chat-hai per CHTPM-ARCHITECTURE-FIX.md §5.1b).

### Selectors

- **Element:** `tag`
- **Class:** `.classname` (single) or `.class1.class2` (all must match)
- **ID:** `#id`
- **Tag + class:** `tag.class`
- **Descendant:** `.parent .child` (space-separated segments matched bottom-up from element)
- **Pseudo-class:** `:hover` (activates on focus)
- **Comma-separated rules:** `sel1, sel2, sel3 { ... }` (one rule per selector, same style)

### Cascade

Specificity: element-tag tier (1) < class tier (2) < ID tier (3). Later rules win at same tier. Descendant combinators matched bottom-up (direct subject is matched first, then ancestor chain is walked).

### Properties

| property | value | rendered | notes |
|---|---|---|---|
| `background-color` / `background` | `#RRGGBB` | flex layout, elem background | aliases; last token if multiple |
| `color` | `#RRGGBB` | text foreground | applies to label text |
| `border` | `Npx solid #RRGGBB` | border width + color | parsed: first token = width, color = `#` value |
| `border-color` | `#RRGGBB` | border color only | |
| `border-width` | `N` | border thickness (px) | |
| `position` | `absolute` | (see below) | relative/static default, element stays in flow |
| `top` / `left` | `N` (px) | offset from parent origin | only meaningful with `position:absolute` |
| `width` / `height` | `N` or `N%` | fixed size (px or pct of parent) | pct computed as `(avail * N) / 100` |
| `padding` | `N` (px) | insets flex children on both axes (cross + main) | does NOT inset `position:absolute` children |
| `gap` | `N` (px) | space BETWEEN consecutive flex children (main axis only) | not before first or after last |
| `display` | `flex` | flex layout engine (`css_layout_pass()`) | anything else → block (children untouched by layout) |
| `flex-direction` | `row` or `column` | main axis orientation | only meaningful if `display:flex` |
| `flex-grow` | `N` (weight) | child consumes weighted share of remaining main-axis space | siblings without flex-grow get fixed size |
| `font-family` | `name` | Xft font lookup (e.g., "Monospace") | |
| `font-size` | `N` (px) | Xft font size | |
| `font-weight` | `bold` or `N` (>= 600 = bold) | text weight | |
| `z-index` | `N` | draw order | larger = drawn later (on top) |

### Known gaps / limitations

- **No flex-wrap:** elements overflow if no room
- **Width-less flex parent:** defaults to w=0 (measured children win)
- **No min-width / max-width**
- **No margin:** use gap + padding instead
- **No grid / box-shadow / border-radius**

`parse_declaration()` silently ignores unrecognized properties (e.g., `animation`, `transform`).

---

## 12. Frame Round-Trip (IPC serialization)

### Pipe format: `#.desktop/entity_menu_frame_<pid>.txt`

One line per Elem (preorder walk: non-title/module first, titles/modules last):

```
tag | id | class | label | extra | onclick | nav_index | active_tab_flag | x | y | w | h | target_id | input_buffer
```

**Pipe escaping:** `|` → `/` inside any field (demarshalling reverses it).

### Serialization functions

- `kh_serialize_frame_*()` (multiple per-mode variants)
- `kh_paint_frame_line()`: formats one line
- Called every redraw (dirty-coalesced via `g_frame_dirty` marker)
- Cross-process IPC: other processes (strip renderer, managers) read and parse this file for frame geometry, focus, current input text

### Preorder walk order

1. All non-title/non-module children (leaf content)
2. All `<title>` children (overlays, positioned)
3. All `<module>` children (never drawn but present in tree)

---

## 13. Render Trigger / Dirty Model

Hybrid: cheap content-hash polling + coalesced Expose events.

### Dirty tracking

- `g_frame_dirty`: coalesced flag (redrawn only when set)
- Triggers:
  - Expose event from X11 (window needs redraw)
  - `reparse_chtpm_if_changed()`: detected `.xhtpm` file mtime change OR state file content-hash change
  - Input handling: keyboard/mouse events that change focus or state
- `mark_frame_changed()`: sets flag; `consume_frame_changed()`: clears after redraw

### Change detection

1. **Template (.xhtpm):** mtime tracked in `g_chtpm_mtime` (nanosecond-resolution `struct timespec` to avoid 1-sec race; 2026-09-01 FIX)
2. **State files:** content-hash via `kh_files_hash()` (FNV-1a), debounced against `g_vars_hash_pending` to ignore partial-write flips

### Polling cadence

Every frame/tick (event-select loop, ~1ms latency), `reparse_chtpm_if_changed()` is called:
- `stat()` on `.xhtpm` and state files (cheap; filesystem cache)
- If changed: reload + re-substitute + re-parse + re-layout + redraw

**Comparison to tpmos:** tpmos uses append-only marker file (`nav_master_ledger.txt`), never reparsed, only composited. xhtpm reparses on each real change (one line of code; marker model used ONLY for dirty-marker between components, not for change detection).

See `09-appendix/forensic-report-flicker.md` for detailed render timing analysis.

---

## 14. Hard Limits Table

| #define | value | overflow symptom | comment |
|---|---|---|---|
| **MAX_ELEMS** | 1024 | silent tree truncation | page projection + chrome; was 512 (2026-09-02) |
| **MAX_CHILDREN** | 320 | child silently dropped | per element; was 64, bumped for 256-tile grids + chrome (2026-08-16) |
| **MAX_PAGE_STACK** | 8 | page nav broken beyond 8 deep | GOTO:/BACK stack depth |
| **KH_MAX_VARS** | 2048 | later vars drop silently | was 256; 256-tile repeat needs 512+, bumped for safety (2026-09-02) |
| **KH_VAR_NAME** | 64 | var name truncated | `${...}` up to 64 bytes |
| **KH_VAR_VALUE** | 2048 | var value truncated | state file value per key; 2026-09-02 check needed for large objects |
| **KH_REPEAT_MAX** | 4096 | repeat count clamped | max expansions per `<repeat>` tag |
| **CSS_MAX_CLASSES** | 8 | classes beyond 8th dropped | per element; class list |
| **CSS_MAX_RULES** | 256 | later CSS rules ignored | per stylesheet |
| **Elem.tag[32]** | 32 bytes | tag truncated | tag name |
| **Elem.id[64]** | 64 bytes | id truncated | also holds `<module args=...>` (safe: modules never drawn) |
| **Elem.label[256]** | 256 bytes | label truncated | also holds `<module src=...>` + display text |
| **Elem.onclick[1536]** | 1536 bytes | action truncated (silent) | bumped 64→512→1536 for emoji-path shell commands (2026-08-25 FIX) |
| **Elem.sprite[256]** | 256 bytes | sprite path truncated | |
| **Elem.input_buffer[256]** | 256 bytes | cli_io text truncated | synced to state file |
| **Elem.target_id[64]** | 64 bytes | target_id truncated | state file key for multi-cli_io |
| **Elem.backspace_action[1536]** | 1536 bytes | delete action truncated | same as onclick size |
| **Repeat buf rcap** | `sz * 48 + 512KB` | repeat output truncated (rare) | pre-allocated; 256-row grid ~600B/row post-sub |
| **Var-subst buf cap** | `strlen(buf) * 4 + 65536` | substitution truncated (rare) | pre-allocated growth factor |
| **Attrs on `<repeat>`** | 512 bytes | attributes truncated in repeat parse | `count=`, `bind=` extracted from first 512B |
| **g_generic_sbars[8]** | 8 entries | 9th sidebar overflows | per-app generic sidebar registry |
| **PATH_BUF** | 4096 | path truncated | internal max path length |

---

## 15. Feature Gaps

### tpmos features with NO xhtpm equivalent

- **Display expressions:** tpmos `fg_expr` / `bg_expr` (inline color rules via substitution) → xhtpm uses CSS (separate stylesheet)
- **Module response file:** tpmos manager writes `gui_state.txt`; only cli_io fields sync to it in xhtpm (full feature not ported yet)
- **Terminal rendering:** tpmos ASCII box-drawing; xhtpm is X11/pixel only
- **Input mode filtering:** tpmos `input_mode="numeric"` per-field; xhtpm accepts all printable (filtering in projector tier would be needed)
- **Fold state:** tpmos tracks per-element fold state in `gui_state.txt`; xhtpm not implemented

### xhtpm features with NO tpmos equivalent

- **CSS layer:** flexible, cascading style (property-driven, not expression-driven)
- **Flex layout:** automatic child positioning (tpmos layout is manual per-element in markup)
- **Sprite textures:** emoji_gen_atlas / sprite.csv pipeline for grid rendering (tpmos has no equivalent visual richness)
- **Multi-line cli_io:** `<cli_io rows="N">` for multi-line composer (tpmos single-line only)
- **Scope confinement:** interact-mode tab scoping (`[^]` prefix, nav locked to subtree, Esc pops) — generic, not just db-hq
- **Content-hash change detection:** debounced FNV-1a hashing of state files instead of mtime (immune to 1-sec resolution race, partial-write flips)
- **Nanosecond-resolution mtime:** `struct timespec` for `.xhtpm` (2026-09-01 FIX, avoids race in bootstrap-then-write scenario)
- **Async reparse:** `reparse_chtpm_if_changed()` called every tick (tpmos is marker-driven, never reparses)
- **XDND drag-drop:** `drop_action=` attribute (2026-08-24)
- **Window opacity:** global + per-window `${opacity}` theme control (2026-08-29, OPACITY_±)
- **Backspace action:** `backspace_action=` for row deletion without closing window (2026-09-01)
- **Generic tabbar:** `<tabbar>` + `<tab>` with focus/scope mechanics (ported from db-hq-only hardcode to generic, 2026-09-03)
- **Instance-dir hook:** argv[3] directory → auto-appends `<instance>/.hq_manager/ui.txt` to vars, exports `KHTPM_ARG3` to modules (2026-09-03)
- **Per-window minimize/restore:** registry file + `FOCUSWIN:` verb for cross-process HQ window focus (2026-09-03)
- **Generic dropdown ACTIVATE:** not just db-hq (2026-09-03)

---

## Appendix: File Locations

**Main sources:**
- Template parser: `/44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (4300+ lines)
- Elem struct + basic tree ops: `/44.xyz.01.00/&.widgits/_shared-lib/khtpm_render_core.c` (shared, text-included, not linked)
- CSS parser + compute: `/44.xyz.01.00/&.widgits/_shared-lib/khtpm_css_parser.c` (compiled separately)
- Draw implementation: `/44.xyz.01.00/&.widgits/_shared-lib/khtpm_draw_core.c` (X11/Xft rendering)

**Reference implementations:**
- tpmos chtpm: `/44.xyz.01.00/101.ledger-player-npc-simple+3/system/chtpm_parser.c` (3000+ lines, standalone tpmos port)
- tpmos behavior rules: `/44.xyz.01.00/101.mutaclsym🧟‍♂️️19.00/PITFALLS.txt` (design notes)

**Design docs:**
- `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/CENTROID_GOLD_STD.md` — renderer patterns (partially outdated; update pending 2026-09-04)
- `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md` — static template + projector philosophy, `<repeat>` v2 sketch
- `#.#.calendar-dox/!.HQ-IQ-BOOK/09-appendix/HANDOFF-chtpm-var-substitution.md` — v1/v2/v3/v4 handoff notes, converted apps table, remaining work
- `#.#.calendar-dox/!.HQ-IQ-BOOK/09-appendix/forensic-report-flicker.md` — render timing, dirty-coalescing rationale
- `#.#.calendar-dox/!.HQ-IQ-BOOK/03-pitfalls/X11-AND-SESSION-PITFALLS.md` — X11 quirks (grabs, events, window manage)

---

## 16. Window Chrome & UX (added 2026-09-04)

Cross-cutting behaviours the renderer applies to every window it draws,
regardless of layout mode. All live in `khtpm_core_render.c` unless
noted; the raw-pixel modes (`run_pchq_board_mode`, `strip_main`) each
carry their own copy where they've been brought to parity.

### Theme frame

- A **2px border in the theme SECONDARY colour** (`g_theme_fg`, from
  `#.desktop/livedesk_theme.pdl` `COLOR|fg|…` — the swatch picker's
  second pick) is drawn around the **whole** window (dock included),
  **last**, right before the present — so sidebar/panel background
  fills can't overpaint the side/bottom edges.
- `load_theme_colors()` runs for **every** window at startup (was
  dock-only — non-dock windows kept the `#cccccc` default and the frame
  looked white against a red/black theme).
- Reloaded live on the `livedesk_theme_changed.txt` marker.
- **Not yet on:** the bottom taskbar strip (`strip_main` render path).

### Chrome buttons are template elements

- `X` / `!` / `_` are declared in the template as
  `<item class="chrome-btn" onclick="CLOSE|TOGGLE_FULLSCREEN|MINIMIZE">`
  (or `id="close"`). The renderer only *lays them out* (top strip,
  right-to-left) — no per-app C draw. Replaces the deleted
  `dbhq_draw_chrome_bar`. `MINIMIZE` still needs `g_default_has_sidebar_panel`.

### Edge-affordance clamp — `kh_clamp_elem_onscreen(Elem*)`

Any *synthetic* edge affordance (chrome `X`/`!`/`_`, `generic_sbar`
`^`/`v` arrows) is forced fully inside the window past the 2px frame:
shrink if larger than the window, then pull in from any crossed edge
(`x+w ≤ g_win_w-6`, same for y/h). The `^`/`v` arrows get a real
badge-width box (54px, right-aligned to the track) so the `[ ]NN.`
nav number is readable and never off-frame. `generic_sbar` `track_x`
is also inset 12px from its region's right edge.

### Window fit

- `layout_sidebar_panel`: window is clamped to **never wider/taller
  than the screen**, then a 14px margin is kept off the right/bottom
  edges (so a right-edge scrollbar isn't flush against the screen border).
- Swatch-grid persistent windows: clamped on-screen from their current x.

### Raise-to-top / focus — `kh_raise_and_focus(Window)`

- `XRaiseWindow` **+ a `_NET_ACTIVE_WINDOW` ClientMessage** to the root
  (source indication 2 = direct user action) **+ `XSetInputFocus`**.
- `XRaiseWindow` alone is a hint Mutter ignores under focus-stealing
  prevention when `override_redirect=false`; the EWMH ClientMessage is
  the sanctioned "activate me" it honours. Redundant-but-harmless when
  `override_redirect=true`.
- Called from: the `FOCUSWIN:` verb (taskbar window-nav click → target
  window) and **ButtonPress on any non-dock window** (click a buried
  window → it comes forward). Also wired into `run_pchq_board_mode`.
- **Not yet:** "lower bar focus raises ALL its members" (taskbar-manager
  side) and pulling a spawned child/context window up with its parent.

### Persistence — `g_default_persistent`

Set once in `main()` from the window class (`database-window` /
`palettes-pal`). Checked at `dispatch()`'s quit gate alongside
`g_default_has_sidebar_panel`: a `<page>`-of-`<repeat>` window (palette)
is NOT a transient menu — firing a tile's `action=` must not
`g_quit=1`. Also drives the swatch-grid widen.

---

## Document Info

- **Author:** Claude (via analysis of xhtpm khtpm_core_render.c + tpmos chtpm_parser.c)
- **Scope:** Complete feature inventory as of 2026-09-04
- **Conventions:** All line/function references are git-tracked paths; no assumed state beyond file boundaries
- **Status:** Ready for CENTROID_GOLD_STD.md update (2026-09-04 task)

