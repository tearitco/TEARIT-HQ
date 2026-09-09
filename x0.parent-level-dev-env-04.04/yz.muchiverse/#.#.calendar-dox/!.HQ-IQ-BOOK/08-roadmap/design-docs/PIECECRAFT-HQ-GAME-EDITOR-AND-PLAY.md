# Piececraft-HQ — the studio: edit maps, script events, play like RPG Maker

**Status:** living vision + next-steps (not a build ticket)  
**Date:** 2026-09-08  
**Supersedes as the “where do games get made?” map:** scattered notes in
`piececraft-hq.md` (2026-08-30 board-window progress),
`TILESETS-EVENTS-AND-GAME-CLONES.md` (find-it-later paths),
`CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` (desktop 3D,
still design-only). Those files stay; this one is the product story.

House root: `44.xyz.01.00`.

---

## 0. One sentence

**Piececraft-HQ is the game-making room:** you drop tiles from palettes
onto a 2D/3D board, you attach RPG Maker–shaped events in **edit**
mode, and **play** mode runs those events with no editor chrome — same
split RPG Maker uses between the map editor and Test Play.

It is **not** a second engine. Palettes, db-hq, events-hq, board-viewer,
and the event registry already exist. The missing work is **wiring them
into one loop** (place → script → play) and then **making play actually
play** (transfer/shop/battle are still kv files).

---

## 1. Product split: Edit vs Play (RPG Maker-shaped)

| | **Edit** | **Play** (Interact ON) |
|---|---|---|
| Who | you / an agent | the player (or you testing) |
| Camera | 2D top / 3D voxel (board-viewer `1`–`4`) | same cameras, no editor overlays |
| Tiles | palettes brush → drop/stamp onto chunks | tiles are the world; no stamp UI |
| Events | visible markers, click voxel → events-hq | pages fire (Autorun / Parallel / Action / Touch) |
| DB | db-hq-pal Actors/Items/CE while paused | game reads the same files; no db window unless you open it |
| Keys | strip / In toggle / File / Desk | 100% game input (legacy `run_pchq_board_mode` contract; NU Interact Mode) |

**Rule:** edit-only chrome (event balloons, collision ghosts, “this
tile has a page”) must not draw in play. Play should feel like RM Test
Play, not like a CAD tool with the hero running around.

**Today:** Interact Mode is the play *input* path (WM-managed board +
var-arm, 2026-09-08). There is **no** first-class Edit/Play *mode flag*
that hides event chrome — because voxel→events-hq and drop-from-picker
are not live yet. When those land, add one projector key
(`play_mode=0|1`) driven by Interact / a toolbar Play, and `show=` the
editor overlays off.

---

## 2. What is already in the room (2026-09-08)

Launch: **HQ → piececraft-hq** (`livedesk:open-piececraft-hq`) or Toys →
**Piececraft-HQ** (not **Piececraft**, which is `piececraft-xyz`).

| Piece | Where | Honest status |
|---|---|---|
| Board window | `@.apps/piececraft-hq/pchq-board.xhtpm` + projector + `pchq_board_action.sh` | 🟡 opens, WM-managed, Interact arms from vars; canvas reads live `canvas_raw` |
| 3D/2D view | `&.widgits/board-viewer` (`bv_render_3d`, `bv_menu_input` POV `1`–`4`) | ✅ engine; pc-hq is the chrome around it |
| Palettes | `button-pal.sh <cat> "$HOUSE"` — rmmv, piececraft (Mineclonia), cdda, emojis, tiled, ohrrpgce, my-palettes, tile-editor | 🟡 pickers live; **no drop onto the board** |
| Assets | `NNEST-12.00/#.NNEST_ASSETS/` via `1.^V-hq/*-ASSET-SOURCE-LOCATION.pdl` | ✅ outside zip; C must not hardcode |
| Sample maps | `pieces/system/maps/{mineclonia_sample,cdda_sample}/` | 🟡 File menu load copies `map.txt` → chunk |
| Events editor | `&.widgits/events-hq` (entity `event_pkg`) | ✅ IR → pal → `cmd_N.sh` |
| Common Events | db-hq-pal CE tab embeds the same editor | 🟡 in-tab; 14 other tabs are field tiles, Terms INI not parsed |
| Event registry | `#.ref/menu/event_commands.registry.pdl` + `mr_*.+x` | 🟡 many cmds **kv-only** (transfer/shop/battle/fade) |
| Tile → event guides | `#.ref/menu/event-guides/` | ✅ authoring sheets; not auto-attached on place |
| Desk persistence | xyzfs `desks/<name>.pdl` `DESK` rows | 🟡 pals persist; `#.desktop/tiles/` packages still vanish |

Older “done” list for the *window* (managed, File/Desk, close):
`design-docs/piececraft-hq.md`. Do not treat that file as the studio
vision — it predates palettes/CE/asset unification.

---

## 3. Long-term vision (2D / 3D editor + playable games)

Think **one studio, many skins**, not five engines.

```
 palettes (rmmv / mineclonia / cdda / tiled / ohr / emoji)
        \  drop / stamp
         \                    db-hq (actors, items, CE, …)
          \                      |
           +--> piececraft-hq board (chunks + camera 2D/3D)
          /         |
 events-hq <--------+  click voxel in EDIT
 (same IR as RM map events)
          |
          v
     PLAY = Interact + event pages + db
     (no editor chrome)
```

