# Top bar drag (2026-10-15)

**What it does.** The top bar (the `dock-header` window of `khtpm_core_render`) can be dragged by its **left margin**, the blank strip before the first cell
where the focus marker (`.` when the window is not focused, `^` when it is) is drawn. Button 1 pressed there and moved drags the bar.
- Kept on screen, and its **bottom edge stays in the top half of the screen** (`y + height <= screen height / 2`).
- On release, if the bar is within **`header_snap_px`** (default 90, row in `#.desktop/livedesk_taskbar.pdl`, straight-line distance) of its home position
  (`strip_x_offset` / `strip_y_offset`, never edited by the drag) it **snaps back**; further away it **stays** and the offset is saved to
  `#.desktop/strip_header_pos.txt` (`dx=`, `dy=`; gitignored), restored at the next start. A snap deletes the file.
- Header only: the bottom bar and all other windows are untouched. No drag while a dropdown is open. A press outside the margin behaves as before.
- Code: `khtpm_core_render.c`, search `g_hdr_dx` (state + `hdr_clamp`, `hdr_drag_finish`, applied in `layout_dock_bar()`, press / motion / release in
  `hq_dispatch_xevent`). Motion moves the X window only (no relayout per tick); every layout re-applies the offset so a redraw cannot snap the bar back.

**Not done / owner's to decide.** The margin has no visible grip graphic (only the existing `.` / `^` marker); the snap radius and the "top half" rule are
constants/settings I chose (90 px, half the screen). The running bar needs the renderer rebuilt (`build_core_render.sh`, or the usual `button.sh reset`) and relaunched.

## Test steps (isolated: a nested X server, nothing on the owner's desktop is touched)
1. Build the renderer to a test folder: `gcc -std=c11 -Wall -O2 $(pkg-config --cflags xft) -I ../../../\&.widgits/_shared-lib -o <out>/khtpm_core_render_new.+x khtpm_core_render.c <shared>/khtpm_css_parser.c <shared>/khtpm_ui_scale.c -lX11 -lXext $(pkg-config --libs xft) $(pkg-config --libs fontconfig) -lm` (cwd `_.monads/_.livedesk-taskbar/ops`).
2. Scratch house: `xyzfs/`, `#.desktop/{livedesk_taskbar.pdl,livedesk_theme.pdl,strip_ui.txt}` copied, with `strip_x_offset` and `strip_y_offset` set to 100; `_.monads/_.livedesk-taskbar/khtpm_strip_header.{xhtpm,css}`.
3. `Xephyr :9 -screen 2400x1000 -ac -noreset &`, then `DISPLAY=:9 khtpm_core_render_new.+x <house> <house>/_.monads/_.livedesk-taskbar/khtpm_strip_header.xhtpm &`. The header appears at 100,100 (find it with `xwininfo -root -tree`, size WxH with H about 22 on this DPI).
4. Drive with `xdotool` (real X pointer events; the relay carries no motion): `mousemove <grip> mousedown 1 sleep 0.15 mousemove_relative --sync 8 4 mousemove <target> sleep 0.15 mouseup 1`, read the position with `xwininfo -id`.
5. Cases and what was observed (2026-10-15, grip at 118,111 in this scratch geometry):
   - small drag +30,+20: snaps home 100,100, no saved file. Mid-drag (button held) the bar was at 130,120: it really follows the pointer.
   - big drag +300,+250: stays at 400,350; file `dx=300 dy=250`.
   - drag to the bottom-right corner: clamped to 1381,478 (right edge on screen, bottom edge = 500 = half of 1000).
   - drag back within about 35 px of home: snaps 100,100, file deleted.
   - exactly 90 px: snaps; 91 px: stays at 191,100 (file `dx=91 dy=0`).
   - press on the HQ cell and drag 200 px: the bar does not move; a click on the HQ cell still reaches the normal click path (a `MOUSE_EVENT` line in the window's relay file), a margin press writes none.
   - click in the margin without moving: no move, no file.
   - restart the renderer with the saved file: comes up at 191,100.
   - **control:** the old renderer (the live binary before this change), same drag: stays at 100,100, no file.
6. Cleanup: kill your test renderer and Xephyr by pid (scan `/proc/*/exe`; never `pkill -f`).

**Not tested:** the real desktop (the live bar was not restarted); the combination with an open dropdown (drag is disabled while one is open by reading `g_dock_menu_w`, not exercised); a multi-monitor setup; and the window manager path (the bar is override-redirect on this machine; the Hyprland / Windows ports were not run).
