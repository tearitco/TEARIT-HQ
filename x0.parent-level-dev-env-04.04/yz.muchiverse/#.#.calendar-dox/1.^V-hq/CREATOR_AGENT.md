# 🛠️ CREATOR_AGENT.md — building menus, pickers & sub-app windows in this house

**Written 2026-08-24**, after a real session wasted multiple rounds confusing the two
menu systems while building palettes (direct instruction to write this: "u seemed to
struggle getting going differentiating tb from hq style menus"). If you are about to
add a dropdown, picker, palette, or any tb-launched window — read THIS first, it is
the map that was missing.

---

## 0. The ONE question that decides everything

> **Is your thing a DROPDOWN under a toolbar cell, or its own WINDOW?**

| | **A. tb-native dropdown** | **B. hq-style window** |
|---|---|---|
| What | the shared popup under a header cell (hq/user/file/desks/pals/player/db/toys/h-ai/date) | a standalone process rendering a `.chtpm` file |
| Renderer | `khtpm_strip_parser.c` draws it from `strip_var_hqitems.txt` | `khtpm_hq_render.+x <house> <file.chtpm>` (+ entity-menu/chat-hai variants) |
| Rows/content source | manager builder publishes buttons to `#.desktop/strip_var_hqitems.txt` | the `.chtpm` IS the content (plus CSS beside it) |
| Use when | choosing an ACTION or CATEGORY (few rows of commands) | showing CONTENT/tiles/grids (a picker surface, scrollable matrix) |
| Lives in | `*.monads/*.livedesk-taskbar/ops/*.c` + `#.desktop/livedesk_taskbar.pdl` + `khtpm_strip_header.chtpm` | `&.widgits/<app>/` (composer script + `.chtpm` + `.css`) or `&.hq-apps/<app>/` |

They compose: a category dropdown row (A) launches a category window (B). Palettes
does exactly this. Do NOT try to render content INSIDE a dropdown and do NOT nest a
second menu inside a window.

---

## 1. System A — tb-native dropdown (the shared popup)

There is exactly ONE popup mechanism. Every header cell shares it:

```
click cell N ──► code 4000+N (KSC_HQ_HEADER_BASE+N)
             ──► manager ktb_hq_open(state,N) runs that cell's BUILDER
             ──► builder fills HQMenuItem[] rows
             ──► published to #.desktop/strip_var_hqitems.txt
                 as <button label="..." onClick="HQITEM:i"/>
             ──► strip parser draws the popup ANCHORED UNDER THE CELL,
                 themed like every other cell's popup
row click i ──► code 5000+i ──► ktb_hq_activate(state,i) ──► dispatch command
```

### Wiring recipe (new cell menu)

1. **Cell id**: add `N|<name>` to `#.desktop/livedesk_header_cell_ids.txt`.
2. **Header template**: `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` —
   the cell button MUST be:
   ```xml
   <button label="palettes" onClick="ACTIVATE:6">
     <row>${strip_hq_items}</row>
   </button>
   ```
   ⚠️ **SELF-CLOSING (`/>`) = BROKEN POPUP.** Without the nested
   `<row>${strip_hq_items}</row>` child the parser has no descendant rows →
   no anchored/themed popup (detached-looking / wrong colors / nothing).
   This cost a real debug round 2026-08-24 ("isn't attached to tb nor same color").
3. **Rows**: PDL-driven pattern — `<cellname>_menu_N_label` / `_cmd` rows in
   `#.desktop/livedesk_taskbar.pdl`, read LIVE at every cell-open (no restart
   needed). Copy `livedesk_build_palettes_menu()` / `livedesk_build_hq_menu()`
   in `khtpm_taskbar_manager.c`. Wire the builder into `ktb_hq_open()`'s switch.
4. **Cancel row**: ALWAYS end the row list with a label-only row (`cancel`, no
   cmd) — activation no-ops/dismisses. Direct instruction 2026-08-24.
5. **Commands**: activation goes through `ktb_hq_activate()`'s dispatch:
   - Reserved forms get C branches that build QUOTED absolute paths from
     `house_root`: `livedesk:open-palette:<cat>`, `livedesk:open-chat-hai`,
     `livedesk:spawn-cursword`, `quit`, `user:*`, … **copy an existing branch**.
   - ⚠️ Raw shell commands containing `&` (e.g. `&.widgits/...` paths) FAIL in
     the generic `sh -c` fallback — `&` backgrounds the command and sh then
     tries to run `.widgits/...` which doesn't exist. Proven via touch-test
     2026-08-24. If your path has `&`, you MUST add a reserved-form branch.
6. Row cap is `KTB_LIVEDESK_DYN_MAX` (24) — builders loop `i=1..max`, skip
   missing labels (rows compact up).

### Hardcoded vs PDL-driven cells

Only `hq` and `palettes` are genuinely PDL-driven today. user/file/desks/
player/db/pals/toys/clock/h-ai builders are C-hardcoded — their PDL-looking
rows are DEAD. Editing those rows wastes a full cycle. See
TASKBAR-MENU-ARCHITECTURE.md's standing-debt section (updated 2026-08-24:
ALL cells are supposed to convert to the hq pattern eventually).

