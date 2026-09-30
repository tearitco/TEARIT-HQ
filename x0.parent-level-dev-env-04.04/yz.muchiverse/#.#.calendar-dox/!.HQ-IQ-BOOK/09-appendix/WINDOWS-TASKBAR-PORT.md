# WINDOWS-TASKBAR-PORT.md — the khtpm taskbar on Windows

**Status: working, 2026-09-26.** The canonical renderer compiles and runs
unmodified on Windows, both bars are populated, and they are launched
fully detached (no terminal to keep open). This file exists so that the
next person to change the taskbar **on Linux** can get Windows back up to
speed without re-deriving any of it.

Read this alongside `PROGRESS-strip-static-layout.md` (why the bars are
static templates) and `../02-architecture/CENTROID_GOLD_STD.md` (the
renderer/manager split this port preserves).

---

## 1. The one-paragraph version

`khtpm_core_render.c` is portable by construction: it has **zero**
`_WIN32` blocks and reaches for X11/Xft only through `#include`, so the
canonical source compiles **unmodified**. What it could not do on Windows
was *find* those headers, because MinGW-w64 ships no X11. The port is
therefore a **header funnel**, not a reimplementation: `ops/win-compat/`
sits ahead of the system include path and forwards every
`X11/*`/`Xft/*`/`extensions/*` header to `khtpm_strip_x11_win.c`, a
self-contained Win32 backend that already existed in the house for the
old strip parser. Three small `.c` files come along: that X11 shim, a
POSIX shim, and `khtpm_win_compat.c`.

**If you take one thing away:** on Windows, never edit
`khtpm_core_render.c` to make it compile. If it doesn't build, the
missing piece belongs in `win-compat/`. A `#ifdef _WIN32` in the
canonical renderer is a bug in the port, not a fix in the renderer.

---

## 2. Build and run

From `_.monads/_.livedesk-taskbar/ops/`:

```powershell
.\build_khtpm_strip_win.ps1          # build only
.\run_khtpm_strip_win.ps1 new        # build fresh, stop old, launch both bars
.\run_khtpm_strip_win.ps1 boot       # launch only, no rebuild (autostart)
.\run_khtpm_strip_win.ps1 stop
.\run_khtpm_strip_win.ps1 status
```

