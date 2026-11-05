# Views, the layout studio, and the xelector's "controlling" look (2026-11-04)

Written from an owner conversation on 2026-11-04. Part 1 is the OWNER'S VISION (not built). Part 2 is what the layout studio really is, checked today. Part 3 is how the coin-shop popup was built and why it wraps badly. Part 4 is the xelector colour question, answered from the code, with a build spec. Every claim is labelled **OBS** (I ran or read it today, with the path), **VISION** (the owner's intent, nothing built), or **PROPOSED** (my suggestion, not built).

## 1. The vision (VISION, not built)

The owner pictures something like **Xcode's "views" with an MVC shape, as visual programming**: you see each view live inside a greater editor, and you chain views together with **RPG-Maker / blueprintz style scripting** (event pages). The owner said the layout work "may be an unfinished standard".

How the words map onto pieces the house already has (OBS: each piece exists; the mapping is mine, PROPOSED):

| MVC / Xcode idea | House piece today | Where |
|---|---|---|
| View | a `.xhtpm` template (tabs, dropdown rows, overlay rows, `show=` / `label=` / `action=` bound to `${key}`) | e.g. `@.apps/rpg-edit-hq/board/pchq-board.xhtpm` |
| Model | the state files and the key=value `ui.txt` the manager publishes | `@.apps/rpg-edit-hq/state/ui.txt` (written by the resident daemon and by every verb run) |
| Controller | verbs (`rpg_edit_hq.+x fire <verb>`) and event pages | `ops/rpg_edit_hq.c`, `petslike_core.c`, `events/<name>/event_pkg` |
| Chaining views by script | event pages and the blueprintz editor / `book.pal` compiler; the "Show Choices" popup of the rpg-space helm | `02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md` (book), `@.apps/rpg-space/events/helm/` |
| "Seeing the views live in a greater editor" | NOT built. The editor draws ONE board; no pane shows a view's template next to its live result | |

Gaps against the vision (OBS unless marked): no live view editor; no UI to chain one view's action into another view; the validator is not green on the editor's own board (part 2).

## 2. The layout studio: what it is (OBS, `@.apps/layout-studio`, README dated 2026-10-09)

- It is a **validator and a catalog**, layers 1-2 of a strategy (`08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`), aimed at weaker agents making `.xhtpm` / `.chtpm` safely. Not a visual editor.
- `ops/+x/layout_check.+x <file> [--ui state/ui.txt]` prints `ERROR/WARN line N: what | fix: how`; `--drift <khtpm_core_render.c>` checks the renderer against the catalog; `CATALOG.md` lists every allowed tag and attribute; `samples/` has a broken and a fixed example.
- README says it was checked against all 238 real layouts with 0 errors (2026-10-09). It did not say what the `--ui` check does for conditional keys.
- Next layers per the README: templates and `layout_op preview`, a `layout_op` CLI, manager scaffolds, then "the studio window that loads a `.xhtpm` and edits it". None of those is built (not re-checked beyond the README and the ops folder listing: `layout_check`, `layout_flow`, `pet_gen`).
- `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` (2026-10-06) is **plan only**: user-made menus and bars drawn inside the game, saved by name, callable from the Events menu, made in an x11-hq editor that an agent can also use.
- In the editor today, `layout.pdl` only chooses the left and right sidebars at window open (`LAYOUT | left | palettes`, `LAYOUT | right | hai`); it is not a view designer.