---

## 2. System B — hq-style window (kptm_hq_render family)

A `.chtpm` layout + `.css` sheet rendered by `khtpm_hq_render.+x` into a real
X11 window (same family: db-hq dashboard, events-hq, chat-hai, entity-menu,
palettes windows). Reference shape: `&.hq-apps/db-hq/dashboard.chtpm`.

### Wiring recipe

1. **Composer script** in `&.widgits/<app>/<app>_menu.sh` (see
   `&.widgits/palettes/palettes_menu.sh`) writes the `.chtpm` per category/key,
   then `exec`s the renderer binary: `khtpm_hq_render.+x <house_root> <file.chtpm>`.
   Kill-any-existing-instance guard by cmdline match before relaunch.
2. **CSS FILE NAME IS DERIVED, NOT CONFIGURED**: renderer takes argv[2]'s path
   and swaps the extension → `palettes-emojis.chtpm` loads
   `palettes-emojis.css`. ⚠️ Writing only `palettes.css` silently produces an
   UNSTYLED window (no error!). Publish `cp palettes.css palettes-$key.css`
   per composed window. Cost a debug round 2026-08-24.
3. **Layout engine contracts** (shared core `khtpm_css_parser.c` +
   `khtpm_render_core.c`):
   - Only FLEX containers lay out their children; block containers leave
     children untouched (caller manages). Rows of tiles need
     `display:flex; flex-direction:row` in css.
   - `apply_css()` is called PER ELEMENT explicitly. db-hq's panel loop only
     styled DIRECT children — nested elements (tiles inside rows) got zero
     style and drew invisible/zero-width. Fixed 2026-08-24 with
     `apply_css_deep()`; if you build a new nested layout, make sure styling
     reaches the leaves.
   - The panel loop force-defaults child height to 22px ONLY when css didn't
     set one (fixed same day — set height in css and it wins).
4. **Scrollable matrix** (palettes pattern): chunk items into
   `<row class="…">` blocks at COMPOSE time; renderer-side post-pass shifts
   rows by a scroll offset, hides out-of-panel rows (w=h=0), draws track+thumb.
   Nav indices are assigned only to VISIBLE (non-zero-size) elements.

### Image tiles (emoji, icons) — NEVER font glyphs

The house default Xft font has NO color-emoji coverage; text emojis draw as
boxes/nothing. The proven mechanism (toolbar clock-face, bookstack):

```
emoji_gen_atlas.+x "<glyph>"  "<dir>/atlas.png"
emoji_xtract.+x  "<dir>/atlas.png" 0 64 "<dir>/sprite.csv"
```

