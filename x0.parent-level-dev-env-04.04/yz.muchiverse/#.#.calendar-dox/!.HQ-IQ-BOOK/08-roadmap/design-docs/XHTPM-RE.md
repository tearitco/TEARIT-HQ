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

**khtpm vs xhtpm/xhtm:** keep that split. **khtpm** = engine (paint an
Elem tree to X11). **xhtpm / xhtm** = layout markup. Not a rename of
the engine.

## Correction (user 2026-09-18) — what “combined” actually meant

I overstated. You did **not** decide to smash unix-exe+IPC programs
into one process because “modularity was too hard.”

**CENTROID_GOLD_STD §1a already says the rule:**

1. If it is already a **binary + file IPC**, **call it** (`fork`/`exec`
   + files). Do not paste it into another `main()`.
2. If two things are the **same job** (paint the same kind of window)
   and someone forked a *private copy of the engine*, **refactor to
   one implementation** — that is compile-time share (`-I` canonical
   `khtpm_draw_core.c`), not process merge.

**What you combined on purpose (correct):**

- Many **HQ / `.xhtpm` windows** (events-hq, palettes chrome, File
  Explorer, entity-menu popup) are the *same process shape*: parse
  markup, layout Elems, X11 loop. One `khtpm_core_render.+x`, different
  templates. That is engine + layout, not IPC.
- **Shared paint/CSS** compiled from `_shared-lib/` in place
  (`SHARED-SOURCE-COMPILE-IN-PLACE.md`). One source, many binaries.
  Still not “one process for everything.”

**What was a misunderstanding if it happened (undo later, not a
philosophy change):**

- Relocating **`tp_desktop_window_rgb.c` into this file as `tp_main()`**
  (argv-dispatched pal process). That binary was already unix-exe+IPC
  (`desktop_pos.txt`, `history.txt`, `interact_relay.txt`, one process
  per pal). Pasting it into the HQ renderer is **rule 1 violated**, not
  “centroid.” Comments in `khtpm_core_render.c` even say it was moved
  *verbatim* and dispatched by argv so we would not `#include` across
  `.c` — that is the wrong escape hatch. The right one was **keep the
  `+x`, share draw via `-I` if needed.**
- Folding **`khtpm_strip_parser.c`** (and `poll_agent_relay`) into the
  same binary. That path *was* file IPC (`livedesk_agent_relay.txt`).
  After the fold the consumer died; `nav.sh nav` is a no-op. Same class
  of mistake.

**Still correctly separate today (do not absorb):**
`file_explorer_manager.+x`, `tp_arm_placer_rmmv.+x`,
`khtpm_taskbar_manager_main.+x`. Explorer Place should **exec** the
placer, not grow `tp_main`.

If an agent “combined because IPC wasn’t the shape,” that is only
valid for **same-shape HQ windows**. If they combined something that
**already** spoke files, they misread centroid.

## Is it bloated?

**The HQ window engine is large but one job.** Pain is **`tp_main`
living in the same `.c` as HQ**, so drop/z/grab edits sit next to
xhtpm layout. Line count ~18.7k is mostly that paste + history, not
proof that File Explorer belongs in another language.

| Process shape | How you get there | Intent |
|---|---|---|
| HQ / `.xhtpm` window | house + template | **one engine**, many layouts |
| Dock / strip | manager `+x` + files; strip paint | **IPC** — do not fold parser back in |
| Desktop pal | was `tp_desktop_window_rgb.+x` | **IPC** — extract `tp_main` back out |
| RMMV placer | `tp_arm_placer_rmmv.+x` | **IPC** — already right |
| FE directory list | `file_explorer_manager.+x` | **IPC** — already right (stat-poll lives here) |

The cliff is **two process types in one `.c`**, not “HQ has too many
windows.” Extracting the pal `+x` again is restoring IPC, not inventing
a new architecture.

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
