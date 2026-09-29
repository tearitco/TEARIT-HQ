# 🪟🍎 compact-win-mac.md — cross-platform orientation

Compact index for Windows/Mac porting work. Not a replacement for the
real docs below — read those for depth. This is "where things stand
and where to look," verified against the actual codebase 2026-09-17
(not just trusted from docs, which do go stale — see §1).

---

## 1. Real current status (verified, not assumed)

**Windows: partial real coverage, several apps deep.** Confirmed on
disk: `win_posix_shim.h` (POSIX shim header, `#ifdef _WIN32` guarded)
exists in 4 real app trees — `101.mutaclsym🧟‍♂️️19.00/ops/`,
`@.apps/piececraft-xyz/ops/`, `@.apps/aomorai-editor/ops/`,
`@.apps/piececraft-hq/ops/` (plus stale copies inside two
piececraft-xyz/piececraft-hq session dirs — those are runtime, not
source). And 3 real `*_win.c` Windows-specific source files exist:
`_.monads/_.livedesk-taskbar/ops/khtpm_strip_x11_win.c`,
`.../khtpm_strip_parser_win.c` (the livedesk taskbar strip's real Win32
GDI reimplementation of the Xlib/Xft subset it needs), and
`&.widgits/tile-picker/ops/tp_desktop_window_win.c`. Per
`WIN-CONVERSION-STATUS.md` §1 table: taskbar, entity windows, several
apps' `button.ps1` launchers, and aomorai-editor's full CHTPM vertical
(keyboard, GL, world-gen) are reported **working**; events-hq/db-hq
windows and the merged 8-mode renderer (`khtpm_core_render.c` née
`khtpm_entity_menu_render.c`) have **no Windows twin at all** — biggest
known gap (see §4).

**Mac: essentially zero platform-specific work.** Confirmed: `find
... -iname "*_mac.c" -o -iname "*mac_shim*"` returns nothing anywhere
under `44.xyz.01.00`. Per `CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md`
Part 2, macOS today runs via **XQuartz** (real X11), not a native port
— `build_core_render.sh` has a `Darwin` leg for XQuartz paths, and a
2026-08-23 pass ran the house under it successfully. So Mac "works"
today only in the sense that it's just Linux/X11 with XQuartz; no
native Cocoa/Metal work has started or is currently planned.

---

## 2. Where the real docs live (read these, don't re-derive)

