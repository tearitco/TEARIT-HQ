# 🪐 chtpm vs khtpm — do they need to be separate? 🧐

Examination requested 2026-08-07 ("i dont wanna repeat myself if we could
have just bulked up chtpm_parser code"). Read `KHTPM-ARCH.txt` next to this
file first.

**TL;DR:** chtpm and khtpm are two *different layers* (in-app screen engine
vs desktop shell) — merging them into ONE binary is the wrong fix. **But the
"repeat myself" feeling is 100% real**, and it is NOT chtpm-vs-khtpm
overlap. It is *fork drift* (28 copies of `chtpm_parser_pal.c`, 15 different
versions) plus *two parallel GL-window stacks* doing the same job. The fix is
de-forking + one shared tile/glyph kernel — not merging the two systems.

---

## 1. What each one is 📚

### chtpm — the IN-APP screen engine 🖥️
Runs *inside one app* and renders that app's screens, HTML-like layout by
layout. Real 1.TPMOS upstream code, tracked as a fork.

| part | file | job |
| ---- | ---- | --- |
| layout parser | `system/chtpm_parser_pal.c` (~3.5k) | parses `.chtpm` files (`<panel>`, `<text>`, `<button href=…>`, `<cli_io>`), renders the ASCII frame → `pieces/display/current_frame.txt`, dispatches clicks → `pending_command.txt`, reads keys from `keyboard/history.txt` |
| rasterizer | `system/chtpm_rgb_render.c` (~1.1k) | rasterizes that ASCII frame char-by-char (`load_glyphs`/`blit_char`/`blit_text`) → `rgb_frame.raw` |
| window | `system/gl_mirror.c` | GLUT window that just blits `rgb_frame.raw` as one textured quad ("a mirror, not a desktop") |

One chtpm process per app, one screen at a time, switching layouts via
`<button href=…>`.

### khtpm — the DESKTOP SHELL 🖼️
Represents *many* entities as small windows on the desktop. House-native
(nothing in 00.10; new in 00.11).

| part | file | job |
| ---- | ---- | --- |
| entity window | `ops/tp_desktop_window.c` (~2.7k) — the real, live, only Linux entity implementation; the planned `khtpm_main.c`/`khtpm_core.c`/`khtpm_plat_x11.c` split never actually replaced it (confirmed dead 2026-08-11, archived) | one borderless GL window per entity; glyph title; drag → `desktop_pos.txt` |
| context menu | `tp_desktop_window.c` `load_methods()` | data-driven menu from `meta.pdl` METHOD rows + `objects.pdl` pages; `STATE` guard rows (menu_stay_open/grab_pointer/grab_keyboard/grab_pointer_while_stay_open) |
| taskbar | `*.monads/*.livedesk-taskbar/ops/khtpm_strip_parser.c` + `khtpm_taskbar_manager.c` — real declarative-layout parser (2 processes). Legacy `tp_taskbar.c` fully retired 2026-08-11, archived. | persistent header + submenu popups + bottom tab bar; unified nav cursor; save/load/session/desk switching; agent relay |
| registry | `tp_desktop_window.c` (`ensure_livedesk_index()`, `livedesk_registry_add/remove()`) | `#.desktop/livedesk_open.txt` + `livedesk-nav-claims/livedesk_nav_claims.txt` |
| popups | `khtpm_show_text.+x`, `khtpm_show_choices.+x` | in-window text/choice popups |

---

## 2. The hard evidence — where the repetition ACTUALLY is 🔍

Checked 2026-08-07 in `44.xyz❤️‍🔥️00.11`:

| artifact | copies in house | distinct versions (md5) | verdict |
| -------- | --------------- | ----------------------- | ------- |
| `chtpm_parser_pal.c` | 28 | 15 | heavily drifted forks |
| `chtpm_rgb_render.c` | 10 | 4 (6 are identical clean copies) | mild drift |
| `gl_mirror.c` | several | mixed | per-project copies |

- The comments in these files say they belong in a **shared-ops** location
  (`yz.muchiverse/2.muchi-verse/shared-ops/`, see `chtpm_parser_pal.c` line 33).
  **That directory does not exist** — only `2.muchi-verse/PAL-NET-STANDARD.txt`
  is there. So the shared convention was *designed* but the files were
  *vendored per project* instead, and then drifted apart.
- khtpm is NOT duplicated across projects (it's one shared `&.widgits/tile-picker/`
  widget) — but *inside* it there is legacy-vs-refactor duplication
  (`tp_desktop_window.c` legacy megafile vs `khtpm_*.c` core/plat split, same
  for the taskbar). That's the KHTPM-ARCH.txt plan to collapse, not a reason
  to merge families.
- `asa`/`ava` live `meta.pdl` files still hardcode **00.10** absolute paths —
  concrete example of what happens when pieces are vendored, not shared
  (the 00.11 tree's own `chat.sh` exists but the menu row still launches 00.10's).

---

## 3. Where they genuinely overlap (the shared kernel 🧬)

These are the only parts that are truly the same job:

1. **Glyph rasterization** — `load_glyphs()`/`blit_char()`/`blit_text()` in
   `chtpm_rgb_render.c` vs sprite/text rendering in khtpm's plat layer. Same
   problem, two implementations (both already ported from the same upstream
   `wraith_rgb_daemon.c`).
2. **Relay-file input polling** — `interact_relay.txt` (khtpm) vs
   `keyboard/history.txt` + `current_frame.txt` (chtpm). Same pattern: poll a
   text file, dispatch, truncate.
3. **"A persistent GL window showing tile/emoji content on the desktop"** —
   `gl_mirror.c` and `tp_desktop_window.c`/`khtpm_plat_*` both do this.
   Two windowing stacks for the same desktop.

Everything else (menus, registry, taskbar, layouts, .chtpm parsing) is
family-specific.

---

## 4. Why they must stay separate 🚧

| axis | chtpm | khtpm |
| ---- | ----- | ----- |
| **lifecycle** | one app, one screen, layout-switching | N entity windows simultaneously + one taskbar + shared registry |
| **content source** | parsed `.chtpm` layout → ASCII frame | data-driven `meta.pdl`/`objects.pdl` menus + sprite |
| **upstream coupling** | *tracked fork* of real 1.TPMOS (`chtpm_parser.c`) — kept close so "steal more pieces from upstream as needed" stays possible | house-native, WIN-COMPAT core/plat split, no upstream |
| **input model** | terminal REPL + keyboard history + button hrefs | right-click menu, drag, relay-injectable (`ACTIVATE_NAV:<N>`) |

Bulking up `chtpm_parser` to *also* be the desktop shell would:
- destroy its upstream-tracking property (every upstream steal becomes a merge conflict);
- bolt a registry/taskbar/menu-guard model onto a per-app layout parser that has no concept of "windows" or "desktop";
- force every app project to ship the desktop shell's weight in their vendored copy.

And bulking up khtpm to *also* parse `.chtpm` layouts would drag a
desktop-wide service into single-app screen rendering. Neither direction is
KISS.

---

## 5. Recommendation ✅ (KISS, matches the instinct)

**Two roles is correct. Two *codebases* with copies is the bug.** Do this:

1. **De-fork chtpm.** Land `chtpm_parser_pal.c`, `chtpm_rgb_render.c`,
   `gl_mirror.c` once in a real shared-ops location
   (`2.muchi-verse/shared-ops/` — make it exist) or a `&.widgits/` shared
   tile-render widget, and symlink per project. 28 forks → 1, drift gone.
2. **Extract the shared kernel** (glyph raster + relay polling + tile blit)
   into ONE shared lib/SOURCES that **both** `chtpm_rgb_render.c` **and**
   `khtpm_plat_x11.c` `#include`. This is the literal "bulk up the shared
   tile code" — the right target instead of `chtpm_parser`.
3. **Keep khtpm's desktop logic in `khtpm_core`** (that's already the
   KHTPM-ARCH.txt plan) and retire the legacy `tp_desktop_window.c` once
   `khtpm_plat_x11.c` lands. One windowing stack in khtpm.
