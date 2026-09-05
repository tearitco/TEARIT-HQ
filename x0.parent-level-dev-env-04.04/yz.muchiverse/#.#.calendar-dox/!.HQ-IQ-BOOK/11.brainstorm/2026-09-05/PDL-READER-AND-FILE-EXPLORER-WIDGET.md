# Brainstorm: pdl-reader, a shared File Explorer widget, the agy-txt legacy-launcher bug, and a toys text-editor refactor

**Status: brainstorm only, not scheduled, no code written.**
Started 2026-09-05, direct request covering four related things in one
go — kept together here because they share one dependency (the File
Explorer widget) and one reference implementation (tpmos's own
`agy-text-editor`/file-browser family).

## 1. The actual feature: `pdl-reader`

A document-reader program: readable docs get registered in a `.pdl`
(one entry per doc — path, title, maybe a page count/format hint), and
`pdl-reader` launches from that list and lets the user read a doc
PDF-style: zoom in/out, navigate by page.

**Dependency, not yet built**: a real, shared **File Explorer widget**.
`pdl-reader` wants to use it (presumably for picking WHICH pdl / doc to
open, or browsing to add one), and — separately but for the same
reason — it should be the SAME widget backing every khtpm window's own
Save As / Load flows, which currently don't have one at all (see §3).
Building the widget once as a real, generic khtpm component (own
manager + `.xhtpm` template + CSS, matching the house's own "real
manager, static template, no hardcoded per-app forms" convention) and
having both `pdl-reader` and every other app's save/load menu consume
it is the whole point of doing this as a widget instead of a one-off
inside `pdl-reader` itself.

### Open questions for the widget itself

- Scope for v1: read-only browse+pick (enough for pdl-reader and a
  Load flow) vs. also a Save-As text-entry path (typed filename +
  directory nav, like tpmos's own file browser — see §2). Both are
  real, needed uses; decide whether v1 does both or Load-only first.
- Where does it live? `&.widgits/` alongside the house's other shared
  widgets (taskbar-settings, palettes, bookmarks) seems right — it's
  used by more than one consumer by design.
- Does it need its own manager process (real directory listing/search
  logic, matching the "real manager, not inline C" rule), with the
  renderer just drawing whatever rows the manager publishes? Given
  every other khtpm widget this session touched follows exactly that
  shape, yes — no reason to special-case this one.
- `pdl-reader`'s own page/zoom navigation is a SEPARATE concern from
  the file explorer widget — the widget only picks a file; the actual
  page-by-page PDF-style reading view is `pdl-reader`'s own real
  content, probably its own `<canvas>`-based render (a page becomes an
  image, panned/zoomed) or a paginated text/markup view depending on
  what kinds of docs `.pdl` ends up pointing at (does it need to
  render arbitrary PDFs, or just the house's own `.md`/`.txt` docs
  rendered as pages? — not yet answered, changes the scope a lot).

## 2. Reference material found while researching this

Two real, existing precedents worth reading before designing the
khtpm-side widget from scratch — both are on the OLDER TPMOS side of
the house (`1.TPMOS_c_+rmmp.0103.0001/`), not khtpm, so they're prior
art / UX reference, not code to port directly:

- **`&.widgits/file-menu/`** (under `44.xyz.01.00/`) — an existing,
  fairly developed TPMOS-family file-menu widget project: its own
  `button.sh`, `ops/fm_menu_input.c`, `ops/fm_compose_frame.c`, and
  several real planning docs already written (`fm-flow-plan.txt`,
  `fm-widget-fix.md`, `widget+plan.txt`, `USER_REPORT.txt`). Genuinely
  worth reading in full before starting the khtpm widget — it may
  already have solved real problems (search, suggestions, directory
  nav) the new widget will hit again.
- **`102.agy-txt/manager/agy_browser_manager.c`** — the real manager
  behind the FILE BROWSER screen captured live below (§2.1). Same
  family, another real implementation of the same idea to compare
  against.

### 2.1 Live-captured reference UX (tpmos `agy-text-editor`, via `chtpm_parser_pal`)

Captured directly from a live run (`1.TPMOS_c_+rmmp.0103.0001/projects/agy-text-editor`,
2026-09-05 00:33) — the real, current behavior of the loader → editor →
file menu → Save As flow this session should treat as the UX spec for
whatever the khtpm-side equivalent becomes (a text editor under
`toys`, and the file explorer widget's own Save As screen shape):

**1. Project loader** — a plain numbered list of projects (`[ ] N.
[name]`), `[>]` marks the currently-navigated row, digit or arrow+Enter
to pick. `agy-text-editor` was entry 3 of 40 real projects in this
list.

**2. Editor's own frame** — a title bar (`A G Y   E D I T O R`), an
active-file path line, a nav row (`[>] 1. [EDIT TEXT (INTERACT)]`), the
actual text content area below a separator, then a second nav block
(`[ ] 2. [NEW FILE]`, `3. [CLEAR FILE]`, `4. [FILE MENU]`,
`5. [EXIT TO OS]`).

**3. FILE MENU** — a real sub-screen, not an inline dropdown: shows the
currently ACTIVE file path, then `[ ] 1. [NEW FILE]` / `2. [SAVE
FILE]` / `3. [SAVE AS...]` / `4. [LOAD FILE...]` / `5. [BACK TO
EDITOR]`, plus a status line ("Ready.") in its own bottom strip.

**4. FILE BROWSER (Save As mode)** — the real file-explorer screen:
  - `MODE: SAVE FILE AS` and `DIR: <current dir>` header lines.
  - A `SEARCH:` field (row 1, initially empty, focusable/typeable) and
    a `FILE:` field (row 2, pre-filled with the currently active path)
    — two SEPARATE text inputs, search vs. the actual save target.
  - An `ACTIVE:` line repeating the current file path for reference.
  - A `SUGGESTIONS:` section — one row per suggestion (here, a
    truncated `...ditor/pieces/document.txt`, i.e. the active path
    again, ellipsis-truncated from the left when too long for the
    row).
  - A `DIRECTORY CONTENTS` section: `[ ] N. [<- BACK]` first, then
    `[DIR] name/` rows, then `[FIL] name (size)` rows (size shown as
    `2KB`/`26B` — human-readable, not raw bytes), long filenames
    ellipsis-truncated the same way suggestions are.
  - Footer actions: `[ ] N. [SAVE FILE]` / `[ ] N. [CANCEL]`.
  - Status line: "Enter Save-As path." while in this mode.

This is a genuinely complete, sensible file-picker shape (search +
explicit target field + suggestions + real directory browsing with
back-nav + human-readable sizes) — a solid reference to match or
deliberately deviate from when the khtpm widget gets designed for
real, rather than reinventing this from nothing.

## 3. `[ ]3.file:` doesn't open a real explorer on Save As / Load yet

Direct report: the khtpm taskbar's own `file:` cell (confirmed live
today via a frame dump — `strip-cell-3`, label built from
`${file_label}`, currently showing `file:pre-design`) "should also
open under `[]3.file:` on save-as/load which it doesn't." Read
together with §1, this is the SAME underlying gap: khtpm has no real,
generic file-explorer widget at all yet, so any window's Save As/Load
action has nowhere real to route to. This is not a separate bug to
fix independently of §1 — building the widget in §1 IS the fix for
this too. Not yet investigated: exactly what `file:` is currently
wired to click-wise (`ACTIVATE` per the frame dump — same generic verb
every other dock cell uses) and what, if anything, currently happens
on click today; worth a real look when this becomes scheduled work,
not guessed at here.

## 4. The `102.agy-txt` legacy-launcher bug (root-caused live, 2026-09-05)

Direct live report — running `sh button.sh r` from
`44.xyz.01.00/102.agy-txt/` fails immediately:
```
button.sh: 117: ./system/renderer: not found
button.sh: 166: ./system/keyboard_input: not found
```
**Root cause, confirmed by reading the script and the binaries
directly** (not guessed): the binaries are completely fine — `file
system/renderer` / `file system/keyboard_input` both show normal,
valid, dynamically-linked x86-64 ELF executables, and they exist right
where the launcher's own header comment expects (`$SCRIPT_DIR/system/`).
The bug is a real path/cwd mismatch inside `button.sh` itself:
- Line 77 does `cd "$SESSION_DIR"` (a fresh per-session directory
  under `pieces/sessions/<id>/`), changing the shell's working
  directory away from `$SCRIPT_DIR`.
- Lines 117/121/166 (and the `gl_mirror`/`chtpm_rgb_render` calls
  around them) then invoke `./system/renderer`, `./system/
  keyboard_input`, etc. — RELATIVE paths, which now resolve against
  `$SESSION_DIR`, not `$SCRIPT_DIR`.
- `$SESSION_DIR` never gets a `system/` directory at all. The script's
  own comment near line 75 says plainly: **"No symlinks — C processes
  resolve shared/persistent files via PRISC_PROJECT_ROOT env var"** —
  i.e. a real, deliberate refactor removed the old symlink-the-shared-
  dirs-into-the-session approach (see the `docs/` symlink discussion
  in the same comment block, "PITFALL 62"). That refactor updated how
  *data* files are found (via the `PRISC_PROJECT_ROOT` env var, which
  IS correctly exported a few lines earlier), but never updated these
  `./system/*` BINARY invocation lines to match — they're still
  relative, from before the symlink removal, and nothing ever
  recreates a `system/` directory inside `$SESSION_DIR` anymore.

**The actual fix** (not yet applied — this pass was documentation
only, per the request): change the relative `./system/renderer`,
`./system/chtpm_parser_pal`, `./system/gl_mirror`, `./system/
chtpm_rgb_render`, and `./system/keyboard_input` invocations to
absolute (`"$SCRIPT_DIR/system/renderer"`, etc.) — the same fix
pattern the `PKILL_PATTERNS` variable earlier in the script already
uses correctly for matching (it searches by `system/renderer` as a
substring, which still matches regardless of cwd, so THAT part was
never broken — only the actual exec calls are).

### "Is there a way to mark this binary as legacy so demos still work?"

Worth answering directly since it changes the next action: **that
framing doesn't quite fit this specific bug.** The binaries aren't
missing, stale, or wrong-platform — they run fine; nothing about them
needs a "legacy" marker or a compatibility shim. This is a plain
one-line-per-call path bug in the launcher script itself, introduced
by a refactor that updated data-file resolution but missed the binary
exec lines. The real fix is the four/five `./system/X` → `"$SCRIPT_DIR/
system/X"` edits above — small, mechanical, and durable (won't need
re-fixing every time something upstream changes, unlike a "legacy"
flag that just suppresses a check).

That said, **a genuine legacy-binary-marker convention could still be
worth having house-wide**, for the real case it WOULD fit: a project
whose binaries are intentionally frozen/unmaintained (no rebuild
expected, might not match the current shared-lib ABI) and whose
launcher should skip a "binary missing, please build" hard failure in
favor of a softer "legacy demo, best-effort" notice. If that's wanted
as a real house convention (not just for this bug), it'd want its own
brainstorm/plan doc — flagging it here as a distinct, smaller idea
surfaced by this conversation, not conflating it with the actual
button.sh fix above.

## 5. Toys text-editor refactor

Direct request: refactor/build a real text-editor program and put it
under `toys` (the khtpm taskbar's existing `toys` dock cell), using
tpmos's `agy-text-editor` (captured live in §2.1) as the UX reference
for what it should feel like to use — NOT as code to port. Per house
standards (`khtpm-house-standards` skill, `CENTROID_GOLD_STD.md`),
this needs to be a real khtpm app: its own manager process (owns the
actual text-buffer state, load/save/new/clear logic) publishing a
plain-text projection, plus a static `.xhtpm`+CSS template rendered by
the shared `khtpm_core_render.c` — not a port of `chtpm_parser_pal`'s
own ASCII-frame rendering loop, and not hand-rolled Elem-tree C in the
shared renderer.

Real design questions this needs before it's a real plan (not
answered here, flagging for when this gets scheduled):
- Text editing itself needs a genuinely multi-line, cursor-addressable
  input surface — the house's existing generic `<cli_io>` element is
  single-line/armed-field shaped (per `khtpm-house-standards`'s own
  description: "armed/typed/committed"). **Decided 2026-09-05**: a new
  `<text_area>` element, sharing code with `cli_io` where it stays
  legible (its existing word-wrap-for-width calc) but with its own
  real cursor/newline handling, using the exact same `^`-armed /
  Escape-disarmed activation convention `cli_io` already has.
  **Updated same day**: `cli_io` itself is also getting a real cursor
  (decoupled from `text_area`, the shared primitive `text_area`'s own
  multi-line cursor logic generalizes from). Full real plan (still not
  implemented):
  `08-roadmap/design-docs/CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-DESIGN.md`.
- Save As / Load reuses the File Explorer widget from §1/§3 once it
  exists — another real reason that widget should land before (or
  alongside) this, not after, so the editor doesn't end up building
  its own one-off file picker that then needs replacing.
- FILE MENU as its own real sub-page (matching §2.1's reference shape)
  fits khtpm's existing `<page>` stack convention already used
  elsewhere in the house (e.g. taskbar-settings' own page structure) —
  likely a good, low-risk match, not a new capability needed.

## Dependency order (informal, not a committed plan)

File Explorer widget (§1/§3) is the one real blocking dependency for
both `pdl-reader` (§1) and the toys text editor's own Save As/Load
(§5) — building it once, generically, before either consumer is the
efficient order. The `102.agy-txt` launcher fix (§4) is fully
independent and small enough to do any time, whenever `102.agy-txt`
itself is next touched — not blocking anything else here.
