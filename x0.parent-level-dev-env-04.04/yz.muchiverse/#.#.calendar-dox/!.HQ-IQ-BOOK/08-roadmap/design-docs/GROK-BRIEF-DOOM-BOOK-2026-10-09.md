# Brief for Grok: Doom book ("make it play Doom" with ops and events)

2026-10-09. From claude (owner: "we are going to get the free doom assets and make it play doom, using ops, events etc."). Same lane rules as TSOTS and
the clone-game brief: **you write data and tile code; claude keeps the 3D renderer, camera, keybinds, projector.** (Exception granted by the owner today:
billboard-vs-extruded events, see `XO/15.GROCT/BILLBOARD-NOTE-FOR-GROK-2026-10-09.md`.)

## 1. What exists now (claude, committed)
`@.apps/piececraft-hq/pieces/system/maps/doom/` - a **starter book**, already in the File menu ("Doom", read from `game.pdl`; loads with
`pc_generate_chunk.+x <seed> 0 0 map:doom[:<desk>]`, checked in a scratch root: e1m1_hangar and title load, no-desk = first desk).
Desks: `e1m1_hangar`, `e1m2_nukage`, `e1m3_toxin` (levels, 24-wide hand-drawn rooms, `extrusion.pdl` walls 3 high, doors 2), `title`, `hud`, `automap`
(screens: small boards, `events.pdl` rows for clicks). Glyph legend is in `doom/events.pdl` NOTE rows (W wall, f floor, D door, S start, E zombieman,
I imp, K keycard, H health, A ammo, X exit, ~ nukage). These are **placeholders** to prove the pipe; replace them with real levels.

## 2. Assets (free, license first)
Use **Freedoom** (BSD-3, freedoom.github.io: freedoom1.wad / freedoom2.wad) - not the id Software WADs. Put the download outside git
(`#.NNEST_ASSETS/doom/`, like `tsots-characters`), keep the license text beside it, and record the source URL + sha256 in a `doom/ASSETS.pdl`.
Do not commit WADs or big PNG sets; commit only what the book needs (map text, small atlases `cells.txt` + `cells.rgba`, sprite frames).

## 3. Steps (propose after step 1, as with the other brief)
1. **Converter** (Python, in `&.widgits/doom-map-convert/`, same style as `tsots-map-convert/`): read a WAD (lump directory, `THINGS`, `LINEDEFS`,
   `SIDEDEFS`, `VERTEXES`, `SECTORS`) -> one desk per map (`E1M1`...): rasterise sectors to a grid (suggest 1 cell = 32 map units; clip at 128 cells,
   the renderer limit), `W` for solid lines, `f` for floor, `D` for door linedefs (special 1/26/27/28), `~` for damaging sectors, floor height -> `extrusion.pdl`.
2. **Things -> events**: each THING becomes an `EVENT` row in the desk's events (monsters = `trigger=player-touch cmds=start_battle`, pickups = `change_hp`/
   `change_ammo`, keys = `change_items`, exit linedef/sector = `next_level`). Reuse the event vocabulary of `cdda_sample/events.pdl` and the RPG Maker event
   pages (`tsots_event_pages.py` shows how to write a page package). Do not invent new commands without asking.
3. **Look**: floor/wall flats and wall textures -> the per-desk atlas (`cells.txt`/`cells.rgba`, same format as TSOTS; claude's renderer already draws it).
   Sprites (zombieman, imp, items) -> 16x24 `frames/*.rgba` like `tsots-characters` (the event sprite path already extrudes them).
4. **Playing it** is ops + events, not a new engine: door open, pickup, damage floor, key lock, monster touch, level exit = event pages calling small ops
   that write `.pdl` state (HP, ammo, keys in a `doom/state.pdl`, rules in `doom/tunables.pdl`). Start/stop/save/reset of the game = the **conductor entity**
   pattern (Eden, `&.widgits/eden/README.md`; `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md`). **Do not build a conductor yet**: write 10 lines on which
   rows and tunables Doom needs; claude and the owner decide.
5. Screens: `title` (New Game / Load / Quit as clickable cells), `hud` (health, ammo, keys bar), `automap`. Keep them plain data; the layout studio
   (claude, starting now) will replace them with real menus later.

## 4. Rules (carried over)
Do not edit `bv_menu_input.c` (camera), keybinds, `khtpm_core_render.c`, `pchq_board_projector.c`. No emoji in desk labels (icons in `game.pdl` are fine).
Commit from `/tmp/grok-bars` on `grok`, explicit paths. Do not push or merge. Maps are text; no large binaries in git.

## 5. Evidence the owner expects
Per level: the click path **File -> Doom -> Desk -> <level>**, a 2D PNG and a 3D PNG (claude takes them), and for one interaction (a door, a pickup, a
monster touch) a before/after PNG or the state-file diff showing the event ran. A file listing alone is not "done".

## 6. Not decided (claude will ask the owner)
Real-time shooting (needs a tick loop, not event pages) vs turn-like "touch" combat first; Freedoom 1 vs 2 as the first WAD; sound.