Same action set and same intent as `run_khtpm_strip.sh` /
`build_khtpm_strip.sh`. Both scripts discover the house root by walking
**up** for a directory holding both `#.desktop/` and `&.widgits/`
(same test as `khtpm_vars.sh`'s `khtpm_find_house()`) — never by
counting path levels, which is how the first attempt got `.monads`
instead of the house root.

`ops/build.ps1` is **dead code** — see §6. Use the two `*_win.ps1`
scripts.

### The two flags that are load-bearing

Both were found by a real failure, not by taste:

- **`-include win-compat/khtpm_win_compat_prelude.h`** — MinGW's own
  headers trip over the POSIX identifiers the canonical source uses
  before our shims get a say.
- **NOT `-municode`** — the renderer's entry point is
  `int main(int argc, char **argv)`, the *narrow* one. `-municode`
  rewrites the CRT to expect `wmain`, and then every `char*` is
  mismatch-reported as `LPCWSTR`. This cost real time to diagnose; don't
  re-add it.

### Link line shape

The renderer **text-includes** its shared modules by bare name:

```c
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c"   /* a real .c, not a header */
#include "khtpm_draw_core.c"
```

so the build needs `-I` on the one canonical
`&.widgits/_shared-lib` (Linux does the same with `-I "$SHARED"`).
`khtpm_css_parser.c` is **linked, not included** — the renderer includes
only the header for declarations but calls `css_load()` and
`css_compute_style()`, which are defined there. Omitting it is a link
error, not a compile error.

Windows libraries instead of the Linux set:
`-lgdi32 -luser32 -lshlwapi -lshell32 -ladvapi32 -lcomctl32 -lcomdlg32`
(no `-lX11`/`-lXft`/`-lXext`; they have no meaning here).

---

## 3. Detachment — the part that is genuinely different

**Direct report (2026-09-26):** the bars had to be started from an open
CLI, and died when that CLI was closed. Linux hides this behind
`setsid env ... & < /dev/null >>log`. PowerShell's `Start-Process` does
**not** give that guarantee: the child stays inside the launching
shell's job object, and when the parent goes away the job is torn down
with it.

`run_khtpm_strip_win.ps1` therefore does not use `Start-Process` at
all. It launches through **WMI** (`Win32_Process.Create`), so the new
process's parent is the WMI service host — it inherits no job object, no
console and no parent. That is the Windows equivalent of `setsid`.

| method | survives parent exit |
|---|---|
| `Start-Process` | **no** — stays in the parent's job object |
| `CreateProcess` + `DETACHED_PROCESS` | partial — detaches the console, but a job object with `KILL_ON_JOB_CLOSE` still reaps it |
| `Win32_Process.Create` | **yes** — created by the WMI service, fully independent |

`Win32_Process.Create` exposes no way to set a child's std handles, so
the Linux script's `>> "$KHTPM_LOG" 2>&1` has no equivalent here.
Nothing is lost: the renderer self-logs to
`#.desktop/khtpm_strip_parser.log`, and the bars never printed to the
terminal to begin with.

The runner also keeps the Linux runner's discipline that matters most:
**never trust a bare exit code for a backgrounded GUI launch.** It
confirms real live PIDs after launch and only then writes
`#.desktop/livedesk_taskbar.pid`. `status` and `stop` work from any
shell, which is the practical test that detachment actually happened.

### Deliberate divergence from Linux: none left in process count

`run_khtpm_strip.sh` launches **only** `khtpm_strip_header.xhtpm`, and so
does the Windows runner now. The header template is
`<window class="dock-header">`, so that one renderer already parses
`khtpm_strip_bottom.xhtpm` as its `g_dock_peer` and builds a real peer
window for it — the bottom bar is drawn by the *same* process.

The Windows runner used to launch the bottom as a second process. That
was a workaround for a Windows-only path bug, not a real platform
difference; see §8. The workaround is removed, because a second renderer
carries its own independent `g_focus_nav` counter — two bars, two nav
selectors, each starting at `1.HQ` and moving only under its own
keyboard grab. That is the whole class of "the arrow keys move the wrong
bar" / "nav is stuck at 1" reports.

Verify with `run_khtpm_strip_win.ps1 status`: the renderer PID count
must be **1**, and that one PID owns both bars.

---

## 4. The bottom bar is empty — and that is a *data* fact

Reported as "the bottom bar is empty". It is not a rendering bug. The
template draws three repeat blocks:

```
<repeat count="${n_tabs}"   bind="tab">   entity cells
<repeat count="${n_hqwins}" bind="hw">    HQ-window cells
<repeat count="${n_sc}"     bind="sc">    shortcut cells
```

and the live `#.desktop/strip_ui.txt` currently holds:

```
n_hqitems=0
n_tabs=1
tab_0_label=dsr_bank_a1
n_sc=0
n_hqwins=0
```

So the bottom bar is **correctly drawing one cell** in a 1322px-wide
bar, which reads as empty. Measured on the live detached bars, the same
build:

| bar | size | distinct colours |
|---|---|---|
| header | 1266x22 | **108** |
| bottom | 1322x22 | **6** |

To make the bottom look populated, give it data: open HQ windows
(`n_hqwins`) or register shortcuts (`n_sc`). Do **not** "fix" this in the
renderer or the template — the template is the house's deliberate
one-row flex layout (see the migration note in
`PROGRESS-strip-static-layout.md`), and the count keys are the manager's
real output contract.

Also note the manager writes `n_hqitems` (an HQ *item* count, used
elsewhere) while the bottom template consumes `n_hqwins` (HQ *window*
count). Both keys are present in the state file and they are different
things; do not conflate them.

---

## 5. Known limits, stated rather than hidden

- **`XSendEvent` is in-process only.** The shim delivers into this
  process's own queue. It reaches self-addressed `ClientMessage`s and an
  in-process drag pair, but it cannot reach a *foreign* client the way a
  real X server would. Cross-process XDND drops are therefore partial.
- **`XSetLineAttributes` records width and dash state but Win32 pens
  have no dash pattern**, so dashed rings render solid.
- **`--dump-and-exit` does not work for the dock.** It only guards
  `hq_run_event_loop`, which the dock path does not use — the dock has
  its own loop, so the process just keeps running.
- **`dump_frame_png()` is Linux-only.** It writes `/tmp/entity-menu-frame.png`
  and shells out to a Linux helper binary with a *raw X window id*. There
  is no Windows equivalent and adding one is not worth it: capture the
  real `HWND` instead (`PrintWindow` into a `System.Drawing` bitmap). That
  is how every measurement in this doc was taken.
- **`select()`** honours its timeout and reports nothing-readable; that's
  what the X connection fd would have done between frames.
- **`waitpid()`** reports `ECHILD` truthfully, because the renderer's
  liveness check branches on it.
- **`st_mtim`** is synthesised from `st_mtime` so the dock-peer freshness
  gate still works. NTFS only stores whole seconds, so that gate is
  1-second resolution — it is documented as such rather than pretending
  to nanoseconds.
- **Mask values are deliberately two-valued.** The shim's pre-existing
  `GCForeground=1` / `GCBackground=2` / `GCFont=4` and
  `ExposureMask=1` / `KeyPressMask=4` are the *old strip parser's own
  invented small numbers*, not real Xlib values — and the existing
  `XCreateGC`/`XCopyGC`/`XGetGCValues` switch on them by value. Changing
  them to the real Xlib constants would silently break every current
  caller. New masks therefore use real Xlib bit positions chosen clear
  of 1/2/4/8, and both sets coexist. **Do not "correct" these.**
- **`XSelectionEvent.target`** is carried by the shim even though real
  Xlib does not declare that field on that struct (it exists only on
  `XSelectionRequestEvent`). `khtpm_core_render.c:10538` writes it, and a
  real server ignores it on the way out. Carrying it is what keeps the
  canonical source unedited.
- **Property store is a shim, not a server.** `XChangeProperty` keeps a
  real per-window store (Replace/Prepend/Append, all three X11 formats)
  so the XDND drop round trip has something to read. Before that it only
  handled `_NET_WM_WINDOW_OPACITY` and discarded everything else, which
  is why every drop was silently discarded.

---

## 6. `ops/build.ps1` is dead — do not use it

It is a machine translation of the four Linux build scripts and cannot
run. Not "unidiomatic" — broken in ways that stop it dead:

- `$CC = "if ($env:CC) { $env:CC } else { "gcc" }"` is a **bash snippet
  assigned to a PowerShell string**, so `& $CC` tries to execute that
  whole sentence as a program name.
- `if (([ "$env:OS" = "Darwin")])` is bash if-syntax inside PowerShell.
- It passes `$(pkg-config --cflags xft)` and `-lX11`, neither of which
  exists here.
- Its `$SHARED` joins `../../../` from `.monads`, overshooting the house
  root by one level.
- It still builds the **retired** `khtpm_strip_parser.c` stack, not the
  current one.

Left in place rather than deleted, because it is a useful record of the
translation attempt and deleting it is a separate decision.

---

## 7. Re-syncing after a Linux change

The fast path, in order:

1. **Did you touch `khtpm_core_render.c`, `khtpm_draw_core.c`,
   `khtpm_render_core.c`, `khtpm_css_parser.*` or a `.xhtpm`/`.css`
   template?** Then just run `.\build_khtpm_strip_win.ps1` and
   `.\run_khtpm_strip_win.ps1 new`. No Windows-side change is needed —
   that is the entire point of the header-funnel design.
2. **Did you use an Xlib call the shim doesn't have yet?** It will fail
   at *link* time with an undefined reference, or at *compile* time with
   a missing declaration. Add the declaration to
   `khtpm_strip_x11_win.h` and the implementation to
   `khtpm_strip_x11_win.c`, plus the `.c` file to `$SOURCES` in
   `build_khtpm_strip_win.ps1` if it's new. Do not touch the canonical
   renderer.
3. **Did you add a new Xlib *header*?** Add a funnel in
   `ops/win-compat/X11/`.
4. **Did you change the manager?** It needs no X11 and no `win-compat`;
   it just needs `-I` on `&.widgits/_shared-lib` for
   `kh_proc_registry.h`.
5. **Verify with real evidence, not a clean compile** (AGENTS.md). A
   green build proves nothing on its own — capture the live `HWND` and
   count distinct colours, as in §4.

### Which files are Windows-owned

Everything below is Windows-specific and has no Linux counterpart —
touches here never need re-porting:

```
ops/win-compat/**              header funnels (X11, Xft, extensions, sys)
ops/khtpm_strip_x11_win.{c,h}  the Win32 X11 backend
ops/khtpm_strip_posix_win.{c,h} POSIX shims
ops/khtpm_win_compat.c         select/waitpid/stat/time/env/process
ops/build_khtpm_strip_win.ps1  Windows build twin
ops/run_khtpm_strip_win.ps1    Windows runner twin
```

Everything else — `khtpm_core_render.c`, the shared modules in
`&.widgits/_shared-lib`, both templates, the CSS, the manager — is the
one canonical copy shared with Linux.

---

## 8. Two real Windows bugs behind "nav is stuck at 1"

Both of these produced the same user-visible report, so they are
recorded together.

### 8a. `g_package_dir` dirname only stripped `/`

`khtpm_core_render.c` derived its package directory with

```c
{ char *slash = strrchr(g_package_dir, '/'); if (slash) *slash = '\0'; }
```

The Windows runner passes `\`-separated argv paths, so `strrchr`
returned `NULL` and `g_package_dir` stayed as the **full
`khtpm_strip_header.xhtpm` file path**. Every `"%s/<something>"` built
from it was then a path *underneath a file* and failed silently. The
one that mattered: `g_dock_peer_path` became
`...\khtpm_strip_header.xhtpm\khtpm_strip_bottom.xhtpm`, so
`parse_chtpm()` left `g_dock_peer` `NULL`, and
`kh_ensure_dock_peer_window()` bailed at its `if (!g_dock_peer) return;`.

Fix: strip whichever of `/` or `\` is actually last, so both path
styles work. **This is a canonical-source fix, not a Windows one** — the
renderer is shared with Linux and had simply never been handed a
backslash path before.

### 8b. `XGrabKeyboard` was a no-op that cancelled itself

On X11, `XGrabKeyboard` routes key events to the grab window no matter
what has focus, which is what makes the taskbar's arrows global. The
Win32 shim only did `SetFocus()`, which cannot move focus to another
thread's window, and real key events kept arriving as `WM_KEYDOWN` to
whatever the foreground window was — the focused renderer window never
saw them.

Two changes in `ops/khtpm_strip_x11_win.c`:

1. A **`WH_KEYBOARD_LL`** low-level hook installed from
   `XGrabKeyboard` and removed in `XUngrabKeyboard`. It translates
   `WM_KEYDOWN`/`WM_SYSKEYDOWN`/`WM_KEYUP`/`WM_SYSKEYUP` into X11
   `KeyPress`/`KeyRelease` with `xkey.keycode` set to the raw Windows
   virtual-key code, which is exactly what the existing
   `XLookupKeysym()`/`XSetInputFocus` path already expects.
2. `XGetInputFocus()` now returns the held grab window while grabbed,
   mirroring X11 grab semantics. **Without this the grab cancelled
   itself on the very first tick**: the renderer polls
   `dock_release_keyboard_if_left()`, saw `XGetInputFocus() == NULL`
   (nothing was focused, because the bars are `WS_EX_NOACTIVATE`), and
   immediately released the grab it had just taken.

#### The hook is deliberately *not* a full exclusive grab

A literal `XGrabKeyboard` equivalent would swallow **all** keyboard
input for the whole session. This hook intercepts only the nav keys —
`Left Right Up Down Return Escape Back Tab` — and returns `CallNextHookEx`
for everything else, so ordinary typing in other apps still works.
`Return` **is** swallowed, because the taskbar uses it to activate the
focused item rather than to insert a newline; `Tab` likewise, for
`DockNav` cycling. If a new dock action ever needs its own key, add it
to `x11_ll_kbd_wants()` in the same file.

#### Proving it, given `SendInput` is blocked here

`SendInput` returns 0 in this session, so the low-level callback cannot
be exercised with synthetic system input. What *is* provable, and was
proven: a posted `WM_KEYDOWN VK_DOWN` moves `g_focus_nav`
(17 → 18 → 19 → 20) and the renderer writes the corresponding
`6000 + nav` code into the shared `#.desktop/strip_history.txt`
(`6018` seen on the wire). **A real human pressing a real arrow key is
still the one unverified link.**

#### Unverified-by-design: ungrab lifecycle

Because `XGetInputFocus()` now reports the grab window rather than the
true foreground window, `dock_release_keyboard_if_left()` cannot detect
the user genuinely leaving the taskbar, and the hook is released only via
the normal `kh_ungrab_kbd()` disarm paths. That matches X11 exclusive-grab
semantics, and is why the hook only swallows nav keys — but it does mean
arrows stay captured while the taskbar considers itself engaged. Watch
for a stuck-grab report; the fix would be a foreground-window check that
is not currently implemented.