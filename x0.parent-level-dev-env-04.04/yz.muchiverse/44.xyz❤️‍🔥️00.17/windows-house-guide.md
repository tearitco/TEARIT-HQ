# Windows house guide — porting Linux house projects

**Purpose:** How future agents onboard a Linux-native house project (e.g. `014.wsr-pal`) so it runs on Windows **without breaking Linux**.  
**Reference project (first successful vertical):** `014.wsr-pal*`  
**Canonical runtime:** `1.TPMOS_c_+rmmp.0103.0001`  
**House testing doctrine:** `_.0.aigent-testing-k3.txt`  
**Date:** 2026-08-06  

---

## 0. Non-negotiables

1. **Linux stays canonical.** Prefer `#ifdef _WIN32` / `#ifndef _WIN32` surgical wraps. Do **not** refactor POSIX paths “to clean them up.”
2. **TPMOS bible §9** (cross-platform parity wraps) is the process/spawn/path doctrine.
3. **K3 before code edits** when debugging display/input: capture frames + receipts, write a root `FRAME_REPORT_*.txt`.
4. **One project at a time.** Pattern proven in `014.wsr-pal` first; copy the pattern outward.

---

## 1. Entry points (button pair)

| Platform | File | Notes |
|----------|------|--------|
| Linux/macOS | `button.sh` | Unchanged |
| Windows | `button.ps1` | Same action names (`compile`, `run`, `kill`, `check`, …) |

### `button.ps1` lessons from 014.wsr

- Keep **ASCII only** in PowerShell (em-dashes / smart quotes break `switch` parse).
- Prefer `if / elseif` over large `switch` blocks if the file grows.
- **Do not** enumerate `Get-Process | … $_.Path` for kill — accessing `.Path` for every process can **hang for minutes**. Match **ProcessName only** + `taskkill /F /IM name.exe`.
- Print progress steps (`[1/4]…`) so hang points are visible.
- Set `PRISC_PROJECT_ROOT=.` and `SKIP_ORCH_COMPILE=1` on `run` (see §3).
- Put MSYS2 MinGW on PATH: `C:\msys64\mingw64\bin` (gcc + freeglut DLLs).

### Toolchain (same as TPMOS `install_deps.ps1`)

```text
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut
# optional: freetype if emoji_gen_atlas is required
```

---

## 2. Scripts to mirror (`.sh` → `.ps1`)

For each project, add Windows twins only where `button.ps1` / C code shells out:

| Linux | Windows |
|-------|---------|
| `scripts/build.sh` | `scripts/build.ps1` |
| `scripts/ensure_entities.sh` | `scripts/ensure_entities.ps1` |
| `scripts/tick_all.sh` | `scripts/tick_all.ps1` |
| `scripts/active_corp.sh` | `scripts/active_corp.ps1` |
| `pieces/os/kill_all.sh` | `pieces/os/kill_all.ps1` (name-only kill) |

**Build flags (GL):**

```text
-LC:/msys64/mingw64/lib -lopengl32 -lglu32 -lfreeglut -lwinmm -lgdi32 -luser32
```

**Binary naming:** MinGW can emit `name`, `name.exe`, or keep `foo.+x`. Prefer checking both `path` and `path.exe`. Launch `.+x` via `cmd /c "path\to\bin.+x"` if needed (TPMOS WINDOWS-RUN-GUIDE).

---

## 3. Path / CWD doctrine (emoji house folders)

House trees often contain emoji/Unicode directory names. **ANSI `fopen` / MinGW `opendir` / `CreateProcessA` break on absolute Unicode paths.**

**Rules that worked:**

1. Launch children with **project directory as CWD**.
2. Set **`PRISC_PROJECT_ROOT=.`** (relative), not absolute.
3. Prefer relative paths in C: `pieces/...`, `ops/+x/...`, `system/prisc+x`.
4. If `access("pieces", F_OK)==0`, force root `"."` in `resolve_root()`.
5. For process spawn: **`CreateProcessW`** (UTF-16) + resolve `foo` → `foo.exe`.
6. Strip `project_root` prefix so spawn uses `system\prisc+x.exe`, not a long absolute Unicode path.

---

## 4. Process management parity

| Linux | Windows |
|-------|---------|
| `fork` + `exec` | `CreateProcess` / `_spawn` |
| `waitpid` loop | `Sleep(100)` poll loop (detached children return immediately) |
| process groups / signals | Tracked PIDs + `taskkill` / `TerminateProcess` |
| `setenv` | `_putenv_s` |

Orchestrator: skip full recompile when `SKIP_ORCH_COMPILE=1` so every `run` does not stall 30–60s.

---

## 5. Input (keyboard)

