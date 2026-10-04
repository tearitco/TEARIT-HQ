# OMARCHY PORT — install pitfalls, native-vs-ported split, and the Xwayland input trap

Written 2026-10-03 on the `omarchy` branch, from a real first boot of
`$.crypts/button.sh run` on this machine. Every claim below was checked against
this box; command output is quoted where it matters. Companion doc:
`X11-AND-SESSION-PITFALLS.md` (the generic X11 rules) — this one is only about
what changes when the host is Omarchy.

---

## 0. TL;DR — the three things that actually blocked a first boot

1. **The build was silently broken by glibc 2.44.** `usleep()` and `nice()` were
   undeclared, so `+x/khtpm_core_render.+x` never built. FIXED, committed
   `ce50abfc7`.
2. **`install-deps.sh` is Debian-only and hard-exits on Omarchy** even though the
   deps it checks are *already present*. It is not the script you want here.
3. **`button.sh run` reports `rc=0` whether or not anything launched.** The
   LAUNCH row only *starts* a background build. Never trust it — see §4.

---

## 1. What this machine actually is

    $ cat /etc/os-release
    NAME="Omarchy"      ID=omarchy

    $ command -v apt-get dnf pacman
    /usr/bin/pacman                       # Arch-based; pacman, NOT apt

    $ echo "$XDG_SESSION_TYPE / $XDG_CURRENT_DESKTOP"
    wayland / Hyprland

    $ pgrep -a Hyprland
    1092 Hyprland --watchdog-fd 4
    1219 Xwayland :0 -rootless -core -listenfd 51 ...

    $ hyprctl monitors -j
    eDP-1  1920 x 1080 @ 1.5 scale

    $ ldd --version | head -1
    ldd (GNU libc) 2.44

The load-bearing fact: **there is no X11 window manager on this box.** Hyprland
is a Wayland compositor; `Xwayland :0` runs `-rootless` purely as a compatibility
shim for X11 clients. The livedesk is 100% X11 (`Xlib`/`Xft`/`XShape`), so the
*entire house* lives inside that shim. Every focus, grab and input-routing
assumption in the code was written against a real X WM and is wrong here. That is
the root of §5.

`DISPLAY=:0` is set and correct; `WAYLAND_DISPLAY=wayland-1`.

---

## 2. Install pitfalls (in the order you hit them)

### 2.1 `install-deps.sh` hard-exits on anything that isn't Debian/Ubuntu

`$.crypts/install-deps.sh:39-44` does `command -v apt-get || exit 1`. On Omarchy
that fires before it ever checks anything:

    == livedesk taskbar (khtpm) build-dependency install ==
    no apt-get found (this script targets Debian/Ubuntu).
    On other distros install the equivalents: pkg-config,
    libx11-dev, libxext-dev, libxft-dev.

**But the deps are already installed.** The script's own check, run by hand:

    $ for pc in x11 xext xft; do pkg-config --exists $pc && echo "OK $pc $(pkg-config --modversion $pc)"; done
    OK x11 1.8.13
    OK xext 1.3.7
    OK xft 2.3.9

So the deps leg is a no-op on Omarchy and the script is pure dead weight. Its
*second* half (`sh build_core_render.sh`, lines 66-76) is the part that matters
and is distro-independent. On this box, run that directly:

    sh _.monads/_.livedesk-taskbar/ops/build_core_render.sh

TODO(candidate): give `install-deps.sh` a pacman branch (`pacman -S --needed
libx11 libxext libxft`) so it stops being a Debian-only script in the house. It
should not be *needed* here, but it should not lie about being broken either.

### 2.2 glibc 2.44 hides `usleep()` and `nice()` — the build breaks

This is the one that actually stopped the desktop coming up.

    khtpm_core_render.c:2808:9: error: implicit declaration of function 'usleep'
    khtpm_core_render.c:2869:13: error: implicit declaration of function 'nice'

Two different causes, worth keeping straight:

