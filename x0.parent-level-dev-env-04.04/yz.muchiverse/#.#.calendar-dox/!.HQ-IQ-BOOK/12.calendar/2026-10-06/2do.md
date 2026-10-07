# 2026-10-06 — 2do: in-game layouts, then the carry-over

Owner-approved order: **sandbox prototype first**, on a copy of the board template, not the live board.
Plan and reasoning: `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md`.

## In-game layouts (new)

- [ ] **Sandbox:** `pchq-board-sandbox.xhtpm` + `layouts-sandbox/` (check `open_pchq_board.sh` can launch a
      different template; the live board and hotbar stay untouched).
- [ ] **Step 1:** a hand-written user menu as a fragment (`layout.chtpm` + `layout.css` + `layout.pdl`):
      title, four action rows (one appends to the game inbox, one runs an op), a `_` button.
- [ ] **Step 2:** `<overlay src="..."/>` in the shared renderer (the one new renderer piece), built behind the
      sandbox template only.
- [x] **Steps 1-2 done 2026-10-06:** sandbox + `<overlay src>` + `canvas-overlay-right`; test-menu draws in the board, nav 29-32; click, typed number + Enter, hide and show verified.
- [x] **Step 3 (part): drag + `_` minimize done 2026-10-06** (pc-hq hotbar slides sideways and minimizes; test-menu free drag). Still to do: `ov-close` (`x`) for context menus, remembered positions, key-driven move. Original line: generic overlay chrome (with MOUSE drag of the title bar, owner 2026-10-06) — `_` minimize into the pc-hq bottom bar, slide along one axis. Also
      closes the pc-hq hotbar gaps (no minimize, no sideways slide).
- [ ] **Step 4:** anchors (centre, at-clicked-cell) clamped to the board.
- [ ] **Step 5:** event command `layout.toggle <id>` in `#.ref/menu/event_commands.registry.pdl`, called from
      the pc-hq Events menu.
- [ ] **Nav index on all interactive layout elements (house accessibility standard, owner 2026-10-06):** plan
      part 4h. Automatic numbering of items/buttons/cli_io in overlays, visible rows only, modal scope for menus,
      focus that holds, studio check. Test every sandbox step by relay AND a real key. Fixes the pc-hq hotbar
      overlay's "nav focus not holding" as the first case.
- [x] **Context menus, first slice done 2026-10-06:** pc-hq generated verb menus draw in the board (plan part 4d). Still: desk entities' own menu.chtpm overlay (actions use the pal's package dir), live right-click test, remembered positions, modal scope.
- [ ] **Later:** `layout_op` CLI, the x11-hq editor window, livedesk host, migrate pc-hq entity context menus.
- [ ] **HUD text box + minimap as layouts (owner, 2026-10-06):** standardize `hud.pdl` to the layout `.pdl`
      with `target=frame` (board-viewer paints into the game image) vs `target=overlay`. Plan part 4g.
      `hud.pdl` keeps working during the move.

- [x] **No absolute paths in pc-hq (owner, 2026-10-06; TPMOS pattern):** `open_pchq_board.sh` generates `pieces/system/locations.pdl` + refreshes `house_root.txt` each launch (generated files untracked + gitignored); `_shared-lib/khtpm_locations.c` `loc_resolve_saved()` resolves a saved path against the live house root (relative, or a stale absolute rebased on `/xyzfs/`); `open_book_page.txt` `pdl=` is written house-relative by `pc_synch_request.sh`; the projector and the board-viewer 2D/3D `page_bound_pdl` resolve through it. Verified: stale absolute path -> 16 page entities (as original), nonexistent path -> 7 (old failure). **Round 2 done (owner: "all of them"):** every reader of `house_root.txt` / `real_project_root.txt` in pc-hq (clock daemon, trigger watcher, chunk + phymoji generators, menu input x3, world manager, compose frame) and the board-viewer (2D, 3D, compose, move range) now goes through `loc_house_root()` / `loc_real_root()` in `khtpm_locations.c` (locations.pdl, then house_root.txt, each validated, then walk up from the folder; absolute / relative / stale-rebased real_project_root). The 6 private copies of resolve_real_root and the 8 private house_root readers are gone. `session_dir.txt`, `holder.txt`, `house_root.txt`, `ctx_menu/*` are generated per launch + gitignored (TPMOS-style generated pointers, not persisted state). **Still to do:** the same sweep for other apps (this one covered pc-hq, board-viewer, hotbar, layout-studio); (done: `pc_trigger_watcher` is now built by pc-hq scripts/build.sh - it was never built, so the bridge-watcher daemon pc_menu_input starts silently never ran; build.ps1 not updated). **Survey of the rest of the house (tracked files containing this machine's path, excluding docs): almost all are records, not pointers** - test/frame reports and harness proofs (text-editor-xyz 278, %.harnesses 131, 102.agy-txt 82, ...), desk entity menu.chtpm (header comment only). **Real ones, owned by other apps, not touched:** `shared/*-ASSET-SOURCE-LOCATION.pdl` (absolute path to #.NNEST_ASSETS), `asa/ava asset.pal` (already points at a different old checkout), `&.widgits/open-hai/state/*` + `&.hq-apps/chat-hai/chat-hai.chtpm` + `network-browser-hq.chtpm` (manager-generated, committed by accident), `file-explorer/fe_clipboard.txt`, `music-player-hq/library.pdl` root=/home/no/Downloads, `#.desktop/entities/*/asset.pal`.

## Carry-over from 2026-10-05

- [ ] Hotbar: bottom-bar number offset past the hotbar's range and routing typed numbers into it (display base
      exists as the `nav-after-top` class, unused by the hotbar now).