| Linux | Windows |
|-------|---------|
| termios raw + `read` | `conio` `_kbhit` / `_getch` (TPMOS keyboard pattern) |
| Map Enter → 10 | Map CR (13) → LF (10) for menu parity |
| Prefer real console | **Windows Terminal / cmd** — mintty/`_kbhit` is unreliable (TPMOS WINDOWS-RUN-GUIDE) |

**K3 key injection format (parser):**

```text
[YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <code>
```

Example Enter: `KEY_PRESSED: 13`  
wsr also uses bare decimals in `pieces/apps/player_app/history.txt` for prisc.

---

## 6. GL / freeglut

- Only `gl_mirror` (or project GL file) should call GL (house architecture).
- Define `GLUT_DISABLE_ATEXIT_HACK` before including freeglut on Windows.
- Link freeglut as above; ensure `freeglut.dll` on PATH.
- **Empty GL window with good RGB data:** fixed-function path needs:
  - `glColor4f(1,1,1,1)`
  - `glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE)`  
  (`GL_MODULATE` × black current color → solid black window).
- Watch **downstream** pulse `pieces/display/rgb_frame_changed.txt` (not only upstream `frame_changed.txt`).
- **`write_file_atomic`:** Windows `rename(tmp, dest)` **fails if dest exists**. `remove(dest)` then `rename`, or copy fallback. Without this, `rgb_frame.raw` freezes after first write.

---

## 7. Directory listing (`opendir` trap)

On some OneDrive / Unicode CWDs, **MinGW `opendir`/`readdir` returns zero entries** even when the folder is full.

**Fix:** on Windows use `_findfirst` / `_findnext` (or `FindFirstFile`) for `corp_*` scans, etc. Keep `opendir` on Linux.

---

## 8. Terminal renderer vs GL (frame “tear” / fast scroll)

### What TPMOS does

From `1.TPMOS…/pieces/display/renderer.c` and `windows_renderer.c`:

| Mode | Trigger | Terminal behavior |
|------|---------|-------------------|
| **History ON** (default if `pieces/display/state.txt` missing or not `off`) | `renderer_pulse.txt` size grows | Print 5 blank lines + `--- FRAME UPDATE at … ---` + full frame (scrollback history) |
| **History OFF** (`state.txt` contains `off`) | same | **In-place redraw:** Linux `\033[H\033[J`; Windows **Console API** clear (`FillConsoleOutputCharacter` + cursor home) |

Audit history still goes to files (`session_frame_history.txt` / ledger), not only the live tty.

wsr-pal `system/renderer.c` matches the same history-on / clear-off split, and adds skip-if-unchanged content compare.

### Why Windows can “scroll really fast” / look like tear

1. **History mode is ON by default** → every pulse reprints a full multi-line frame into scrollback. Rapid pulses → rapid scroll.
2. **GL is independent** (`gl_mirror` + `rgb_frame.raw`) → GL can look fine while the terminal is thrashing.
3. **Windows CRLF hazard:** wsr `write_crlf()` emits `\r` before every `\n`. If `current_frame.txt` already has `\r\n`, output becomes `\r\r\n` and consoles can “tear” or jump oddly. Linux frames are usually LF-only, so the bug is Windows-skewed.
4. **Linux “no tear”:** same history semantics, but LF-only + terminal behavior differs; also fewer double-CR artifacts.

### How to fix (when you care) — do not invent, copy TPMOS

**Option A — static terminal UI (closest to “no tear,” still history on disk):**

```text
# pieces/display/state.txt
off
```

Uses clear-and-redraw. On Windows, prefer **Console API clear** (TPMOS `windows_renderer.c`), not only ANSI (Windows Terminal usually handles ANSI, but Console API is the house precedent).

**Option B — keep history mode, stop thrashing:**

- Ensure skip-if-unchanged stays (wsr already has it).
- Stop growing `renderer_pulse.txt` unless compose content actually changed (parser side).
- Normalize line endings in the terminal renderer before `write_crlf` (treat `\r\n` as one newline).

**Option C — dual renderer like TPMOS:**

- Linux: `renderer.c`
- Windows: `windows_renderer.c` (clear-screen API + history mode)

**GL path:** leave alone for this issue; K3 gate is RGB vs GL checksum equality.

### Applied fix (014.wsr `system/renderer.c`, 2026-08-06) — real TPMOS Windows path

**Root cause of “fast scroll / tear”:** wsr used Linux-style **history scroll**
(print full frame + blanks into scrollback) from a **separate** `renderer`
process. On Windows that races raw-mode keyboard + large multi-page frames.

**What Windows TPMOS actually does** (`pieces/chtpm/plugins/orchestrator.c`
`render_thread_func` under `_WIN32`):

```c
// NOT spawn renderer.+x for the main console path —
// inline poll of renderer_pulse.txt, then:
if (is_history_on()) { printf("\n\n\n\n\n--- FRAME UPDATE ---\n"); }
else { system("cls"); }
// then printf current_frame.txt
```

