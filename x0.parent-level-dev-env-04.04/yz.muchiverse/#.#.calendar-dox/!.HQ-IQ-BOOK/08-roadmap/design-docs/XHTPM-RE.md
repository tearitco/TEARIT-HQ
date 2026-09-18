# XHTPM-RE — confidence, bloat, and how we’d split the renderer

**Date:** 2026-09-18  
**Status:** guidance. Not a refactor this pass. Name-change note included.

## Confidence (honest)

On **today’s drop/inventory work:** medium-high on the *leads*, medium on
the *pixels*. `rename()` into inventory is proven (m8 via xdotool, ninja
by you). Highlight paint and “is the pointer over this window” fought
WM position, z-filter unmap, and lost mouse events. Those are
tractable, not mysterious.

On **`khtpm_core_render.c` as a place to keep adding features:** low
comfort. I can still land small, local fixes (chrome trio, breadcrumb
wrap, explorer stat-poll). Each one costs more than it should because
the same file is HQ window, dock, strip, palettes swatch, *and*
`tp_main()` desktop pal. A “raise while drag” change has to live next
to z-layer unmap, 3D raymarch, cursword grab, and XDND. That is the
tax, not a lack of skill in the original design.

I am not more “experienced at architecting this house” than the people
who already chose **separate `+x` + file IPC**. That *is* the house
pattern. The 18k-line merge was an optimization that now fights the
pattern.

## Is it bloated?

**Yes, as a single translation unit.** ~18.7k lines in
`khtpm_core_render.c` (2026-09-18). Complexity is not “too many
helpers”; it is **too many programs in one process**:

| Mode | How you get there | Should be |
|---|---|---|
| HQ / `.xhtpm` window | `argv` house + template | stay: one small *window* renderer |
| Dock / taskbar strip | same binary, other template | already a sibling manager; keep IPC |
| Desktop pal / `tp_main` | `argc==2` package_dir | **own `+x`** (it already was `tp_desktop_window`) |
| Palettes swatch + RMMV arm | same binary + `tp_arm_placer_rmmv` | placer is already separate — good |
| Dump `'p'` | in-process `dump_frame_png` | keep in whoever owns the X window |

House rule already on disk: no cross-`.c` linking to share behavior
inside one binary; fork/exec + files. Folding `tp_desktop_window_rgb.c`
*verbatim* into this file (2026-09-01 comments in the file itself) is
exactly the moment modularity was traded for “one binary, one
dispatch.” That trade is now due.

**20k lines is not “unfactorable.”** It is the usual cliff where *every
new verb* (drop highlight, z-raise, Place overlay) requires a week of
mode-hunting. Better to split **before** Place-out, pc-hq↔desk, and
drag-preview pile on. After those, you will still split, just with more
scar tissue.

## MVC is the wrong slogan here

MVC assumes one process, shared memory, a controller object. This
desktop is already **many processes, one filesystem.** That is closer
to:

- **Unix filters:** each `+x` reads files, writes files, dies or loops.
- **Entity component, not MVC:** a pal is a **directory**; windows
  *project* it. Inventory is `mv`.
- **The 2026-ish “screaming architecture”** (organize by *feature
  process*, not by layer): `file_explorer_manager.+x`,
  `tp_arm_placer_rmmv.+x`, `khtpm_core_render.+x` (window only).

The first CHTPM/W$R incarnation you pointed at (separate executables,
unlinked, file IPC, shmem later) is the right ancestor. Do not
“upgrade” to a classic MVC monolith. Evolve IPC: text files → optional
`mmap` later, **same paths**.

A practical split (order matters):

1. **Extract `tp_main` back to `khtpm_entity.+x`** (desktop pals).
   File IPC already: `desktop_pos.txt`, `active_z`, drop_zones,
   history. This is the extraction that makes drag/drop/z *local*.
2. **Keep one HQ renderer** for `.xhtpm` (File Explorer, palettes
   picker chrome, entity-menu). Cap it; new games get a new `+x`, not
   a new `g_is_*` flag.
3. **Placer stays its own `+x`** (already). Explorer Place should
   *exec* it, not grow `khtpm_core_render`.
4. **Managers stay managers** (`file_explorer_manager` already
   file-IPC). Today’s live-list fix belongs *there*, not in the
   renderer — that is the modularity working.

Do **not** start this extract in the same burst as Place-grid. Land
small FE fixes, then a dedicated extract PR with a golden pal
(`m8` / ninja) still dragging.

## Name: chtpm → xhtpm → xhtm

House already moved markup toward **`.xhtpm`**. You want the
convention finished as **`xhtm`** (HTML-shaped, X11). Fine, but it is
a **rename campaign**, not a refactor:

- Templates: `.xhtpm` → `.xhtm` (or keep xhtpm if grep cost is the
  enemy — pick one and freeze).
- Docs/binary names: `khtpm_*` is the *engine*; `xhtm` is the *markup*.
  Don’t rename the C symbol soup in the same PR as `tp_main` extract.

If we refactor, **complete the markup name in that same campaign**,
file-by-file, with a tiny `button.sh` still accepting the old
extension for one cycle.

## What I did vs what I did not

- **Did (this burst):** File Explorer manager `stat`s `current_dir`
  every 50ms and relists if mtime/nlink changed — ninja drop should
  appear without close/reopen. Rebuild manager; reopen explorer once.
- **Did not:** Place → tic-tac-toe overlay (still `fe_place_armed.txt`
  only). Still waiting on your OK for that spawn.
- **Did not:** start extracting `tp_main`. This doc is the audit seed.

## Ask

1. OK to do Place-grid next (exec `tp_arm_placer_rmmv`, pal spawn on
   click, **not** RMMV tile stamp)?
2. After that, extract `tp_main` → `khtpm_entity.+x` as the first
   real split — yes/no?
3. Freeze markup name as `xhtm` or keep `xhtpm`?
