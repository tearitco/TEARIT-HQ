# Windows conversion status (house)

**Date:** 2026-08-22 (dir works; cli opens no keys; toys list no launch; next = db-hq/events-hq PE)  
**Dev model:** Linux primary; Win = compatibility (see `#.WIN-COMPAT-RULE.md`)  
**Livedesk now:** `8.21.GROK-win.md` (includes **Next to do**)  
**Related:** `windows-house-guide.md`, `khtpm-win-fix.txt` (history), `KHTPM-ARCH.txt`

**Next coding pass:** **db-hq** then **events-hq** (X11 shim + CreateProcess; skip Read). Rollback zip `44.xyz-00.17-win-2026-08-22-todos.7z` then this-pass zip. Entity rgb **runs**. HQ dir = Explorer. `EMERGENCY_CLOSE.ps1` at house root. Toys populate, launch not proven. Cli console has no keys.

---

## 1. What works on Windows

| Area | Status | Entry |
|------|--------|--------|
| **014.wsr-pal** | Working vertical (CHTPM + freeglut) | `button.ps1` |
| **$.crypts** autostart | Working: house-relative PDL, star-alias, CreateProcessW + `CREATE_NO_WINDOW` | `$.crypts/button.ps1` / Desktop `livedesk-start-button.exe` |
| **KHTPM entity windows** | **Working** — `tp_desktop_window_rgb.exe`. RMB menus. `EMERGENCY_CLOSE.ps1` kills them. Sprites still opaque squares. | after strip |
| **Livedesk taskbar** | **Working** — same parser+manager + `khtpm_strip_x11_win.c`. HQ **dir** ok. HQ **cli** opens, no keys. Toys list ok, launch not. Submenus use header HWND. | Desktop exe or `button.ps1 run` |
| **Assets** | Portraits/monsters from `Desktop\assets-4win`; `asset_path_win=` additive | see plan L2 |
| **Book-stack / bible** | Assets root → `Desktop\assets-4win\bible-ench.twins+ai]b2` | portable `run.ps1` |
| **01.muchi-pals** | `button.ps1` present (compile/run/kill/check); DIR → Explorer | project-local |
| **aomorai-editor** | Win build + launcher + keyboard + CONFIRM_START → board-viewer | `@.apps/aomorai-editor/button.ps1` |
| **piececraft-xyz** | Ported same Win stack as aomorai | `@.apps/piececraft-xyz/button.ps1` |
| **board-viewer** | `button.ps1 run-widget`; compose/3D seed fixes landed; live GL polish **next session** | `&.widgits/board-viewer` §4b |
| **101.mutaclsym** | `button.ps1` added (all-in-1 CHTPM stack; no widgets) | `101.mutaclsym*/button.ps1` |
| **event-ez launch wiring** | Menu → `open_event_ez.ps1` → `event-ez/button.ps1` (no bash) | **partial** — see §2 |

---

## 2. Events (ez) from KHTPM context — **NOT fully fixed**

### Symptom
RMB on a desk entity / rancher → **Events (ez)** does not reliably open a usable event-ez GL window on Windows.

### What *was* fixed (launch path only)
1. **Dispatch parity with Linux** — Linux `dispatch_action` always runs  
   `system("%s '%s' &", action, package_dir)`.  
   Win `khtpm_plat_win` now appends **package_dir** as argv[1] and prefers  
   `@.apps/MUCHI_RANCHER/ops/open_event_ez.ps1` when the action is `open_event_ez.sh`.
2. **No bash for Win menu path** — `open_event_ez.ps1` only starts  
   `&.widgits/event-ez/button.ps1 run` (sets `EZ_PKG_NAME` / `EZ_PKG_DIR`).
3. **`button.sh` corruption** — earlier agent patch corrupted  
   `&.widgits/event-ez/button.sh` (BOM + duplicated body).  
   File was **rewritten cleanly** (not restored from zip). Linux path is valid again.  
   Prefer editing carefully or using native `button.ps1` only on Win — do not re-break with multiline PowerShell regex patches.

### Why it is still incomplete (guidance for next agent)

This is **not** a missing menu string. The menu now points at the right Win launcher. The remaining failures sit **below** launch, in the CHTPM stack and package METHOD lines.