4. **Result:** 2 roles, 1 shared kernel, 0 per-project forks, and the next
   hard-vvar bot reuses the same kernel instead of copying yet another one.

### One-line answers
- Could khtpm absorb chtpm? → ❌ different layers (desktop shell vs app screen).
- Could `chtpm_parser` bulk up to do khtpm? → ❌ wrong merge target (upstream fork, no desktop concepts).
- Should they SHARE a glyph/tile kernel? → ✅ **yes, that's the whole point.**
- Should they share input/relay polling? → ✅ yes.
- Should they share one window binary? → ❌ no.

---

## 6. Files to look at
- `&.widgits/tile-picker/ops/KHTPM-ARCH.txt` — the khtpm core/plat plan.
- `&.widgits/tile-picker/ops/khtpm_core.h` — the shared desktop-logic API.
- `101.mutaclsym🧟‍♂️️+18.01/system/chtpm_parser_pal.c` — the fork header (explains the tracked-fork stance, lines 30-90).
- `014.wsr-pal💸️📌️+2/system/chtpm_rgb_render.c` — the rasterizer header (explains the GL-mirror contract, lines 1-60).
- `014.wsr-pal💸️📌️+2/system/gl_mirror.c` — the "mirror, not a desktop" GL window.
- `041.pal-chain⛓️/pieces/chtpm/layouts/login.chtpm` — a `.chtpm` layout sample.
- `2.muchi-verse/PAL-NET-STANDARD.txt` — the (currently) only thing in the intended shared-ops home.
