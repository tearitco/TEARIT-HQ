# 🛠️ CREATOR_AGENT.md — building menus, pickers & sub-app windows in this house

**Written 2026-08-24**, after a real session wasted multiple rounds confusing the two
menu systems while building palettes (direct instruction to write this: "u seemed to
struggle getting going differentiating tb from hq style menus"). If you are about to
add a dropdown, picker, palette, or any tb-launched window — read THIS first, it is
the map that was missing.

---

## 0. The ONE question that decides everything

> **Is your thing a DROPDOWN under a toolbar cell, or its own WINDOW?**

| | **A. tb-native dropdown** | **B. hq-style window** |
|---|---|---|
| What | the shared popup under a header cell (hq/user/file/desks/pals/player/db/toys/h-ai/date) | a standalone process rendering a `.chtpm` file |
| Renderer | `khtpm_strip_parser.c` draws it from `strip_var_hqitems.txt` | `khtpm_hq_render.+x <house> <file.chtpm>` (+ entity-menu/chat-hai variants) |
| Rows/content source | manager builder publishes buttons to `#.desktop/strip_var_hqitems.txt` | the `.chtpm` IS the content (plus CSS beside it) |
| Use when | choosing an ACTION or CATEGORY (few rows of commands) | showing CONTENT/tiles/grids (a picker surface, scrollable matrix) |
| Lives in | `*.monads/*.livedesk-taskbar/ops/*.c` + `#.desktop/livedesk_taskbar.pdl` + `khtpm_strip_header.chtpm` | `&.widgits/<app>/` (composer script + `.chtpm` + `.css`) or `&.hq-apps/<app>/` |

They compose: a category dropdown row (A) launches a category window (B). Palettes
does exactly this. Do NOT try to render content INSIDE a dropdown and do NOT nest a
second menu inside a window.

---

## 1. System A — tb-native dropdown (the shared popup)

There is exactly ONE popup mechanism. Every header cell shares it:

```
click cell N ──► code 4000+N (KSC_HQ_HEADER_BASE+N)
             ──► manager ktb_hq_open(state,N) runs that cell's BUILDER
             ──► builder fills HQMenuItem[] rows
             ──► published to #.desktop/strip_var_hqitems.txt
                 as <button label="..." onClick="HQITEM:i"/>
             ──► strip parser draws the popup ANCHORED UNDER THE CELL,
                 themed like every other cell's popup
row click i ──► code 5000+i ──► ktb_hq_activate(state,i) ──► dispatch command
```

### Wiring recipe (new cell menu)

1. **Cell id**: add `N|<name>` to `#.desktop/livedesk_header_cell_ids.txt`.
2. **Header template**: `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` —
   the cell button MUST be:
   ```xml
   <button label="palettes" onClick="ACTIVATE:6">
     <row>${strip_hq_items}</row>
   </button>
   ```
   ⚠️ **SELF-CLOSING (`/>`) = BROKEN POPUP.** Without the nested
   `<row>${strip_hq_items}</row>` child the parser has no descendant rows →
   no anchored/themed popup (detached-looking / wrong colors / nothing).
   This cost a real debug round 2026-08-24 ("isn't attached to tb nor same color").
3. **Rows**: PDL-driven pattern — `<cellname>_menu_N_label` / `_cmd` rows in
   `#.desktop/livedesk_taskbar.pdl`, read LIVE at every cell-open (no restart
   needed). Copy `livedesk_build_palettes_menu()` / `livedesk_build_hq_menu()`
   in `khtpm_taskbar_manager.c`. Wire the builder into `ktb_hq_open()`'s switch.
4. **Cancel row**: ALWAYS end the row list with a label-only row (`cancel`, no
   cmd) — activation no-ops/dismisses. Direct instruction 2026-08-24.
5. **Commands**: activation goes through `ktb_hq_activate()`'s dispatch:
   - Reserved forms get C branches that build QUOTED absolute paths from
     `house_root`: `livedesk:open-palette:<cat>`, `livedesk:open-chat-hai`,
     `livedesk:spawn-cursword`, `quit`, `user:*`, … **copy an existing branch**.
   - ⚠️ Raw shell commands containing `&` (e.g. `&.widgits/...` paths) FAIL in
     the generic `sh -c` fallback — `&` backgrounds the command and sh then
     tries to run `.widgits/...` which doesn't exist. Proven via touch-test
     2026-08-24. If your path has `&`, you MUST add a reserved-form branch.
6. Row cap is `KTB_LIVEDESK_DYN_MAX` (24) — builders loop `i=1..max`, skip
   missing labels (rows compact up).

### Hardcoded vs PDL-driven cells

Only `hq` and `palettes` are genuinely PDL-driven today. user/file/desks/
player/db/pals/toys/clock/h-ai builders are C-hardcoded — their PDL-looking
rows are DEAD. Editing those rows wastes a full cycle. See
TASKBAR-MENU-ARCHITECTURE.md's standing-debt section (updated 2026-08-24:
ALL cells are supposed to convert to the hq pattern eventually).

---

## 2. System B — hq-style window (kptm_hq_render family)

A `.chtpm` layout + `.css` sheet rendered by `khtpm_hq_render.+x` into a real
X11 window (same family: db-hq dashboard, events-hq, chat-hai, entity-menu,
palettes windows). Reference shape: `&.hq-apps/db-hq/dashboard.chtpm`.