| Layer | Issue | Why it matters / how to think about it |
|-------|--------|----------------------------------------|
| **A. CHTPM stack is freeglut-on-Win, not drop-in X11** | event-ez needs muta-style `system/`: `chtpm_parser_pal`, `chtpm_rgb_render`, `gl_mirror`, `prisc+x`. Local `gl_mirror.c` trees under event-ez / older muta copies are still **X11/GLX** unless replaced by the **014.wsr freeglut** Win build (copy `gl_mirror.exe` or compile wsr’s `gl_mirror.c` with freeglut flags). | Without a Win freeglut `gl_mirror.exe` + freeglut DLLs on PATH (`C:\msys64\mingw64\bin`), launch can succeed and you still get **no GL window**. |
| **B. Session isolation** | Linux uses `ln -s` session → system/ops/pal. Win needs **junction/copy** of bins (`button.ps1` pattern from aomorai/wsr). Symlinks often need admin; incomplete session → parser starts, GL never gets a frame. | Mirror aomorai/wsr session setup; do not assume symlink parity. |
| **C. Frame race** | Must wait for non-empty `pieces/display/current_frame.txt` (or project’s frame file) before meaningful GL / rgb_render. | Early exit or black GL if race lost — same pitfall aomorai/wsr button already handles with a short poll loop. |
| **D. Absolute METHOD paths (ava/asa)** | Desk `meta.pdl` still has long `env EZ_… sh …/event-ez/button.sh run-widget` with `/home/no/...`. | Portable strip is incomplete for complex `env` lines. Rancher path was fixed to relative `open_event_ez.sh` + Win `.ps1`. **ava/asa Events (ez) still need relative METHOD rewrite** or core rewrite of env lines. |
| **E. Design rule** | Do **not** reimplement event-ez UI inside KHTPM. Fix launch + CHTPM Win stack only. | Matches `#.WIN-COMPAT-RULE.md`. |

### Why I think launch “looks fixed” but product is not done

- KHTPM only **spawns** a script. On Linux that script is bash + symlinks + X11 GLUT; on Win those three pieces are different shims.
- We closed the **argv / package_dir / no-bash** gap so the right process *starts*.
- We did **not** finish proving that event-ez’s own CHTPM binaries are Win freeglut-capable and that ava/asa METHOD lines are portable.
- aomorai-editor (this session) is the better reference for a full CHTPM Win vertical: `button.ps1` + `scripts/build.ps1` + surgical `#ifdef _WIN32` in keyboard/orchestrator/ops + copy wsr `gl_mirror.exe`.

### Recommended fix order (Events ez)
1. Confirm event-ez (or its muta donor) has Win freeglut `gl_mirror.exe` (copy from `014.wsr-pal*/system/gl_mirror.exe` if needed).  
2. Smoke **outside** KHTPM first:  
   ```powershell
   $env:EZ_PKG_NAME='m6_golddeity'
   $env:EZ_PKG_DIR='(house)\@.apps\MUCHI_RANCHER\entities\m6_golddeity\event_pkg'
   cd (house)\&.widgits\event-ez
   powershell -ExecutionPolicy Bypass -File .\button.ps1 run
   ```  
3. If that works, RMB Events (ez) should work for ranchers via `open_event_ez.ps1`.  
4. Rewrite ava/asa `meta.pdl` METHOD Events (ez) to relative form or a small `open_event_ez.ps1` call.  
5. Do not re-break `button.sh` with PowerShell multiline regex patches.

---

## 3. Architecture (current)

```
ENTITY — real, current
  DESIGN: tp_desktop_window_rgb.c (livedesk-taskbar monad, Linux X11)
  WIN:    tp_desktop_window_rgb.exe (same .c + x11_win shim). Built.

TASKBAR — real, current (2026-08-21)
  DESIGN: khtpm_strip_parser.c + khtpm_strip_layout.c + manager.c
  LINUX:  Xlib/Xft (build_khtpm_strip.sh)
  WIN:    khtpm_strip_x11_win.c shim (button.ps1 compile)
          Desktop: livedesk-start-button.exe

RETIRED 2026-08-11 (archived, do not reference as current)
  tp_taskbar.c, tp_taskbar_win.c (legacy taskbar)
  khtpm_taskbar_core.c, khtpm_taskbar_main.c, khtpm_taskbar_plat_win.c,
  khtpm_taskbar_plat_x11.c (an earlier abandoned taskbar-core-split
  attempt that never actually shipped either — was ALSO stale in
  button.ps1's own build steps since 2026-08-10, unrelated to the
  archiving)
```

Win taskbar: **top of screen** (OS bar owns bottom). Entity `khtpm_pos_clamp` leaves top pad.

---

## 4. aomorai-editor (Win button — frames + GL fixed)

### Empty terminal + empty GL (fixed 2026-08-06)
**Root causes (same house/wsr pitfalls):**
1. **`PRISC_PROJECT_ROOT` / `getcwd` absolute Unicode** (emoji house path) → MinGW ANSI `fopen` fails → no `current_frame.txt` → terminal blank, no `rgb_frame.raw` → black GL.
2. **Weak `win_spawn`** in aomorai’s older `chtpm_parser_pal.c`: no `.exe` suffix, `CreateProcessA` only → `prisc+x` never launched.