**2D** is the RM map / Tiled / CDDA top-down sheet. **3D** is the same
chunk extruded (emoji/voxel pipeline already in `bv_render_3d`). Switching
POV is a camera, not a second map format.

**Playable games we actually want** (same table as
`TILESETS-EVENTS-AND-GAME-CLONES.md` §5, product wording):

1. **RPG Maker–like** — rmmv tiles + events-hq + db-hq. Play = transfer,
   shop, battle, show text, gold. *Blocker:* those cmds are still files;
   palettes do not stamp the board; voxel does not open events-hq.
2. **Minecraft-like** — Mineclonia picker + `mineclonia_sample` + chest/
   door/furnace guides. Play = place/break, inventory, gravity. *Blocker:*
   no voxel hit-test, no move-route/gravity, shop/craft UI missing.
3. **CDDA-like** — UltiCa picker + `cdda_sample` + doors/traps/loot.
   Play = time, loot, combat. *Blocker:* `battle_processing` is kv;
   no map transfer; monsters not on guide sheets.
4. **Later:** Civ (terrain glyphs + gold/turns), GTA-shaped (needs a
   tileset PDL first — do not start without one).

The **desktop itself** (pals on the livedesk, cursword as interact
controller) is a *related* long track
(`CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md`). It is
**not** required to ship RM-like Test Play inside piececraft-hq. Do not
block studio work on desktop-3D.

---

## 4. Next steps (ordered, smallest loop first)

Do **not** start Civ/GTA. Do **not** revive `run_pchq_board_mode()`.
Do **not** hardcode `#.NNEST_ASSETS` in C.

### Loop A — Edit: put a tile on the board (unblocks everything visual)

1. **Drop / stamp from palettes onto the pchq canvas.** Palettes already
   write `arm-rmmv` / `arm-pc` / `arm-cdda` brush files. Board must
   accept that brush (click-to-stamp is enough; XDND is bonus). Mutaclysm
   `101.drag-drop-test=ON` is a **different** window — do not wait on it.
   Stand-in today: `pchq_place_cell.sh`.
2. **Persist placed cells** into chunk files **and** desk `DESK` rows so
   reset does not wipe `#.desktop/tiles/` packages
   (`TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt`).
3. Optional: tile-editor under palettes stays a *sheet* editor; the
   *map* editor is the board.

### Loop B — Edit: attach an event (RM map event)

4. **Click voxel in edit → open events-hq** on that cell’s `event_pkg`
   (create if missing). Same editor as entity events and db-hq CE.
5. **On place, optionally seed** from `event-guides/` (chest → open
   inventory cmds, door → transfer, etc.). Guides are data; do not
   special-case C per game.
6. **Edit overlay:** marker on cells that have pages. Hidden when
   `play_mode=1`.

### Loop C — Play: RM Test Play

7. **Play = Interact ON** (already) **plus** hide edit overlay **plus**
   common-events manager + map-event pages actually running.
8. **Promote kv cmds to gameplay**, one at a time, starting with what
   a 5-minute RM demo needs: Show Text, Change Gold, Transfer Player
   (camera + chunk), then shop UI, then battle. Until then, calling
   them “done” in the registry is a lie for play.
9. **db-hq** field edits must write the files play reads (not only
   `#.desktop/db_hq_*.state.txt`). Terms tab needs an INI/list projector
   (`db-tabs-remaining.txt` — Terms is last on the ladder on purpose).

### Loop D — 2D/3D as one map

10. Keep one chunk format; POV keys stay engine-side (`1`–`4`).
11. Desktop 3D + cursword halo is **after** Loops A–C unless the owner
    explicitly pulls it forward.

---

## 5. What “done” looks like for a first playable

A stranger can:

1. HQ → piececraft-hq (or Toys → Piececraft-HQ).
2. Palettes → rmmv or piececraft → stamp a floor and a door.
3. Click the door in edit → events-hq → Transfer or Show Text → save.
4. Press **In** (play). Walk (arrows). Door runs. No event balloons.
5. Quit / reset; map and event are still there.

Until (2)–(4) work on hardware, piececraft-hq is a **viewer with File
load**, not a studio.

---

## 6. Pointers (do not duplicate)

| Need | File |
|---|---|
| Palettes / asset PDLs / sample maps / clone blockers | `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md` |
| Board window history (managed, File/Desk) | `design-docs/piececraft-hq.md` |
| Interact / focus (WM-managed, no idle steal, canvas engage) | `09-appendix/pc-hq-leg-vs-nu-fix.md`, `design-docs/PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md` |
| Event registry / IR | `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`, `#.ref/menu/event_commands.registry.pdl` |
| db-hq remaining tabs | `#.ref/menu/db-tabs-remaining.txt` |
| Tiled / OHR / My Palettes / tile-editor | `MY-PALETTES-TILED-OHR-TILE-EDITOR-DESIGN.md` |
| Desktop 3D + cursword (later) | `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` |
| Feature catalog row | `10-user-docs/FEATURE-CATALOG.md` piececraft-hq |

---

## 7. Non-goals

- New renderer `g_is_pchq` / bringing back 810-line `run_pchq_board_mode`.
- Hardcoded asset paths in C.
- Playable GTA/Civ before a tileset PDL and Loop A.
- Using `event.commands.remaining.txt` as truth.
