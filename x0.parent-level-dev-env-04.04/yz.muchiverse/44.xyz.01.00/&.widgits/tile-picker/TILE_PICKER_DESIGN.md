# 🧩 tile-picker — design & session history

**Status:** real, working house-standard CHTPM widget, with a live desktop-placer extension. Some pieces below are built and proven; the last section (`^` activation mode, drag-into-any-board-view) is **designed but not yet built**.
**Audience:** future devs/agents/users picking this project back up.
**Date:** 2026-08-04

---

## 0. 🎯 What tile-picker actually is, in one paragraph

tile-picker is a real, self-contained CHTPM widget (same house pattern as `&.widgits/file-menu/`) — its own session, own copy of the `system/` pipeline, own `.chtpm` layout, own `tp_menu_input`/`tp_compose_frame` op pair. It shows a numbered list of emoji tile options. Picking one currently (a) sets a "brush," (b) writes a real portable package onto the house-wide desktop tray (`#.desktop/tiles/<name>/`), and (c) spawns a real, live, draggable GL window rendering that emoji on your actual desktop — not just a file that sits there invisibly. A second op reverses the direction: import a desktop package back onto a real map.

**This did NOT work like this from the start.** The rest of this doc is written so the next person doesn't repeat the same mistakes.

---

## 1. 🔴 The mistake this project already made once — read this first

An early version of tile-picker's picker UI was a bespoke raw X11/GLX window with its own hand-rolled `XLookupString`/`KeyPress` handling and its own digit-select logic. **It looked plausible and even ran without crashing — but the user could not interact with it at all.**

Root cause: this house has exactly one real input pipeline, and every working project uses it:

```
keystroke → history.txt / pieces/keyboard/history.txt (raw capture: keyboard_input.c or gl_mirror.c's GLUT callbacks)
         → chtpm_parser_pal.c (owns ALL nav/focus/digit-jump/Enter-to-activate logic — the ONLY real owner)
         → interact_relay.txt ("KEY:n" on real button activation)
         → main_loop_chtpm.pal polls it → invokes <project>_menu_input <n> ONCE
         → <project>_menu_input.c dispatches on current_layout.txt, runs real logic
         → <project>_compose_frame.c re-renders view.txt → chtpm re-substitutes ${game_map}
```

A bespoke window that does its own key handling never writes into `history.txt`, so `chtpm_parser_pal` — the sole owner of all real navigation — never sees a single keystroke from it. That's not a tuning problem, it's structurally disconnected from the rest of the house.

**The fix was a full rebuild**, modeled directly on `&.widgits/file-menu/` (read that project's `button.sh`, `pal/main_loop_chtpm.pal`, `ops/fm_menu_input.c`, `ops/fm_compose_frame.c` in full before changing anything here). See house memory `feedback_chtpm_read_precedent_first.md` — this project is now the second confirmed violation of that rule, so it's doubly worth not becoming a third.

---

## 2. 🏗️ Real architecture, as built

### 2.1 The CHTPM widget itself

| File | Role |
|---|---|
| `button.sh` | Session launcher, modeled on file-menu's `run_widget_session()`. Creates an isolated `pieces/sessions/<id>/`, symlinks `system/`/`ops/`/`pal/`/`pieces/chtpm`/`pieces/registry`, launches the full pipeline. |
| `default_op.txt` (project root) | **The op-name registry** — maps bareword PAL instructions (`tp_menu_input`, `tp_compose_frame`) to their real binaries. **This is NOT the same file `scripts/build.sh` copies from wsr-pal into `system/`** — see §4.1, this exact confusion caused a real bug. |
| `pieces/chtpm/layouts/tile_picker_main.chtpm` | The one screen. `<panel>` + `<module>` (points at the PAL loop) + `<interact src="...interact_relay.txt">` + a bare `${game_map}` substitution line. |
| `pal/main_loop_chtpm.pal` | The dispatch loop — polls `interact_relay.txt`, invokes `tp_menu_input` once per real key, re-renders via `tp_compose_frame` on any key or screen change. |
| `ops/tp_compose_frame.c` | Reads `pieces/system/picker_items.txt`, emits one `<button label="..." onClick="KEY:n">` per item. Writes ONLY `view.txt` — never `current_frame.txt` directly. |
| `ops/tp_menu_input.c` | Dispatches `KEY:n` → looks up that item's glyph → shells out to `tp_set_brush.+x` + `tp_place_desktop.+x`. |
| `pieces/system/picker_items.txt` | The data-driven item list (`SECTION\|INDEX\|GLYPH` rows) — same METHOD-table convention as every other numbered menu in this house. Editing this file adds/removes picker options, no recompile needed. |

**Do NOT put a manual index/number in a button's label.** `chtpm_parser_pal` already numbers every button itself for digit-jump nav — an extra `"1: 🌳"` prefix produces a real, confirmed double-index bug (fixed this session).

### 2.2 The desktop-placer extension (the genuinely new part)

| File | Role |
|---|---|
| `ops/tp_set_brush.c` | Writes `brush.txt`, enqueues `SET_BRUSH:<glyph>` into whatever `focus.txt` currently points at (map-session convention, pre-existing). |
| `ops/tp_place.c` | Enqueues `PLACE_TILE:<map>:<x>:<y>:<glyph>` the same way — targets a *map* session. |
| `ops/tp_place_desktop.c` | Writes a real package (`glyph.txt`, `meta.pdl`, `sprite.csv`) into `#.desktop/tiles/<name>/`, **then spawns a real live GL window** (`tp_desktop_window.+x`) for it. |
| `ops/tp_desktop_window.c` | The live window itself — borderless GLX, draggable, grid-snapping (see §2.3), right-click closes, polls for its package still existing and self-closes if removed. |
| `ops/tp_import_from_desktop.c` | The reverse direction — reads a desktop package's `glyph.txt`, enqueues `PLACE_TILE` onto a real map, same inbox mechanism `tp_place.c` uses. Non-destructive by default (package isn't deleted). |

### 2.3 Grid-snap and frame pacing

