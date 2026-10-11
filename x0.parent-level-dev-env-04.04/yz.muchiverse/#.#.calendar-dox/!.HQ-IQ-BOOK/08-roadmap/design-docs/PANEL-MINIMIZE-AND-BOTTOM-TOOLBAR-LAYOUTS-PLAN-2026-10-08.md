# Panels that minimize into a bottom toolbar, and a bottom toolbar for every layout (plan, 2026-10-08)

Status: **PLAN, nothing built.** Owner (2026-10-08, while testing video-player-hq): "the panels should have a minimize button that puts them in the bottom tb of layouts; all layouts should get a bottom tb"; "it can be a minimize ... like how live desk works with apps".

## What exists (read in the renderer and observed)
- **Window minimize:** every sidebar+panel window has chrome `_` `!` `X`. `_` (action `MINIMIZE`, `khtpm_core_render.c` ~8899) hides the whole window and writes `#.desktop/livedesk_hq_windows_<pid>.txt` (`minimized=1`); the taskbar strip lists the window (`hw_N_label=🪟 Video Player · playing` in `strip_ui.txt`); a file `livedesk_hq_restore_<pid>.txt` brings it back. Whole windows only, not panels.
- **Bottom bar:** a `<footer>` element (pc-hq's entities bar, generic wrapped cells with a +/- row pager) exists for the plain sidebar+panel layout only.
- **Found while building video-player-hq:** a `<footer>` inside a flex-row page (`display:flex`, canvas-craft style) breaks that page: every region collapses to zero width. Region widths in a flex page also only work written one property per line, in px.
- **App-side stand-in (video-player-hq):** the manager publishes `side_on`/`side_cls`/`side_label`; the sidebar's class comes from `${side_cls}` and its rows use `show="${side_on}"`; a footer button toggles it. Works, but each app re-implements it.

## Proposal (generic, in the renderer, gated by a template attribute so every existing window is byte-identical)
1. `<sidebar minimizable>` / `<panel minimizable>`: the renderer draws a small `-` button in the region's top corner (nav-numbered like the chrome buttons).
2. Pressing it folds the region to a thin strip (or hides it) and **adds a cell for it to a bottom bar** (a synthesized footer; the label is the region's `title=` or id). Pressing the cell restores it. Fold state is a window-local value (not persisted at first).
3. **Every layout gets the bottom bar** when any region is minimizable: plain sidebar+panel (reuse the existing footer path), and the flex-row layout (fix the footer-in-flex collapse first, since three-pane windows are the ones that need it).
4. Window `_` stays as it is; nothing here changes the taskbar protocol. (Option for later: a minimized *region* could also appear in the taskbar strip.)

## Rules to follow (from `khtpm-house-standards` and the shared-layout caution)
Route through an existing layout path before adding one; clip (park off-screen at y=-100000), never translate; the generic scroll/key code owns scroll cursors; nav-number only visible rows; `assign_nav_and_layout()` runs many times per frame so every mutation must be idempotent; ZERO per-app code (no `g_is_<app>`); gate everything on the new attribute.

## Order, each with a renderer frame-dump harness case and a mutant
1. A case that reproduces the footer-in-flex collapse (scratch window, frame dump geometry), then fix it. 2. `minimizable` on the plain layout with the bottom cell; harness: fold, cell appears, restore. 3. The same in the flex layout (irc-chat-hq and video-player-hq become the first consumers; remove video-player's app-side stand-in). 4. Roll out to canvas-craft and other three-pane windows one at a time.

## Open questions for the owner
Whether a minimized panel should also appear in the taskbar strip or only in the window's own bottom bar; whether fold state should persist per window; the label to show (title or an icon).