**Fixes (ported from 014.wsr pattern):**
- `resolve_root()` prefers `"."` when CWD has `pieces/` (parser, rgb_render, renderer, keyboard, ops).
- `win_spawn` = `.exe` resolve + `CreateProcessW` (wsr copy).
- `button.ps1` sets `PRISC_PROJECT_ROOT=.` and `WorkingDirectory=session` (not absolute session path in env).
- Waits for non-empty `current_frame.txt` then `rgb_frame.raw` before starting `gl_mirror`; opens a **Normal** terminal `renderer` window.

**Smoke (verified):** session `current_frame.txt` ~556+ bytes with menu; `rgb_frame.raw` 1966080; processes: parser, prisc+x, rgb_render, gl_mirror, renderer.

| Piece | Notes |
|-------|--------|
| `button.ps1` | compile / run / kill / check; session isolation; `PRISC_PROJECT_ROOT=.` |
| `scripts/build.ps1` | MinGW → `system\*.exe`; house-root `014.wsr*` for freeglut `gl_mirror.exe` |
| `chtpm_parser_pal.c` | Win prefer `.` + CreateProcessW win_spawn |
| `keyboard_input.c` | conio `_kbhit`/`_getch` arrows (TPMOS/wsr); Linux termios unchanged |
| `orchestrator.c` | CreateProcessA + `.exe` |
| `ops/win_posix_shim.h` | flock / kill / ftruncate / mkdir_p |
| `gl_mirror` | copied from 014.wsr freeglut PE |

### How to run
```powershell
cd (house)\@.apps\aomorai-editor
powershell -ExecutionPolicy Bypass -File .\button.ps1 compile
powershell -ExecutionPolicy Bypass -File .\button.ps1 run
# Expect: TERM renderer window + freeglut GL with menu chrome
# Ctrl+C in the keyboard console, or: .\button.ps1 kill
```

### Keyboard / arrows — **OK (user confirmed 2026-08-06)**
- Console path: 014.wsr / 1.TPMOS style `_kbhit` + `_getch`, arrows → 1000–1003.
- GL path: freeglut `gl_mirror` (wsr PE) → `KEY_PRESSED: N` into session `pieces/keyboard/history.txt` when GL has focus.
- **Do not re-open keyboard as a bug** unless a new regression appears.
- Still require a **native Windows console** for the term keyboard process (not mintty) if using console arrows.

### CONFIRM_START / world gen (fixed this session)
Earlier “Enter not seeding world” was **not** Enter missing — it was spawn/fopen:
1. **`ops/+x/*.+x` spawn** — `CreateProcessA` / PATHEXT cannot run `.+x` (err 123). Fix: wide `CreateProcessW` after `CopyFileW` to temp `.exe` (`win_run_pe` in `pc_menu_input.c`).
2. **Host write paths** — emoji absolute house paths break MinGW ANSI `fopen`. Prefer `PRISC_PROJECT_ROOT=.`, house-relative host roots (`@.apps/...`), and `host_fopen` (`MultiByteToWideChar` + `_wfopen`) for anything under the emoji house.
3. **Unix `popen` with `PRISC_PROJECT_ROOT=...`** — gated off on Win (breaks `cmd`).
4. **UTF-8 no BOM** for state files (`Write-Utf8NoBom` / C strip BOM) — BOM broke first-line `key=` reads.

Smoke: CONFIRM seed matched disk (`world_01/state.txt`, chunk z-layers under `pieces/system/chunks/`).

### Known aomorai follow-ups
- Local `system/gl_mirror.c` remains X11-leaning source; **runtime** uses wsr freeglut PE.
- Dual `rgb_render`/`renderer` (orch + button) is harmless; can dedupe later.
- Board widget live polish: **§4b** (next session).

---

## 4b. board-viewer 2D/3D — status for **next session**

### Goal
GL window shows real **2D emoji map** and/or **3D raymarch** of the focused host (aomorai / piececraft), not blank / pure-ASCII chrome.

### Already fixed (code landed 2026-08-06; smoke on disk, full live GL not closed)

| Item | Detail |
|------|--------|
| **Emoji path `host_fopen`** | `bv_compose_frame.c`: terrain legend, entities, phymoji, hero emoji, view/pdl markers — all host-side opens go through `host_fopen`. Without this, board loads but legend fails → ASCII `.`/`,`/`s` and `Selected: ?`. |
| **Smoke** | Focus `@.apps/aomorai-editor`: `Selected: grass`, UTF-8 emoji bytes in `view.txt`, `bv_render_3d` → `rgb_frame_3d_overlay.raw` ~1.2MB (non-zero pixels), re-compose writes MAP3D `0x01` marker. |
| **Launch seed order** | `button.ps1`: compose → **bv_render_3d** → re-compose so first GL frame can take 3D overlay (matches `pal/main_module.pal`). |
| **`.+x` on Wait** | `Invoke-HouseBin` always stages `.+x` → temp `.exe` (including `-Wait`); bare `& path.+x` does not run PE on Windows. |
| **Focus wiring** | `focused_project_root=@.apps/...` + `house_root.txt` (absolute, no BOM); `resolve_host_root` joins house + relative. |
| **3D default** | Hosts with `board_manifest.txt` default `render_mode=1`; if overlay missing, compose falls back to 2D emoji for that frame. |

