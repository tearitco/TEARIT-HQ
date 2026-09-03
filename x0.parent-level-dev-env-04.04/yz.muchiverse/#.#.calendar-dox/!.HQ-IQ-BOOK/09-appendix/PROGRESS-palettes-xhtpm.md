# PROGRESS — palettes → static xhtpm + projector

**Branch:** `chtpm-var-substitution`  **Date:** 2026-09-03
**Status:** emojis + elements categories done (parallel); rmmv / debug /
piececraft / user-pallet NOT ported.

## What changed

`khtpm_core_render.c`'s `g_is_palettes` path (`dbhq_inject_palette_tiles()`,
~line 2544) injected tile-grid `<row>/<button>` markup at runtime from
`palettes_manager.+x`'s published `palettes-<cat>_state.txt`. For the two
plain sprite-grid categories that C is now replaced by a static template
+ projector.

- **`palettes-emojis.xhtpm`, `palettes-elements.xhtpm`** — static.
  `class="palettes-pal database-window"` (NOT `palettes` → `g_is_palettes`
  stays 0). Tiles are `<item class="swatch" sprite= label= action=>` in a
  single `<repeat count="${n_tiles}" bind="t">` directly under `<page>`.
  The **generic swatch-grid layout** (`khtpm_core_render.c` ~8722, keyed
  on `class="swatch"` among page children, NOT on the flag) lays them
  6-wide at 34px — verified: x = 16/58/100/142/184/226, y steps 42.
  Two `<module>`s, both forked by `kh_launch_window_modules`:
    1. `palettes_manager.+x args="<cat>"` — UNMODIFIED. Republishes
       `palettes-<cat>_state.txt` with current-house sprite paths on
       launch (the on-disk file had stale pre-migration paths).
    2. `ops/palettes_projector.+x` (`id="<cat>"`) — reads that TSV
       (`glyph \t label \t sprite_dir`), writes
       `state/palettes-<cat>_ui.txt` (`n_tiles`, `t_<i>_glyph`,
       `t_<i>_sprite`, `empty`), content-gated, 400ms idle sleep.
- **`ops/palettes_projector.c`** + `ops/build_palettes_projector.sh`.
- **`button-pal.sh <emojis|elements> [house]`** — parallel launcher.
  Old `palettes_menu.sh launch_cat` + `palettes-<cat>.chtpm` stay as
  rollback.

## Verified headless
`button-pal.sh emojis` → renderer + manager(emojis) + projector(emojis)
all run; `ui.txt` `n_tiles=113`; PNG shows the 6-wide emoji sprite grid,
transparent PNGs clean on the theme bg (also exercises the `d31c17b7`
sprite-matte fix). `button-pal.sh elements` → `n_tiles=49`, all laid out
(compounds without a pre-generated sprite fall back to the glyph label,
same as the old C path).

## rmmv port plan (the one real remaining palette)

rmmv (rpg-maker tiles) is not a plain sprite grid - it has:
- a **tab strip** of sheet letters A-E (`publish_rmmv()` in
  `palettes_manager.c` -> `g_pal_active_tileset`), click ->
  `palettes_menu.sh set-rmmv-tab <pkg> <letter>`
- a **tileset chooser row** (`publish_rmmv_options()` ->
  `g_pal_opt_tileset_*`), click -> `set-rmmv-tileset <pkg> <prefix>`
- the **tile grid**, but each tile click **arms a brush** (not `place`):
  `palettes_menu.sh arm-rmmv <sprite_dir> <tileset> <category> <label>
   <winx> <winy> <winw> <winh>` -> `tp_arm_placer_rmmv.+x` (full-screen
  InputOnly click-capture; see its header for the XWayland history)
- an **armed hint line** the renderer polls from `state/rmmv_armed.txt`

Port shape (no g_is_palettes):
1. `palettes-rmmv.xhtpm`: `<tabbar class="rmmv-tabs">` (5 `<tab>` A-E,
   `action="... set-rmmv-tab ${PKG} <L>"`), a second `<tabbar>` or a
   `<repeat>` row for tileset prefixes, then the `<repeat count="${n_tiles}"
   class="swatch">` grid with `action="... arm-rmmv ${t.sprite} ..."`,
   and a `<text label="${armed_hint}" show="${armed}"/>`.
2. `palettes_projector.c` (extend, or an rmmv branch): read
   `palettes-rmmv_state.txt` + `rmmv_active.txt` + `rmmv_armed.txt`;
   emit `n_tiles`/`t_<n>_sprite`/`t_<n>_arm_args`, `n_tabs`/`tab_<n>_*`,
   `n_sets`/`set_<n>_*`, `armed`/`armed_hint`, active-class flags.
   The window rect for `arm-rmmv` args 5-8: pass `${PID}` isn't enough -
   needs x/y/w/h. Either the renderer exposes `${WIN_X}` etc. (new
   builtins, like `${PID}`) OR `arm-rmmv` reads the window geometry
   itself from `/proc` + xdotool-free `XGetGeometry` in a tiny helper.
3. `palettes_menu.sh` verbs (`set-rmmv-tab` / `-tileset` / `arm-rmmv`)
   are UNCHANGED - already the file-ops split.
The armed click-capture (`tp_arm_placer_rmmv.+x`) is unchanged.

## NOT ported — follow-ups
- **rmmv** (rpg-maker tiles): tab strip (A-E sheet letters), tileset
  chooser row, and the armed-brush click-capture chain
  (`palettes_menu.sh arm-rmmv` + `tp_arm_placer_rmmv.+x`). The tab /
  chooser / armed-hint state all come from `palettes_manager.c`'s
  `publish_rmmv()` / `publish_rmmv_options()` and the renderer's
  `g_pal_active_tileset` / `g_pal_active_category` / `rmmv_armed.txt`
  polling. Needs: two `<tabbar>`-ish rows + the grid, projector emitting
  the tab/tileset/armed keys, `palettes_menu.sh` verbs unchanged.
- **debug**: rows carry `toggle:<idx>` / `clear` / `noop` actions
  (`publish_debug()`), plus a read-only `debug.txt` content dump. Small.
- **piececraft**, **user-pallet**: stub categories (`palettes-stub.template.chtpm`)
  — trivial once the pattern above is the norm.
- Grid is not scrollable — tall window, rows past the height clamp are
  not serialized (matches the old `g_pal_forced_h` behaviour). Wrap in
  a `<scrolllist>` later if wanted (needs the swatch-grid layout to run
  inside a scroll region).
- Nav badges paint over the 34px tiles (same cosmetic as swatch-picker;
  `604802d4` badge-chip pass).
- Not wired into `livedesk_taskbar.pdl` `open-palette:*` — parallel only.