* **`nice()`** is declared by glibc in `<sys/resource.h>`, *not* in `<unistd.h>`.
  `khtpm_core_render.c` included `<unistd.h>` (line 74) and never
  `<sys/resource.h>`. Plain missing include.
* **`usleep()`** was *removed* in POSIX.1-2008. glibc only declares it when
  `_DEFAULT_SOURCE` / `_BSD_SOURCE` / `_SVID_SOURCE` / `_XOPEN_SOURCE < 700` is in
  effect. `khtpm_core_render.c:1` pinned exactly one macro:

      #define _POSIX_C_SOURCE 200809L

  which on its own implies none of those. **This is why `-std=gnu11` does not fix
  it** — the explicit `#define` at line 1 pins the feature set regardless of the
  `-std` flag, so the std-flag instinct is a dead end here. (Verified: `gnu11`
  reproduced both errors identically.)

Both functions are used by `ktb_toggle_zorder_respawn()` — the 30 ms inter-cell
stagger and the `nice(8)` CPU-priority yield.

**Fix as committed** (`ce50abfc7`), at the *source* and not on the build line, so
every build path (`build_core_render.sh`, `build_khtpm_strip.sh`, anything added
later) inherits it:

    #define _GNU_SOURCE     /* implies _DEFAULT_SOURCE/_BSD_SOURCE/_XOPEN_SOURCE */
    #define _POSIX_C_SOURCE 200809L   /* unchanged, still there for CLOCK_MONOTONIC/getline */
    ...
    #include <unistd.h>
    #include <sys/resource.h>   /* nice() */

Generalisable lesson for the house: **on modern glibc, `_POSIX_C_SOURCE` alone is
not a feature set, it is a restriction.** Any file that pins it and then calls
anything outside POSIX.1-2008 needs `_GNU_SOURCE` as well.

### 2.3 Only Windows binaries are checked in

`ops/+x/` in git contains `*.exe` only. A fresh clone has no native `+x`
binaries, so the very first `button.sh run` must build everything from source
(a ~1 MB `khtpm_core_render.c`). Budget real time for that first boot; it is not
instant.

---

## 3. Build/lifecycle gotcha: the `.build_failed.txt` dead-man's switch

`build_khtpm_strip.sh` writes `+x/.build_failed.txt` before starting and removes
it only on its last line (`build_khtpm_strip.sh:64`, `:301`). `livedesk_splash.c`
polls it and shows a red "BUILD FAILED" banner if it is still there when the
build should have finished.

If you build *only* via `build_core_render.sh`, that marker is never cleaned and
a later launch shows a false BUILD FAILED. **Prefer `build_khtpm_strip.sh`** — it
clears the marker itself on success, which is the honest signal.

---

## 4. `rc=0` from `button.sh run` is not evidence anything started

    crypt_autostart: launch 'tool-bar' -> .../run_khtpm_strip.sh [boot]
    crypt_autostart: launch 'tool-bar' done (rc=0)     # <-- and the taskbar was ABSENT

The LAUNCH row backgrounds the build/launch and returns immediately, so `rc=0`
only means "the launcher was invoked". This is precisely the failure mode
`install-deps.sh`'s own header documents (lines 5-19): build-time dep missing →
binary never lands → nothing draws → `rc=0` anyway.

**House rule, restated because it is load-bearing here: verify with real
evidence.** After a launch, check actual processes and actual windows:

    pgrep -af 'khtpm_core_render|khtpm_taskbar_manager|khtpm_entity'
    DISPLAY=:0 xprop -root _NET_CLIENT_LIST

A good boot looks like (verified 2026-10-03):

    khtpm_taskbar_manager_main.+x  <house_root>
    khtpm_core_render.+x          <house_root> .../khtpm_strip_header.xhtpm
    khtpm_entity.+x              .../pals/cursword        (x9 pals)
    _NET_CLIENT_LIST: 0x600002, 0x800003, ...  (12 windows)

