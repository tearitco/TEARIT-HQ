# MAC_COMPAT.md — handoff brief for the macOS leg of cross-platform compatibility

**Written:** 2026-08-22 by the Linux-side opencode agent (ox-alpha), at the human's direction.
**You are:** a Mac-native agent about to work on this tree. Read this whole file first, then the
orientation docs it points to. Your job is the **third leg** of the house's compatibility effort:
Linux (canonical, always current) → Windows (done, taskbar + entity windows live) → **macOS (you)**.

**Doctrine first — same as the Windows leg:**
1. **Linux is canonical.** You write shims/`#ifdef`s and platform notes. You do NOT fork the
   product, rename files per-OS, or rewrite PDL contents to "mac spelling". There IS no mac
   spelling needed — see §2.
2. **No symlinks anywhere, ever** (house rule, `!.HOUSE_STDS.md` §A.2 + pitfall 7b — they break
   on Windows; don't reintroduce them for Mac convenience either).
3. Real code over stale docs (`!.HOUSE_STDS.md` §A.4): verify on your machine, then write back.

---

## 0. Orientation reading order (~15 min)

| Order | File | Why |
|---|---|---|
| 1 | `#.#.✅️.cal-user-sum/1.^V-hq/INDEX.md` (yz.muchiverse root, one level up) | routing index incl. its 🌐 Cross-Platform section |
| 2 | `!.HOUSE_STDS.md` (this dir) | house mechanics: sessions, digit-dispatch, pitfalls §F |
| 3 | `../8.21.GROK-win.md` (yz.muchiverse root) | how the WINDOWS leg was done — your closest template: shim strategy, what ran, pitfalls |
| 4 | `$.crypts/win-trip.sh` header comment | the round-trip mechanics you must NOT break |
| 5 | `sim-smell-fix.md` §"📦 MANAGER HANDOFF" | current state of the world + behavioral gotchas |

## 1. What this house actually is (60-second version)

A desktop of small C apps ("monads"/projects) sharing one engine: `.pal` scripts interpreted by
`system/prisc+x` (the VM), UI rendered by `chttpm_parser_pal` (menus/buttons from `.pdl` piece
files) into `pieces/display/current_frame.txt`, rasterized by `chtpm_rgb_render` (FreeType) into
`rgb_frame.raw`, displayed by X11 mirrors (`gl_mirror` / `x11_mirror`) or ASCII renderers.
Identity/login data lives in `0.user-pal👤️`; user home trees at `<house>/xyzfs/users/<uuid>/`.
The livedesk (`*.monads/*.livedesk-taskbar`) is the desktop itself: taskbar strip + entity windows.

Every project launches via its own `button.sh` (`run|compile|kill|...`). Sessions are
copy-in/persist-out dirs under `pieces/sessions/<ts>-<pid>/`.

## 2. macOS-specific facts that change YOUR job vs. the Windows job

This is why your leg should be EASIER than Grok's Windows leg:

- **APFS/HFS+ happily store `*` in filenames.** The entire star-name problem (`*.monads` →
  `_.monads`, resolve-time aliasing, win-trip.sh) is **Windows-only**. On a real Mac:
  - Do NOT run `win-trip.sh to-win`. Copy the tree as-is.
  - If your checkout arrives with `_.monads` names anyway (it was routed through Windows),
    restore with `$.crypts/win-trip.sh to-linux` FIRST — that also does the chmod below.
- **Permissions come back locked from any Windows/exFAT hop.** After any copy-in:
  `chmod -R 777 <house>` (matches the house's normal Linux perms; same thing win-trip.sh does).
- **APFS default is case-INSENSITIVE.** Before anything else, check for case-only collisions:
  `find . -name "*[a-z]*" | sort -f | uniq -di -f0` style sweep — any two paths differing only in
  case WILL clobber each other on the Mac. Known suspects: `TSC_ELO` vs lowercase neighbors,
  `README.md`/`readme.md` pairs. Report collisions; fix by renaming the LATER duplicate only.
- **clang ≠ gcc**: clang is stricter (more warnings-as-errors in some spots, different
  `-Wno-*` flags accepted). Expect build fixes like unused-result/stringop-truncation flags being
  rejected or unneeded. Don't dumb down warning levels globally — fix real issues, keep flags
  per-file where the build scripts have them.
- **FreeType path differs**: Homebrew = `/opt/homebrew/include/freetype2` (Apple Silicon) or
  `/usr/local/include/freetype2` (Intel). Build scripts hardcode `-I/usr/include/freetype2`.
  Preferred fix: detect via `pkg-config --cflags --libs freetype2` in build scripts, guarded so
  Linux behavior is unchanged.
- **XQuartz**: every Xlib/Xft binary (taskbar parser, entity window, x11_mirror, gl_mirror)
  runs under XQuartz — functional but ugly and slow. For THIS leg that's acceptable: goal is
  "runs correctly", native Aqua rendering is explicitly OUT of scope (same call the Windows leg
  made before getting budget for shims).
- **`setsid` DOES NOT EXIST on macOS** (no such binary/libC function). It's used in button.sh
  cleanup traps and several harness scenarios. Portable replacement: launch background children
  with `nohup ... &` + `disown`, or a tiny C helper compiled on the fly, or `perl -e 'setsid; exec @ARGV' -- ...`.
  Grep first: `grep -rn "setsid" --include="*.sh" .`
- **`ldconfig` doesn't exist**; `pkill` exists but behaves slightly differently; `/proc` does not
  exist (any C code reading /proc needs guarding). Grep for all three.
- **Emoji/color fonts**: Linux renders color emoji via FreeType + Noto Color Emoji. macOS has
  Apple Color Emoji (CBDT/sbix) — FreeType can load it, but font PATHS differ. Expect the
  emoji atlas/xtract ops to need a font-path config knob (env var escape hatch, same pattern the
  house already uses: see HOUSE_ROOT / USERPAL_LOGIN_ROOT).
- **Absolute paths**: known leftover `/home/no/...` METHOD lines in some PDLs break everywhere,
  not just Mac. Grep + fix them relative while you're in there (see ../8.21.GROK-win.md's
  "Path hygiene" section).

## 3. Walkthrough — do these phases IN ORDER, stop after each and record status

### Phase A — land the tree correctly
1. Copy/clone the house onto the Mac.
2. `chmod -R 777 <house>`.
3. Case-collision sweep (see §2). Fix/report findings.
4. Confirm `*.monads` etc. survived intact: `ls *.monads` must show star-named dirs.

### Phase B — toolchain
1. Xcode Command Line Tools (`xcode-select --install`).
2. Homebrew: `brew install freetype freeglut pkg-config` + XQuartz (XQuartz from
   xquartz.org, not brew cask, for full Xft support).
3. Record exact versions in MAC-CONVERSION-STATUS.md.

### Phase C — compile sweep (the big one)
1. Run the existing runner: `bash "$.crypts/compile-runner.sh"` (from house root).
   Baseline on Linux: **44/44 PASS**. Every non-PASS is a real finding.
2. Fix failures one project at a time, preferring edits INSIDE build scripts (pkg-config
   detection, clang flag adjustments) over C changes; only touch `.c` files when something is
   genuinely non-portable (e.g. `/proc`, `#include <X11/...>` guards are fine).
3. Keep a table in MAC-CONVERSION-STATUS.md: script | status | fix applied | date.
4. Do NOT mark a script fixed until it passes twice in a row.

### Phase D — livedesk smoke test
1. `cd $.crypts && bash button.sh r` (or the autostart action directly).
2. Expect windows under XQuartz; verify taskbar paints, pals spawn, kill works cleanly
   (no stray processes after `button.sh k`).
3. Log every deviation — don't fix UI polish, log it.

### Phase E — harness truth
1. `bash "$.crypts/harness-runner.sh" --list` then run a few key ones (login-signup, qtc).
2. setsid replacements will likely be needed here first (§2).
3. Record pass/fail vs the Linux baseline documented in `sim-smell-fix.md`.

### Phase F — write-back (REQUIRED, not optional)
1. Create/update `MAC-CONVERSION-STATUS.md` next to this file with the tables from C–E.
2. Update the 🌐 table in `#.#.✅️.cal-user-sum/1.^V-hq/INDEX.md`: flip the macOS row's Status +
   point it at your status doc.
3. Append Mac-specific pitfalls to §4 of this doc (below) — the next agent reads this file first.

## 4. Pitfalls that bit previous legs (append yours here)

- (Windows) NTFS can't store `*` names → aliasing machinery + win-trip.sh exist because of it;
  Mac doesn't need any of it — don't port those shims.
- (Windows) PowerShell `$` expansion, BOM-on-write, console-flash on spawn, Toolhelp kills —
  all irrelevant to you; listed so you know why some `#ifdef _WIN32` code looks strange.
- (Linux→any) Sessions are copy-in/persist-out: mid-session, real-root state does NOT change;
  rebuilds don't reach running sessions. See sim-smell-fix.md MANAGER HANDOFF items 1–7.
- (macOS) **CRLF in autostart.pdl after win-trips**: canonical C parsers strcspn-strip `\r\n`
  but bash `read` keeps the `\r` → entities launched from a shell parser died instantly
  (trailing-`\r` pal-path arg). mac-start-livedesk.command strips it per line; found live
  2026-08-22. Any new bash PDL parser needs the same one-liner.
- (macOS) **Stale Linux ELF binaries**: shipped `+.x` files are ELF; "build if missing" checks
  skip them silently → purge non-Mach-O binaries BEFORE running builders, or you'll test Linux
  code and think it's yours. `for f in +x/*.+x; do file "$f" | grep -q Mach-O || rm -f "$f"; done`
- (macOS) **brew PATH + pkg-config universes**: brew lives at `/usr/local/bin` (Intel), absent
  from non-login shells; brew's pkg-config doesn't see XQuartz's `.pc` files. Guard:
  `PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig` + `-I/opt/X11/include
  -L/opt/X11/lib` placed BEFORE raw `-lX11` on link lines (clang has no default /opt/X11 search).
- (macOS) **emoji helpers ship as prebuilt ELF** from wsr-pal — on Darwin build them from source
  (`emoji_gen_atlas.c` needs freetype + `-I<wsr>/ops` for its local stb headers).
- (macOS) **rg is not installed** on this Mac — scripts assuming rg silently no-op; use grep.
- (any) build_khtpm_strip.sh re-derives `$SHARED` from `$(dirname "$0")/../..` AFTER an earlier
  `cd $(dirname $0)`: invoking via relative path from house root double-counts the prefix and
  fails. Run from inside the ops dir or with absolute paths (not Mac-specific).
- (macOS) **Duplicate taskbar stacks (pidfile race)**: `ensure_taskbar_running()` checks
  `livedesk_taskbar.pid` then falls back to a `/proc` scan — no `/proc` on macOS, and a stale
  pidfile means every entity spawns its OWN strip (six identical stacks live, found 2026-08-22).
  Launcher must rm the stale pidfile AND wait for the fresh one to be live before launching
  entities (mac-start-livedesk.command does both).
- (macOS) **Stale-CLOSE poison**: `button.sh quit` relays a CLOSE line into each REGISTERED
  entity's `<pal>/interact_relay.txt`; if that entity was already dead/unregistered the line
  just sits there — the next entity to bind that package reads it on its first main-loop poll
  and exits(0) with ZERO output ("silent death", one different victim per launch). A launcher
  must truncate all `interact_relay.txt` under the pals tree at startup.
- (macOS) **bash `while read` drops the final PDL row** when autostart.pdl lacks a trailing
  newline (live: skipped the last entity, ava). Use `while IFS= read -r line || [ -n "$line" ]`.
- (macOS) **Header wider than the display**: strlen*8 cell widths overflow small screens;
  fixed runtime-only in khtpm_strip_parser.c (`__APPLE__`): window clamped to screen,
  `present_rgb_fit()` downscales the buffer at present-time (nearest neighbor), clicks are
  inverse-mapped to natural coordinates before hit-testing; popup anchors were already
  proportional. Entity tiles get a `_WIN32`-mirror work-area clamp in tp_desktop_window_rgb.c.
- (macOS) **No `setsid`**: every manager/HQ spawn site used `setsid nohup ...`; macOS has no
  setsid → HQ menu rows (dir/cli/db/events-hq), open-hai/chat-hai, and reset/office RESPAWN all
  silently no-oped (close still worked — it goes through the file relay). Fix pattern: `KTB_SETSID`
  macro (`""` on `__APPLE__`, `"setsid "` elsewhere) spliced into each snprintf; plus
  `ktb_portable_darwin()` translating xdg-open→open in run_shortcut. Same translation done at
  runtime in tp_desktop_window_rgb.c's action runner for entity METHOD actions. Found live
  2026-08-23.
- (macOS) **`/proc/self/exe` doesn't exist** → any self-path readlink returns -1 and callers
  bail ("cannot resolve own path", live: khtpm_show_choices). Shared shim header
  `&.widgits/tile-picker/ops/self_exe.h`: `self_exe_readlink()` = `_NSGetExecutablePath`+realpath
  on Darwin, plain readlink elsewhere; drop-in for the readlink idiom. NOTE: keep
  `#include <stdlib.h>` INSIDE it — realpath's home — or standalone consumers fail to compile.
- (macOS) **`_POSIX_C_SOURCE 199309L` hides `snprintf`**: Apple's libc only declares
  snprintf/vsnprintf under POSIX ≥2001; old pal sources pinning 199309L fail under clang
  ("undeclared library function"). Bump those defines to 200809L (keeps CLOCK_MONOTONIC).
  Live: khtpm_choice_picker.c line 1.
- (macOS) **prisc exec resolution was missing from older trees**: 19.00's prisc+x.c resolves a
  relative `exec ./target` against g_pal_dir (legacy-shared-fix.md §3.10); +18.0G's copy predated
  that fix → book-stack's event.pal ran `./dispatch.sh` against the CALLER'S cwd → silent no-op.
  Backported both halves (g_pal_dir resolve in main + exec_target in ALL THREE cmd-build
  branches — first backport attempt missed the 1-arg/bare branches, rc=32512=127 gave it away).
- (macOS) **fork'd children inherit stdout pipes**: khtpm_show_choices forks the picker then its
  parent exits, but callers do `PICKED=$(show_choices ...)` — bash waits for EOF on the pipe,
  which the long-lived picker now holds → dispatch.sh hung forever after a pick unless the
  picker happened to be dead already. Fix: child dup2's /dev/null onto stdout before execl
  (stderr kept). Any fork-and-exit supervisor needs the same one-liner.
- (macOS/XQuartz) **Popups spawn off-screen from saved Linux coords**: desktop_pos.txt carried
  x=2320 on a 1680px display → choice picker mapped fully off-screen (unclickable, invisible).
  Two layers: tp_desktop_window_rgb.c now write_pos()s the CLAMPED position at startup, and
  khtpm_choice_picker.c clamps g_win_x/y against DisplayWidth/Height itself (defensive, covers
  stale files / any caller).
- (macOS) **Font fallback chains must try the KNOWN-CJK name FIRST**: fontconfig on this Mac
  silently substitutes missing fonts (live: both "Noto Sans CJK SC" and "DejaVu Sans" resolved
  to ADTNumeric.ttc, zero CJK coverage) — and that substitution makes XftFontOpenName SUCCEED,
  so any "Noto first, Heiti second" chain never reaches the real font. Prove coverage, not
  open-success: FcCharSetHasChar(charset, 0x4E2D). Verified 2026-08-23: only Heiti SC
  (/System/Library/Fonts/STHeiti Light.ttc) covers Han here; khtpm_strip_parser.c's strip_font()
  now opens it first on __APPLE__. Pixel-truth check: xwd of the live header + raw decode shows
  glyph pixels in the datetime zone.
- (XQuartz) **_NET_WM_STATE_ABOVE is IGNORED** (empirical 2026-08-23, /tmp/ztest-style two-
  window probe: keep-above window still stacked below a later-mapped peer). So the Windows-leg
  HWND_TOPMOST intent (avatar_window.c:953 et al) has NO EWMH equivalent here. Remaining options:
  periodic XRaiseWindow from each GL loop (top of MANAGED stack only - never above OR
  tiles/taskbar), or OR-flip (true topmost, loses WM framing). Decision pending user.
- (bash portability) **setsid shim for scripts**: same KTB_SETSID idea as the C side, as a
  `SETSID="setsid"; [ "$(uname)" = "Darwin" ] && SETSID=""` header + unquoted `$SETSID` at each
  spawn site. Applied 2026-08-23 to livedesk-taskbar's button_taskbar_stats/settings +
  run_khtpm_strip.sh, $.crypts/scrypts/openall/run.sh, install-xyzos kpi4/kpi5 scenarios, and
  mutaclsym test-harn-same demo_module_split_smoke.sh.

## 5. Do NOT do

- Do not rewrite PDLs, rename `*.` trees "for mac", or port the `_win.c` shim pattern — wrong OS.
- Do not introduce symlinks.
- Do not resurrect archived taskbar code (see ../8.21.GROK-win.md "Do not resurrect").
- Do not change Linux build scripts' behavior on Linux (guard every edit: `uname`/pkg-config).
- Do not chase native-Aqua rendering; XQuartz-functional is the bar for this leg.

---

*House doctrine summary: Linux canonical · other OSes are shims · no symlinks · no per-OS renames ·
verify everything yourself, write back what you learned.*