**Run today:** `layout_check ../rpg-edit-hq/board/pchq-board.xhtpm --ui ../rpg-edit-hq/state/ui.txt` = **44 errors, 0 warnings**, all of the form "`${key}` is not a key the manager publishes" (for example `pal_armed`, `pal_can_next`, `side_right`, `page_post_*`, `n_ent`, `load_label`). The same check on the board as committed BEFORE the coin shop (`4a9735f96`) also gave 44, so none of them come from the coin shop. I did not investigate why those keys are flagged: either the manager publishes them only conditionally (like my popup keys did) or another writer publishes them; the board draws fine, so the validator is stricter than the renderer or the keys are absent only at the moment of the check. **Answer found 2026-11-05 (OBS, read-only, no board launched):** the 44 errors were mostly the CHECK, not the board. The editor board's variables come from several state files, and I had passed only `state/ui.txt`. `layout_check` takes `--ui` repeatedly; with `state/ui.txt` plus every `board/state/*.txt` (sides.txt, pal.txt, prefs_ui.txt, ui.txt) it reports **19**. The 19 left are 13 distinct keys: `load_label/load_ok/load_warn` (published by the taskbar manager, `khtpm_taskbar_manager_main.c`, not by this app), `pal_armed` and `time_label` (published only in some states: `pal_side.c`, `petslike_core.c`), and `page_post_1..3_label/_act`, `book_post_3_label/_act` (CORRECTION, same day: NOT dead. They are fixed menu slots filled from `toolbar.pdl` `SECTION | book_menu_post_N_label` rows; the key names are built, so a grep for the full name finds no writer. Today only book slots 0-2 and page slot 0 are filled, the rest are absent and hidden by `show=`. Do not delete them; `rpg-pet/toolbar.pdl` lines 7-14 document the scheme). Command: `layout_check board/pchq-board.xhtpm --ui state/ui.txt --ui board/state/ui.txt --ui board/state/sides.txt --ui board/state/pal.txt --ui board/state/prefs_ui.txt`.

**My own regression, fixed:** the coin-shop popup first published its title / text / coins / last-answer keys only while the popup was open, which added 4 errors (48); `petslike_core.c` now always publishes all of them (empty while closed), back to 44. House rule that follows: **publish every key a template uses, empty when not applicable.**

## 3. The coin popup was NOT made with a layout (OBS)

- It is one hand-written overlay row, `lshop-ov`, in `pchq-board.xhtpm` (`class="canvas-overlay-at ov-chrome rp-chat-ov theme"`), copied from the rpg-space helm popup (`ask-ov`). Width is fixed by CSS (`.rp-chat-ov { width: 190px }`, `.rp-menu-row { width: 200px; height: 20px }` in `pchq-board.css`).
- **Wrapping:** a long `<text label=...>` wraps in WIDTH, automatically, but the row does not grow in HEIGHT, so the second line overlapped the next row (first popup screenshot, 2026-11-04: "buy 4c sell 2c you have 50 coins" over "pick Buy or Sell"). Worked around by splitting the text into short lines and shortening the receipt (`bought 1 cdda.rag  -4c`). The editor chat pane wraps in C for the same reason (commit `a7fa54a69`: "chat pane wraps lines (10 rows, no overlap)").
- **BUILT 2026-11-05: `ov-wraptext`.** An overlay `<text class="... ov-wraptext">` now gets as many `ROW_H` rows as its label needs at the column width (`scroll_row_span()`, the same greedy wrap the draw uses; helper `kh_ov_text_rows` in `khtpm_core_render.c`, capped at 8 rows), the popup grows to fit and the rows below move down. Opt-in per row: every other overlay text keeps one row. The coin popup's text and receipt rows use it. OBS live frame (editor relaunched with the rebuilt renderer, receipt set to a 92-character sentence): it wrapped to 4 lines, the popup grew, Buy 1 / Sell 1 / Close sit below with no overlap. Headless mode still returns 1 row (no Xft to measure), so the pal text mirror does not see it. No harness case: the proof is the frame (a layout harness for the renderer does not exist).
- **Superseded question, kept:** whether a wrap-aware text element exists for overlay rows, or whether the layout studio could own this popup. **PROPOSED:** look in `khtpm_core_render.c` for a text row that measures wrapped height (the scroll text `ov-scrolltext` / `ov-scroll` rows do something similar) before writing more popups by hand, and have `layout_check` warn on a `<text>` label that can exceed its row width.
- Dropdowns: the renderer scrolls an in-board tab dropdown that would run off the screen (window of rows + a pinned last row; `dropdown_max_rows` in `#.desktop/hq_ui.pdl`); it has real submenus only for taskbar popup windows (OBS: `khtpm_core_render.c` "real submenu adjacency"). That is why the coin shop is one short dropdown plus a popup, not a nested menu.