---

## 5. The input problem — why nav keys never reach the taskbar

**Symptom:** the taskbar never gets key/io at the "window focus" level. Clicking a
cell does not give it keyboard nav; it looks like native apps are stealing input.

**This is not a binding conflict.** Checked and cleared: Omarchy's default
bindings (`/usr/share/omarchy/default/hypr/bindings/*.lua`) bind *no* arrow,
Return or Escape keys, so nothing there is eating nav input.

**The real cause is the Xwayland focus model.** Under `-rootless` Xwayland:

* **An X client cannot give itself focus.** `XSetInputFocus()` from inside an
  Xwayland client is advisory at best. Only the *compositor* decides which surface
  has keyboard input. The house already knows this and works around it
  (`g_win_managed_focus`, `khtpm_core_render.c:2922-2926`, re-asserts
  `XSetInputFocus` on an idle tick) — that hack is a no-op under Xwayland.
* **`XGrabKeyboard` does not do what it did.** The dock calls
  `dock_grab_keyboard()` → `XGrabKeyboard(dpy, cw, True, GrabModeAsync,
  GrabModeAsync, CurrentTime)` (`khtpm_core_render.c:5780-5784`). Under a real X
  WM a keyboard grab is display-wide and effective. Under Xwayland the compositor
  still owns the keyboard, so the grab is at best a no-op for real keys and at
  worst freezes every *other* X client house-wide while Hyprland keeps routing
  keys to its own focused client — which presents exactly as "something else is
  stealing my input". The codebase already recorded that a grab here was
  "a real, confirmed regression" (`khtpm_core_render.c:6745-6749`).
* **Omarchy ships its own bar.** `omarchy-bar` is a Wayland layer-shell surface.
  Layer-shell surfaces sit above Xwayland clients and take input in their region.
  Any overlap with the strip's band is an input wall the X side cannot see or
  fight.

**So the fix is Omarchy-side, not house-side.** Three parts, in order of
importance:

