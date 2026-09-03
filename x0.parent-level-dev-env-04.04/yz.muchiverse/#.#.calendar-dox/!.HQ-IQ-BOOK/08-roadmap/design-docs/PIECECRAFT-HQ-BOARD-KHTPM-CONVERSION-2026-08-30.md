# piececraft-hq board-viewer → khtpm conversion — real status (2026-08-30)

## The real, clarified ask (corrected after an earlier wrong-scope attempt)

Direct instructions, in order:
1. Board-viewer's real 3D view/camera/interact-mode itself needs to be
   converted to a real khtpm window (`khtpm_entity_menu_render.c` family) -
   not just an info panel alongside the existing legacy widget. Safe to
   do this invasively since it's `piececraft-hq`'s own copy - the real,
   shared `&.widgits/board-viewer` (used by piececraft-xyz/civ-txt/
   tactics-txt) stays untouched.
2. The menu (Switch World / interact / camera controls) is a real khtpm
   sub-window that opens under/within the game view in the same window
   context - same real pattern as db-hq's own Common Events picker
   (a modal that closes on exit, not a separate detached popup).
3. Entry point lives IN-GAME, at the same level as "Interact mode" - NOT
   the desktop taskbar's global `!.HQ` dropdown. An earlier attempt wired
   this into `#.desktop/livedesk_taskbar.pdl`'s `hq_menu_N` rows (the
   system-wide HQ menu) - confirmed wrong, reverted in full (see git
   history same day - `livedesk_taskbar.pdl`/`&.widgits/palettes/
   pallets.pdl` restored to their pre-this-session content).
4. Real, direct instruction on approach: **"u should do it the same way
   the legacy chtpm parser does it. if possible steal code/ops w/e u
   have to."** - port real, proven code, don't reinvent.

## What's REAL and PROVEN as of this note

**`@.apps/piececraft-hq/ops/pchq_board_view_poc.c`** - a real, minimal,
standalone proof-of-concept. Its `load_frame()`/blit logic is a direct,
deliberate port of `&.widgits/_shared-lib/ops/x11_mirror.c`'s own
`resize_to_frame()`/`load_frame()`/`x11_display()` (same real RGBA32-
file-to-XImage-via-XPutPixel loading, same real `XPutImage` blit call) -
adapted to read `pieces/display/rgb_frame_3d_overlay.raw` directly (the
REAL 3D raymarch buffer `bv_render_3d.c` writes, BEFORE
`bv_compose_frame.c` composites it into the flat `rgb_frame.raw`
x11_mirror.c itself reads) instead of the flat 2D frame.

**Live-verified, real screenshot evidence**: launched piececraft-hq for
real, found its real live board-viewer session dir, ran this PoC against
it - real 3D content (trees, hero, a placed block, ground, sky) rendered
correctly in a genuinely new, bare X11 window, zero GL, zero khtpm code
involved yet. Confirms the core real pixel pipeline works end-to-end via
the stolen/ported blit mechanism - this is NOT a guess or an assumption,
it's a real, working, screenshotted result.

## What's still real, honest, NOT done

This PoC has **zero khtpm integration** - no Elem/CSS chrome, no real
window mode in `khtpm_entity_menu_render.c`, no menu, no interact/camera
key handling, no real session-discovery (the board-viewer session dir was
passed in by hand, not resolved automatically the way
`open_board_widget()`'s own real `ledger_peers.+x` lookup does it for the
legacy engine).

**Real next steps, not started:**
1. Port this same proven blit logic INTO `khtpm_entity_menu_render.c` as
   a real new window mode (`<window class="...">`-selected, same real
   mode-dispatch shape every other mode in that file already uses).
2. Real session-discovery: resolve the live board-viewer session dir for
   piececraft-hq automatically (port `ledger_peers.+x`'s own real lookup,
   or a similar mechanism) instead of a hand-passed argv.
