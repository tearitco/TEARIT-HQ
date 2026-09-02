# 🪟 5-POV-WIDGIT — Board Widget's Camera/Extrusion System (Confirmed Design)

**Status: PLAN, fully confirmed against real code — nothing built yet.** Written 2026-08-02, after a real back-and-forth correction cycle: earlier drafts of this feature (see `@.apps/BOARD_WIDGET_ARCHITECTURE.md`) got the camera/emoji/extrusion model wrong twice before landing here. This document exists specifically so a future agent (or this session, resuming) doesn't have to re-derive it — every claim below is grounded in a direct code read, cited by file:line, not inferred from a doc comment alone (several doc comments in this house were themselves found stale this session).

**Companion doc:** `@.apps/BOARD_WIDGET_ARCHITECTURE.md` covers the widget's process architecture (separate program, GL window, ledger discovery, real-time sync, cmd bus). **This document covers only the camera/POV/extrusion/emoji rendering system** — read both before implementing.

---

## 0. THE TWO WRONG DRAFTS, AND WHY (so the mistake isn't repeated a third time)

1. **First wrong draft**: proposed "5 numbered view modes" where emoji was mode 5, collapsing `render_mode` (2D/3D) and `emoji_mode` (on/off) into one axis, reasoning that "the widget only has 2D board data so there's no real 3D to speak of." **Wrong** — the user's direct instruction is that the widget WILL have a real 3D raymarch mode, with 2D content extruded into it, not flattened away.
2. **Second near-miss**: after reading `extrude-emoji.md` (mutaclysm's own bug report about 3D wall texturing), nearly concluded that "3D mode is always emoji-textured, so emoji_mode and render_mode might really be the same axis." **Half right, half wrong** — 3D mode genuinely IS always textured (confirmed below), but that does NOT mean `emoji_mode` collapses into `render_mode` — 2D mode still has its own real, independent emoji_mode-driven flat-vs-textured distinction. They are separate flags; 3D just happens to not consult one of them.

**The actual, confirmed truth, direct from code** (`ops/compose_rgb_frame.c:2280-2323`, with the code's own comment at `:2284-2288` quoted in full below) is in §2.

---

## 1. TERMINOLOGY NOTE

Mutaclysm's own field names (`render_mode`, `camera_mode`, `emoji_mode`) are used throughout this doc when citing mutaclysm's real code, since renaming them mid-citation would make the citations harder to verify. The widget's own implementation is free to use different field names in its own `state.txt` (`board_render_mode`, etc.) — not a naming requirement, just avoid ambiguity when reading this doc against the cited source.

---

## 2. THE CONFIRMED render_mode / camera_mode / emoji_mode SYSTEM

### 2a. `render_mode` (0 = 2D flat, 1 = 3D raymarch) — GL-window-only, genuinely bidirectional

- Toggled by `'0'`, `ops/choice.c:914,944-947`: `int is_3d_toggle = (key == '0'); ... if (is_3d_toggle) { render_mode = !render_mode; }` — a real, confirmed, bidirectional flip (this session's own direct investigation; two stale comments elsewhere in `choice.c` falsely implying otherwise were found and fixed this same session).
- **`render_mode` only affects the GL mirror window.** `ops/compose_frame.c` (the ASCII terminal renderer) **never reads `render_mode` at all** — the terminal is always flat 2D ASCII, full stop, regardless of what the GL window is showing. Direct quote, `ops/compose_rgb_frame.c:2286-2288`: *"this whole branch is a no-op in ops/compose_frame.c (ASCII), matching real wraith's own convention that '0' only changes what the GL view shows."*
- **This has a direct, important implication for the widget**: since the widget per `WIDGIT_BIBLE.md` has **no ASCII renderer at all** (widget profile = `ascii_renderer=0` always), the widget's GL window IS its only surface — so `render_mode`'s 2D/3D distinction is the widget's ENTIRE rendering story, not a secondary GL-only feature layered on top of a primary ASCII view the way it is in mutaclysm's own app.

### 2b. `camera_mode` (1-4) — only meaningful while `render_mode==1`

- `ops/choice.c:915`: `int is_pov_key = (render_mode == 1 && (key == '1' || key == '2' || key == '3' || key == '4'));` — digits 1-4 are ONLY reinterpreted as camera-POV switches while in 3D mode; in 2D mode those same digits keep their normal action-bar meaning (pickup/drop/etc. in mutaclysm's own case — N/A for this widget, which has no action bar in that sense, see §5).
- The 4 presets (`ops/compose_rgb_frame.c`'s `camera_mode` switch, cited in this session's earlier research): 1=first-person, 2=third-person, 3=free-roam, 4=bird's-eye/"board game" — mode 4 is explicitly commented in mutaclysm's own source as the closest analog to a top-down strategy view.

### 2c. `emoji_mode` — genuinely orthogonal, but NOT symmetric between 2D and 3D (this is the part both wrong drafts missed)

Direct code, `ops/compose_rgb_frame.c:2279-2323`:

```c
if (render_mode == 1) {
    /* emoji_mode is irrelevant here (real terrain colors either
     * way, no flat-vs-emoji distinction in 3D) ... */
    render_3d_view(...);
} else {
    for (... 2D flat blit loop ...) {
        glyph_to_rgb(glyph, emoji_mode, &r, &g, &b);   // ALWAYS computes a flat base color
        ... draw flat-color tile ...
        if (emoji_mode && ...) {                        // THEN, only if emoji_mode, blit a real emoji texture ON TOP
            blit_emoji_tile(...);
        }
    }
}
```

**Confirmed, precise semantics:**
- **`render_mode==1` (3D)**: `emoji_mode` is read into scope (`compose_rgb_frame.c:1759`) but **never consulted anywhere inside the wall raymarch pipeline** (`raymarch_walls_3d()`, lines ~1066-1183 per this session's own earlier citation — zero `emoji_mode` references in that line range, confirmed by grep). Walls are **always** ray-marched as textured AABBs, sampling real emoji-voxel data (`sample_voxel8_pixel()`/`voxels_8.csv`) — there is no "plain-color 3D" option. Direct quote again: *"no flat-vs-emoji distinction in 3D."*
- **`render_mode==0` (2D)**: `emoji_mode` is a REAL, independent, meaningful toggle. `emoji_mode=0` → flat solid color per glyph only (`glyph_to_rgb()`'s own `rgb_top` registry field). `emoji_mode=1` → the SAME flat color as a base, with a real emoji texture composited on top (`blit_emoji_tile()`).

**What this means for the widget, stated plainly**: `render_mode` (2D/3D) and `emoji_mode` (flat-color/textured) are **two separate, real flags** — exactly matching mutaclysm's own current, working code, not collapsed into a single "5-mode" selector as the first wrong draft proposed. The widget should carry both fields independently in its own state, toggle `render_mode` via `'0'` (genuinely bidirectional), and `emoji_mode` via its own key/command (mutaclysm's own `'e'`/`'E'` binding for this was removed — see §2d — the widget should pick its own binding, not assume `'e'` is free, since mutaclysm repurposed it for camera yaw within `is_pov_key`'s own control scheme).

### 2e. FULL camera key scheme (added 2026-08-02, direct user question: "wasdzxqe rt camera controls muta has")

Real, confirmed via direct code read of `ops/camera_control.c` (dispatch, mode-gated, no-op unless `render_mode==1`) and `ops/compose_rgb_frame.c` (the actual camera-math per mode). This is the FULL key set, distinct from hero movement (which stays on plain arrows/wasd walking, untouched by any of this).

**State variables** (persisted, e.g. mutaclysm's own `hero/state.txt`): `cam_yaw`, `cam_pitch`, `cam_pan_x`, `cam_pan_y`, `cam_pan_z`, `cam_z_level`. `YAW_STEP`/`PITCH_STEP` = 10° per keypress, pan = 1.0 map-unit per keypress — these are **continuous, repeatable increments**, not single jumps; holding/repeating a key keeps moving the camera, exactly like a real free-cam control scheme, not a fixed set of 4 static snapshots.

| camera_mode | q/e (yaw) | r/t (pitch) | w/a/s/d (pan) | c/v (z-level) | f (reset) |
|---|---|---|---|---|---|
| 1 — first person | ✅ ±10° | ✅ ±10°, clamped [-89,89] | ❌ (locked to hero) | ❌ | yaw=180, pitch=6 |
| 2 — third person | ✅ | ✅ | ❌ (locked to hero) | ❌ | yaw=180, pitch=6 |
| 3 — free roam | ✅ | ✅ | ✅ w/s→pan_z ±1.0, a/d→pan_x ∓/±1.0 | ✅ ++/-- | pan=(0,0,0), yaw=180, pitch=-90 |
| 4 — bird's eye | ❌ no-op | ❌ no-op | ✅ w/s→pan_y ∓/±1.0, a/d→pan_x ∓/±1.0 | ✅ | pan centered on hero's pos_x/pos_y |

Per-preset base camera math (`compose_rgb_frame.c:1564-1606`, `:821-823`):
- **1 (first person)**: `pitch=cam_pitch`, eye-height `cam_y_off=0.9`, `cam_z_off=0.0`, `yaw=cam_yaw` — camera pinned to hero's own (col,row), looking through its eyes.
- **2 (third person)**: fixed `pitch=-45.0`, `cam_y_off=4.0`, `cam_z_off=-3.0` — floats behind-and-above the hero, opposite its facing.
- **3 (free roam)**: starts from bird's-eye-style height math (`cam_y_off = 12.0 + cam_z_level*2.0`) but yaw/pitch/pan are all fully user-controlled — position = hero-relative + `cam_pan_x/y/z`. This is the genuinely free camera.
- **4 (bird's eye / "board game")**: fixed `pitch=-90.0` (straight down), `cam_y_off = 12.0 + cam_z_level*2.0`, `yaw=180.0` (not 0 — a deliberate correction for a left-handed-Z-down mirroring quirk, per the source's own comment). Camera position is **absolute map coordinates** (`cam_pan_x/y`), NOT hero-relative — the only mode where the camera can detach entirely from any single entity. `view_radius` is doubled in this mode too.

Switching `camera_mode` (via `'1'`-`'4'`, only live while `render_mode==1`) resets defaults: modes 1/2 → `yaw=180,pitch=6`; modes 3/4 → `yaw=180,pitch=-90,pan=(0,0,0)`.

**For this widget**: since board-viewer has no "hero" (only a selector cursor, §7), modes 1/2 (hero-locked) don't map cleanly — mode 4 (bird's-eye, absolute map coords, camera detachable from any single entity) is the natural default and the one explicitly requested as most relevant to a board-overview widget. Modes 1-3 should still be built (per the resolved open item in §6 — "all 4 needed for debugging, not scoped down"), but the widget's own selector should likely stand in for "hero" in modes 1/2's hero-lock logic, same role mutaclysm's own hero plays there.

### 2f. render_mode requires real extrusion to be visible — sequencing note

Toggling `render_mode` to 1 (3D) with no raymarch renderer built yet produces an empty/undefined 3D pass — it is not visually meaningful until §3's extrusion work (raymarch walls/terrain + entity extrusion) is actually implemented for this widget. Build order should be: (a) camera state + full key dispatch (this section) wired and persisting correctly — testable even before any 3D pixels exist, by inspecting the state file directly or via the receipt/PNG mechanism in §8 — then (b) the actual 3D render pass, so `'0'` has something real to reveal.

### 2d. Stale-doc correction made this session (for context, already fixed in mutaclysm's own tree)

`ops/choice.c`'s own header comment used to claim `'i'`/`'I'` and `'e'`/`'E'` are still live, unconditional key dispatches for `interact_mode`/`emoji_mode`. **Both are false as of the current code** — `is_interact_toggle`/`is_emoji_toggle` were removed (`choice.c:898-899`: *"is_interact_toggle ('i') and is_emoji_toggle ('e') REMOVED: 'e' is camera yaw, interact_mode entered via panel/'9' only."*). `emoji_mode` today is only toggled via the standalone `ops/toggle_emoji.c` binary (a real, working, unconditional `emoji_mode = !emoji_mode`), wired as a numbered `piece.pdl` METHOD row, never a raw keypress `choice.c` itself intercepts. Both stale paragraphs were corrected in mutaclysm's own `ops/choice.c` this session (2026-08-02), per direct instruction to fix stale docs found along the way — see that file's own inline correction markers for the full replacement text.

---

## 3. EXTRUSION — what "everything 2D should be extruded to 3D" concretely means

### 3a. The real, working precedent (`extrude-emoji.md`, mutaclysm's own bug report, read in full this session)

Mutaclysm's 3D pipeline (`ops/compose_rgb_frame.c`, a **custom CPU-side software raymarcher**, not WebGL/Three.js) already does real extrusion for **non-walkable terrain/furniture** (walls):

- `render_3d_view()` (`:1287`) runs passes: floor (`:1438-1475`), walls (`:1481-1482`), a debug cube (`:1484-1492`).
- `raymarch_walls_3d()` (`:1066-1183`): for any glyph where `glyph_walkable_3d()` returns false (`:1122`), it runs a real ray/AABB slab test (`ray_aabb_hit_3d()`, `:924-965`) against a full unit cube `(col,col+1) × (0,1) × (row,row+1)`, and on a hit, computes the hit-face UV and samples the actual pre-baked emoji-voxel texture (`sample_voxel8_pixel()`, backed by `voxels_8.csv`) for that pixel. **This is genuine Minecraft-style textured-block geometry, ray-marched — not a flat quad viewed at an angle.**
- **Confirmed, documented gap**: entities (hero, monsters, items, the xlector cursor) do **not** get this treatment. They fall through to the floor pass (`:1450-1456,1472-1473`) as flat, untextured, solid-color quads at `y=0` — because `glyph_walkable_3d()` defaults any glyph not present in the terrain/furniture registries to `walkable=1` (`:864`), and hero/monster/item glyphs are never registry entries. The report's own "fix direction" section (§109-129 of that file) lays out exactly how to add a 4th pass for entities (billboard or AABB, sampling the same `voxels_8.csv` machinery) — **never implemented in mutaclysm itself.**

### 3b. Confirmed decisions for THIS widget (from direct user answers, 2026-08-02)

1. **Selective, terrain-dependent extrusion** (not "everything is a cube") — matches mutaclysm's own current split conceptually (some tiles get real height, some stay flat), reinterpreted for civ-txt's own terrain types: e.g. hills/forest/mountains get real height, plains/grass likely stay flat or near-flat, water might be a lowered plane rather than a raised block. **The exact terrain→height mapping is not yet decided — real open item, see §6.** The point confirmed is the STRUCTURE (some tiles extrude, some don't, driven by terrain type), not a specific height table yet.
2. **Entities DO get extruded for this widget** — going further than mutaclysm's own current code. Cities, units, and the selector cursor become real 3D objects (billboard or AABB — same open design call `extrude-emoji.md` itself leaves unresolved at line 119-121: *"entities are likely meant to look like standing sprites rather than solid blocks — a design call for whoever implements this"*), textured from the same `voxels_8.csv`/emoji-voxel machinery walls already use. This is genuinely new work — it is building the fix mutaclysm's own `extrude-emoji.md` describes but never implemented, adapted for civ-txt's/tactics-txt's own entity types (cities, units) instead of mutaclysm's own (hero, monsters, items).
3. **Direct port/adapt of mutaclysm's real raymarch code**, not a fresh implementation — `raymarch_walls_3d()`, `sample_voxel8_pixel()`, `get_voxel8_cached()`, `ray_aabb_hit_3d()`, and (once built, per point 2 above) the entity-extrusion pass described in `extrude-emoji.md`'s own fix direction — all copied and adapted to read civ-txt's/tactics-txt's own terrain/unit registries instead of mutaclysm's `terrain_types.txt`/`furniture_types.txt`/monster/item registries. Proven, working ray-marching math; swap the data source, not the algorithm.

---

## 4. WHAT THE WIDGET'S OWN STATE NEEDS (concrete fields, not yet built)

Per §2's confirmed model, the widget's own session state (its own `state.txt`, or wherever the project's convention puts per-session mutable fields) needs, at minimum:

```
render_mode=0          # 0=2D flat, 1=3D raymarch extruded — toggled by '0', genuinely bidirectional
camera_mode=4           # 1-4, only consulted while render_mode==1; default 4 (bird's-eye) makes sense as the widget's own starting POV given its whole purpose is board-overview
emoji_mode=1             # independent flag; matters in render_mode=0 (flat-vs-textured), irrelevant while render_mode==1 (3D is always textured)
focused_project_root=... # per BOARD_WIDGET_ARCHITECTURE.md §4 — which host project's real files this widget is currently reading
selector_x / selector_y  # the widget's own xlector-equivalent cursor position (§6a of the companion doc)
```

---

## 5. NAV MODE — SUPERSEDED AGAIN 2026-08-02 (same day, later): the METHOD-row mechanism below never actually worked, real fix documented in `interact-fix-widget.txt`

**This section (below, kept for history) was itself wrong in a way that wasn't caught until live-testing.** It proposed a numbered `piece.pdl` METHOD row dispatched through `bv_menu_input.c` as "sufficient and simpler" than `chtpm_parser_pal.c`'s own `onClick="INTERACT"` mechanism. That reasoning was backwards. Direct trace of the parser's own `${piece_methods}` button-generator (the code that turns a piece.pdl METHOD row into a real chtpm button) showed it **always** emits `onClick="KEY:N"` for any command string outside a small parser-command whitelist (`LOAD_PROJECT:`, `LAUNCH:`, `MP3:`, `BACK`, `RELEASE`) — `"INTERACT"` is not on that whitelist, so a METHOD row can **never** produce a real `onClick="INTERACT"` button, no matter what its command column says. The engine's entire native interact machinery (mode flag, raw arrow-key relay into `interact_relay.txt`, ESC-exit consumed before any project op runs, the `[>]`→`[^]` focus-glyph swap) is gated purely on that literal `onClick="INTERACT"` string appearing on a real, hand-written `<button>` element in the `.chtpm` XML itself — exactly mutaclysm's own `game.chtpm:7` shape. **Full investigation, root-cause trace, and the actual working fix: see `&.widgits/interact-fix-widget.txt`.**

**Corrected mechanism, now live in `board_viewer.chtpm`:**
```
<button label="Nav Mode (camera + selector)" onClick="INTERACT" /><br/>
```
a real static button, not a piece.pdl row. Once clicked, `chtpm_parser_pal.c` itself:
- sets its own internal `active_index` (no file board-viewer's own ops need to read for gating),
- relays raw keys (remapped arrows → 1000-1003) into `interact_relay.txt` **only** while this element is active — `bv_menu_input.c` is therefore only ever invoked with those codes while genuinely in nav mode, so **no gating logic is needed in this project's own ops at all**,
- consumes ESC itself, before `bv_menu_input.c` ever sees it,
- flips the `[>]`/`[^]` glyph automatically.

`bv_compose_frame.c` reads the engine's own already-exported `pieces/display/active_gui_is_typing.txt` (written by `export_active_index()`) purely for its own "Nav mode: ON/OFF" status line — this is read-only/informational, nothing in this project gates on it.

**The confirmed design decision from the original §5 below still holds** — camera controls and the selector cursor share the SAME single nav-mode entry point (unlike mutaclysm's own two-independent-axis design, where camera POV is gated only by `render_mode==1`, with zero reference to `interact_mode`). That decision didn't change; only the mechanism implementing it did. Since this widget's camera controls (§2e below) will also only ever be dispatched while inside the same real `onClick="INTERACT"` element, this unification is now automatic/structural rather than something board-viewer's own code has to enforce by hand.

<details><summary>Original (superseded) §5 text, kept for history</summary>

**This section originally claimed the widget could bind `'0'`/`'1'`-`'4'` "directly and unconditionally," with no gating needed at all, reasoning the widget has no competing action-bar to collide with. That reasoning was incomplete and led to a real, live-caught mistake**: P5's first implementation let arrow keys move the selector cursor completely unconditionally, with no "enter navigation mode" step at all — directly contradicted by direct instruction and by re-investigating mutaclysm's own real xlector mechanism, which is NOT automatically active either.

**Confirmed via direct code read of mutaclysm's real `game.chtpm`/`chtpm_parser_pal.c`/`choice.c`:**
- The xlector/free-roam cursor is entered via a real, **numbered CHTPM button** — `<button label="Control Hero" onClick="INTERACT" />` (`game.chtpm:7`), selected the same digit-jump+Enter way as any other button, not a raw keypress. Its own footer states this explicitly: `[enter] Control Hero  [esc, while controlling] Back to Menu`. Once engaged, `chtpm_parser_pal.c` switches that context into a raw-key-relay mode (bypassing the normal numbered-menu dispatch) so arrows can drive the cursor; ESC always exits back to normal control.
- **However**, mutaclysm's own camera POV controls (`'1'`-`'4'`) are a genuinely SEPARATE mechanism from the xlector — gated purely on `render_mode==1` (3D on via `'0'`), with **zero reference to `interact_mode`** anywhere in `is_pov_key`'s own condition (`ops/choice.c:915`). In mutaclysm's real, current code, you can freely use camera POV controls while controlling the HERO (not the xlector) too, as long as 3D mode is on. The two are independent axes there.

~~A real, numbered `piece.pdl` METHOD row — "Enter Nav Mode"... dispatched through `bv_menu_input.c`... NOT requiring `chtpm_parser_pal.c`'s own special `onClick="INTERACT"` raw-relay mechanism — a plain METHOD row calling this widget's own op is sufficient and simpler...~~ **← this specific claim is the part that was wrong, see above.**

</details>

---

## 6. OPEN QUESTIONS — genuinely undecided, do not guess, ask before building

1. **Terrain→height mapping**: which of civ-txt's terrain types get real extruded height, and how much? (Needs a registry field analogous to mutaclysm's own `walkable` column, or a new one, e.g. `height=`.)
2. **Entity extrusion shape**: billboard (flat sprite that always faces camera) or true AABB (solid textured block) for cities/units/selector? `extrude-emoji.md` itself flags this as unresolved even for mutaclysm's own hypothetical fix.
3. **`emoji_mode`'s own key binding** for the widget, since mutaclysm's own `'e'` is unavailable (repurposed for camera yaw within POV mode) — pick a free key or a numbered command.

**RESOLVED (2026-08-02), direct instruction**: all 4 camera modes are needed regardless of board size — *"it needs all the pov and camera views, if for no other reason than debugging. later they maybe locked but not for debug."* So tactics-txt's fixed 10×10 board gets the full 1-4 camera set too, same as civ-txt — not scoped down. **New, separate open item this creates**: a future *player-facing* build may want to lock some modes to a fixed default (e.g. always mode 4 for normal play) while keeping all 4 available in a debug/dev build — the lock mechanism itself (a config flag? a build-time switch? a runtime dev-mode toggle?) is not yet designed and should be treated as its own later decision, not assumed to be any particular shape yet.

---

## 7. CAMERA CLAMP + SELECTOR CURSOR (2D-mode viewport, still needed even with 3D added)

Even with real 3D raymarching added (§2-3 above), `render_mode==0` (2D flat) is still a real, first-class mode the widget supports — so the 2D viewport/camera-clamp system is still needed, not replaced by 3D. Ported near-verbatim from mutaclysm's own `ops/compose_frame.c` (camera clamp) and `ops/move_player.c`/`ops/choice.c` (xlector cursor), per this session's own earlier research:

- **Camera (2D mode)**: viewport rect (size TBD per board — tactics-txt's board is a fixed 10×10, so likely the whole board always fits; civ-txt's board can be arbitrarily large per `map_scale`, so the clamp matters there) follows an anchor, clamped so it never scrolls past the board edges (`cam_x = clamp(anchor_x - VIEWPORT_W/2, 0, map_w - VIEWPORT_W)`, and the same for Y — exact math already proven in `ops/compose_frame.c:829-838`). In this widget, the anchor is always the selector cursor (no "hero" concept to branch on, unlike mutaclysm's hero-vs-cursor anchor switch).
- **Selector cursor** (the xlector equivalent — call it "selector" for this widget, avoiding mutaclysm-specific naming): arrow keys move it, clamped to board bounds, uncollided (pure viewing/selection — no collision to check, since there's no player-piece walking around this board). Enter selects the tile (writes a `SELECT_TILE:<x>:<y>` cmd-bus entry per `BOARD_WIDGET_ARCHITECTURE.md` §5, and/or shows a "what's here" readout locally). Escape is a no-op in this widget (no "mode" to exit — the widget IS always in cursor-navigation mode; unlike mutaclysm's dual hero-movement/interact_mode split, there's no hero-movement mode to fall back to here).
- **In `render_mode==1` (3D)**: the selector's position still drives the camera in `camera_mode` 1/2 (follow-style presets) the same way mutaclysm's own xlector anchor-switch works (`ops/compose_frame.c:827-828`'s real pattern: camera follows whichever entity is "live" — here, always the selector, never a hero). `camera_mode` 3 (free-roam) detaches the camera from the selector entirely — see open question §6.4 in this doc for whether tactics-txt's small fixed board even needs this.

---

---

## 8. HEADLESS GL VERIFICATION — real, working precedent, ported for this widget (added 2026-08-02)

Direct instruction: build test tooling using "receipts and even png drops" for the GL window, since an agent has no way to look at a live GLUT window directly. This is NOT a new idea to invent — mutaclysm already has a real, working, live-tested version of exactly this:

- **Receipt files** (plain text, cheap to check programmatically): `ops/compose_rgb_frame.c` writes `pieces/display/rgb_frame.receipt.txt` (frame dimensions + a checksum) alongside the raw pixel buffer `rgb_frame.raw` it produces. `system/gl_mirror.c` (`write_gl_display_receipt()`, `:453`, body `:204-227`) separately writes `gl_display.receipt.txt` after actually uploading a frame to the GPU/window — so comparing the two receipts cross-checks "did the source frame get rendered from" against "did the GPU actually display it," catching the class of bug where the source is correct but the GL window is stuck on a stale frame.
- **Real PNG dump**: `ops/dump_rgb_png.c` is a small, real debug tool (vendored `stb_image_write.h`, no other deps) that converts `rgb_frame.raw` straight into an actual viewable `.png` file — built specifically because, per its own header, "an agent has no way to look at a live GLUT window directly." This has been live-tested in mutaclysm's own tree already, not aspirational.

**Plan for board-viewer**: port `ops/dump_rgb_png.c` (and the two receipt-writing snippets, if not already present in the `chtpm_rgb_render.c`/`gl_mirror.c` binaries board-viewer already copies from wsr-pal — check first, since these may already be compiled in) so the widget's own GL output can be verified by a fresh agent (or this session) purely from the filesystem: run the widget headless-but-with-DISPLAY (`RUN_PROFILE=widget` under a real X server, or Xvfb if none is attached), inject keys into `interact_relay.txt` the same way the earlier nav-mode tests did, then read the receipt files and/or dump a PNG and inspect it — no live human eyes on a GLUT window required for routine regression testing. Keep this CPU-safe per the house's standing rule: always under `timeout`, verify no leftover process afterward.

---

*End of doc. Read `@.apps/BOARD_WIDGET_ARCHITECTURE.md` for the surrounding process architecture (separate widget program, GL window spawn/focus, real-time sync, drag-and-drop re-pairing plan) and `&.widgits/interact-fix-widget.txt` for the nav-mode root-cause investigation — this document is the camera/extrusion/emoji rendering model only.*
