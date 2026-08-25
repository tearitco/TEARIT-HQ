# Windows compatibility rule (house, 2026-08-06; taskbar truth 2026-08-21)

## Development model
- **Primary development is on Linux.** New features, menus, nav, packages,
  PDL, sprite pipelines land in the Linux code path first.
- **Windows must stay compatible**, not become a second product tree.
- **Do not rewrite features for Windows** when Linux changes.

## How Win compat is maintained
1. Put design/behavior in **shared C** (or plain files: PDL, sprite.csv, ledgers).
2. Isolate OS differences behind **thin shims** only:
   - window create / events / GL (X11+GLX vs Win32+WGL)
   - process spawn / kill
   - path separators, binary suffix (.+x vs .exe)
   - optional `asset_path_win=` next to Linux `asset_path=` (not a fork of logic)
3. Pattern of truth: `egg_window.c` style (`#ifdef _WIN32` vs POSIX), not
   a growing parallel `*_win.c` that reimplements menus/nav/registry.

## Agent / contributor checklist
- [ ] Feature works on Linux first
- [ ] No new absolute `/home/no/...` load-bearing paths (relative house paths)
- [ ] If Win needs a hook, add a **platform API**, not a copy of the feature
- [ ] button.ps1 only launches/builds; it does not own design logic
- [ ] Prefer one binary sourceset that compiles on both OS

## Explicit non-goals
- Maintaining two full KHTPM implementations (Linux file + Win rewrite)
- "Port by re-coding" every Linux change into Windows-only files
- Requiring Windows as the place features are invented


## Win path / process pitfalls (keep short)
- **Emoji house paths:** MinGW ANSI `fopen` fails on absolute Unicode. Prefer
  `PRISC_PROJECT_ROOT=.` + CWD, house-relative host roots (`@.apps/...`), or
  `host_fopen` (`MultiByteToWideChar` CP_UTF8 + `_wfopen`).
- **`. +x` PE:** not a Win extension — stage to temp `.exe` before spawn.
- **State files:** UTF-8 **no BOM** (BOM breaks first-line `key=` parsers).
- **Status / handoff:** `#.WIN-CONVERSION-STATUS.md` (this house root).

## Related
- `#.WIN-CONVERSION-STATUS.md` (house root)
- `16.A7_GROK_WALKOFF.md` under `yz.muchiverse/#.grok...J28-grok-user-sum/`
- livedesk-win-fix-lvl2.txt §15–16
- khtpm-win-fix.txt ARCH RULE
- windows-house-guide.md
- `8.21.GROK-win.md` (**current** livedesk Win)
- !.linux-absolute-FIXME-a6.txt (missing here; relative PDL is the rule)

## KHTPM entity layout — UPDATED 2026-08-11, this section's original plan never shipped
- The `khtpm_core.c`/`khtpm_plat_win.c`/`khtpm_plat_x11.c` shared-core split
  described below was built, but the X11 half never actually replaced
  `tp_desktop_window.c` on Linux — it sat unreferenced by the real build
  for the rest of this house's life and was confirmed dead + archived
  2026-08-11 (see `KHTPM-ARCH.txt`'s own updated note for the full
  retrospective).
- **2026-08-22:** live entity is `tp_desktop_window_rgb.c` compiled to
  `.exe` with `khtpm_strip_x11_win.c`. Do not launch tile-picker
  `tp_desktop_window.exe`. Close pals with house-root `EMERGENCY_CLOSE.ps1`
  (the `.sh` is Linux-only). Next: db-hq/events-hq PE, not a second UI.
- Archived 2026-08-11: `khtpm_core.c` / `khtpm_plat_win.c` / `khtpm_plat_x11.c`.
- See `8.21.GROK-win.md` and `KHTPM-ARCH.txt`.

## Taskbar — UPDATED 2026-08-11, legacy retired
Legacy `tp_taskbar.c`/`tp_taskbar_win.c` (and an earlier abandoned
`khtpm_taskbar_core.c`/`khtpm_taskbar_plat_win.c`/`khtpm_taskbar_plat_x11.c`
split that never shipped either) are fully retired — archived to
`*.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip`, originals
deleted. The real, current taskbar is `khtpm_strip_parser.c` (Linux, Xlib
+ real declarative-layout parser) + `khtpm_taskbar_manager.c`/
`khtpm_taskbar_manager_main.c` (shared business logic, no Xlib) under
`*.monads/*.livedesk-taskbar/ops/`.

**Windows 2026-08-21:** same `khtpm_strip_parser.c` + manager, compiled
with `khtpm_strip_x11_win.c` (Xlib subset). Not a second strip. Nav,
`[>] n. label`, chtpm layouts shared. See `8.21.GROK-win.md`.

The old `khtpm_taskbar_core.c` / `khtpm_taskbar_plat_win.c` split never
shipped and is archived. Do not revive it.
