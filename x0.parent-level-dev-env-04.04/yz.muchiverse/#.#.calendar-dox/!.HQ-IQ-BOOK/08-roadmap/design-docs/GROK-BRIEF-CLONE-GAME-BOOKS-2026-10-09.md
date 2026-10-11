# Brief for Grok: books, levels and screens for the clone games

2026-10-09. From claude (owner approved the direction). **Data work only.** Same lane as TSOTS: you write files; claude owns the renderer, camera,
projector and the layout studio. Read `!.HANDOFF-NEXT-PHASES-RPG-MAKER-TILES.md` first for the lane and safety rules (they apply here unchanged).
Companion: `LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md` (why screens are plain data for now).

## 1. Goal

Add **a new dwarf-fortress-type book**, and give each of the other clone games a proper **book with basic levels and basic screens**, so each shows up in
pc-hq as File -> <game> -> Desk list -> pages, the same way TSOTS does. No game rules yet. Start/stop of a game is claude's job (conductor pattern, see 5).

## 2. What a book is (read from the real samples today)

A book = a pc-hq map project: `@.apps/piececraft-hq/pieces/system/maps/<book_id>/`
- `game.pdl` (key rows): `GAME | type | board-game`, `icon`, `label`, `n_chunks | 1`, `chunk_0_x | 0`, `chunk_0_y | 0`, `n_desks | N`, then
  `desk_1_id | <id>`, `desk_1_label | <label>` ... up to `desk_N_*`. (Copy the format of `cdda_sample/game.pdl`; TSOTS has 98 desks the same way.)
- one folder per desk: `<desk_id>/map.txt` (glyph rows; `W` wall, `f` floor, `.` floor, letters for things) + optional `<desk_id>/extrusion.pdl`
  (`EXTRUDE | <glyph> | <height>`, `META` rows) for 3D height.
- optional `events.pdl` at the book root: `EVENT | x=8 y=8 glyph=D | <event page path or trigger=on-click cmds=...>`, `NOTE | <glyph> | <meaning>`.
  `cdda_sample/events.pdl` and `mineclonia_sample/events.pdl` are the models (door, chest on-click, trap player-touch).
- 2D view: a desk without `map.png` draws glyph colours (fine for these games). 3D view: the glyph world (modes 1-4). Do not touch the tile atlas path.

Existing starting points (verified present today): pc-hq maps `cdda_sample` (1 desk), `mineclonia_sample` (1 desk), `default` (2 desks),
`test_terraces`, `test_walls`, `tsots`; the civ game design in `@.apps/civ-txt/CIV_TXT_DESIGN.md`; livedesk pages `civ-test` and `dwarf-fortress-test` exist in the
owner's page menu (check what they contain before adding, do not overwrite).

## 3. Per game: what to make (propose first where marked)

For each book: **3-6 level desks + 2-3 screen desks**, small (16x16 up to 48x32), readable in the 2D view. Keep desk labels short (the menu is narrow).

| Book id | Status | Levels (desks) | Screens (desks) |
|---|---|---|---|
| `dwarf_fortress` | **new** | `surface` (trees, a river `~`, the wagon), `fort_1` (a carved fort floor: rooms, stairs `<` `>`), `mine_1` (rock `#`, ore `*`, a shaft), `caverns` (open cave with water) | `title`, `stockpiles` (grid of storage cells), `jobs` (a list of work spots as clickable cells) |
| `cdda` | extend `cdda_sample` or new `cdda` | `street`, `house_inside`, `shop_inside`, `basement` | `title`, `inventory`, `map_key` |
| `mineclonia` | extend `mineclonia_sample` or new `mineclonia` | `overworld`, `cave`, `village`, `nether_gate` | `title`, `crafting`, `inventory` |
| `civ` | new (use `civ-txt` design) | `world_map`, `capital`, `border_region` | `title`, `research`, `reports` |
| battlefront / others | **ask the owner** (only named in a note, no design file) | - | - |

**Proposal step (do this first, 1 short doc):** one file per game listing exactly the desk ids, labels and glyph legend you will use. Send it to claude
before writing 40 folders.

**What a "screen" is for now:** a desk whose `map.txt` is a small board and whose `events.pdl` rows (`trigger=on-click`) name what each cell does
(`cmds=...`, free text is fine, no real commands yet). The layout studio replaces this later with real menus; do not invent a screen renderer.

## 4. Rules (carried over)

- Do not edit: `bv_render_3d.c`, `bv_gpu_raymarch.*`, `bv_menu_input.c`, `khtpm_core_render.c`, `pchq_board_projector.c`, `keybinds.pdl`, `bv_state.txt`, camera anything.
  **The File menu now reads `pieces/system/maps/*/game.pdl` (claude, 2026-10-09)**: a new book folder with a `game.pdl` appears in File automatically,
  labelled `<icon> <label>` from its own `GAME` rows (folder name if no label), sorted by folder name. No code row is needed; just make `game.pdl` correct.
- Do not run Synch by hand, do not push or merge, do not commit `xyzfs/users`. Commit from `/tmp/grok-bars` on branch `grok` with an explicit path list.
  Keep `pc_generate_chunk.c` untouched except the shared hunk already discussed.
- Do not generate large binaries; maps are text.
- Names: ids lowercase with underscores, labels short. No emoji in desk labels (the menu font shows a box).

## 5. Start / stop (research only, for you)

Games start and stop through a **conductor entity** (Eden is the worked example, `&.widgits/eden/README.md`, `08-roadmap/design-docs/GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md`):
a context menu of METHOD rows (Setup, Start, Pause, Resume, Save, Load, Stop, Reset); each row an event page running one op; the rules in `.pdl`
files (`game.pdl` manifest, `tunables.pdl`, ledgers). Harness-proved (`eden_loop` 305 checks); the GUI path is not verified.
**Your research task (after the books):** for each clone game write 10 lines: which conductor rows it needs, which `.pdl` tunables its rules would be,
and what is missing from Eden's template to host it. Do not write rules or ops. claude and the owner decide.

## 6. Evidence the owner expects (house priority)

Per game, in `XO/15.GROCT/clone-books/`: (1) the file list; (2) a note with the click path **File -> <game> -> Desk -> <page>**; (3) claude runs pc-hq and sends the PNGs of the Desk menu and one level in 2D and one in 3D. A compile or a file listing alone is not "done".

## 7. Open questions for the owner (claude will ask)

1. Is battlefront a book now, or later?
2. Should `cdda_sample` / `mineclonia_sample` be extended in place or left as samples with new full books beside them? (Default here: new books, samples untouched.)
3. Is "screen" = a desk with clickable cells acceptable until the layout studio exists?
