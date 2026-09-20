# Brainstorm: font size / UI scale control in Settings

**Status: brainstorm only, not scheduled, no code written.**
Started 2026-09-05, direct request: "we never did fontchange or font
size (and change surrounding sizes of windows, like buttons etc.
should we write a plan doc for this first?" — yes, this is that doc,
one level below a full plan: real current-state findings + options,
not yet a committed design.

## Why a doc first

This isn't a small, isolated toggle like `click_two_step` (one bool,
one read site). "Font size" implies scaling text AND the boxes text
lives in — buttons, chrome height, row heights, badge chips, dock
height — across every window mode this shared renderer draws
(default/popup, tile, dock, swatch-grid). That's a lot of surface
area to touch once, so it's worth being honest about scope before
starting, per the house's own standing rule (`CENTROID_GOLD_STD.md`)
against half-finished, per-mode-inconsistent features.

## Real current-state findings (checked live in code, 2026-09-05)

- `#.desktop/hq_ui.pdl` already documents a `font_scale` key ("font_scale:
  multiplier applied to both font sizes and layout box sizes... Range
  0.5-3.0") and ships a real value (`font_scale=1.25`).
- **That value is not read by anything.** `khtpm_core_render.c`'s own
  `scaled()` function — the one thing every font-size and
  layout-box-size call site in the file goes through (`scaled(28)` row
  heights, `scaled(4)`/`scaled(6)` paddings, badge chip padding, etc.)
  — is a plain identity function:
  ```c
  static int scaled(int base_px) {
      return base_px;
  }
  ```
  The comment directly above it describes a `g_dbhq_font_scale`
  variable "read from `#.desktop/hq_ui.pdl`" that **does not exist
  anywhere in the file** (confirmed via grep — zero declaration or
  assignment sites, only stale comments referencing it). This is the
  same class of stale-comment problem this session already found and
  fixed once for `click_two_step` and `dbhq_load_font_scale()` — a
  real feature was apparently designed and partially documented, then
  either never finished or removed later without the comments being
  updated. Do not trust the comments near `scaled()` as evidence
  anything is wired up; verified today that it isn't.
- Font SIZES themselves (as opposed to layout box sizes) come from two
  places: a CSS-settable `font-size` per element
  (`CssStyle.has_font_size`/`font_size`, `khtpm_css_parser.c`) that
  most elements never set, falling back to hardcoded literals scattered
  through the file (`"DejaVu Sans:pixelsize=12"`, `=10`, `=9`, etc. —
  at least 3 distinct hardcoded font specs found in a first pass).
- Settings (`taskbar-settings-pal.xhtpm`/`taskbar_settings_projector.c`/
  `swatch_picker_manager.c`) currently has exactly two real toggles:
  opacity (`OPACITY_MINUS`/`OPACITY_PLUS`) and `click_two_step`
  (added this session). No font-size control exists there yet, and no
  UI element type for a slider/stepper exists in the xhtpm vocabulary
  — the opacity buttons are just two `<item>` buttons with `+`/`-`
  actions, not a real range control; font size would likely reuse that
  same "two buttons, step the value, redraw" shape rather than
  inventing a slider primitive, unless we decide otherwise (see
  options below).

## What "surrounding sizes" actually means, concretely

If font size becomes real and adjustable, at minimum these fixed
constants/literals in `khtpm_core_render.c` would need to become
scale-aware (this list is from a first grep pass, not exhaustive):

- `CHROME_H` (window title-bar height, currently `24`)
- `DOCK_BAR_H` (taskbar strip height, currently `36`)
- `POPUP_ROW_H` (context-menu row height, currently `28`)
- Row heights inside db-hq/events-hq-style layouts (`scaled(28)` calls)
- Button/chip padding (`scaled(4)`, `scaled(6)`, badge chip padding)
- The nav-badge chip's own size (currently computed from the badge
  font's own ascent/descent + a fixed `chip_pad` — see the badge-chip
  work from earlier today, `khtpm_draw_core.c`)
- Sprite/icon sizes that are currently fixed pixel caps (e.g. the
  dock's own `24px` sprite cap for short bars)
- Window minimum/default sizes computed at launch (`g_win_w`/`g_win_h`
  defaults) — a bigger font needs a bigger default window or text
  clips immediately on open

This is exactly why `scaled()` already exists as a single choke point
in the code — the right architecture (one function every size call
site funnels through) is already there and half-wired; what's missing
is (a) actually loading a real scale value into it, and (b) auditing
which of the size constants above go through `scaled()` today vs. are
still raw literals that would silently NOT scale.

## Options to consider (none chosen yet)

1. **Finish wiring the existing `font_scale` PDL key.** Lowest-risk,
   reuses infrastructure that's already half-built and already
   documented in `hq_ui.pdl`. Load it once at startup (same pattern as
   `click_two_step`), make `scaled()` actually multiply, and audit the
   raw-literal size constants above to route them through `scaled()`
   too. Live-reload (like this session's `click_two_step` fix for the
   taskbar) would need the same `hq_ui_pdl_reload_if_changed()`
   treatment so a running window/taskbar picks up a Settings change
   without a restart.
2. **Scope it to fonts only first, sizes later.** Get text genuinely
   resizable without also solving every box-size call site in one
   pass — accept that buttons/chrome stay fixed-size initially (text
   may clip/overflow at extreme scales) and treat "surrounding sizes"
   as a real, separate, second pass once font-only scaling is
   live-verified. Smaller first step, but risks shipping something
   that looks broken at non-default scale until pass 2 lands.
3. **New UI control shape**: reuse the existing opacity-style
   `+`/`-` button pair (fits today's xhtpm vocabulary with zero new
   parser work) vs. inventing a real slider/range element (more
   general, useful beyond font size later, but new parser + draw work
   plus a testable input handling story).
4. **Where the live value is read from**: continue using
   `#.desktop/hq_ui.pdl` (matches `click_two_step`, `focus_grab`,
   `emoji_sprite_view` — one shared file for all house-wide UI knobs)
   vs. a dedicated `font_scale.pdl` — no strong reason found yet to
   split it out; default to keeping it in `hq_ui.pdl` unless a reason
   turns up.

## Open questions worth settling before this becomes a real plan

- Does font scale apply house-wide (every window, like `click_two_step`)
  or per-window (a user might want a bigger dock but normal-size HQ
  windows)? `click_two_step`'s own house-wide precedent argues for
  house-wide as the default, simpler choice.
- Minimum/maximum bounds — the PDL comment already says "range
  0.5-3.0"; is that still the right range once box-size scaling is
  real (a 3x window chrome height might be absurd at small windows)?
- Does this need its own live PNG-dump verification pass across every
  window MODE (dock, tile, popup, swatch-grid) before calling it done,
  given `scaled()` is called from many of them? Given this session's
  own repeated lesson (verify live, not just "it builds"), yes —
  flagging it now so the eventual plan doc budgets time for it.

## Next step

When this is ready to become real work: pick an option above, write
the real plan (either promoted into this same file or split into
`08-roadmap/design-docs/` if it turns out large), and log the decision
in `12.calendar/<date>/` when it's actually scheduled.
