# OMARCHY BRANCH — OPEN ITEMS (carried forward 2026-10-03)

Parking note for the `omarchy` branch. Companion to
`03-pitfalls/OMARCHY-PORT.md` (which records what was *done* and why). This file
records what is **still open**, including things that are diagnosed but not yet
fixed, and one measurement that does not add up yet.

Branch state at time of writing: `omarchy` == `origin/omarchy`, four commits on
top of `claude` (`8c7d1d9d6`):

    ce50abfc7  fix: expose usleep()/nice() so the livedesk taskbar builds on glibc 2.44
    58f18af39  fix: set WM_CLASS on every livedesk top-level window
    caf5a81ef  fix: ICCCM input hint + WM_TAKE_FOCUS on pal windows      [partly reverted]
    34dbc9726  revert: scope WM_TAKE_FOCUS to the dock bars only

Verified working on this host: the livedesk launches, the top bar, the bottom
bar, and entity context menus all take key/nav input.

---

## DONE (context only, no action needed)

* glibc 2.44 build break — `usleep`/`nice`. See `OMARCHY-PORT.md` §2.2.
* `install-deps.sh` is Debian-only and hard-exits on pacman systems
  (`OMARCHY-PORT.md` §2.1). Cosmetic here — deps are already installed.
* `WM_CLASS` on all top-level windows. This was the *actual* input blocker:
  Omarchy ships `o.window({class="^$", title="^$", xwayland=true}, {no_focus=true})`
  (`/usr/share/omarchy/default/hypr/windows.lua:9-20`), so windows arriving with
  an empty class were **explicitly denied focus** by Omarchy. Fixed.
* ICCCM `WM_TAKE_FOCUS` handshake, scoped to the dock bars. Pals deliberately
  stay unmanaged — see the correction in `OMARCHY-PORT.md` §5.2, do not
  re-apply the reverted version.

---

## OPEN 1 — Taskbar overflow ("too big", items overlapping)

### Root cause found: the dock never applies the screen-relative auto-scale

`hq_ui.pdl` documents `ui_scale` + `ui_ref_*` as scaling "taskbar size,
entity/grid size and the strip's left margin … with the monitor", and
`ui_ref_width=2496` / `ui_ref_height=1664` is the screen the layouts were tuned
on. But:

* `khtpm_entity.c:65` does `#include "khtpm_ui_scale.c"`, and
  `kh_ui_apply_scale()` (`:318-325`) computes
  `g_ui_scale_pct = font_scale × kps_auto_pct(...)`.
* `khtpm_core_render.c` **never** includes it and never reads `ui_scale` /
  `ui_ref_*`. `:12987` sets `g_ui_scale_pct` from `font_scale` alone.

So on this 1920x1080 screen:

    auto = min(1920/2496, 1080/1664) = min(0.769, 0.649) = 0.649
    entities : 1.25 x 0.649 = 81%
    dock     : 1.25         = 125%     <-- ~1.54x too big

**The fix is to wire it up, not to change a number.** Mirror `kh_ui_apply_scale()`
inside `khtpm_core_render.c` so the dock's `scaled()` (`:3193-3199`) uses the same
`font_scale × auto` product. Bonus: once wired, `ui_scale=0.8` in `hq_ui.pdl`
actually does something, live-reloaded, no rebuild — which is the "there should
be a `.pdl` that decides" instinct, already half-built and just not connected to
the dock.

Note: `origin/opencode` has **identical** sizing values (`font_scale=1.25`,
`ui_scale` and `ui_ref_*` both commented out) — it is an older state, and has
nothing to copy here.

### OPEN 1b — secondary: unscaled dock padding

Even with the auto-scale wired, these are hardcoded and never pass through
`scaled()` while the text does: `DOCK_CELL_GAP 16`, `DOCK_NAV_BADGE_PX 36`,
`DOCK_SPRITE_PX 24`, `DOCK_FOCUS_BOX_W 64` (`:5749-5765`), the literal `6`/`10`
pads in `dock_item_cw()` (`:5915-5924`), and a hardcoded `36` badge offset at
`&.widgits/_shared-lib/khtpm_draw_core.c:1245`. A two-digit `"[ ]16."` badge is
~40px against a 36px budget. The house already fixed this exact class once for
the pager (`:5952-5967`); `dock_item_cw()` never got the same treatment.

### OPEN 1c — header row cannot wrap