- [x] **Hotbar (desk): the holder's own visual in the header (owner, 2026-10-06) - DONE for the desk, NOT for pc-hq.** Header now reads `[sword picture] cursword - slot: self` above the nine slots (proved with a real PNG of the real template through a scratch renderer: slots still `1..9`, command field `10`, window 760px).
      How: `hotbar_manager.c` publishes `holder_sprite=<holder entity dir>` (empty when it has no `sprite.csv`; one publisher for desk and pc-hq, pc-hq prefixes `hb_`); `hotbar-desk.xhtpm` header is a one-item toolbar row `<item class="hb-head no-nav sprite-inline" sprite="${holder_sprite}">`.
      Renderer (shared, +10 lines, `khtpm_core_render.c` toolbar-row item loop): class `no-nav` on a toolbar-row item = laid out and drawn, no nav number, not in `g_nav[]` (the dock strip already honoured it; ordinary rows did not). Opt-in; only other user is taskbar-settings' font-name cell (a label that already meant it).
      Facts learned (so nobody repeats the dead ends): (1) items under 64px high draw their sprite LEFT of the label (capped at 24px unless `sprite-big`); a taller item centres a big sprite and hides the slot row; (2) in a toolbar row only `<item>` children draw (a `<text>` there is parked off-screen); (3) only `<item>` carries `sprite=`; `<canvas>` forces a 30 fps tick (bad for a static bar); (4) an unpublished `${var}` is empty and `show=` hides the element; (5) items are nav-numbered in document order, so a header item shifts every slot digit unless it is `no-nav`.
      Takes effect for a hotbar window opened after the 22:35 renderer build AND started with the new manager (the owner's running one, started before, keeps the old header text until it is reopened).
- [ ] **Hotbar (pc-hq): same header picture.** Not built. The overlay's first child must stay the `<text class="ov-title">` (it is the drag handle, `kh_overlay_title_hit`); add a one-item `no-nav` toolbar row with `sprite="${hb_holder_sprite}"` under it in `pchq-board.xhtpm` (`hb_holder_sprite` already published by the manager). Needs a look on the real pc-hq board (its window is the owner's; launch a scratch copy, never the live one).
- [ ] Hotbar: entity images (sprites) in the pc-hq slots and the pc-hq bottom bar (the board publishes no
      `ent_N_sprite`; the bottom bar could show fewer cells to make room).
- [ ] Hotbar: nav focus not holding on the pc-hq overlay.
- [ ] pc-hq bottom bar numbers start at 7+ (shared window sequence); owner asked why not 1 like the desk.
- [ ] pc-hq context menus are separate windows: can leave the viewport and do not minimize with the board.
      Short term: clamp to the board rect.
- [ ] Per-entity z in the page row (entities are now placed at ground level, one below the hero's z).
- [ ] x/y of the xelector are remembered across sessions (z now resets to level 1).
- [ ] Re-verify: pc-hq cli_io with Interact ON; desk grab retry against a real second holder; the `!` fullscreen
      toggle on windows other than the hotbar (it misbehaved on the hotbar; unchanged elsewhere).
- [ ] Autostart row for the desk hotbar is untested (needs a taskbar reset).
- [ ] Chatbot reply window for the hotbar cli_io (needs a backend choice: entity Chat/OpenRouter or HORN).
- [ ] Rotate keys (OpenRouter key is in git history; Groq/Poolside scratch keys were in a transcript).
- [ ] **Q008 (^.grave):** two HORN transports live side by side in `^.hai-horn` since the opencode merge. The old `ops/horn_chat_openrouter.c` is NOT dead: `horn_chat.sh`, `halo_test_harness.sh`, `horn_chat_test.sh` still call it. Port or remove those, then delete it. Details: `44.xyz.01.00/^.grave/quests/Q008-retire-old-horn-transport/QUEST.md`.
- [ ] **HALO (Q001):** ported + merged on local `claude` (`9028e0706`); still needs a LIVE `halo_chat.sh` run (provider key), HORN e2e, and the owner's call on auto-promotion (default is now OFF, `elementary_hs`). Owner also has to confirm the phone migration (`c41e7f935`) was intended (Q005 said it needed his OK first).
