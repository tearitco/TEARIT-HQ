# Plan — RMMV img-directory tabs on the palettes rmmv picker

**Source:** `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` ~L2902 (user 2026-08-28).
**Status:** PLAN. Dir tabs exist; **Tilesets still reads house
`palettes/tilesets/rmmv/`** (working — leave that chrome). Other img
tabs **do not yet show real folder PNGs**.
**NEXT (awaiting human approval — do not execute):** §10 relocate
**all** img including tilesets to NNEST-11.17 + PDL path, then PNG
placement for the non-tileset dirs.

---

## 1. Ask (verbatim intent)

Add a **top tab row like db-hq** that switches between RPG Maker tile-related
directories under `www/img`. **Tilesets is already done.** Do the **others**.

External tree (PDL: `RMMV-ASSET-SOURCE-LOCATION.pdl`; mount may be unplugged):

`…/www/img/` → `animations, bank, battlebacks1, battlebacks2, characters,
enemies, faces, parallaxes, pictures, sv_actors, sv_enemies, system,
tilesets, tiltes1, tiltes2, titles1, titles2` (+ `index.html` skip).

**In-house copies already present** (do not wait on the USB mount):

| Location | What’s there |
|---|---|
| `&.widgits/palettes/tilesets/rmmv/` | 31 tileset sheets (current picker) |
| `&.widgits/palettes/assets/` | animations, battlebacks1/2, characters, faces, parallaxes, sv_actors, sv_enemies, system, titles1/2 |

---

## 2. What “like db-hq” means here

db-hq: one **window tabbar**, numbered `[1]…[N]`, content swaps under it.

rmmv picker today: **no window `<tabbar>`**. Tabs are **injected** into the
panel (`pal-tab-row` = A/B/C/D/E sheet letters; bottom chooser = World /
Inside / …). House nav already numbers those buttons.

**Plan:** keep injection (TPMOS: manager publishes, renderer injects). Add a
**first** injected row of **directory** tabs (full names), same `pal-tab` +
onclick + badges as A–E. Do **not** invent a second chtpm or a new window.

```
[ Tilesets | Characters | Faces | … ]     ← NEW, db-hq-shaped names
[ A | B | C | D | E ]                     ← ONLY when dir = tilesets
[ 48px tile grid … ]
[ World | Inside | Outside | … ]          ← ONLY when dir = tilesets
```

---

## 3. Which directories become tabs

**Include** (real asset kinds a picker needs):

1. **Tilesets** (default, current behavior)
2. Characters  
3. Faces  
4. sv_actors  
5. sv_enemies  
6. **Enemies** — include (user: dirs are not empty; local `assets/` may
   omit them, USB `www/img/enemies` is the real set — scan mount + local)
7. battlebacks1  
8. battlebacks2  
9. Parallaxes  
10. **Pictures** — include (same: not empty; scan mount + local)  
11. Animations  
12. System  
13. titles1 / titles2 if they have files  

**Skip:** `index.html`, `bank` (not a sprite sheet family), empty dirs,
`tiltes1`/`tiltes2` unless those folders actually exist with PNGs (look
like title typos).

Scan order for each dir: **local house copy first**, then PDL
`game_www_dir/img/<name>` if the mount is up. Never hardcode a USB path
in C; read `RMMV-ASSET-SOURCE-LOCATION.pdl`.

---

## 4. What each tab shows (superficial v1)

| Dir | Grid contents | Crop |
|---|---|---|
| **Tilesets** | Unchanged: 48px kinds, A1/A2 autotile blocks, A–E + tileset chooser | existing `publish_rmmv()` |
| **Characters / sv_*** | One cell **per PNG file** (whole sheet as a thumbnail) | sample to 48×48 `sprite.csv` |
| **Faces** | One cell per face sheet PNG (not 2×4 cell split yet) | same |
| **Enemies / sv_enemies** | One cell per battler PNG | same |
| **battlebacks / parallaxes / titles / pictures** | One cell per image | same |
| **Animations** | One cell per animation sheet | same |
| **System** | One cell per system PNG (window, icons, …) | same |

v1 does **not** slice character walk cycles or face index cells. Click
still uses existing `palettes_menu.sh place` (whole-file glyph). Cell-level
face/char pick is a later pass.

Cap ~80 files per dir so the sprite pool does not blow up.

---

## 5. State / wiring (no JSON)

`rmmv_active.txt` gains `dir=tilesets` (keep `tab=` letter and `tileset=`
prefix). `set_rmmv` in `palettes_menu.sh` must **preserve all three**
fields when writing one (today it only keeps tab XOR tileset — that
would wipe `dir`).