(prebuilt in `*.monads/*.livedesk-taskbar/ops/+x/`). `sprite.csv` =
`# resolution=64` header + 64×64 `r,g,b,a` lines. Elements carry
`sprite="<dir>"` (directory containing `sprite.csv`); `Elem.sprite` field +
`draw_elem()` blit landed 2026-08-24 in the shared core (ported from
`khtpm_strip_parser.c`'s `tab_sprite()`/`blit_tab_sprite()`). Missing csv =
text-label fallback, never blank/crash. Cache dir convention:
`&.widgits/<app>/sprites/<kind>/<NNN>/sprite.csv`, generated lazily by the
composer (skip if exists).

---

## 2.5 Nav-index assignment protocol (System B / `khtpm_hq_render` family)

**The standard, direct instruction (2026-08-12): content gets nav 1..N first;
chrome/window controls (the close button) ALWAYS get the LAST nav index, never
nav 1.** This is deliberate, not an oversight — defaulting focus onto an
unlabeled close button left no visible `[>N]` badge anywhere on screen at
launch, and numbering close as 1 collides with the natural expectation that
digit `1` should jump to the first real content row.

**Real order, `assign_nav_indices()` (`khtpm_hq_render.c`):**
1. Any window-specific structural nav (tabbar tabs, sidebar items, panel
   buttons — only for modes that have them, e.g. db-hq's Common Events).
2. `assign_generic_onclick_nav(window)` — the generic pass: any element
   anywhere in the tree carrying its own real `onClick=` becomes numbered,
   in tree-walk order. This is what palettes' emoji tiles ride — no
   palette-specific nav code exists, it's 100% this generic mechanism.
3. `g_close_elem->nav_index = ++g_n_nav` — **always last**, after every real
   content element above.

**REAL BUG, FOUND + FIXED 2026-08-25 (correcting this section's own earlier,
too-hasty claim below) — read this before trusting a single `--dump-and-exit`
result for anything nav-related.** `assign_nav_indices()`'s own header comment,
and `assign_generic_onclick_nav()`'s own separate comment, BOTH claimed
`clear_nav_indices()` "zeroes the whole tree at the top of every
`assign_nav_indices()` pass" — but the actual call was never wired in. Real,
live consequence: **frame 1 numbers correctly** (every `Elem` starts at
`nav_index=0` from `elem_new()`'s memset, so `assign_generic_onclick_nav()`'s
`nav_index == 0` guard passes for everyone) — **frame 2 onward, every tile
already has a non-zero nav_index from frame 1, so that same guard silently
skips ALL of them.** `g_n_nav` never grows past ~0 again, so
`g_close_elem->nav_index = ++g_n_nav` collapses to `nav=1` (colliding with
tile 1's now-frozen, stale label) and no digit above 1 is ever valid again.

This is exactly why a quick `--dump-and-exit` sanity check (which only ever
renders frame 1) kept showing the CORRECT `nav=113`/`nav=114` split and made
the bug look fixed/nonexistent — **the bug only manifests from frame 2
onward, which a one-shot dump-and-exit process can never reach.** Confirmed
by testing the SAME live, multi-frame, relay-driven window both before and
after the fix (see `assign_nav_indices()`'s own real fix comment in the
source for the exact repro). Real fix: `clear_nav_indices(window);` is now
the literal first line of `assign_nav_indices()`, not just implied by a
comment.

**Standing rule from this**: never trust a single `--dump-and-exit` frame to
validate ANY stateful/multi-frame behavior (nav persistence, live reload,
scroll state) — it only ever proves frame-1 correctness. Drive at least 2-3
real frames via relay (a digit/key press, then a second dump) before treating
nav/focus behavior as verified.

`g_close_elem` is a separate global (`&g_close_elem_storage`), NOT a child of
the window's own Elem tree — it's drawn via its own direct
`draw_elem(g_close_elem, 0)` call in `draw_chrome_bar()`, not the normal
`render_tree()` walk. This means it's invisible to any tooling that only
walks `g_window`'s children (the receipt-port's own `emit_hq_object()` had
exactly this gap until 2026-08-25 — fixed by also emitting `g_close_elem`
explicitly after the tree walk). Any new debug/audit tooling for this family
needs the same explicit extra step, or it will silently miss the close
button every time.

---

## 2.6 Pitfall: don't reach for the parser when nav state "isn't showing"

2026-08-25, bookmarks migration (khtpm_entity_menu_render.c): live report
was "it never puts a default '>' in bookmarks in 4 or any" plus arrows not
visibly moving anything. The instinct was to suspect the low-level stuff —
XftFont caching, draw order, the parser — and in fact a real dangling-font
bug WAS found and fixed there first. But that fix didn't resolve this
report. The actual bug was two `nav_index` assignment passes running back
to back on the *same* tree in `dbhq_assign_nav_indices()`: an old
`g_dbhq_current_tab == DB_HQ_COMMON_EVENTS_TAB` panel-button loop (which
runs for EVERY db-hq window by default, since that tab enum is the
default state even for a tabbar-less window) numbered bookmarks' buttons
1-3, then a newer generic onclick-nav pass ran unconditionally right after
and renumbered the *same* buttons 4-6 — so `g_focus_nav` (defaulting to /
jumping to 1-3) never matched any live element's `nav_index` (which sat at
4-6). Nothing about the parser, font cache, or draw order was wrong; two
higher-level dispatch passes were fighting over one tree.

**The lesson** (direct instruction, worth repeating for any future agent
here): when nav/focus state "isn't showing" or "isn't moving," don't
default to auditing the parser or the low-level draw primitives first.
Print BOTH the state (`g_focus_nav`, keypress handler) and what's actually
on the live Elems (`nav_index` at draw time) side by side — a debug
`fprintf` in the two spots, one build/relaunch cycle — before touching
anything structural. In this case the state (`g_focus_nav`) was updating
perfectly on every real keypress the whole time; only the rendered
`nav_index` values were wrong, and only because of an assignment-order
bug two call sites away from either symptom.

---

## 2.7 Standing rule: no UI element without mirror keyboard accessibility

Direct instruction, 2026-08-25: "there should be no ui element without mirror kbd
accesibility." Every real interaction a mouse can do in this house's khtpm-family
windows must have a keyboard-only equivalent that actually reaches it — not just
"technically possible via some key," but genuinely reachable without ever needing the
mouse. This is a real, live-caught gap class, not a hypothetical:

Palettes' own grid scroll (`g_pal_scroll`, ported 2026-08-25 from the now-deleted
`khtpm_hq_render.c`) initially only exposed scrolling via mouse wheel and `Page_Up`/
`Page_Down` — a keyboard-only user pressing plain arrow keys at the top/bottom edge of
the visible rows just hit a dead stop, because off-screen rows are deliberately excluded
from nav numbering (see §2.6's own w>0/h>0 check). Page_Up/Down being keyboard-technically-
possible was NOT good enough — it's a separate, less discoverable key than the arrow keys
someone's already using to navigate, so the real fix was making the SAME arrow keys
auto-scroll one row into view at the edge, landing on the same column, exactly like any
accessible listbox/grid widget would.

**How to apply:** whenever a window gains ANY mouse-only affordance (scroll, drag, a
button with no natural nav_index, a hover-only reveal, etc.), ask directly "what does a
user with only a keyboard do here" before considering the feature done - don't wait for
a live report to find the gap, per the direct instruction this rule itself is written
from. Known open instance as of this writing: bookmarks has no scroll at all yet (fixed-
size window, no scroll for 10+ rows) - when it gets one, it should get this same
edge-triggered arrow-key auto-scroll from the start, not as a follow-up fix.

---

## 3. Verifying without eyes (agent workflow)

- **Headless frame dump**: `khtpm_hq_render.+x <house> <file.chtpm>
  --dump-and-exit` writes `$house/#.desktop/db-hq-frame.png` (or `/tmp/…`),
  then exits. Analyze pixels programmatically (python zlib PNG decode;
  cluster counts: gold tile-bg px, colored sprite px). Never trust a bare
  "process is alive".
- **Dropdown driving**: `HOUSE=<house> bash
  $house/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh {nav|row|key|esc|
  frame|mgrcode} …`. Codes: `4000+N` open cell N's popup, `5000+i` click row
  (HQITEM:i). ⚠️ The parser layer's Enter relay is unreliable — drive
  `mgrcode` directly.
- Dropdown rows publish live: check `#.desktop/strip_var_hqitems.txt` content
  after opening instead of restarting anything.

## 4. Where things live

| Thing | Path |
|---|---|
| header cells template | `*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` |
| cell ids | `44.xyz.01.00/#.desktop/livedesk_header_cell_ids.txt` |
| dropdown rows (PDL) | `44.xyz.01.00/#.desktop/livedesk_taskbar.pdl` |
| manager (builders/dispatch) | `*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c` |
| strip renderer (popup drawing) | `…ops/khtpm_strip_parser.c` (`draw_popup_win`, `tab_sprite`) |
| hq window renderer | `…ops/khtpm_hq_render.c` (+ `_shared-lib/khtpm_render_core.c`, `khtpm_css_parser.c`) |
| shared Elem/core CANONICAL SOURCE | `&.widgits/_shared-lib/` — builds COPY from here; editing the local copy is lost on rebuild |
| emoji→sprite tools | `…/*.monads/*.livedesk-taskbar/ops/+x/emoji_{gen_atlas,xtract}.+x` |
| palettes reference impl | `&.widgits/palettes/` (composer, css, sprites, audit log) |
| design docs | `au11-hq/TASKBAR-MENU-ARCHITECTURE.md`, `#.ref/menu/palletes/pallette-design.txt` |

*Rebuilds: taskbar pair `build_khtpm_strip.sh`; hq renderer `build_db_hq.sh`
(both in `…ops/`). Restart tb: `run_khtpm_strip.sh new`. `.pdl`/`.chtpm`/`.css`
are data — no rebuild needed.*
