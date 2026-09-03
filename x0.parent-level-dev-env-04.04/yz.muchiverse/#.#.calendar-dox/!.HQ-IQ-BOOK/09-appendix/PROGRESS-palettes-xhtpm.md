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