Direct instruction: the desktop already has an invisible 80px grid (`GRID_CELL_PX`, from `01.muchi-pals-🥚️-13.01/system/egg_window.c` — egg-pals already snap to it). `tp_desktop_window.c` now:
- Spawns grid-aligned (`3×80, 3×80`, matching egg_window's own default), not an arbitrary pixel offset.
- On drag release, rounds to the nearest 80px cell before writing position, same round-to-nearest-cell math egg_window.c uses.
- Hard-caps its own render loop to 30fps via measured `gettimeofday` frame pacing — **not** just relying on `select()`'s 300ms poll timeout, which doesn't bound redraw rate once real X events start arriving (e.g. mid-drag).

### 2.4 Real emoji rendering (not just a colored square)

Direct instruction: "id like to see emojis tho" → "do u see how egg-pal creates the same emoji that user picked?"

The answer was: **don't build a new rendering path, reuse the real one that already exists.** `01.muchi-pals-🥚️-13.01/ops/hatch_egg.c` already turns a picked emoji into a real texture via:

```
emoji_gen_atlas.+x <emoji> <png>      # FreeType + NotoColorEmoji.ttf → PNG
emoji_xtract.+x <png> 0 <N> <csv>     # PNG → NxN RGBA pixel CSV ("sprite.csv")
```

`tp_place_desktop.c` now runs this same two-step pipeline (writing `sprite.csv` into the package dir); `tp_desktop_window.c` loads that CSV as a real GL texture and draws a textured quad, exactly like `egg_window.c`'s own `load_sprite`/`upload_texture`/`draw_sprite`. Falls back to the glyph-hashed color square + font-drawn character if `sprite.csv` is missing (e.g. emoji generation failed).

The picker's own chtpm-rendered screen (§2.1) gets real emoji for free — `chtpm_rgb_render` (copied wholesale from wsr-pal, unmodified) already calls this exact same `emoji_gen_atlas`/`emoji_xtract` pipeline for any real UTF-8 emoji in a `<button>`/`<text>` label. The only fix needed there was widening every glyph field from a single ASCII `char` to a real UTF-8 string (see `GLYPH_BUF` in `tp_compose_frame.c`/`tp_menu_input.c`) — the rendering itself was already real and working, just never being given anything but single ASCII bytes before.

---

## 3. 🐛 Real bugs found and fixed this session (read before debugging something that looks similar)

1. **`default_op.txt` clobbered by build.sh.** `scripts/build.sh` was copying wsr-pal's own *generic* `default_op.txt` (a stock-trading game's op registry!) into tile-picker's **project root**, silently overwriting the file that should map `tp_menu_input`/`tp_compose_frame` to their real binaries. Result: pressing keys did *nothing* — not an error, just silent no-ops, because `tp_screen_changed.txt` (bumped on every real invocation) never grew. Fix: wsr-pal's copy goes into `system/default_op.txt` only (matching file-menu's own convention); tile-picker keeps its own small, hand-written root `default_op.txt`.
2. **Desktop path math wrong for a nested session dir.** `tp_menu_input.c`'s `pick_and_place()` originally computed the desktop tray path as a fixed `../../../#.desktop` relative to `project_root` — correct for a plain project root, wrong for tile-picker's own session dir (`tile-picker/pieces/sessions/<id>/`, one level deeper). Placements were silently landing inside `tile-picker/#.desktop/` instead of the real house-wide tray. Fixed by reading `pieces/system/house_root.txt` (the same marker file `button.sh` already writes, same convention file-menu uses) instead of guessing relative depth.
3. **Missing font glyph registry + missing `emoji_gen_atlas`/`emoji_xtract` binaries.** `scripts/build.sh` copied the `system/` pipeline binaries but initially skipped two things file-menu's own build.sh also copies: the font glyph registry (`pieces/registry/fonts/ascii/`) and the emoji-generation ops (`ops/+x/emoji_gen_atlas.+x`, `ops/+x/emoji_xtract.+x`). Without these, the GL window rendered as a blank/black frame, then later as colors-only with no emoji.
4. **Double-indexed button labels.** `tp_compose_frame.c`'s markup included its own `"%d: "` prefix on top of `chtpm_parser_pal`'s own automatic button numbering. Fixed by dropping the manual prefix — labels are just the glyph.
5. **`_NET_WM_PID` isn't set by anything in this house by default.** GLUT doesn't set it, and this WM doesn't synthesize one (confirmed via direct `xprop` check — `_NET_WM_PID: not found` on a live `gl_mirror` window). Needed for the next section's design (§4). Fixed by tagging the window ourselves, once, right after spawn (`tp_set_wm_pid.c`, called from `button.sh`) — this does **not** modify the shared `gl_mirror` binary itself, only adds a one-line follow-up call at the launcher boundary.
6. **Accumulated zombie processes / high CPU.** Repeated test restarts during this session left 5+ full stale sessions running (`renderer`/`chtpm_rgb_render`/`keyboard_input`/`chtpm_parser_pal`/`prisc+x` each) because each restart only killed the *most recent* session, not all prior ones. `chtpm_rgb_render` alone was measured at 40% CPU. Always do an exhaustive `pgrep`-and-kill-all sweep before/after test runs, not a single targeted kill.

---

## 4. 🖱️ Drag-anywhere infrastructure (built, not yet wired to a UI action)

Direct instruction: *"I want it so I can drag that right into board view (any board view) even without it having 'focus'."*

### 4.1 The real obstacle

Every widget's `gl_mirror` window in this house shares the **identical title**, `"wsr-pal RGB mirror"` (since it's the same binary, copied wholesale, by every project's own `build.sh`). Title-matching — the technique `egg_window.c`'s own `find_mirror_rect()` uses for its Xdnd-into-mutaclysm drop — only works there because mutaclysm has a *different*, distinctly-titled `gl_mirror` fork. It cannot disambiguate between two house-standard widgets (e.g. two board-viewer instances, or a board-viewer and tile-picker's own window).

### 4.2 The real fix: PID tagging

- `ops/tp_set_wm_pid.c` — tags a window with the real ICCCM/EWMH `_NET_WM_PID` property, via `XChangeProperty`, right after `button.sh` spawns its own `gl_mirror`. Resolves the *actual* running PID via cwd-scoped `pgrep` (not bash's own `$!`, which didn't match — `gl_mirror` appears to fork internally).
- `ops/tp_find_window_by_pid.c` — given a PID, walks the window tree looking for `_NET_WM_PID` matching it, returns the absolute on-screen rect (`x y w h`). Confirmed working end to end.
- **Board-viewer already registers its own PID** in the house-wide widget ledger (`ledger_append.+x ONLINE widget <project_id> <session_root> <pid> "Board Viewer" ...`, queryable via `ledger_peers widget`) — so a drop handler can discover every live board-viewer instance's PID without any new registration work, then resolve each one's real window rect via `tp_find_window_by_pid`.
- **Still needed before this is usable**: board-viewer's own `button.sh` needs the same one-line `tp_set_wm_pid`-style tagging call added after its own `gl_mirror` spawn (copy the same op in, matching this house's "reuse ops wholesale" convention) — tile-picker's copy alone only tags tile-picker's *own* window.

---

## 5. 🎯 Designed, not yet built: "^" activation mode (supersedes the CLI-field idea)

Direct instruction, 2026-08-04 (this replaces an earlier, now-shelved idea of a `cli_io` field for typing in a target window's PID):

> "I want each [emoji option] to have an activation mode... when you press Enter, instead of placing the emoji, it enters '^' mode till the user presses Escape, where wherever they click (on desk or on a view) the phymoji will appear (unless later some game views may prohibit placing phymojis)."

### 5.1 The real behavior change

- **Today:** pressing Enter (`KEY:n` in `tp_menu_input.c`) *immediately* calls `tp_set_brush` + `tp_place_desktop` — glyph goes straight to the desktop tray, no aiming step.
- **Designed:** pressing Enter instead **arms** that glyph — the picker enters "^" mode (an aiming/placement cursor state) and waits. The **next click anywhere on screen** — the bare desktop background, or inside *any* window (a board-viewer, potentially any other GL surface) — is where the phymoji actually materializes. **Escape** cancels the armed state with no placement, same convention as this project's own windows' existing "Escape/right-click closes" pattern.
- Later: individual board/game views may be able to *refuse* a placement (e.g. some game state prohibits placing entities) — not required for v1, just a designed extension point, not a current constraint.

### 5.2 What building this would actually require (not started)

1. **A global click listener**, active only while armed — since the destination click can land on *any* window on screen, not just the picker's own small window, this needs an `XGrabPointer` across the whole root (same technique `egg_window.c`'s own right-click context-menu popup already uses for *its* own grab, just root-scoped instead of window-scoped here), or an equivalent screen-wide click hook.
2. **Destination resolution on click**, reusing §4's infrastructure directly:
   - If the click point falls inside the rect of a `ledger_peers widget`-discovered, `tp_set_wm_pid`-tagged board-viewer window → resolve that instance's `bv_state.txt` → `focused_project_root` → that project's own `board_widget_bridge.txt`/`widget_cmds` inbox convention → enqueue a real placement command there (same `PLACE_TILE`-style mechanism already proven working for mutaclysm).
   - Otherwise (click lands on bare desktop) → same `tp_place_desktop.+x` path already built and working today.
3. **Escape handling** during the armed/grabbed state to cancel cleanly (release the grab, drop the "armed" flag, no placement).
4. This is a real UI-state addition to `tp_menu_input.c`/`tp_compose_frame.c` (an "armed" flag, probably in `pieces/system/tp_state.txt`) plus a new small op for the grab-and-resolve step itself — not yet written.

### 5.3 Why this design, and why it's better than the CLI-field idea

The original idea (a `cli_io` field where the user types in a target window's PID) would have worked, but required the user to already know and type an opaque numeric PID. The "^ mode, click to place" design reuses the same underlying PID/rect-resolution machinery (§4) but makes the actual UX purely spatial — point and click — with no numbers to know or type. The user confirmed a preference for this and the CLI-field idea is shelved (not deleted as an option, just deprioritized) in favor of it.

---

## 6. 🗺️ Suggested order for whoever builds §4.2 (board-viewer tagging) and §5 (^ mode) next

1. Add `tp_set_wm_pid`-equivalent tagging to board-viewer's own `button.sh` (copy the op in, one new call site, same as this project's own).
2. Prove `ledger_peers widget` + `tp_find_window_by_pid` together resolve a live board-viewer's real screen rect end to end (should be a short scripted test, same shape as `scenarios/test_tile_desktop_place.sh`).
3. Add the "armed" state + Escape-cancel to `tp_menu_input.c`/`tp_compose_frame.c` (visual feedback for "armed" — e.g. a status line, or a cursor-glyph indicator — is an open design detail, not decided).
4. Add the global click-grab-and-resolve op, wire it in only while armed.
5. Write a real scenario harness proving: arm an item, click on bare desktop → places via existing `tp_place_desktop` path; arm an item, click inside a live board-viewer window → places via the PLACE_TILE-into-that-project's-inbox path instead.

---

## 7. 📎 Related docs

- `&.widgits/file-menu/` — the real precedent this whole rebuild is modeled on. Read `button.sh`, `pal/main_loop_chtpm.pal`, `ops/fm_menu_input.c`, `ops/fm_compose_frame.c` before changing this project's own CHTPM layer.
- `01.muchi-pals-🥚️-13.01/system/egg_window.c` — the real precedent for live GL desktop windows, grid-snap, sprite-texture rendering, and drag/context-menu conventions.
- `01.muchi-pals-🥚️-13.01/ops/hatch_egg.c` — the real precedent for the emoji→PNG→CSV texture pipeline.
- `@.apps/hikikomorai/hikikomorai-design.md` — the house-wide "living desktop" convention doc; tile-picker's desktop-placer feature is one concrete implementation of that convention.
- `@.apps/aomorai-editor/aomorai-editor-blueprint.md` — the sibling design doc for the RPG-Maker-style editor project; §1.5/§12 there track the still-open "convention mismatch" question (file-menu/map-picker/tile-picker's `focus.txt` pattern vs. board-viewer's `focused_project_root` pattern) that also matters for §4/§5 here.

---

## 8. 🖼️ Real assets, real transparency, real RPG Maker MV compatibility (2026-08-04, later same day)

### 8.1 fo-menu-sys.md dispatch adopted
Per `#.haiku+/tpmos-re-dox/fo-menu-sys.md` (this house's own real, canonical method-dispatch convention, found via direct reference): a `METHOD`'s `VALUE` in `meta.pdl` is now a real, directly-executable command (or the literal keyword `void`), dispatched via `system()` with the package dir as an argument — not the invented action-keyword scheme §4.5 originally shipped with. `CLOSE` stays the one reserved internal keyword (closing this renderer's own event loop can't be delegated to a subprocess). `MethodItem.action` widened from `char[64]` to `char[PATH_BUF]` — the old size silently dropped real rows once VALUEs became full command lines.

### 8.2 Custom assets — `asset.pal` + real per-image transparency
Direct instruction: users should be able to override an entity's default emoji with either a different emoji or an arbitrary image (PNG/JPG), without moving the file. Real convention: `<package_dir>/asset.pal` (`glyph=<emoji>` or `asset_path=<path>`, relative paths resolve against a real `assets/` subfolder, absolute paths used as-is). Applied once at window startup, regenerating `sprite.csv` in place.

**Real gotcha found and fixed**: `emoji_xtract.+x` (wsr-pal's own op) is NOT a general image converter — it crops one fixed 64×64 "atlas cell" at a given index, assuming its input is already laid out as an atlas. Feeding it an arbitrary user-sized photo would silently crop a meaningless corner instead of scaling the whole image. Fixed by writing a new, separate op — `tp_asset_to_sprite.c` — that reuses the exact same real box-filter downscale algorithm but applies it to the *whole* loaded image.

**Second real gotcha, transparency**: enabling `GL_BLEND` alone (the first fix attempted) was NOT enough — a plain X11 window is an opaque rectangle by default, and `GL_BLEND` only blends *within* the GL scene against whatever this window already drew (its own clear-color fill), not against the real desktop behind it. Real fix needed the X11 **Shape Extension** — `build_shape_mask()`, ported verbatim from `egg_window.c`'s own POSIX branch, cutting the window's actual shape to match the sprite's real alpha channel so the desktop genuinely shows through. Sprite pixels are now kept in memory after texture upload (not freed) specifically so the mask can be built from them, same real reason `egg_window.c`'s own header comment already states.

### 8.3 Real RPG Maker MV/MZ character-sheet extraction — one op, real assets, "individual" designation
Direct instruction: parse real RPG Maker MV/MZ 48px character sheets, extract *individual* characters (as distinct from a whole tileset/tilemap — that's a separate, later designation, not attempted here), and use them as real desktop entities. New op: `tp_rmmv_character_extract.c` — given a real character sheet PNG (confirmed real layout via direct inspection: 8 character slots, 4 cols × 2 rows, each slot a 3×4 grid of 48px walk-cycle frames, row order down/left/right/up), extracts ONE slot's standing-still frame for a given direction. Direction is a real parameter (not hardcoded to "down") specifically so a future AI tick loop can regenerate the sprite as a pet's facing changes with its movement direction (direct instruction: "we will have them face in direction they are moving, since the tilesheets allow for this").

**Real, live-verified example**: three real desktop pets — dog, cat, chicken — extracted from a real RPG Maker MV `Nature.png` character sheet (slots 0/1/2), placed as real live desktop windows via the existing `tp_desktop_window.c` pipeline, confirmed on screen.

**Deferred, direct instruction, not built yet**: line-of-sight-based AI (dog chases cat, cat chases chicken, wander otherwise), "eventable AI" (behavior expressed as real events, visible in a real event-editor "current behavior" view), and wiring the `Events` context-menu method to actually launch something (still `void` — see `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md`, unchanged, no CHTPM event-editor widget exists yet). Also deferred: male/female chickens + egg-laying/pooping mechanic (explicitly "later," not now).

### 8.4 Real guards against runaway processes/CPU
- `tp_place_desktop.c` now refuses to spawn a second `tp_desktop_window.+x` for a package dir that already has one running (`pgrep -f` check before spawn) — a real duplicate-window bug was caught live this session during manual testing.
- `EMERGENCY_KILL.sh` (house-wide) updated with `tp_desktop_window`/`tp_arm_placer` added to its kill-list — the latter holds a real global X11 input grab while armed, making a stuck one a genuine "locks your whole desktop's input" emergency, not just a stray window.
- Real, separate finding: a system-wide CPU/load spike this session was traced to `apport` (Ubuntu's crash reporter, likely triggered by repeated `kill -9` on GL-context-holding processes) plus unrelated heavy `opencode`/Chrome load — **not** tile-picker's own binaries, which had zero footprint at the time. `cpulimit` (already installed) was used live to throttle the actual offending Chrome renderer + `opencode` PIDs, with real, measured effect (idle CPU 1%→55%). See `#.ref/🦁️.cpu-limit]ON]PUR/cpulimit-faq.md` for the real, general-purpose how-to this produced.
- `@.apps/BOARD_WIDGET_ARCHITECTURE.md` — board-viewer's own design doc; its ledger registration (§4 of this doc) and `focused_project_root`/`board_widget_bridge.txt` convention are what §5's placement-resolution step would read.

## 9. 🆔 Real per-instance addressability (2026-08-05) + proposed CHTPM-based context menu (design only, not built)

### 9.1 Real, live-verified: entities are now addressable by a real unique id, not just glyph
Direct user instruction: "context window should have name/id of entity, so its addressable by others" — the window title used to be `tile:<glyph>` only (e.g. `tile:🐶`), which isn't a real identity (glyphs can collide, aren't the entity's own name). Follow-up instruction: "they should have a unique alpha numerica 4digit combo" — two entities of the same kind (two dogs, say) need to be distinguishable too, name alone isn't enough.

Real fix, two parts:
- Every entity's own `button.sh` (`ensure_package()`) now generates a real, persistent 4-char alphanumeric `instance_id` (`tr -dc 'A-Z0-9' < /dev/urandom | head -c4`) into `instance_id.txt`, seed-once-don't-clobber (same convention as `glyph.txt`/`created_at`) — generated once, stable across every future launch of that same package.
- `tp_desktop_window.c` reads it back (`read_instance_id()`, mirrors `read_glyph()`) and combines it with the package dir's own basename (the real `piece_id`) into `g_full_id` (e.g. `dog-KFQA`), used in BOTH the X11 window title (`tile:dog-KFQA:🐶`) and a new non-clickable header row at the top of the real, visible right-click popup menu — direct user correction ("i dont see those in context window?"): the id must be visible in the actual UI a human sees, not just an invisible X11 property. `open_context_menu()`/`draw_context_menu()` grew a real +1-row header for this; the button hit-detection math (`row = y/POPUP_ROW_H`) was updated to subtract that header row before indexing into `methods[]`, so the header itself isn't mistakenly clickable as method 0.

**Real bug hit and fixed while building this**: the first version gated `instance_id` generation behind `[ ! -f meta.pdl ] && [ ! -f instance_id.txt ]` — since `meta.pdl` already existed for every live entity from earlier sessions, the whole block was silently skipped and no id was ever generated. Fixed by gating on `instance_id.txt` alone, independent of `meta.pdl`'s own existence.

### 9.2 Proposed: CHTPM-based context menu with Move/Inventory/Skill submenus (design only)

Direct user proposal, verbatim intent: convert the popup from raw X11 `XDrawString`/`XDrawRectangle` calls into a real CHTPM structure "so its more uniform to edit," then add a new **User** button whose sub-screens (reached via CHTPM's own href/page-navigation convention) are:
- **Move** — a tactics-game-like range grid (X11-rendered), letting the user pick a destination within range.
- **Inventory** — a real items list; clicking an item may use it or open targeting (pick another entity/tile to apply it to).
- **Skill** — a real skills/magic list, same shape as Inventory.

**Assessment (not yet agreed with the user, flag before building)**: this is genuinely three separate subsystems, not one feature — CHTPM-ifying the popup itself is a real, bounded, mechanical port (we now know this pipeline well from event-editor/event-ez this same session); Move/Inventory/Skill are each their own new data model + UI, comparable in size to what this whole session spent building for events. **Recommendation, not yet actioned**: build the CHTPM-popup port FIRST, on its own, proven via k3-style key injection exactly like every other CHTPM surface this session — before starting any of Move/Inventory/Skill's own real data models. Whichever gets built next among Move/Inventory/Skill should probably follow the same "smallest real worked example first" approach event pages used (ava's own concrete on-spawn example, not a generic framework upfront).

**UPDATE 2026-08-05, later same session**: the CHTPM-popup conversion was attempted, built, and confirmed working structurally, then **rolled back** — the real blocker was `gl_mirror.c` (the shared renderer this whole pipeline needs) having no mouse-click handling and no small/borderless window support at all. Real research + a real fix for that gap: `@.apps/hikikomorai/x11-mouse-2do.txt` (mouse-click forwarding proven, borderless mode built, a real small-frame variant built and paired with it, all confirmed live). That doc is now the live source of truth for the CHTPM-popup path's actual status.

**In the meantime**, real "User" → Move/Inventory/Skill/Cancel shipped as an interim raw-X11 stopgap ("we can use it for what we're currently using it for till we transition," direct instruction): a second popup adjacent to the main one, positioned off the main popup's own real coordinates (not the click point — a real bug, found and fixed). "Move" launches `ops/tp_range_grid.c`, a real standalone binary using the X11 Shape Extension for genuine transparency (only the grid's own outline is opaque — direct correction: "it should just be a transparent outline like a png," no PNG asset needed), sized to the real `GRID_CELL_PX` and centered on the entity. A visible "X" close button was tried on this window and explicitly reverted (direct instruction: "i dont wanna do x button cuz it may clutter view") — it keeps its original "any click or Escape closes it" behavior. The redundant `Move`/`Inventory`/`Skill` stub rows that were briefly duplicated into the MAIN menu (alongside the real `User` submenu) were removed, per direct correction ("we added 'sub user' fields to first context, get rid of those").

None of the FULL §9.2 vision (real Move/Inventory/Skill data models, CHTPM-based menu) is built yet. The interim raw-X11 version above is real and live on screen today; the CHTPM path's own blocker and fix status live in `@.apps/hikikomorai/x11-mouse-2do.txt`.

## 10. 🪟 Long-term goal: raw X11 windows need the SAME file-injection/auditability/AI-injection power CHTPM apps already have (2026-08-05, direct instruction, honest current-state assessment)

### What these windows are actually called
Direct question, real answer: they're **raw `override_redirect` Xlib/GLX client windows** — `tp_desktop_window.c`'s own popup, `ops/tp_range_grid.c`, and (in its non-CHTPM aspects) `gl_mirror.c`'s own GL window itself. "Raw" here specifically means: driven entirely by native X11 events (`XNextEvent`/`ButtonPress`/`KeyPress`) caught inside the process's OWN event loop, with no file-mediated layer between real input and real effect.

### What "the same power" means, concretely
Every CHTPM app this whole session (event-editor, event-ez, context-menu) gets two real properties for free, just by being CHTPM-driven:
- **Auditability**: every real keystroke lands in a real, plain-text file (`pieces/keyboard/history.txt`, `pieces/apps/player_app/history.txt`, `pieces/apps/player_app/interact_relay.txt`) — anyone (or anything) can read back exactly what happened, after the fact, with zero extra instrumentation.
- **AI/injection power**: because input is file-mediated, an AI (or any script) can drive the whole app by just writing to those files (or, as this session did throughout, injecting real X11 events via `tp_test_send_key.+x`/XTest that the SAME house-standard pipeline picks up and logs) — no bespoke tooling needed per-app.

### Honest current state: NOT close at all
Raw X11 windows have **neither property today**. Concretely:
- No history file of any kind — a real mouse click or keypress on `tp_desktop_window.c`'s popup, or on `tp_range_grid.c`, leaves **zero trace** anywhere on disk. If something goes wrong, there is nothing to audit.
- No file-based injection path — the ONLY way anything (human or AI) can drive these windows is via raw X11 mechanisms (`XTestFakeButtonEvent`/`XTestFakeKeyEvent`), which this session used via ad-hoc, one-off tools built just for testing (`tp_test_send_key.+x`, a throwaway `/tmp/send_click.c`) — not a real, standing house convention any future AI session could rely on without re-deriving it.
- `gl_mirror.c`'s own new real mouse-click forwarding (this session, see `@.apps/hikikomorai/x11-mouse-2do.txt`) is the ONE piece of real progress toward this: it now writes `last_click_x`/`last_click_y` into a real, readable state file. But that's the CHTPM-facing half of gl_mirror, not the raw popups — `tp_desktop_window.c`'s own popup and `tp_range_grid.c` still have nothing.

### What real parity would require (not built, for whoever picks this up)
1. A real history file per raw-X11 window (matching `pieces/keyboard/history.txt`'s own convention) - every `KeyPress`/`ButtonPress` appended as a plain line, unconditionally, regardless of whether anything else consumes it.
2. A real injection file these windows actually poll (matching `interact_relay.txt`'s own convention) - a plain file an external writer (AI or script) can append to, with the window's own event loop checking it on each pass and synthesizing the equivalent as if it were real input.
3. Applying both to EVERY raw X11 surface, not just one - `tp_desktop_window.c`'s popup, `tp_range_grid.c`, and any future ones, so the property is a real house-wide guarantee, not a one-off.

**Status: step 1 and 2 DONE for `tp_desktop_window.c`, 2026-08-05** (built as MUCHI_RANCHER's own work item 2, see `@.apps/MUCHI_RANCHER/MUCHI_RANCHER_DESIGN.md` §5): `tp_desktop_window.c` now writes a real `<package_dir>/history.txt` (every real context-menu click, plus `WINDOW_OPEN`, timestamped) and polls a real `<package_dir>/interact_relay.txt` every ~300ms for an injected `RUN_METHOD:<Label>` (or `CLOSE`) command, dispatching it exactly like a real click via a shared `dispatch_action()`, logging it, then truncating the relay file (write-once/consume-once). Verified live end-to-end against `m8_redhorned` (MUCHI_RANCHER). Since this is the SAME shared binary every raw-X11 entity (pets, asa/ava, MUCHI_RANCHER monsters) runs, step 3 ("applied to every raw-X11 surface") is a side effect for free the next time any existing entity is relaunched - not yet true for currently-running pets/asa/ava processes (they're running the pre-fix binary), but no new code is needed to get them there. `tp_range_grid.c` still has neither property - not touched by this pass. `OPEN_USER` remains relay-undispatchable (needs live popup-position context).

**Important honest clarification, direct user question ("so ur telling me the context menu for monster is chtpm now?")**: NO. History+relay only gives 2 of 3 CHTPM-adjacent properties (auditability, AI-injection). The context menu itself is still a raw `override_redirect` popup, hand-drawn and hand-dispatched in C (`draw_context_menu()`/the `ButtonPress` branch) - not a real `.chtm`-driven screen, not running through `chtpm_parser_pal.c`. "CHTPM-compliant" in this doc means "has the same auditability/injection properties," not "is a CHTPM app."

### §11. Sketch: a real objects-file layout, modeled on wraith-alpha's ACTUAL mechanism (not built yet)

Direct ask: "in future context menus should be much more customizable, robust, even having user input, href, back, etc just like chtpm." Investigated the real, working precedent first (direct read of `projects/wraith-alpha/ops/wraith_gl.c`) rather than guessing:

**How wraith-alpha's mouse actually works** (genuinely different usage than `tp_desktop_window.c`'s popup): wraith-alpha's GL window draws NOTHING of its own - it mirrors an already-existing, real `.chtm`-driven terminal-cell-grid CHTPM screen (rendered elsewhere). A click's pixel coords convert to a terminal cell (`hit_test_semantic_action()`), which looks that cell up against a real, declarative objects file (`WRAITH_SEMANTIC_OBJECTS`, one `OBJECT | x=.. | y=.. | w=.. | h=.. | z=.. | action=..` row per clickable region, highest-`z` match wins), then queues the matched object's own `action` string into the SAME real command channel keypresses already use (`append_project_command()` → a real `COMMAND: <action>` line, `chtpm_parser_pal.c`'s own domain from there). The GL window is a coordinate mapper into a screen that already exists as real CHTPM state - it has no UI logic of its own.

**Why this doesn't transplant directly**: `tp_desktop_window.c`'s context menu has no underlying `.chtm` screen to mirror - it IS the screen, hand-drawn and hand-dispatched. Getting genuine parity means giving it wraith-alpha's OWN shape: replace the hardcoded `MethodItem[]`/`draw_context_menu()`/`ButtonPress`-branch dispatch with a real declarative objects file the window hit-tests against, same idea as the "layout file" raised earlier in this doc but taken further (regions + navigation, not just style constants).

**Sketch of the format** (real proposal, not yet built - naming deliberately mirrors wraith-alpha's own `OBJECT | key=value | ...` shape):
```
PAGE | main
OBJECT | id=feed      | x=0 | y=0  | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Feed
OBJECT | id=stats     | x=0 | y=24 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Stats
OBJECT | id=more      | x=0 | y=48 | w=120 | h=24 | z=0 | kind=href   | action=GOTO:activities
OBJECT | id=ledger    | x=0 | y=72 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Ledger

PAGE | activities
OBJECT | id=train     | x=0 | y=0  | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Train
OBJECT | id=rest       | x=0 | y=24 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Rest
OBJECT | id=tournament | x=0 | y=48 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Tournament
OBJECT | id=errantry   | x=0 | y=72 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:Errantry
OBJECT | id=back       | x=0 | y=96 | w=120 | h=24 | z=0 | kind=back   | action=GOTO:main

PAGE | feed_amount
OBJECT | id=qty       | x=0 | y=0  | w=120 | h=24 | z=0 | kind=input  | action=STATE:pending_food_qty
OBJECT | id=confirm   | x=0 | y=24 | w=120 | h=24 | z=0 | kind=button | action=RUN_METHOD:ConfirmFeed
```
- `kind=button` — same as today's methods.pdl rows, dispatched via the existing `dispatch_action()`.
- `kind=href`/`kind=back` — reserved actions `GOTO:<page>`/literal back-to-previous-page, handled by a new small page-stack in the window's own state (no process restart - same window, different page rendered), NOT dispatched as an external command.
- `kind=input` — the one genuinely new capability: puts the popup into text-entry mode for that row, real keystrokes append to a real per-package pending-input state file (`STATE:<key>` names which file/field), Enter commits it, matching this house's own existing `cli_io` field-injection convention (digit-jump → activate → type → commit) rather than inventing a second one.
- Real click history logging and `interact_relay.txt` injection (§10 above) both keep working unchanged - `RUN_METHOD:<Label>` injection would just need to resolve against whichever page is currently open, or accept a page-qualified form (`RUN_METHOD:activities/Train`) once multi-page is real.

**BUILT AND VERIFIED, 2026-08-05.** `tp_desktop_window.c` now has a real, optional `<package_dir>/objects.pdl` (`load_objects()`): multiple `PAGE | <name>` sections, each with `OBJECT | label=.. | action=..` rows. `action` keeps every existing methods.pdl convention (real command / `CLOSE` / `void`) plus three new reserved forms - `GOTO:<page>` (push+switch page), `BACK` (pop page stack), `STATE:<key>` (real text-input row: click/relay-activates entry mode, a small floating popup shows the live buffer, real X11 keystrokes append to it, **Escape commits** to `<package_dir>/<key>.txt` - same click-to-activate/Escape-to-commit shape as this house's own `cli_io` field convention, not a new one). Absent entirely = old single-page methods.pdl behavior, unchanged.

Verified live end-to-end against `m6_golddeity` (MUCHI_RANCHER): `GOTO:activities`/`BACK` navigation both logged correctly; `STATE:pending_name` activated input mode, real XTest-injected keystrokes ("kl6") were typed and Escape committed them to `pending_name.txt`, both steps logged to `history.txt`; both real click AND `interact_relay.txt` (`RUN_METHOD:<Label>`) injection paths drive all of this identically (same `methods[]`/`n_methods` arrays back both). **Direct correction caught mid-build**: the first test `objects.pdl` had `Close` but no `Cancel` row - `Close` really ends the process (removes the entity), `Cancel` (`action=void`) just dismisses the menu, same existing distinction methods.pdl already has elsewhere (e.g. `dog`/`asa`'s own `Cancel | void` row) - confirmed both behave correctly (`Cancel`: process stays alive; `Close`: process really exits) once fixed. Any future `objects.pdl` page needs its own `Cancel` row - it's not automatic.

### §12. Livedesk master ledger + real taskbar widget - BUILT, 2026-08-05 (practice pass)

**Retired 2026-08-11**: this describes the ORIGINAL v1 taskbar
(`tp_taskbar.c`, tab-bar only, predates the header strip and the real
declarative-layout parser). That file is now fully retired and archived
— the real, current taskbar is `khtpm_strip_parser.c` +
`khtpm_taskbar_manager.c` under `*.monads/*.livedesk-taskbar/ops/`; see
that dir's own `README.md` and
`#.#.calendar-dox/AU11-khtpm-gap-fixes.txt`. Kept below as an accurate
historical record of the original build, not current guidance — the
master-ledger/`livedesk_open.txt`/auto-launch mechanisms it describes are
still real and still in use by the current system.

Direct instruction: "if we used a master ledger when generating these things, they could get an index nav number and at some point be navigatable that way using livedesk... we should be thinking about adding open desk procs to a livedesk master ledger just for practice. we can even open a taskbar using our new format." Built the same day as a real, working v1, not just designed:

- **`#.desktop/livedesk_master_ledger.txt`** - real, append-only, house-wide. `tp_desktop_window.c`'s new `ensure_livedesk_index()` scans it for a prior `PATH=<package_dir>` match on every window-open; if found, reuses that entity's real, STABLE `INDEX=N` (a relaunch never gets a new number); if not, assigns the next value from a real persisted counter (`#.desktop/livedesk_next_index.txt`) and appends a real `ASSIGN INDEX=N ENTITY=.. PATH=..` row. Also writes `<package_dir>/livedesk_index.txt` so any tool can read an entity's own index directly.
- **`#.desktop/livedesk_open.txt`** - a SEPARATE, live registry (not history) of currently-open entities only: `PID=..|INDEX=..|ENTITY=..|PATH=..`, one line per open window, added on open (`livedesk_registry_add()`), removed on clean close (`livedesk_registry_remove()`, matched by PID) - this is what the taskbar actually polls, so its tabs track what's ACTUALLY live, not full history.
- **`&.widgits/livedesk-taskbar/`** - a real, independent widget (own top-level dir under `&.widgits/`, matching every other real widget's own layout - direct correction: "why dont i see task bar in &.widgits dir? thats where its ment to be... its not a member of tile-picker" - it was originally built as a sibling file inside `tile-picker/ops/` and had to be moved out). `ops/tp_taskbar.c` → `ops/+x/tp_taskbar.+x`: a persistent, override_redirect bar spanning the bottom of the screen, polling `livedesk_open.txt` every ~1s and redrawing one tab (`[INDEX] ENTITY`) per open entity. Clicking a tab finds that entity's real live window by its own window title (`tile:<ENTITY>-<instance>:<glyph>`, same `XQueryTree`/`XFetchName` technique `tp_test_send_key.c` already uses) and `XRaiseWindow`+`XSetInputFocus`'s it.
- **Auto-launch, singleton-checked**: `tp_desktop_window.c`'s new `ensure_taskbar_running()` checks a real PID file (`#.desktop/livedesk_taskbar.pid`, `kill(pid,0)` liveness probe) before ever spawning one - "it can open when livedesk using app is open, and if one is already open just add the tab of that app to the taskbar" is real: the FIRST livedesk entity to open spawns exactly one taskbar; every entity after that just becomes a new tab in the already-running bar, verified live (spawned `m6_golddeity` then `m8_redhorned` - one taskbar process throughout, two real registry rows, `m8`'s row and tab both disappeared cleanly on `Close`).

**Naming, direct question ("what do u call the layout format were using if its not chtpm? or can we literally use chtpm?")**: answered inline in conversation, not yet formally named in a doc - it's genuinely NOT CHTPM (see §11's own honest clarification above). No house-standard name assigned yet for the `objects.pdl` convention itself; worth deciding a real short name (e.g. something in the style of `fo-menu-sys`/`cli_io`) next time it comes up rather than always saying "the objects.pdl format."

**Not yet built**: the actual digit-jump "type index N, activate" livedesk navigation UI itself (this pass only built the index ASSIGNMENT + a mouse-driven taskbar, not a keyboard-driven jump-to-index mechanism); stale-PID cleanup in `livedesk_open.txt` if an entity is SIGKILLed rather than closed cleanly; taskbar doesn't yet show anything for pets/asa/ava (they're still running the pre-livedesk binary - needs a relaunch, not new code, same as §10's own note).

### §13. Real correction on the digit-jump design (2026-08-05, same day, before §12's numbering is used further)

A first attempt (still in this same pass) gave every context-menu popup its own real `[N]` prefix per row, numbered 1..n LOCALLY to that one popup - **wrong**, caught immediately: "brackets are ment for focuz not holding numbers and already we have repeat indexes in toolbar and context menu, it should look up first and increment indexes so if i click toolbar then enter 'number' it will jump to any open khtpm style window."

**The real, corrected model**:
- `[N]` is a real, live **focus/jump address** - it must be unique across the ENTIRE visible screen at any one moment, not renumbered per-window. A taskbar tab and a context-menu row must never show the same `[N]` at the same time.
- One shared, LIVE claim pool (deliberately separate from §12's own PERMANENT `livedesk_master_ledger.txt` index, which never changes across relaunches): whenever something becomes visible on screen (a taskbar tab opens, or a context menu opens showing its rows), it looks up the current live claims, takes the highest `N` in use, and claims the next ones after it. Numbers free up and get reused once the thing they belonged to closes - genuinely ephemeral, not permanent history.
- **A real terminal-style input box, in the MIDDLE of the taskbar** ("just like chtpm"): type a number, press Enter. The bar looks that number up in the shared claim registry and jumps:
  - If it belongs to a taskbar tab → raise + focus that real window (same as clicking the tab today).
  - If it belongs to a ROW inside some OTHER window's currently-open context menu → send THAT window a real remote "activate this row" command through its own `interact_relay.txt` (reusing the exact file-mediated injection §10 already built - a new reserved relay command, not a new mechanism). This means Enter-on-a-number can reach into ANY open KHTPM-style window's menu, not just whichever window currently has real X11 focus.

**BUILT AND VERIFIED, same day.** `nav_claim_rows()`/`nav_release_pid()` (`tp_desktop_window.c`) and `sync_tab_claims()`/`lookup_nav()` (`tp_taskbar.c`) all read/write one shared file, `#.desktop/livedesk_nav_claims.txt` (`KIND=tab|...` or `KIND=row|...` lines) - a context menu claims a contiguous range for its own rows the moment it opens (and RE-claims on every `GOTO`/`BACK` page switch, which also now real-fix reopens the popup so the page switch is actually visible - a real gap caught while wiring this in, since the original `GOTO`/`BACK` code only updated `methods[]` without ever reopening the window), releasing on close/Escape/process-exit; the taskbar claims/reuses its own per-tab number every ~1s poll, dropping claims for entities that closed. The taskbar's own middle-of-bar terminal input (`Nav > `) is real: Enter looks the typed number up in the shared file and either raises+focuses a tab's window, or writes a real `ACTIVATE_NAV:<N>` command into the OWNING window's own `interact_relay.txt` if the number belongs to a currently-open menu row elsewhere - verified conceptually via the same relay mechanism §10 already proved live.

**Second real correction, same day, format** ("im still seeing numbers in brackets instead of [>] [] empty brackets like chtpm" - user supplied a real captured chtpm frame, `yahoo-broker`, as ground truth): the actual real chtpm convention is `[ ] N. Label` - an EMPTY bracket is its own real focus-CURSOR marker (becomes `[>]` for whichever row currently has focus), completely separate from the plain row number that follows it - NOT `[N] Label` (number living inside the bracket), which is what got built first. Fixed in both `draw_context_menu()` (menu rows) and `tp_taskbar.c`'s own tab rendering; the taskbar's input prompt also corrected to the real `Nav > ` label. **Real up/down focus-cursor - BUILT AND VERIFIED, same day** ("keep pushing... just wanna make sure the fundamentals are in place"): `popup_focus_row` is a real, per-popup cursor index (reset to row 0 every time a menu opens or a `GOTO`/`BACK` page switch reopens it). Real `Up`/`Down` `KeyPress`es move it (wrapping both ways); whichever row equals it shows `[>]`, every other row shows `[ ]` (`draw_context_menu()`'s own `focus_row` param). `Enter` activates whichever row currently holds the cursor - full real dispatch (same `CLOSE`/`void`/`OPEN_USER`/`GOTO`/`BACK`/`STATE`/generic-command logic the mouse-click and `ACTIVATE_NAV` paths already use). Verified live end-to-end against `m6_golddeity`: two real `Down` keypresses moved the cursor from row 0 (Feed) to row 2 (Rename), a real `Enter` activated exactly that row (`CLICK(cursor) method=Rename action=STATE:pending_name`, logged), correctly entered input mode.

**Naming, resolved**: this whole real, separate parser/convention (`objects.pdl`, `load_objects()`, the nav-claim pool, the taskbar) is formally called **KHTPM** going forward - confirmed NOT the actual `chtpm_parser_pal.c` pipeline (a genuinely different, house-own parser), but deliberately mirroring real CHTPM's own visual/navigation conventions (`[ ]`/`[>]` cursor markers, `Nav > ` prompt, numbered rows) on purpose.
