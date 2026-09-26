# HOWTO: add a new taskbar dropdown row, or a new palette category

Practical companion to `TASKBAR-MENUS-DATA-DRIVEN.md` (the broader
architecture doc — read that for the "why," this doc is the "how,"
scoped to what's *already real* today, 2026-09-22). Its own status
header ("DESIGN — not started") is now stale for several cells —
`palettes`, `pals`, and `toys` are already `.pdl`-driven, verified
directly tonight while fixing a real bug caused by a missing row.

**The short version**: most taskbar dropdown cells are NOT hardcoded
C. Adding a row is usually a text edit to one file
(`#.desktop/livedesk_taskbar.pdl`), no rebuild, no restart — the file
is read fresh every time a dropdown opens.

---

## 1. Adding a row to an already-pdl-driven cell (the common case)

Cells that read rows straight from `livedesk_taskbar.pdl` via
`livedesk_pdl_menu_rows()`/`livedesk_build_palettes_menu()` (check
`khtpm_taskbar_manager.c` for the specific cell's build function to
confirm — most are, some directory-scanning ones like `pals` bracket a
real scan with optional pre/post rows instead, see §3):

1. Open `#.desktop/livedesk_taskbar.pdl`.
2. Find the cell's existing rows — they're named
   `<cell>_menu_<N>_label` / `<cell>_menu_<N>_cmd`, 1-indexed,
   contiguous (e.g. `palettes_menu_1_label` .. `palettes_menu_11_label`).
3. Insert your new row **before** any trailing "cancel"/terminator row
   (bump that row's own `<N>` by one) — the builder stops reading at
   the first missing `_label`, so a gap breaks everything after it.
4. Pick a real `_cmd` value. Two shapes exist:
   - `livedesk:<verb>` — dispatched inline in
     `ktb_hq_activate()`/similar (grep the exact string to find the
     handler before assuming behavior).
   - `livedesk:open-palette:<cat>` — routes through
     `&.widgits/palettes/button-pal.sh <cat> <house_root>`, which maps
     `<cat>` to either a real ported `palettes-<cat>.xhtpm` template or
     the generic stub. Confirm `<cat>` is a real, valid value in that
     script's own `case` statement — an unlisted `<cat>` silently
     falls through to the stub, not an error.
5. Done. No rebuild. Verify live (see §4) rather than assuming it
   worked — a missing row is a silent gap, not a crash.

**Real example, exactly as done tonight** (`livedesk_taskbar.pdl`):
```
SECTION      | palettes_menu_10_cmd   | livedesk:open-palette:generate
SECTION      | palettes_menu_11_label | my palettes (terumon)
SECTION      | palettes_menu_11_cmd   | livedesk:open-palette:my-palettes
SECTION      | palettes_menu_12_label | cancel
```
Root cause this fixed: the `my-palettes` category (real, working,
backend fully functional) had **no row pointing to it anywhere** —
the only way to reach it was running `button-pal.sh my-palettes`
directly, bypassing the UI entirely. A palette/category existing on
disk is not the same as it being reachable — always check for the
actual dropdown row.

---

## 2. Adding a new palette category

Two real, different cases:

**A) A new "My Palettes" library (import-only, arbitrary images)** —
the case used for terumon:
1. Drop your images into `&.widgits/palettes/my-library/<name>/`
   (one subdirectory = one library entry; `guess_cell_px()` in
   `palettes_manager.c` auto-detects tile size, falling back to
   whole-image-as-one-swatch for non-dividing sizes — a single
   character PNG like terumon's needs no special handling).
2. Make sure `my-palettes` itself has a dropdown row (§1) — this is
   the step that's easy to forget, since the library import path and
   the dropdown-row-existing are two completely independent things.
3. No rebuild needed — `mypal_maybe_import()` scans `my-library/` live.

**B) A new fully-ported category** (its own `palettes-<cat>.xhtpm` +
projector, like `emojis`/`elements`/`rmmv`) — bigger, real C/template
work, not a data-only change. Read `PROGRESS-palettes-xhtpm.md` and an
existing ported category's `.xhtpm` as your template before starting;
out of scope for this doc.

---

## 3. Directory-scanning cells (pals, toys, user) — pre/post rows only

Some cells can't be pure static rows (a row per real pal/toy/account
would mean hand-editing the pdl every time one's created). These use
`livedesk_pdl_menu_rows_staged()`: the pdl supplies optional rows
**around** a real live scan —
`<cell>_menu_pre_<N>_label`/`_cmd` and
`<cell>_menu_post_<N>_label`/`_cmd`. The scan itself (e.g. every real
pal directory) is never something you add to the pdl — you can only
add fixed rows before or after it. **Real, confirmed bug this exact
mechanism had tonight**: the pal-directory scan filled the entire
array before a post-row (`Cancel`) could ever be appended, because no
room was reserved for it — check `KTB_LIVEDESK_DYN_MAX` (the array
cap) against the real, current scanned-item count before assuming a
post row will actually show up; a full array silently drops trailing
rows, it doesn't error.

---

## 4. Verify live — don't assume a pdl edit "worked"

`.pdl` edits take effect on the *next dropdown open*, no rebuild, no
restart. But "no error" is not evidence it worked. Real, minimal
verification loop (same one used to confirm tonight's fix):

```sh
cd <house_root>
HOUSE="<house_root>" bash "#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh" esc
HOUSE="<house_root>" bash "#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh" nav <cell_number>
grep -o 'label="[^"]*"' "#.desktop/strip_var_hqitems.txt"
```
The `grep` output is the real, live row list for whatever's currently
open — compare it directly against what you expect, not against what
you typed into the pdl. Full relay/testing conventions:
`#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt` — read it before
reaching for `xdotool`/pixel dumps, which are real last resorts, not
the first move.

---

## 5. Real, house-wide rule this whole pattern rests on

**A feature existing on disk (a palette library, a scanned pal, an
entity) is a separate fact from a user being able to reach it through
the taskbar UI.** Every dropdown cell is its own, independent, pdl-row
(or scan+pdl-bracket) list — nothing auto-discovers a new category or
auto-adds a row just because the underlying content exists. When
something "isn't showing up," check the dropdown's own row list
first, before assuming the underlying feature is broken.
