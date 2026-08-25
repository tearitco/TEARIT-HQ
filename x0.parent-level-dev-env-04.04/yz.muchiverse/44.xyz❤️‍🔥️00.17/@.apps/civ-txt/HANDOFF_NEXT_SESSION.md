# Handoff: civ-txt — P1 Skeleton + Real Board Widget (updated 2026-08-02)

**From:** Sonnet 5
**To:** Next agent picking up civ-txt — assume ZERO context, read this whole doc first.

## ⚠️ This handoff supersedes ALL earlier versions of itself. The old "View Map" href-based screen is GONE.

An earlier version of this doc described `map.chtpm`/a same-session "View Map" screen as the current-but-wrong implementation. **That implementation has been fully deleted** (`map.chtpm`, `map_module.pal`, `civ_compose_frame.c`'s old map render branch — all removed). civ-txt's board is now shown by a **real, separate, working GL-window widget** — `&.widgits/board-viewer` — spawned as its own OS process, not a screen switch. This is fully built, live-verified, and working as of this handoff. **Read `&.widgits/BOARD_WIDGET_PROGRESS.txt` and `&.widgits/!.HOUSE_STDS.md` before touching anything board/widget-related** — they capture the full story and every real bug found/fixed building this.

---

## 1. WHAT'S BUILT AND WORKING

### 1a. civ-txt itself (P1 skeleton, unchanged in scope from earlier — still no P2+)
- Real setup screen (Victory/Map/Combat, independently selectable) → `CONFIRM_START` → real navigation to main screen → real `END_TURN` loop with ledger logging. Same proven pattern as `@.apps/my-chara-txt`.
- **New since the last handoff**: `CONFIRM_START` now ALSO generates a real terrain grid (`pieces/system/board.txt`) — a real 10×10 (small) or 14×14 (medium) grid of terrain glyphs (`.`=plains, `f`=forest, `^`=hills, `~`=water, `C`=capital at board center), not a placeholder. This is what the board widget actually reads and renders.
- `OPEN_BOARD_WIDGET` command (in `civ_menu_input.c`) spawns `&.widgits/board-viewer/button.sh run-widget <civ-txt real root>` via `setsid` (required — plain `system()+&` does NOT create a new process group, and an earlier bug had a `timeout` wrapping civ-txt's own test run cascade-kill the widget). This is a real numbered METHOD row on the main screen (`main.chtpm`'s own `${piece_methods}`), NOT a `<button href>` — civ-txt's own screen never changes when you open the board.

### 1b. The board-viewer widget (separate project, `&.widgits/board-viewer/`) — the big new piece
A fully real, working, camera-controllable board viewer with an actual 3D raymarch view:
- **2D mode** (default): real emoji-textured terrain rendering, camera-follows-selector panning (clamped to board edges), arrow-key selector movement.
- **Nav mode**: a real, static `<button onClick="INTERACT">` in the widget's own layout (the shared engine's reserved mechanism — do NOT try to reinvent this via a piece.pdl row, it structurally can't work, see `&.widgits/interact-fix-widget.txt`). Enters/exits cleanly, ESC always works.
- **3D mode** (`'0'` key): a REAL per-pixel raymarcher (Amanatides & Woo grid-DDA, ray/AABB intersection, real voxel-texture sampling from `pieces/registry/emoji_assets/<hex>/voxels_16.csv` — NOT a rasterizer, NOT solid colors dressed up as 3D; this was explicitly asked for and verified). All 4 camera modes work (`'1'`-`'4'`): first-person, third-person, free-roam, bird's-eye — full `q/e/r/t/w/a/s/d/c/v/f` key scheme ported from mutaclysm. A legend/reference cube (mutaclysm's own X+red/X-blue/Y+green/Y-brown/Z+white/Z-dark convention) renders at the board's origin corner as an orientation aid.
- **2D and 3D show visually consistent textures** — both read the same real, on-demand-generated voxel assets keyed by the same emoji codepoints.

**Full architecture/investigation docs** (all in `&.widgits/`): `BOARD_WIDGET_ARCHITECTURE.md` (process architecture), `5-pov-widgit.md` (camera/rendering model, full key table), `interact-fix-widget.txt` (nav-mode mechanism), `view-vs-muta.md` (the rgb_frame.raw race fix + camera orientation bug fixes), `!.HOUSE_STDS.md` (general reference for building another widget like this), `BOARD_WIDGET_PROGRESS.txt` (full chronological build log — read this for the *story* and every real bug hit).

## 2. REAL BUGS FOUND AND FIXED THIS SESSION (relevant if you touch the shared engine or any menu_input op)

1. **CORRECTED, real mistake caught same day**: `civ_menu_input.c`'s own `(key-'0')-1` digit formula was briefly suspected (and even "fixed") as an off-by-one bug — it is NOT a bug. `chtpm_parser_pal.c`'s own `${piece_methods}` button generator starts its internal `method_idx` at 2 (not 0/1), so the FIRST numbered row is really sent as raw key `'2'`, and `(key-'0')-1` correctly compensates. The "fix" was reverted before shipping. Full corrected writeup: `&.widgits/!.HOUSE_STDS.md` §A.3. **Do not touch this formula.**
2. **`'q'` used to quit the whole GL window** unexpectedly (a leftover exception to the house-wide "never use q to quit" rule, in `gl_mirror.c`) — fixed at the source (`014.wsr-pal💸️📌️+2/system/gl_mirror.c`), affects every project using that shared binary, including civ-txt's own GL mirror if it uses `'q'` for anything.
3. **A real `rgb_frame.raw` write-write race** between the shared `chtpm_rgb_render` daemon and any project's own 3D-render op — real fix is writing a separate overlay file, not suppressing the daemon (can't be suppressed). See `view-vs-muta.md`.
4. **2D emoji rendering silently fell back to flat colors** because board-viewer had no `pieces/registry/terrain/terrain_types.txt` registry file — the generic on-demand emoji path is real but less reliable than the hand-curated registry path every other project (including mutaclysm) actually uses. Fixed by adding that registry file. **civ-txt itself doesn't render emoji directly (it's ASCII-only), so this doesn't affect civ-txt's own rendering — only board-viewer's.**

## 3. WHAT'S NOT BUILT (still true, unchanged scope)

Everything from P2 onward per `CIV_TXT_DESIGN.md` §10: cities (`FOUND_CITY`), tech tree, units/movement, AI civs + sequential turn dispatch, combat resolution, diplomacy, victory checking, the automation/decision_mode layer (my-chara-txt has one, civ-txt doesn't). **None of this is started.** The board widget now shows REAL terrain, but there are still no cities/units to place on it — that's real P2+ work, not yet begun.

**Recommended next step for civ-txt itself**: `CIV_TXT_DESIGN.md`'s own P2 ("Found city, basic production queue, `cities.chtpm`"). Once cities/units exist, the board widget's own selector/`SELECT_TILE` command-bus hookup (`BOARD_WIDGET_ARCHITECTURE.md` §5) becomes meaningful — right now selecting a tile in the widget doesn't do anything back in civ-txt, since there's nothing to found/move yet.

**Recommended next step for the widget family**: tactics-txt gets its own board-viewer wiring next (same shared widget, paired via focus argument the same way civ-txt is) — see `tactics-txt/HANDOFF_NEXT_SESSION.md`.

---

*End of handoff. Game state was left fresh (setup screen, no options picked) — check `pieces/system/config.txt` before assuming anything about progress. The board widget is real and working; test it via `&.widgits/board-viewer/button.sh run-widget <civ-txt-root>` or through civ-txt's own main-screen "Open Board Widget" numbered action.*