- `07-install-and-ship/windows-mac/WIN-CONVERSION-STATUS.md` — the
  living per-app Windows status table + the events-ez deep-dive
  (why launch "looks fixed" but the CHTPM stack underneath isn't) +
  the aomorai-editor full-vertical writeup (the best current reference
  example) + a "next projects" queue and an agent checklist.
- `07-install-and-ship/windows-mac/WIN-COMPAT-RULE.md` — the standing
  rules (no symlinks, path separators, `setsid`/`nohup` gaps). Read
  before touching any Windows code.
- `08-roadmap/design-docs/CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md` —
  the real architecture: `kh_plat.h` shared platform module for
  managers (shipped v1), the renderer seam for `khtpm_core_render.c`
  (guidance only, NOT started — explicitly do not pre-build it), and
  the "cautionary tale" of a seam built ahead of a real consumer that
  bit-rotted and got archived. Read this before designing any new
  seam.
- `08-roadmap/design-docs/CROSS-PLATFORM-PENDING-2026-08-29.md` — the
  delta backlog of what landed on Linux since the last verified
  Windows/Mac pass and hasn't been checked on either leg yet. Item #1
  is the missing renderer twin (highest impact).
- `08-roadmap/browser-prompting/platform-passes/12.grok-windows-mac-compat-delegation.md`
  — a delegation template for tasking an external model (Grok) to
  audit/fix an *existing but untested* Windows shim; carries the real
  house cross-platform rules (fork/exec vs `_spawnl`, `setpgid` has no
  Windows equivalent, path separators, font-path fallback). Doesn't
  name new concrete work items beyond "audit the shim you're handed."

---

## 3. The established porting pattern — copy this shape

Reference example: `_.monads/_.livedesk-taskbar/ops/khtpm_strip_x11_win.c`
paired with `khtpm_strip_parser.c` — a small Win32 GDI file
implementing just the Xlib/Xft subset (`Display`, `XEvent`, window +
pixmap + text primitives) that the *shared, unmodified* parser/layout
code calls. The parser doesn't know it's on Windows. This is the house
reference pattern of truth (also cited in
`CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md`'s ground rules).

For a smaller app-level POSIX gap (flock/kill/usleep/open, not a whole
Xlib subsystem), the pattern is a header-only `win_posix_shim.h`
included after standard headers — see
`@.apps/aomorai-editor/ops/win_posix_shim.h`: `#ifdef _WIN32` block
only, no-op on Linux, "platform only, not design logic" per its own
comment. Four real apps already use this exact header shape (§1).

Rule for both: **surgical `#ifdef` wraps only — never refactor
established POSIX logic "to clean it up" for Windows compat** (from
the Grok delegation doc's house rules, and restated in
`WIN-COMPAT-RULE.md`).

---

## 4. What's genuinely NOT started

- **Native macOS port** — zero platform-specific files exist. Current
  "Mac support" = XQuartz (i.e. real Linux/X11 via an X server), not a
  Cocoa/Metal port. If native Mac is ever wanted, it reuses the same
  seam as a third backend — not scoped, not prioritized.
- **`khtpm_core_render.c` Windows twin** — the single shared renderer
  for db-hq/events-hq/chat-hai/Settings/palettes/popup/entity-menu/
  Debug (8 window modes) has zero `_WIN32` guards and no `_win.c`
  sibling. Confirmed still true by the `*_win.c` search above (only 3
  files exist, none is this one). This blocks the most windows on
  Windows today.
- **Renderer platform seam itself** — `CROSS-PLATFORM-SEAM-AND-SHARED-
  INFRA.md` Part 2 is explicitly "guidance only, NOT started," and
  explicitly says not to pre-build it before a real Windows/macOS pass
  is scheduled (a nearly identical seam rotted once already —
  `khtpm_plat.h`, orphaned in `&.widgits/tile-picker/ops/`).
- **New tile-picker ops from the 2026-08 session** (`tp_arm_placer_rmmv.c`,
  `tp_place_desktop.c`/`_rmmv.c`) are real X11-only, zero `_WIN32`
  branches — per `CROSS-PLATFORM-PENDING-2026-08-29.md` item 2.
- **`kh_plat.h` v2** (`listdir`, `fopen_utf8` for emoji paths) and the
  `khtpm_taskbar_manager.c` migration off its 49 inline `#ifdef`s —
  not done, prerequisite work is tracked in the seam doc's sequencing.
- **Windows script-spawn policy undecided**: bundled `sh.exe` vs
  `.ps1` sibling-per-`.sh` — `kh_plat_run_house_script`'s Windows
  branch is currently a logged no-op stub.

`08-roadmap/OPEN-ITEMS.md` item #2 ("Cross-platform (Windows/Mac)
work: pending") is stale as a one-liner — it should point here now;
see the OPEN-ITEMS edit made alongside this doc.

---

## 5. If you're picking this up — checklist

1. Read `WIN-COMPAT-RULE.md` first, then this file's §2 doc list in
   order (status → seam → pending → delegation template).
2. Check `win_posix_shim.h` in the target app (or the closest sibling
   app that has one) against the target app's own actual POSIX calls
   before writing new shim code — don't assume the existing shim is
   complete for a new app; diff what it covers vs. what's called.
3. Build with the real MinGW/cross-compile setup documented in
   `WIN-CONVERSION-STATUS.md` (aomorai-editor §4 has the fullest
   working example: `button.ps1 compile`/`run`, `scripts/build.ps1`)
   before assuming anything is broken — several "bugs" in that doc
   turned out to be spawn/fopen/path issues, not logic bugs.
4. If the task touches `khtpm_core_render.c`: read
   `CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md` Part 2 in full before
   writing a seam — it names the exact Xlib/Xft surface to extract and
   warns explicitly against building it speculatively.
5. Never invent Mac-native work unless explicitly asked — today "Mac"
   means XQuartz, and that already works via the Linux/X11 build.
6. Update `WIN-CONVERSION-STATUS.md` / `CROSS-PLATFORM-PENDING-*` (or
   file a fresh dated pending doc) with whatever you find or fix —
   these are living working docs, not one-shot reports.