`layout_dock_toolbar_row()` (`:5871-5913`) assigns an `x` to every child and
clamps only its **return value**:

    col_x += cw + DOCK_CELL_GAP;
    used = col_x - x;
    if (used > max_w) used = max_w;   /* clamps the RETURN only - no break, no re-wrap */

The bottom bar has a real flex-wrap engine (`.dock-flexrow`, pager, hidden row
2); the header deliberately uses this old hand-packed path and does not. So the
header's only options today are shrink or clip. Wiring the auto-scale may make
this moot — re-measure before building a wrap path for it.

---

## OPEN 2 — Geometry measurement does not add up (BLOCKS sizing work)

Verified directly, and it contradicts the numbers used in OPEN 1:

    $ DISPLAY=:0 /tmp/w      # tiny Xlib probe
    DisplayWidth=1920 DisplayHeight=1080

    $ hyprctl clients -j     # xwayland windows with no "tile:" title
    at [133, 33]   size [1013, 30]     floating, pinned=False
    at [176, 63]   size [145, 420]     floating, pinned=False
    at [133, 690]  size [1014, 30]     floating, pinned=False

Unexplained, so **resolve before sizing work**:

* The header should be `x = strip_x_offset = 200` and ~1520 wide
  (`#.desktop/livedesk_taskbar.pdl:152-153`). Observed x=133, width ~1013.
* `DOCK_BAR_H = scaled(36)` = 45 at 125%. Observed height 30.
* Only **3** X11 windows have no `tile:` title, but there should be 2 dock bars.
  The third (`145x420`) matches `pager_cell_w=145`, so it may be the pager cell
  rather than a dock bar — unconfirmed.
* Both dock windows have **no `WM_NAME`** at all (`xprop` → "not found").

So either `hyprctl` is reporting something other than the dock windows, or the
dock's own geometry is not what the code says. Do not trust the overflow
estimates in OPEN 1 until this is pinned down — they were derived from
`kh_screen_w()` and the layout constants, not from the real windows.

---

## OPEN 3 — Dropdown nav "keeps jumping back to top"

Keys *do* arrive (cell nav works), so this is nav-state reset, not focus.
Two candidates, **neither confirmed** — instrument, do not guess. This code has
already collected three adjacent fixes with overlapping symptoms
(`:5914-5967`, `:6197-6229`, `:6619-6651`, `:6913-6942`), so a fourth blind fix
is how the last three happened.

1. **`dock_nav_after_step()` (`:10179-10188`) runs on every arrow press** and does
   `XSetInputFocus(..., CurrentTime)` + `dock_grab_keyboard()`. Under Xwayland
   that fights the compositor; the resulting re-layout hits the clamp at
   `:6951-6953` / `:6987-6989`, which snaps `g_focus_nav` back to
   `g_dock_drop_lo` = top of the dropdown.
2. **`g_dock_drop_hi` is written unconditionally** by both dropdown layouters
   (`:5195-5196`, `:6268-6269`) while `_lo` guards on `if (!g_dock_drop_lo)`. If
   `_hi` lands too small, every step past it is yanked back to `_lo`.

Restarting does **not** help — already rebuilt and relaunched with the current
code and cell nav works while dropdown nav does not.

Suggested first probe: log `g_focus_nav`, `g_dock_drop_lo`, `g_dock_drop_hi`,
`g_n_nav` on each arrow key and see which one moves.

---

## OPEN 4 — Entity alpha / transparency

Two separate problems; neither is fixed.

### 4a. Live opacity changes need a restart (bug)

Whole-window `_NET_WM_WINDOW_OPACITY` works, but **only at launch**:

    # with 0.60 in livedesk_theme.pdl + a restart -> every window reports
    _NET_WM_WINDOW_OPACITY = 2576980377      # exactly 0.60 x 0xFFFFFFFF. correct.

    # change 1.00 -> 0.60 while running, touch livedesk_theme_changed.txt, wait
    _NET_WM_WINDOW_OPACITY = 4294967295      # still 1.00. no effect.

So `theme_changed_dirty()` (`khtpm_entity.c:806-817`) is not firing. Restart-only
is a bug in its own right — the whole point of the marker file is live reload.

### 4b. No per-pixel alpha for 9 of 10 pals

