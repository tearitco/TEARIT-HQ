# 🤝 palettes-handoff — 2026-08-24 end-of-session

**Purpose:** hand the palettes work to the next agent. Everything below was verified
live this session; every remaining task has a how-to AND a how-to-check. Read
`CREATOR_AGENT.md` first if you don't yet know the tb-dropdown vs hq-window split.

---

## ⚡ TL;DR

The **6.palettes toolbar cell** now has a working tb-native dropdown (10 categories +
cancel) that opens **db-style matrix windows** per category. The **emojis** window is
the reference implementation: a scrollable grid of 113 tiles rendered as real
PNG-derived sprite textures (`emoji_gen_atlas.+x`/`emoji_xtract.+x` → `sprite.csv`),
click-to-place onto desk via tile-picker's proven chain. Verified live by user
("looks good") and programmatically via headless frame-dump pixel analysis.

**NOT done (at 2026-08-24, this doc's own date):** every category other than emojis
still shows font-glyph text tiles (elements/chemicals) or an honest stub (rmmv,
piececraft, cdda, df, kenney, paint, generate). **CORRECTED 2026-08-29 doc-audit
pass: `rmmv` is no longer a stub** — it now has a real tab bar, a real tileset
chooser, and real state round-trip (`rmmv_active.txt`, `set-rmmv-tab`/
`set-rmmv-tileset` handlers in `palettes_menu.sh`) — see `TILE-SYSTEM-DESIGN.md` for
current status. The other categories' status here wasn't re-verified this pass.
Placement-on-click has not been re-verified since the sprite rework.
Scroll handlers exist in code but are not visually verified with the new 56px rows.
All other taskbar cells' dropdown builders remain C-hardcoded (documented debt).

---

## ✅ What landed this session (do not redo)

| Piece | Where | Proof |
|---|---|---|
| Palettes header-cell popup anchored+themed (nested `<row>${strip_hq_items}</row>` child added to ACTIVATE:6 button) | `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` | user confirmed "that ones perfect" |
| `palettes_menu_1..11_label/cmd` PDL rows incl. trailing cancel | `44.xyz.01.00/#.desktop/livedesk_taskbar.pdl` | published rows visible in `#.desktop/strip_var_hqitems.txt` after open; cancel dismisses w/o launching |
| `livedesk_build_palettes_menu()` + dispatch branches `livedesk:open-palette:<cat>` | `…ops/khtpm_taskbar_manager.c` | mgrcode 5001 launched emojis window; audit log |
| Dynamic row cap (was literal `i<=9`) | same file, hq menu builder comment | 11 rows publish fine |
| `Elem.sprite` field + `sprite=` attr + `hq_sprite()`/`hq_blit_sprite()` (ported from strip parser's tab_sprite/blit_tab_sprite) | **canonical source `&.widgits/_shared-lib/khtpm_render_core.c`** + `khtpm_hq_render.c`; build copies from shared-lib — editing local copies is lost | frame dump: colored sprite px present |
| `apply_css_deep()` (nested elements got zero style before) + panel height default only when css doesn't set one | `khtpm_hq_render.c` layout_pass | gold #ffd700 tile bgs appeared (41k px in dump) |
| Sprite pipeline in composer (lazy per-glyph cache `sprites/emoji/<NNN>/sprite.csv`, 113 generated) | `&.widgits/palettes/palettes_menu.sh` | `sprites/emoji/001..113/` exist |
| Per-key css publishing (`cp palettes.css → palettes-$key.css`; renderer derives css path from chtpm name) | same script, `compose_window()` | unstyled-window bug gone |
| Standing-debt note: ALL hardcoded cell builders must convert to hq PDL pattern eventually | `au11-hq/TASKBAR-MENU-ARCHITECTURE.md` | dated UPDATE section |
| `CREATOR_AGENT.md` two-systems map + INDEX links | `#.#.calendar-dox/1.^V-hq/` | this doc's prerequisite |

---

## 📋 REMAINING TASKS (in order)

### T1 — Re-verify click-to-place on the sprite matrix (15 min, do FIRST)
Clicking a tile runs `onClick="exec:<abs>/palettes_menu.sh place <glyph>"` →
`tp_set_brush.+x <state> <glyph>` then `tp_place_desktop.+x <state> <desk_dir>`
(chain unchanged this session, but not re-proven post-rework).

**How:** launch emojis window (see Env), click any tile (real mouse or
`tp_test_send_click.+x` at a known tile coordinate from a frame dump).
**Check:** `$HOUSE/&.widgits/palettes/audit/palettes.log` gains
`placed glyph <X> onto …`; a new tile dir appears under
`$HOUSE/#.desktop/tiles/`. If placement fails, suspect the exec: quoting of
multi-codepoint glyphs (🗡️ has VS16) — compare against a plain emoji first.

### T2 — Sprites for chemicals+compounds (elements) window (~5 min, sprites pre-generated)
`compose_elements()` still emits font-glyph tiles. The CSV
(`#.ref/menu/palletes/chemistry_tiles_expanded🏆.csv`, 50 compounds) already has an
emoji column — reuse the exact emoji mechanism with a SEPARATE cache root so
numbering doesn't collide.

**Sprites already generated** (2026-08-24 pre-gen): 49 of 50 exist at
`&.widgits/palettes/sprites/elements/<001-050>/sprite.csv` (64px, atlas+extract
pipeline). Aspirin (CSV row 20, index 020) has an empty emoji field — assign one in
the CSV before regenerating that sprite, or leave it (text fallback).

**How:** add `"$EMOJI_SPRITES/../elements"` as arg 3 to `emit_tiles_matrix` in
`compose_elements()` — same pattern as `compose_emojis()` but with a distinct root.
Keep 4-col wide-tile layout. Add `ensure_emoji_sprite`-style loop before the emit
call (use `sprites/elements/` instead of `sprites/emoji/`; numbering is row-based).
**Check:** `sh palettes_menu.sh compose elements` writes sprite= attrs into
`palettes-elements.chtpm`; `--dump-and-exit` on that chtpm → pixel-analyze:
colored (non-gray) px >> before; no empty columns.

### T3 — Visual scroll verification (~20 min)
Handlers exist: wheel Button4/5 (step 2), PgUp/PgDn, thumb geometry + redraw,
offset reset on reload (`khtpm_hq_render.c`: g_pal_scroll block ~line 465,
post-pass ~829-850, keys ~1808). Never checked on-screen with 56px rows.

**How:** open emojis window, scroll to bottom (12 rows total, ~9 visible).
**Check:** last row ("cancel" is in the DROPDOWN not here; last emoji rows)
fully inside panel, no half-clipped row bleed; thumb shrinks/moves; digit-nav
can't focus an off-screen tile (nav assigns only non-zero-size elems).
Frame dumps at offset 0 and max should differ in row content.

### T4 — Real pickers for stub categories (the big one, design doc §BUILD ORDER)
rmmv / piececraft / cdda / df share ONE generic tile-grid picker fed per-category
(BUILD ORDER step 4); kenney 3d similar; paint = color grid; generate = tts-first
panel. Stubs currently open the design doc (honest placeholder, keep until real).

**How:** start with whichever category's data exists (see `ls #.ref/menu/palletes/`;
cdda/rmmv/df asset dirs were never seeded this session — check house assets first,
e.g. `@.apps/piececraft-xyz`). Generalize `compose_emojis()`'s matrix+sprite shape;
tile-grid sources may need `emoji_gen_atlas` replaced by direct PNG→sprite.csv
conversion — check `ops/tp_asset_to_sprite.c` (exists, prebuilt) before writing
anything new.
**Check:** per category: compose emits real tiles; dump-and-exit shows them;
click places via the same place verb (or category-appropriate action declared in
pallets.pdl).

### T5 — PDL externalization of the other cells' builders (standing debt, separate effort)
user/file/desks/player/db/pals/toys/clock/h-ai builders are C-hardcoded; ALL must
become hq-pattern PDL readers. Full instructions already written — see
TASKBAR-MENU-ARCHITECTURE.md "UPDATE 2026-08-24" in the standing-debt section.
Do NOT edit those cells' dead PDL rows before converting their builder.

### T6 — Housekeeping
- Tick BUILD ORDER items in `#.ref/menu/palletes/pallette-design.txt` as T3/T4 land
  (step 1 done, step 6's dropdown wiring DONE, step 2 superseded by the sprite
  matrix — rewrite honestly).
- If entity-menu/chat-hai/events-hq get rebuilt next, they'll pick up Elem.sprite
  automatically (additive field, zero behavior change) — nothing to do, just don't
  be surprised by the new field.

---

## 🔧 ENV QUICK REFERENCE

```sh
H="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00"
OPS="$H/*.monads/*.livedesk-taskbar/ops"

# rebuild renderer (copies _shared-lib over local core copies FIRST - edit shared-lib!)
bash "$OPS/build_db_hq.sh"          # -> OK +x/khtpm_hq_render.+x
# rebuild+restart taskbar pair:
bash "$OPS/build_khtpm_strip.sh" && bash "$OPS/run_khtpm_strip.sh new"

# recompose + relaunch a palette window (kills old instance):
setsid "$OPS/+x/khtpm_hq_render.+x" ...   # NO - use the launcher:
setsid sh "$H/&.widgits/palettes/palettes_menu.sh" "$H" emojis &

# headless verification (no eyes needed):
timeout 8 "$OPS/+x/khtpm_hq_render.+x" "$H" \
  "$H/&.widgits/palettes/palettes-emojis.chtpm" --dump-and-exit
# -> $H/#.desktop/db-hq-frame.png or /tmp/db-hq-frame.png; pixel-analyze with
#    python zlib decode: count gold (#ffd700→bucket(224,192,0)) and
#    max-min>48 colored clusters. Baseline good frame: 229 clusters, 41k gold,
#    54k colored @1125x783.

# drive the dropdown (Enter relay unreliable - use codes):
HOUSE="$H" bash "$H/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh" mgrcode 4006  # open palettes popup
HOUSE="$H" bash "$H/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh" mgrcode 5001  # click row 2 (emojis)
HOUSE="$H" bash "$H/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh" mgrcode 5010  # row 11 = cancel
cat "$H/#.desktop/strip_var_hqitems.txt"   # rows published at open (live PDL read)

# audit trail:
tail "$H/&.widgits/palettes/audit/palettes.log"
```

⚠️ Bash-tool gotcha hit repeatedly this session: launching X apps with plain `&`
in the tool call can hang it to timeout even when detached — wrap launches in
`( setsid … >/dev/null 2>&1 & )` and never rely on that call's output.
