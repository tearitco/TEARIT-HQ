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
- [ ] **Later:** `layout_op` CLI, the x11-hq editor window, livedesk host, migrate pc-hq entity context menus.
- [ ] **HUD text box + minimap as layouts (owner, 2026-10-06):** standardize `hud.pdl` to the layout `.pdl`
      with `target=frame` (board-viewer paints into the game image) vs `target=overlay`. Plan part 4g.
      `hud.pdl` keeps working during the move.

## Carry-over from 2026-10-05

- [ ] Hotbar: bottom-bar number offset past the hotbar's range and routing typed numbers into it (display base
      exists as the `nav-after-top` class, unused by the hotbar now).
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