## 4. The xelector: lighter / thinner green while it controls an entity?

**Owner request (2026-11-04):** when the xelector is armed and CONTROLLING an entity, draw it in a lighter colour and/or thickness of green than when it moves FREE of any entity.

**Is it a thing in the code today? No (OBS).**
- 2D: `&.widgits/board-viewer/ops/bv_render_2d.c` ~line 1626-1640: one fixed colour `xr = 40, xg = 255, xb = 80`, a 6 px border ("6px, not 2. A 2px line disappeared into the painted tiles"), drawn for any `sel_x, sel_y >= 0`. No branch on possession.
- 3D: `bv_render_3d.c` line 4102: `ADDWIRE(g_xelector_x+0.15, ..., 40,255,80)` (the green wire box, footprint 0.15-0.85 so it reads bigger than an entity's 0.25-0.75); in camera mode 1 the xelector marker is skipped on purpose (you ARE it). No branch on possession.
- The two docs that describe the xelector, `18.pc-hq/XELECTOR-ENTITY.md` and `18.pc-hq/CURSWORD-POSSESSION-DESIGN.md`, do not mention colour, thickness, lighter or green (grep, 2026-11-04; only those two files were searched). `bv_2d_piece_test.c` line 98 only says "green like the 3D wire box".

**The data to decide already exists (OBS):**
- `@.apps/rpg-edit-hq/rpg-edit-hq-pchq/pieces/xelector_01/state.txt` has `possessed_id=hero_01`, `last_possessed_id`, `pre_possess_x/y/z` (the live file read today said `possessed_id=hero_01`).
- 3D already parses it: `g_xelector_possessed_id` (line 1062) filled from `possessed_id=` (line 1139) or from the page glyph (line 1107).
- 2D already reads it into the page row's `poss` glyph field (`bv_render_2d.c` lines 591-598: `read_kv_str(path, "possessed_id", poss, ...)`).
- **A discrepancy to know about:** the 3D HUD line `poss ...` (`bv_render_3d.c` line 2441) prints `g_hero_present ? "hero_01" : "-"`, not `g_xelector_possessed_id`. On the live editor the HUD showed `poss -` while `state.txt` said `possessed_id=hero_01`. So the HUD word "poss" is not the xelector's possession state. Decide which one the new look should follow and fix the HUD to match; do not trust the HUD line as the test oracle.

**BUILT 2026-11-05 (owner: "do that the easiest, most modular way"; the owner did not pick values, so the defaults below are mine and are data, change them without code):**
- `&.widgits/_shared-lib/xelector_look.c`: ONE pure text-include (the house rule for pure code shared by 2+ consumers: no header, no link). It holds the defaults and `xel_look_pick(controlling, free_rgb, free_px, ctl_rgb, ctl_px)`; each renderer reads its own `view.pdl` and its own xelector state and asks it. Malformed or out-of-range values fall back to the default (rgb 0..255 each, px 1..12).
- Defaults: FREE `40,255,80` 6 px (exactly what it always was); CONTROLLING `150,255,170` 3 px. `view.pdl` rows override them: `VIEW | xelector_free_rgb | r,g,b`, `VIEW | xelector_free_px | n`, `VIEW | xelector_ctl_rgb | r,g,b`, `VIEW | xelector_ctl_px | n`.
- "Controlling" = the xelector state file's `possessed_id` names an entity (`""`, `-` and `.` mean none).
- 2D (`bv_render_2d.c`, both drawing branches, painted-map 6 px and tile double line): colour and thickness. 3D (`bv_render_3d.c`, the maker-map wire box): COLOUR only (the box has no thickness knob), cached and re-read at most every 60 frames. The 3D HUD `poss` line now prints the xelector's `possessed_id` (it printed `g_hero_present`).
- Fix 2026-11-05: `possessed_id=none` is what the house xelector state writes for nobody (a live frame showed HUD `poss none`); my first version treated it as controlling. `xel_look_controlling` now also treats `none` (any case) as free; a new check proves it and was negative-tested by removing the rule (the case failed, then restored: 59 PASS).
- Evidence: `pieces_2d_parity.pal` 58 PASS (9 check rows added by this change, so 49 before; I did not observe the old count): FREE saturated and none of the controlling colour; CONTROLLING lighter and none of the free colour; CONTROLLING has fewer border pixels than FREE; a `view.pdl` override of colour and thickness takes effect; a malformed row falls back; and a NEGATIVE renderer built with `-DBV_2D_NO_XEL_LOOK` (possession ignored) makes the test FAIL on exactly the controlling and data checks. 3D: compiled clean with the real build flags and the real binary rebuilt, NOT run: no 3D picture was taken (an engine relaunch is heavy); the shared function is the one the 2D test proves, the 3D call site and HUD line are read-checked only. Running sessions keep their old binary until their next launch.

**The original proposal, kept for the reasoning:**
1. Two looks, both DATA (a `view.pdl` row each, so the colours are not buried in C): `VIEW | xelector_free_rgb | 40,255,80` and `VIEW | xelector_free_px | 6` (what it is today: the bolder, saturated one); `VIEW | xelector_ctl_rgb | <lighter green, owner picks, e.g. 150,255,170>` and `VIEW | xelector_ctl_px | <thinner, owner picks, e.g. 3>`.
2. The renderers choose the look from `possessed_id` (non-empty = controlling). 2D: at the border loop (`for (int t = 0; t < 6; t++)`) use the chosen colour and thickness. 3D: the `ADDWIRE` colour at line 4102 (wire thickness may be fixed in 3D: then colour only, and say so).
3. Do not change anything else: same footprint, same camera behaviour, mode 1 still hides the marker.
4. Test (pal harness, no Python): extend the existing 2D piece test style (`bv_2d_piece_test.c` has `greens()` and `xel_at()` helpers) or a new case that renders the same cell twice, with `possessed_id` empty and then `hero_01`, and requires the border colour and pixel count to differ in the expected direction; plus a mutation run (swap the two looks, the case must fail). Also run the existing piece test so the 2D camera behaviour is unchanged.
5. Owner questions: exact lighter colour and thickness (answered by defaults, changeable as data); should the free look stay exactly as today (yes, unchanged); should the 3D HUD `poss` follow `possessed_id` (yes, done); what counts as "controlling" for an NPC the player does not own (ANY non-empty `possessed_id` for now; still open: only the player's hero?).

## 5. Where to look
- Layout studio: `@.apps/layout-studio/README.md`, `CATALOG.md`, `ops/layout_check.c`; strategy `08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`; plan `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md`.
- Event pages / blueprintz: `02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md`, `16.game/EVENT-VERB-PAGES-DESIGN.md`, `16.game/S1-EVENT-LEDGER.md`, `02-architecture/PAL-VS-EVENTS.md`; the helm popup `@.apps/rpg-space/events/helm/` and its `ask-ov` row.
- Window templates: `02-architecture/XHTPM-WINDOW-BUILD-GUIDE.md`; renderer `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`.
- Xelector: `18.pc-hq/XELECTOR-ENTITY.md`, `18.pc-hq/CURSWORD-POSSESSION-DESIGN.md`, the two renderers above.
- The coin popup and its harness: `16.game/SHOP-SLICE-SPEC-2026-11-04.md`, `&.widgits/_shared-lib/harness/cases/coin_shop_ui.pdl`.