### Not finished / next-session checklist
1. **Live smoke end-to-end:** aomorai Confirm & Start → board-viewer GL shows emoji or 3D (user eyes on freeglut window).
2. **If still blank:** check session `view.txt` / `current_frame.txt` / `rgb_frame.raw` / overlay; ensure `chtpm_rgb_render` is muta/wsr fork with MAP3D composite; freeglut DLL beside `gl_mirror.exe`.
3. **If 2D ASCII only:** rebuild `ops/+x/bv_compose_frame.+x` after `host_fopen` fix; confirm `terrain_legend.txt` opens under host.
4. **If 3D black but overlay size OK:** rgb path not compositing marker, or GL reading wrong session CWD (`PRISC_PROJECT_ROOT=.` + session WD).
5. **piececraft-xyz** same widget path — re-smoke after aomorai looks good.
6. **Do not** re-chase keyboard into gl_mirror unless user reports a new regression.

### How to run (standalone)
```powershell
cd (house)\&.widgits\board-viewer
powershell -ExecutionPolicy Bypass -File .\button.ps1 compile   # if bins missing
powershell -ExecutionPolicy Bypass -File .\button.ps1 run-widget ..\..\@.apps\aomorai-editor
```

### Key files
- `&.widgits/board-viewer/ops/bv_compose_frame.c` — 2D view + emoji + MAP3D marker  
- `&.widgits/board-viewer/ops/bv_render_3d.c` — sole 3D overlay writer (`rgb_frame_3d_overlay.raw`)  
- `&.widgits/board-viewer/button.ps1` — Win session + seed order  
- Host: `pieces/system/board_manifest.txt`, `terrain_legend.txt`, `chunks/chunk_0_0/chunk_0_0_z*.txt`  
- TPMOS ref for keys/GL only: `1.TPMOS_*/projects/wraith-alpha/ops/wraith_gl.c` (not required for board next step)

---

## 5. Next projects (queue)

| Project | Win button | Notes |
|---------|------------|--------|
| **aomorai-editor** | **done** (keyboard OK) | CONFIRM_START + board open; board GL polish §4b |
| **piececraft-xyz** | **done** (ported) | same stack; re-smoke board after §4b |
| **board-viewer** | **GL board fix** | muta rgb/MAP3D/atomic + short-path launch; aomorai/piececraft rgb PE updated |
| **101.mutaclsym** | **input launch fix** | short-path CWD + stage PE to TEMP; re-smoke live if still soft |
| **yahoo-app** | **button.ps1** | bank.chtpm; wsr PE donor; broker widget not yet Win |
| **my-chara-txt** | **button.ps1** | main.chtpm; session isolation; wsr PE donor |
| **my-biotech** | **button.ps1** | same shape |
| **my-lawyer** | **button.ps1** | same shape |
| **event-ez full GL** | partial | §2 — stack + METHOD, not menu wiring |
| **DIR → Explorer** | done for adam/asa/muchi-pal paths (ShellExecute) | recheck if desk DIR still wrong |
| 014.wsr terminal history-off | deferred | optional later |

---

## 6. Agent checklist (any new app)

- [ ] `button.ps1` with compile/run/kill/check (ASCII only)  
- [ ] Prefer `.exe`; name-only process kill (do **not** enumerate `Process.Path` — hangs)  
- [ ] `PRISC_PROJECT_ROOT=.` or session dir; CWD = project or session  
- [ ] No `/home/no` in new PDL  
- [ ] Shared design in core where KHTPM-related  
- [ ] CHTPM apps: copy freeglut `gl_mirror` from 014.wsr or compile wsr source with freeglut flags  
- [ ] Ops POSIX: thin `#ifdef` / small shim header — not a second design tree  
- [ ] **Emoji house paths:** never rely on MinGW ANSI `fopen` for absolute host paths — use relative roots or `host_fopen` (`_wfopen` UTF-8)  
- [ ] **`.+x` binaries:** stage to temp `.exe` before CreateProcess / Start-Process  
- [ ] State files: UTF-8 **no BOM**  
- [ ] Document open issues in this file if half-done  

END WIN-CONVERSION-STATUS.md