New verb: `set-rmmv-dir <pkg> <dirname>`.

`rmmv_options.txt` extra lines (renderer already parses `TAB|` / `TILESET|`):

```
DIR|tilesets|Tilesets
DIR|characters|Characters
…
ACTIVE_DIR|tilesets
```

Manager (`palettes_manager.c` `publish_rmmv*`): if `dir` is not tilesets,
scan that folder’s PNGs, write cached `sprites/rmmv/<dir>/<nnn>/sprite.csv`,
publish the state list; **do not** emit A–E or tileset chooser rows.

Renderer (`dbhq_load_palette_options` + `dbhq_inject_palette_tiles`):

- Parse `DIR|` / `ACTIVE_DIR`.
- Inject directory tabs as **two rows** (wrap to a second `pal-tab-row`,
  ~8 names per row). Not one overflowing strip.
- Inject A–E + chooser **only** when `ACTIVE_DIR` is `tilesets`.
- Widen `.pal-tab` (or `.pal-dir-tab`) so “battlebacks1” is readable
  (today `.pal-tab` is 64px for a single letter).

House nav: dir tabs first, then sheet letters (if any), then tiles,
then chooser, close last.

---

## 6. Files to touch (when implementing)

- `&.widgits/palettes/palettes_menu.sh` — `dir` merge + `set-rmmv-dir`
- `*.monads/*.livedesk-taskbar/ops/palettes_manager.c` — dir scan + publish
- `khtpm_entity_menu_render.c` — options parse + inject dir row
- `&.widgits/palettes/palettes-rmmv.css` — dir-tab width
- `RMMV-ASSET-SOURCE-LOCATION.pdl` — optional `img_dir` key (not required)

Do **not** restyle db-hq Terms/CE. Do not rebuild tileset autotile math.

---

## 6b. Cell slicing (what it is — not v1)

RMMV packs **many cells on one PNG**:

- **Faces:** typically 4×2 faces on one sheet. “Slicing” = crop one face
  index as its own picker cell.
- **Characters:** 3×4 walk grid (four dirs × three frames) × 8 actors on
  a sheet. Slicing = one actor / one facing, not the whole `Actor1.png`.
- **Tilesets:** already sliced (48px kinds / autotile blocks). That’s
  why Tilesets is a different inner UI.

v1 for Characters/Faces/Pictures/Enemies: **one thumb per file**. Slice
later. Tilesets keep current cell/kind slicing (after the double-press
fix).

---

## 6c. FIX FIRST — A/B/C and Dungeon/Inside need 2–3 presses

**Do this before dir tabs.** New dir tabs would use the same
`onclick=exec:palettes_menu.sh set-rmmv-*` + manager poll. Copying that
as-is would make Characters/Faces lag the same way.

**Cause (in code, not a guess):**

1. Tab/chooser click is `hq_run_detached()` — **double-fork, does not
   wait**. `set_rmmv` writes `rmmv_active.txt` in a grandchild.
2. Same click **redraws immediately** from the **old**
   `rmmv_options.txt` / state file, so A–E highlight and the grid stay
   on the previous sheet.
3. `palettes_manager` only `publish()`s every **1 second**
   (`usleep(1000000)`). A same-second mtime skip was patched with a
   content compare, but that only helps **after** the manager wakes.
4. `set_rmmv` only keeps **one** other field (tab XOR tileset). A tab
   click can drop `tileset=`; a chooser click can drop `tab=`. That
   matches “sometimes 3 presses” on the lower row.

So the first press often only queues a file write; the UI catches up on
press 2 or 3 when a poll finally lands.

**Fix (do not ship dir tabs until this is one-press):**

1. **`set_rmmv`:** read existing `tab`, `tileset`, **and `dir`**; rewrite
   all of them, changing only the field you meant. Never drop the others.
2. **Optimistic UI:** on A–E / chooser click, set `g_pal_active_*` from
   the button **in the renderer**, re-inject, *then* exec the writer.
   Highlight must move on press 1 even if sprites arrive a beat later.
3. **Manager:** for `category=rmmv`, poll **≤100ms** (or wake on
   `rmmv_active.txt` content change), not 1s. Keep the content-compare
   gate so we don’t recrop every tick.
4. Do **not** add dir tabs by cloning today’s `set-rmmv-tab` onclick
   until (1)+(2)+(3) are live.