The ARGB32 visual request is hard-gated to cursword
(`khtpm_entity.c:4555`, `if (g_is_cursword) have_argb_visual = XMatchVisualInfo(...32...)`).
Everything else gets the plain opaque `DefaultVisual` plus a forced opaque
background (`:4583-4584`). Sprite alpha is **binarised** to a 1bpp
`XShapeCombineMask` silhouette (`:2450-2455`, `:2471`) — a hard cutout, not
translucency. `XRender`/`PictFormat`/`Composite` are absent and `-lXrender` is
not linked (`build_core_render.sh:22`). `khtpm_css_parser.c` has no
`alpha`/`opacity`/`rgba` and silently drops unknown properties.

Also: there is **no per-entity alpha concept at all**. The only alpha in the house
is one house-global `COLOR|opacity`, driven by an `Opacity -/+` stepper in the
**taskbar-settings** pal (`&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm:62-63`),
not any entity menu.

Undecided, needs a call: per-entity alpha control, real per-pixel translucency, or
a CSS `alpha` property. They are three different pieces of work.

Two latent bugs in the opacity writer to avoid inheriting
(`khtpm_core_render.c:603-683`): it rewrites the theme file from a **16-line**
`char lines[16][PATH_BUF]` buffer with uncapped `fgets` then truncates the real
file; and `:646-647` `if (opacity_line_idx < 0) return;` makes `Opacity -/+` a
silent no-op on a theme file lacking the row.

---

## OPEN 5 — `_NET_WM_STATE_ABOVE` is not sticking

Before any testing, **every** window read `_NET_WM_STATE = none`, including both
dock bars. `apply_dock_window_hints()` (`:379-405`) sets `_NET_WM_STATE_ABOVE`,
and it can be set by hand and does read back:

    $ DISPLAY=:0 xprop -id 0x600003 -f _NET_WM_STATE 32a \
        -set _NET_WM_STATE _NET_WM_STATE_ABOVE
    $ DISPLAY=:0 xprop -id 0x600003 _NET_WM_STATE
    _NET_WM_STATE_ABOVE

but Hyprland still reports `pinned: False` for it. So the property is stored and
the compositor is ignoring it for these windows. Unresolved: is this Hyprland
not honouring `ABOVE` from X11 clients, or is something clearing it?

This is why the always-on-top story is currently incoherent — see OPEN 6.

---

## OPEN 6 — Screensaver overlap (needs a decision, not a fix)

Omarchy's screensaver is **not** a lock surface. It is an ordinary fullscreen
floating window:

    /usr/share/omarchy/default/hypr/apps/system.lua:34-37
    -- Fullscreen screensaver.
    o.window("org.omarchy.screensaver", { fullscreen = true })
    o.window("org.omarchy.screensaver", { float = true })

Nothing is composited above everything, so this is purely z-order — which is why
the bottom bar can appear over it at all.

* **Top bar — should be achievable.** Managed X11 window; see OPEN 5 for why it
  currently is not.
* **Entities — not achievable from house code.** They are `override_redirect`, so
  Hyprland does not manage them and cannot pin or order them. Their position
  relative to a Wayland surface is Xwayland's call.

Open question worth settling first: **is the bottom bar genuinely drawn over the
screensaver image, or is the screensaver simply not covering that strip?** Those
need completely different fixes.

Deliberately not acted on: rendering pals over the screen when you step away is a
privacy regression, and since `org.omarchy.screensaver` is a plain window rather
than a lock, `omarchy-system-lock` is what actually locks. Worth a real decision
before making pals show over it.

---

## Deferred / low priority

* **Gate off `XGrabKeyboard` under Xwayland.** `dock_grab_keyboard()` (`:5780-5784`)
  is display-wide and, per the house's own note at `:6745-6749`, was already a
  confirmed regression once. Inert at best under Xwayland, capable of freezing
  every other X client at worst. Gate it on an env flag the Omarchy launch path
  sets.
* **`install-xdg` writes a GNOME `.desktop`** into `~/.config/autostart/`, which
  Hyprland/Omarchy ignores. The house-native hook is `o.launch_on_start(...)` in
  `~/.config/hypr/autostart.lua` (`default/hypr/helpers.lua:118`).
* **Optional Hyprland rule** (documented in `OMARCHY-PORT.md` §5 step 1, keys
  verified against real Omarchy usage): if focus still proves flaky after
  OPEN 3, `o.window("^MuchiverseLivedesk$", { float = true, no_follow_mouse =
  true, stay_focused = true })`.
* **Long-term:** reliable keyboard/pointer grabs and true translucency both want
  a real Wayland client (layer-shell for the dock, `ext-session-lock` or a focused
  xdg-shell surface for input). That is a port, not a patch.