3. Real khtpm chrome (title/close, matching every other khtpm window) +
   a real digit-nav menu row set for interact-mode/camera-mode controls,
   reading/writing the same real state files `bv_menu_input.c` already
   uses (`bv_state.txt`, `arrow_config.txt`'s key bindings).
4. The real "sub-window opens under/within the game view, closes on
   exit" pattern - needs the same real technique db-hq's own Common
   Events picker uses (a real modal overlay, not investigated in depth
   yet for this specific port).
5. Wire the real in-game entry point (a new row in piececraft-hq's own
   `pieces/chtpm/layouts/main.chtpm`, at the same level as the existing
   "Interact mode"/game-verb rows) - not the taskbar's global menu.

Not committed to `khtpm_entity_menu_render.c` yet - this note and the PoC
file are the real, honest checkpoint before that larger integration
starts.

## REAL CORRECTION (2026-08-30, same day) - the real, true-parity model

Direct correction after the first real integration pass (items 1-3
above all landed, but with a real, wrong local hand-rolled nav/chrome
system): **"thats wrong. thats not what the legacy chtpm peice board-
view did u need to stick as closely to that model as possible. absolute
parity. research it and see where u went wrong."**

Real research finding: `board_viewer.chtpm` has a real, declarative
`<interact src="pieces/apps/player_app/interact_relay.txt"/>` + a
reserved `onClick="INTERACT"` button - `chtpm_parser_pal.c` (the legacy
engine) handles ALL real nav/focus/arrow-relay/ESC/the real `[>]`<->`[^]`
glyph swap NATIVELY, zero app-side code needed - the genuine "for free"
system, living entirely in the legacy engine. Separately,
`system/chtpm_rgb_render.c` (a real, shared COMPOSITOR daemon, distinct
from the window-display step) already reads BOTH the real text chrome
`chtpm_parser_pal.c` renders AND the real 3D overlay `bv_render_3d.c`
writes, and blits them into ONE real, fully-composited `rgb_frame.raw`
(`blit_overlay()`/the real `MAP3D_MARKER` protocol) - the SAME file
`x11_mirror.c` itself blits.

**Where the first pass went wrong**: read `rgb_frame_3d_overlay.raw`
DIRECTLY (skipping the real compositor's own output) and hand-drew a
separate, local `[>]N.`/`[ ]N.` nav-badge chrome system on top - a real
parallel reimplementation, not real reuse, despite superficially
"stealing" the pixel-blit half of x11_mirror.c.

**Real fix, DONE**: `run_pchq_board_mode()` fully rewritten to blit
`rgb_frame.raw` (already containing the real chrome/status/3D/footer,
rendered natively) with only a minimal real title+close chrome bar of
its own, and to forward EVERY real key/click into board-viewer's own
real relay files via direct ports of x11_mirror.c's own
`append_key()`/`write_click_kv()`/`map_special_key()` - zero local nav/
click logic left in this file. `board-viewer/button.sh` gained a real
`NO_RGB_COMPOSITOR` flag, separate from `NO_GL`, so the real compositor
keeps running (making `rgb_frame.raw` exist) even with no window of
board-viewer's own ever mapping.

**Live-verified real parity**: a real forwarded Enter keypress flipped
the engine's own real `[>]`->`[^]` focus glyph, confirmed via before/
after screenshot - the real engine's own native interact-mode state
machine, not a local approximation. Items 2-4 in the "not started" list
above are now MOOT (the real engine's own native nav/interact system IS
the menu - no separate local nav-map or sub-window pattern needed for
what the engine already provides for free); item 5 (a real in-game
entry point row) may still be worth adding as a discoverability
affordance, but the real mechanism itself no longer needs building.

## Real layout direction for the eventual full menu (2026-08-30, direct instruction, not built yet)

"lets put interact mode and menu nav? at teh bottom of the screen below
the view" - when the full Elem/nav-system menu gets built (see the real
open item above), Interact-mode controls and menu nav belong BELOW the
3D view, not in the top chrome bar the current local nav-map uses.
Direct instruction: "dont worry about the menu yet" when this was raised
- recorded here so it isn't lost, not acted on in this pass.