### Wiring recipe

1. **Composer script** in `&.widgits/<app>/<app>_menu.sh` (see
   `&.widgits/palettes/palettes_menu.sh`) writes the `.chtpm` per category/key,
   then `exec`s the renderer binary: `khtpm_hq_render.+x <house_root> <file.chtpm>`.
   Kill-any-existing-instance guard by cmdline match before relaunch.
2. **CSS FILE NAME IS DERIVED, NOT CONFIGURED**: renderer takes argv[2]'s path
   and swaps the extension → `palettes-emojis.chtpm` loads
   `palettes-emojis.css`. ⚠️ Writing only `palettes.css` silently produces an
   UNSTYLED window (no error!). Publish `cp palettes.css palettes-$key.css`
   per composed window. Cost a debug round 2026-08-24.
3. **Layout engine contracts** (shared core `khtpm_css_parser.c` +
   `khtpm_render_core.c`):
   - Only FLEX containers lay out their children; block containers leave
     children untouched (caller manages). Rows of tiles need
     `display:flex; flex-direction:row` in css.
   - `apply_css()` is called PER ELEMENT explicitly. db-hq's panel loop only
     styled DIRECT children — nested elements (tiles inside rows) got zero
     style and drew invisible/zero-width. Fixed 2026-08-24 with
     `apply_css_deep()`; if you build a new nested layout, make sure styling
     reaches the leaves.
   - The panel loop force-defaults child height to 22px ONLY when css didn't
     set one (fixed same day — set height in css and it wins).
4. **Scrollable matrix** (palettes pattern): chunk items into
   `<row class="…">` blocks at COMPOSE time; renderer-side post-pass shifts
   rows by a scroll offset, hides out-of-panel rows (w=h=0), draws track+thumb.
   Nav indices are assigned only to VISIBLE (non-zero-size) elements.

### Image tiles (emoji, icons) — NEVER font glyphs

The house default Xft font has NO color-emoji coverage; text emojis draw as
boxes/nothing. The proven mechanism (toolbar clock-face, bookstack):

```
emoji_gen_atlas.+x "<glyph>"  "<dir>/atlas.png"
emoji_xtract.+x  "<dir>/atlas.png" 0 64 "<dir>/sprite.csv"
```

(prebuilt in `*.monads/*.livedesk-taskbar/ops/+x/`). `sprite.csv` =
`# resolution=64` header + 64×64 `r,g,b,a` lines. Elements carry
`sprite="<dir>"` (directory containing `sprite.csv`); `Elem.sprite` field +
`draw_elem()` blit landed 2026-08-24 in the shared core (ported from
`khtpm_strip_parser.c`'s `tab_sprite()`/`blit_tab_sprite()`). Missing csv =
text-label fallback, never blank/crash. Cache dir convention:
`&.widgits/<app>/sprites/<kind>/<NNN>/sprite.csv`, generated lazily by the
composer (skip if exists).

---

## 3. Verifying without eyes (agent workflow)

- **Headless frame dump**: `khtpm_hq_render.+x <house> <file.chtpm>
  --dump-and-exit` writes `$house/#.desktop/db-hq-frame.png` (or `/tmp/…`),
  then exits. Analyze pixels programmatically (python zlib PNG decode;
  cluster counts: gold tile-bg px, colored sprite px). Never trust a bare
  "process is alive".
- **Dropdown driving**: `HOUSE=<house> bash
  $house/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh {nav|row|key|esc|
  frame|mgrcode} …`. Codes: `4000+N` open cell N's popup, `5000+i` click row
  (HQITEM:i). ⚠️ The parser layer's Enter relay is unreliable — drive
  `mgrcode` directly.
- Dropdown rows publish live: check `#.desktop/strip_var_hqitems.txt` content
  after opening instead of restarting anything.

## 4. Where things live

| Thing | Path |
|---|---|
| header cells template | `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` |
| cell ids | `44.xyz❤️‍🔥️00.17/#.desktop/livedesk_header_cell_ids.txt` |
| dropdown rows (PDL) | `44.xyz❤️‍🔥️00.17/#.desktop/livedesk_taskbar.pdl` |
| manager (builders/dispatch) | `*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c` |
| strip renderer (popup drawing) | `…ops/khtpm_strip_parser.c` (`draw_popup_win`, `tab_sprite`) |
| hq window renderer | `…ops/khtpm_hq_render.c` (+ `_shared-lib/khtpm_render_core.c`, `khtpm_css_parser.c`) |
| shared Elem/core CANONICAL SOURCE | `&.widgits/_shared-lib/` — builds COPY from here; editing the local copy is lost on rebuild |
| emoji→sprite tools | `…/*.monads/*.livedesk-taskbar/ops/+x/emoji_{gen_atlas,xtract}.+x` |
| palettes reference impl | `&.widgits/palettes/` (composer, css, sprites, audit log) |
| design docs | `au11-hq/TASKBAR-MENU-ARCHITECTURE.md`, `#.ref/menu/palletes/pallette-design.txt` |

*Rebuilds: taskbar pair `build_khtpm_strip.sh`; hq renderer `build_db_hq.sh`
(both in `…ops/`). Restart tb: `run_khtpm_strip.sh new`. `.pdl`/`.chtpm`/`.css`
are data — no rebuild needed.*
