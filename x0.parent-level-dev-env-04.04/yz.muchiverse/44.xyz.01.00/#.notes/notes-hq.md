# notes - hq

_dev notes for the hq subsystem. Opened from HQ menu -> notes-hq._


## 2026-09-08 — toolbar disappears under fullscreen video (pals stay visible)

**Symptom:** YouTube (Chrome) fullscreen → desktop entities/pals still
visible (good), but the taskbar strip is covered.

**Likely cause (not yet fixed — investigation only):**

1. Inconsistent zorder state on disk:
   - `#.desktop/livedesk_override_redirect.pdl` = `override_redirect=true`
   - `#.desktop/khtpm_zorder_mode.state.txt` = `mode=above`
   `render_managed_wm_hints()` in `khtpm_core_render.c` does
   `if (g_override_redirect) return;` FIRST — so while the strip is
   override_redirect, the "above" mode never actually applies its
   `_NET_WM_STATE_ABOVE` + `_NET_WM_WINDOW_TYPE_DOCK` hints. The strip
   is a plain override_redirect top-level with no dock/above hint.

2. Under Wayland/GNOME, Chrome runs in Xwayland. When an Xwayland
   client goes fullscreen, mutter promotes that surface to the top of
   the Xwayland stack — **above** the house's override_redirect
   windows. So the fullscreen video covers everything house-side.

3. Why the pals survive but the strip doesn't: each pal renderer runs
   a continuous redraw loop that calls `XRaiseWindow(self)` every
   frame, so they pop back above the video within a frame. The strip
   uses the marker-gated ("DIAMOND") redraw — it only repaints/raises
   when *strip* state changes. A playing video changes nothing in
   strip state → the strip never re-raises → it stays buried.

**Options to consider later (do NOT do yet):**
- (best) set `override_redirect=false` so the strip is genuinely
  WM-managed + `_NET_WM_WINDOW_TYPE_DOCK` + struts. A real DOCK window
  with struts is kept visible by mutter even over fullscreen (that's
  how GNOME's own top bar behaves). Also fixes the inconsistent state.
- (cheap) low-frequency unconditional `XRaiseWindow(strip)` on a
  ~1–2 s timer, bypassing the marker gate — pops back like the pals,
  but fights the compositor / can flicker.
- (event-driven) watch other windows' `_NET_WM_STATE` /
  `_NET_ACTIVE_WINDOW` and re-raise the strip when a fullscreen
  appears.
