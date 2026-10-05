# _attic-gtk-embed — shelved, not deleted

**Shelved 2026-09-03.** These were a proof-of-concept for the "embed a
real engine" path (NB-JS-ENGINE-ROADMAP.md §8 path B/C). The house
decision is **path A: hand-built, a few pages of code at a time, do our
own rendering** — no engine embed (not CEF, not WebKitGTK). So this is
parked here in case that call is ever revisited.

| file | what it was |
|---|---|
| `nb_webkit_view.c` | minimal WebKitGTK window; grew an `--xembed <xid>` mode to reparent the page into a child X window of a khtpm-drawn frame |
| `nb_embed_demo.c` | Xlib PoC: draw a fake chrome strip, create a content sub-window, spawn `nb_webkit_view --xembed` into it |
| `build_nb_webkit_view.sh` | compiles the above only if `webkit2gtk-4.0` + `gtk+-3.0` dev pkgs exist (they do on this box: libwebkit2gtk-4.0 2.50.4, ~86 MB .so) |
| `nb_open_real.sh` | the cheap escape hatch — shell out to `nb_webkit_view.+x` else `$NB_REAL_BROWSER` / firefox / chrome / xdg-open. This one actually worked. |

### Status when shelved
- `nb_webkit_view` (own top-level window): built + ran (X11, ~180 MB RSS on example.com).
- `--xembed` reparent: buggy — GTK fights `XReparentWindow` of its own
  toplevel; `gdk_x11_window_foreign_new_for_display` threw "drawable is
  not a native X11 window" and the view exited. A GtkPlug/GtkSocket
  XEMBED handshake, or a raw-Xlib global GDK event filter on the
  container, would be the fix.
- `nb_open_real.sh`: worked (with `DISPLAY=:0` on the spawn).

### If revisited: the real answer was "our own pixels"
Neither XEmbed (engine owns the rect) nor a separate window is what the
house wants. The only "our chrome + real engine" that keeps khtpm
owning every pixel is **offscreen render → blit the buffer** (CEF's OSR
mode). That needs CEF (~200 MB, ship it) — explicitly rejected here as
"all of chromium". Hence: hand-built.