`windows_renderer.c` is the dedicated Windows binary (compiled **to**
`renderer.+x`) and documents **Console API clear**. File history still
goes to `pieces/debug/frames/session_frame_history.txt`.

**wsr-pal Windows policy (matches that intent):**

| Concern | Windows | Linux |
|---------|---------|--------|
| Live console | **Always clear + redraw** (Console API) | History ON scroll / OFF ANSI clear |
| Frame history | Always append `pieces/display/frame_history.txt` | Same |
| CRLF | Safe `write_crlf` (no double CR) | Same helper |
| Skip duplicate | CR-normalized content compare | Same |

So: **history is the file** (K3 / audit); **live Windows terminal is cls-style**, not a multi-frame scroll race.

---

## 9. K3 debug checklist (frame history + RGB receipts)

Use on every display/input debug cycle **before** more C edits.

### Evidence paths (wsr-pal)

| Artifact | Role |
|----------|------|
| `pieces/display/current_frame.txt` | Live text frame |
| `pieces/display/frame_history.txt` | Terminal renderer audit log |
| `pieces/display/rgb_frame.receipt.txt` | Upstream RGB writer (`checksum_fnv1a64`, w/h) |
| `pieces/display/gl_display.receipt.txt` | Downstream GL upload (`loaded_rgba_checksum_fnv1a64`) |
| `pieces/display/rgb_frame.raw` | Pixel buffer |
| `pieces/display/rgb_frame_changed.txt` | Downstream pulse for GL |
| `pieces/keyboard/history.txt` | `KEY_PRESSED` injection |
| `debug.txt` | Parser spawn / module log |

### Capture script (014.wsr)

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\k3_frame_capture.ps1 -Topic smoke -InjectEnter
```

Writes root:

- `FRAME_REPORT_<yyyyMMdd-HHmm>_<topic>.txt`
- `FRAME_HISTORY_RAW_<yyyyMMdd-HHmm>.txt`

### Empty-mirror rule

| RGB receipt | GL receipt | Raw white pixels | Conclusion |
|-------------|------------|------------------|------------|
| present | checksum **matches** RGB | > 0 | Buffer good; fix GL **draw** / window |
| present | checksum **differs** | any | Stale GL load / pulse / rename race |
| present | match | all black | Glyph load / render empty |

---

## 10. Port checklist (copy per project)

- [ ] `button.ps1` (actions parity with `button.sh`; fast kill; progress logs)
- [ ] `scripts/build.ps1` + critical helper `.ps1` twins
- [ ] Orchestrator: Win spawn, `SKIP_ORCH_COMPILE`, relative env root
- [ ] `keyboard_input`: conio path; Enter 13→10
- [ ] `renderer`: usleep/Sleep; optional Windows clear for history-off
- [ ] `gl_mirror`: freeglut link, GL_REPLACE, relative root, raw-size poll
- [ ] `chtpm_rgb_render`: atomic write with `remove`+`rename`; relative root
- [ ] `chtpm_parser_pal` / `prisc+x`: CreateProcessW / popen aliases / temp paths
- [ ] Ops that shell out: dual path or pure C (`_findfirst` not `opendir` for listings)
- [ ] `ops/connect_op`: `system(curl…)` on Windows if no fork
- [ ] K3 capture once: stack live + frame + RGB/GL checksum PASS
- [ ] Project-local `WIN-PORT-NOTES.txt` (optional short log)

---

## 11. What 014.wsr already proved

| Area | Status |
|------|--------|
| Compile MinGW + freeglut | OK |
| Stack launch (7 procs) | OK |
| Frame text (WSR menu) | OK |
| RGB pixels + debug PNG | OK |
| RGB/GL checksum match | OK |
| Key inject K3 format | OK (Enter expands Actions) |
| `button.ps1` parse + non-hang kill | OK |
| Terminal history scroll UX | Known issue (this doc §8); GL primary play path |

---

## 12. Do not do

- Do not rewrite shared house architecture for Windows-only.
- Do not use `Get-Process … $_.Path` filters for kill.
- Do not set absolute `PRISC_PROJECT_ROOT` through emoji paths.
- Do not claim “GL empty” without comparing RGB/GL receipts and raw pixel stats.
- Do not recompile the whole tree on every `run` by default.

---

*End of windows-house-guide.md — first written from the 014.wsr Windows port session.*

---

## Dev model addendum (2026-08-06)

**Most development happens on Linux; Windows is compatibility, not a rewrite target.**

See **WIN-COMPAT-RULE.md**: shared design logic once; Win/X11 only as thin shims
(egg_window style). Do not reimplement menus/nav/registry in parallel `*_win.c`
files when Linux changes.