**After the fix:** dir tabs use the same `dir=` field + 100ms publish +
optimistic highlight.

---

## 7. Out of scope this pass

- Autotile / map canvas (`TILE-SYSTEM-DESIGN.md`)
- Palettes T1 click-to-place re-verify (still valid, separate)
- Face 2×4 / character 3×4 cell pickers
- Copying the whole USB `www/img` tree into git
- `bank`, empty dirs, HTML

---

## 8. How to check

1. Open palettes → RPG Maker Tiles.
2. Top row shows **Tilesets** (active) plus Characters, Faces, …
3. Tilesets still has A–E and World/Inside/… and the same 48px kinds.
4. Click **Characters**: A–E/chooser hide; grid is one thumb per PNG
   under `assets/characters/`.
5. Nav badges on dir tabs; digit jump switches dir.
6. `rmmv_active.txt` contains `dir=characters` without dropping `tab=` /
   `tileset=`.
7. Relay / `palettes-rmmv_state.txt` first; PNG dump last
   (`_.0.aigent-testing-k9.txt`).

---

## 9. Decisions (locked from user 2026-08-28)

1. **Enemies + Pictures:** include. Dirs are not empty (mount/www/img).
2. **Cell slicing:** explained above; v1 = whole-file thumbs except
   Tilesets (already sliced).
3. **Dir tabs wrap to a second row.**
4. **Order:** **fix double-press on A–E + tileset chooser first**, then
   add dir tabs on the fixed path.
5. **2026-08-28 — do not copy PNGs into `&.widgits/palettes/assets/`.**
   That bloats the house. Tilesets currently still live under
   `palettes/tilesets/rmmv/` (working). Relocate **everything** per §10
   after approval.

---

## 10. NEXT TASK — AWAITING APPROVAL (do not run yet)

**Human (2026-08-28):** other dir tabs do not show the **actual images**
from the folders yet. Tilesets **is** showing images and working —
leave that picker chrome alone until the relocate. Do **not** keep
growing `palettes/assets/`. Move **ALL** img including **tilesets**
**out** to a folder **above the house**, at:

`/home/no/Desktop/…/NNEST_CLEAN_PARENT/NNEST-11.17/`

(the tree the human sometimes **zips**). Still on local disk, **not**
inside `yz.muchiverse/44.xyz…/&.widgits/palettes/`. Reference that
folder from **`.pdl`** (`RMMV-ASSET-SOURCE-LOCATION.pdl`) so the path
can change (drive letter, win/mac) without a C rewrite.

**Intended, not executed:**

1. **Layout on disk** (proposed name, can change at approval):
   `NNEST-11.17/rmmv-www-img/`
   with the same children as `www/img/`:
   `tilesets/`, `characters/`, `faces/`, `sv_actors/`, `sv_enemies/`,
   `enemies/`, `battlebacks1/`, `battlebacks2/`, `parallaxes/`,
   `pictures/`, `animations/`, `system/`, `titles1/`, `titles2/`.
2. **Move** (not copy-into-git) existing house PNGs:
   `&.widgits/palettes/tilesets/rmmv/*.png` → `…/rmmv-www-img/tilesets/`
   `&.widgits/palettes/assets/<dir>/` → `…/rmmv-www-img/<dir>/`
   then **delete** the house copies so palettes stops bloating.
   USB `www/img` can stay the source of truth if already there; the
   NNEST folder is the **stable local** zip-side copy.
3. **PDL** (only path the C manager may read):
   ```
   SOURCE | img_root | <absolute NNEST-11.17/rmmv-www-img>
   SOURCE | tilesets_dir | <img_root>/tilesets
   ```
   `scan_rmmv_dir()` and non-tileset scans both use `img_root/<dir>`.
   No hardcoded `/media/…` and no `palettes/assets/` fallback after
   this lands.
4. **Then — other PNG placements (how):**
   - **Tilesets tab:** keep current A–E + kind crop + `place` (already
     works; only the **file path** changes via PDL).
   - **Every other dir tab:** one 48px **thumb per PNG file** in
     `img_root/<dir>/` (not cell-sliced yet). Manager writes
     `sprite.csv` **cache** under palettes/sprites (small) — **not**
     duplicate full PNGs.
   - Click still `palettes_menu.sh place <id>` → existing
     `tp_set_brush` / `tp_place_desktop` chain, same as emoji/tilesets.
   - Faces 4×2 / characters 3×4 **cell slicing** is a later pass, not
     this relocate.

**Stop until the human says go.** No mkdir, no mv, no PDL edit in this
turn.