1. **Give every livedesk window a real `WM_CLASS`, then make Hyprland treat the
   dock as a focusable floating window.** Without a class, *no* rule can match it
   at all — see §5.1, which is the actual fix and the one to do first.

   With the class set, add to `~/.config/hypr/bindings.lua` (Omarchy's Lua config,
   loaded *after* Omarchy defaults so package updates won't clobber it):

   ```lua
   -- Livedesk: bare X11 windows inside rootless Xwayland, WM_CLASS is
   -- "MuchiverseLivedesk" (set house-side). Force them floating and stop
   -- focus-follows-mouse from pulling input away mid-navigation.
   o.window("^MuchiverseLivedesk$", {
     float = true,
     no_follow_mouse = true,
     stay_focused = true,
   })
   ```

   `o.window` (Omarchy `default/hypr/helpers.lua:142`) is a thin wrapper over
   Hyprland's own `hl.window_rule`. The key names above are **verified** against
   real Omarchy usage (`no_follow_mouse` at `apps/jetbrains.lua:1`,
   `stay_focused` at `apps/davinci-resolve.lua:11`, `float` at
   `apps/localsend.lua:2`). Other keys you may also want — `noskiptaskbar`,
   `no_anim`, `pin`, `tag`, `opacity` — were **not** verified against this
   install; check `hyprctl configerrors` after reloading before relying on them.

   `no_follow_mouse` is the important one: focus-follows-mouse otherwise yanks
   input away the moment the pointer drifts toward another window, which reads as
   "something else is stealing my keys".

   Autostart note: `button.sh install-xdg` writes a **GNOME** `.desktop` into
   `~/.config/autostart/`, which Hyprland/Omarchy ignores. Use Omarchy's own hook
   in `~/.config/hypr/autostart.lua` instead:

   ```lua
   o.launch_on_start("sh /home/jbez/TEAR_IT_OM/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/$.crypts/button.sh run")
   ```

   (`o.launch_on_start(command)` exists — `default/hypr/helpers.lua:118`.)

2. **Stop the X-side keyboard grab from fighting the compositor.** Once Hyprland
   grants focus, `XGrabKeyboard` adds nothing and risks the display-wide lockout
   described in §3 of `X11-AND-SESSION-PITFALLS.md`. Gate
   `dock_grab_keyboard()` on an env flag the Omarchy launch path sets, so the
   house keeps its old behaviour on a real X WM and skips the grab under
   Xwayland.

3. **Keep Omarchy's bar out of the strip's band.** Either move the strip below
   the bar's exclusive zone or make `omarchy-bar` ignore that region. The strip
   currently sits at `strip_x_offset=200`, `strip_y_offset=50`
   (`#.desktop/livedesk_taskbar.pdl:152-153`) — check that against the bar's
   height before assuming the overlap.

Anything that needs a *native* input path (real keyboard grabs, real pointer
grabs, reliable key repeat while navving) has to become a Wayland client. That is
a port, not a patch — see §7.

---

## 5.1 The actual root cause: every livedesk window had an EMPTY `WM_CLASS`

This is the one that matters, and it is a **house-side** bug, not a compositor
misfuxture. It is easy to misdiagnose as "Hyprland steals my input".

`khtpm_core_render.c:16142-16144` already documents the house's intent:

> xwayland-grab-access-rules allowlists by WM_CLASS - these windows need a real
> WM_CLASS ("MuchiverseLivedesk"), matched by ...

…but the class hint was only ever actually set on a few **popup** windows in
`khtpm_core_render.c` (`:16146`, `:17113`, `:17979`). It was **never set on the
main top-level windows**, in either binary:

* `khtpm_entity.c` — 10 `XCreateWindow` sites, **zero** `XSetClassHint` calls.
* `khtpm_core_render.c` — the generic path at `:20049`, the tile-mode window at
  `:17096`, the bottom dock bar at `:19445`, the dock menu at `:6454` — all
  missing.

Consequence, confirmed live before the fix — every livedesk window was
unidentifiable:

    $ hyprctl clients -j | jq -r '.[] | select(.xwayland) | .class'
    ''            # taskbar header
    ''            # taskbar bottom bar
    ''            # tile:cursword-CURS
    ''            # tile:book-stack-BKST:...
    ...           # all 11 xwayland windows, every one empty

With an empty class:

* no Hyprland/windowmaker window rule can ever match the dock,
* `hyprctl dispatch focuswindow class:...` can never target it,
* Xwayland's `xwayland-grab-access-rules` allowlist can never name it.

So *any* compositor-side fix is unreachable until the class exists. This is why
the symptom looks like "the compositor won't give me focus" when in fact the
compositor has no name to match on.

**Fix applied** — `XSetClassHint` right after each top-level `XCreateWindow`, in
the same compound-literal form the house already uses at `:17113`:

    XSetClassHint(dpy, win, &(XClassHint){(char *)"MuchiverseLivedesk", (char *)"MuchiverseLivedesk"});

Verified after rebuild + relaunch — all 11 windows now identify:

    $ hyprctl clients -j | jq -r '.[] | select(.xwayland) | .class' | sort -u
    'MuchiverseLivedesk'

Only now are the compositor-side rules in §5 step 1 actually able to match.

Note the X11-side grab remains a genuine problem regardless — see §5. X11
`XGrabKeyboard` is display-wide and effectively inert under Xwayland, so it
should still be gated off on this host rather than relied upon.

---

## 5.2 CORRECTION — pals must stay unmanaged; only the dock bars get ICCCM focus

**An earlier version of this section was wrong and is corrected here.** It claimed
the fix was to flip `#.desktop/livedesk_override_redirect.pdl` to `false` and give
the pals ICCCM hints. That was a misread of what the pals *are*, and it was
reverted. Do not re-apply it.

The distinction that matters:

| | what it is | who may manage it |
|---|---|---|
| **pals / entities** | free, movable, playable pieces that snap to **the house's own** grid | **nobody.** `g_override_redirect=1` (`khtpm_entity.c:101`) is load-bearing — it keeps every compositor from touching their geometry, z-order or shape masks |
| **entity context menus** | short-lived transient popups | could be Wayland-managed; the house already has a proven path for this |
| **dock bars (header + bottom)** | the taskbar itself | genuinely WM-managed, and the only windows that want an ICCCM focus handshake |

Making the pals WM-managed to "fix focus" was the wrong fix for the right symptom:
it handed their layout to Hyprland, which is precisely what they must not have.
They are not mini `x11-hq` windows — `x11-hq` *is* a compositor-style managed
window; a pal is not.

So the fix is **split by role**, and only the managed windows get managed-window
treatment:

1. **`WM_CLASS` on every top-level window** (`khtpm_core_render.c` generic path
   `:20049`, tile window `:17096`, bottom dock bar, dock menu, and
   `khtpm_entity.c`'s pal window). Purely identification — it does **not** make a
   window managed, and it is what let the dock bars escape Omarchy's `no_focus`
   rule in §5.1. Safe to keep on the pals.
2. **ICCCM `WM_TAKE_FOCUS` handshake, dock bars only.** Registered behind
   `if (window_is_dock())` near window creation, handled in
   `hq_dispatch_xevent()`. The dock bars already had `input=True` from
   `apply_dock_window_hints()`; what they lacked was the handshake. Deliberately
   **not** applied to the pals — their input path is the house's own, not the
   compositor's.

Verified live after the split — the roles are observably different, which is the
point:

    dock bars  : WM_HINTS input=True,  WM_TAKE_FOCUS advertised
    all 10 pals: WM_HINTS input absent, WM_TAKE_FOCUS absent   (unmanaged, free)

Confirmed working on this host: **top bar, bottom bar, and entity context menus
all take key/nav input.**

### 5.3 Do not commit the pdl

`#.desktop/livedesk_override_redirect.pdl` stays `override_redirect=true` — it is
the pals' grid guarantee. It is runtime state (per `AGENTS.md`, `*.pdl` is never
swept into a code commit) so it is never committed anyway; do not "fix" it by
editing it either. If a future session ever needs the pals managed, that is a
per-pal decision made in the pal's own config, not a house-wide flip.

---

## 6. Geometry/scale facts that will bite any layout work

* Screen is **1920x1080, scale 1.5**. `kh_screen_w()`
  (`khtpm_core_render.c:3204-3205`) returns `DisplayWidth` = **1920**; the 1.5
  scale does not change X11 coordinates. Headless builds fake `1920x1080`
  (`g_headless` branch, same function) — so headless and this real box are
  coincidentally the same width, which hides size bugs from headless testing.
* **Omarchy exports `GDK_SCALE=2`** (`~/.config/hypr/monitors.lua`,
  `hl.env("GDK_SCALE", "2")`). Every process the livedesk spawns inherits it.
  Harmless for raw Xlib, but it changes how any GTK child app sizes itself — a
  frequent source of "the app is huge/wrong inside the livedesk" reports.
* Omarchy also exports `OZONE_PLATFORM=wayland`, `QT_QPA_PLATFORM=wayland;xcb`,
  `GDK_BACKEND=wayland,x11,*`, `MOZ_ENABLE_WAYLAND=1`. **Any native app the
  livedesk launches will try Wayland first**, not X11. If an app must talk to the
  livedesk over X11, it has to be launched with the X11-forcing env explicitly
  re-set, or it will silently pick the Wayland backend and not appear in
  `_NET_CLIENT_LIST`.
* House UI scale is **independent and additive**: `#.desktop/hq_ui.pdl:28` has
  `font_scale=1.25` → `g_ui_scale_pct=125` → `scaled()`. Only *some* constants are
  scaled (e.g. `DOCK_BAR_H`), which is why the dock is ~25% bigger than its
  constants suggest. See §6 of the taskbar-overflow notes below.

### Known taskbar overflow (measured, not theoretical)

The header row is laid out by `layout_dock_toolbar_row()`
(`khtpm_core_render.c:5871-5913`). It assigns an `x` to **every** child and only
clamps the *return value*:

    col_x += cw + DOCK_CELL_GAP;
    used = col_x - x;
    if (used > max_w) used = max_w;   /* clamps the RETURN only — no break, no re-wrap */

With `max_w = sw - ox*2 = 1920 - 400 = 1520` and 17 cells at `font_scale=1.25`
needing ~2000px+, the tail cells (`store`, `network`, `h-ai`, the CJK datetime,
the PID cell) land at `x > g_win_w` and draw outside the window. The window is
sized from the *clamped* width and content is clipped, never reflowed
(`XResizeWindow` at `khtpm_core_render.c:9998-10003`). That is the "taskbar too
big / items overlapping" report.

Aggravating factor: cell width is measured with real Xft metrics
(`dock_text_px()`, `khtpm_core_render.c:5836-5868`) but the *padding around* it is
hardcoded unscaled — `DOCK_CELL_GAP 16`, `DOCK_NAV_BADGE_PX 36`, `DOCK_SPRITE_PX
24`, `DOCK_FOCUS_BOX_W 64`, plus literal `6`/`10` pads. The nav badge alone is
budgeted at 36px but a two-digit `"[ ]16."` at the scaled font is ~40px, and
`khtpm_draw_core.c:1245` still hardcodes a `36` badge offset. The house already
fixed this exact bug class once for the pager (`khtpm_core_render.c:5952-5967`)
and `dock_item_cw()` never got the same treatment.

Also note `&.widgits/_shared-lib/khtpm_ui_scale.c` is **dead code for the dock** —
zero references from `khtpm_core_render.c`. The real scale is `g_ui_scale_pct`.
Don't go looking there for the answer.

---

## 7. Alpha / transparency status

* There is **exactly one** alpha concept in the house: a house-global
  `COLOR | opacity` row in `#.desktop/livedesk_theme.pdl` (currently `1.00`).
* It is applied as a **whole-window** EWMH hint (`_NET_WM_WINDOW_OPACITY`),
  not per-pixel: `set_window_opacity()` at `khtpm_entity.c:213-219` and
  `khtpm_core_render.c:429-434`.
* The only UI for it is an `Opacity -/+` stepper in the **taskbar-settings** pal
  (`&.widgits/taskbar-settings/taskbar-settings-pal.xhtpm:62-63` →
  `khtpm_core_render.c:7875-7895` → `write_theme_opacity()`
  `khtpm_core_render.c:603-683`). It is **not** in any entity menu, and it is
  global, not per-entity. So "alpha is not available for the entities" is
  literally true: there is no entity-scoped alpha to have.
* `#.desktop/livedesk_theme_changed.txt` — the marker both writers create so
  entities re-apply opacity — **does not exist**, so the opacity write path has
  never actually run in this house root. It is still at the shipped `1.00`.
* Per-pixel alpha is architecturally unavailable for 9 of the 10 entities: the
  ARGB32 visual request is hard-gated to cursword only
  (`khtpm_entity.c:4553-4558`, `if (g_is_cursword) ... XMatchVisualInfo(... 32,
  TrueColor ...)`) and everything else gets the plain opaque DefaultVisual plus a
  forced opaque background (`khtpm_entity.c:4583-4584`). Sprite alpha is
  *binarised* into a 1bpp `XShapeCombineMask` cutout
  (`khtpm_entity.c:2450-2455`, `:2471`) — a hard silhouette, not translucency.
  `XRender`/`PictFormat`/`Composite` are not used and `-lXrender` is not even
  linked (`build_core_render.sh:22`).
* The CSS parser has **no** `alpha`/`opacity`/`rgba` support — `CssStyle`
  (`khtpm_css_parser.h:16-56`) has no such field, and `parse_declaration()`
  (`khtpm_css_parser.c:49-116`) silently ignores unknown properties. So
  `opacity: 0.5` in any `.css` is accepted as syntax and then dropped.

Two latent bugs in the opacity write path, flagged while reading (not the
reported symptom, but they will bite whoever wires this up):

* `write_theme_opacity()` (`khtpm_core_render.c:603-660`) rewrites the theme file
  from a **16-line** `char lines[16][PATH_BUF]` buffer using `fgets` with **no
  input length cap**, then `fopen(path,"w")` truncates the real file. It only
  survives because the file is currently 5 lines.
* `khtpm_core_render.c:646-647` — `if (opacity_line_idx < 0) return;` means on a
  theme file lacking the row, `Opacity -/+` silently changes nothing while still
  applying to the settings window. A "looks like it works but doesn't" dead end.

---

## 8. Native to Omarchy vs. has to be ported

**Already native / free**

* Hyprland's Lua config (`~/.config/hypr/*.lua`, `hl.*` DSL) — user overrides in
  `bindings.lua`/`input.lua`/`autostart.lua` are loaded *after* Omarchy defaults,
  so they survive package updates. That is the right place for the focus rules in
  §5. Note `autostart.lua` is the intended hook for launching the livedesk at
  login; `install-xdg` in `button.sh` installs a **GNOME** `.desktop` autostart
  file, which Hyprland/Omarchy ignores — use `o.launch_on_start(...)` instead.
* pacman, `pkg-config`, and the X11/Xext/Xft dev headers — all already correct.
* `GDK_SCALE`, `omarchy-bar`, and the rest of `/usr/share/omarchy/bin` — just be
  aware they exist and that they interact with the livedesk (bar input region,
  inherited scale env).

**Must be patched house-side**

* `_GNU_SOURCE` + `<sys/resource.h>` (§2.2) — done, `ce50abfc7`.
* `install-deps.sh` needs a non-Debian branch or should stop pretending (§2.1).
* Dock header layout must clamp/wrap instead of only clamping the return value
  (§6).
* Un-scaled dock constants vs `font_scale=1.25` (§6).
* `install-xdg`'s GNOME `.desktop` is the wrong autostart mechanism here (§8).

**Needs a real decision, not a patch**

* **Reliable keyboard/pointer grabs and focus.** X11 grabs are display-wide and
  effectively dead under Xwayland. Either accept compositor-mediated focus and
  drop the X grabs, or port the strip to a Wayland client (layer-shell surface +
  `zwlr_layer_shell_v1` for a real always-on-top dock, `ext-session-lock` or a
  focused xdg-shell surface for input). The layer-shell route is the one that
  actually matches what this house wants from a dock, and it also solves the
  `omarchy-bar` overlap properly.
* **Per-pixel alpha for entities** needs an ARGB visual for every entity
  (drop the `g_is_cursword` gate at `khtpm_entity.c:4555`, stop forcing the opaque
  background at `:4583-4584`) and, for true translucency, either `-lXrender` +
  `Composite` or a real compositor protocol. Under Hyprland the honest modern
  answer is a compositor-side alpha/opacity rule per window.

---

## 9. Reproducing a clean first boot on a fresh clone

    cd <house>/$.crypts
    sh button.sh check                 # binary + pdl present?
    sh ../_.monads/_.livedesk-taskbar/ops/build_khtpm_strip.sh   # full build, clears .build_failed.txt
    sh button.sh run
    # then VERIFY, do not trust rc=0:
    pgrep -af 'khtpm_core_render|khtpm_taskbar_manager|khtpm_entity'
    DISPLAY=:0 xprop -root _NET_CLIENT_LIST

Shut down with `sh button.sh quit`. `reset` is the harder
kill-everything-then-rebuild-and-relaunch path and correctly refuses to launch a
stale binary on build failure.
