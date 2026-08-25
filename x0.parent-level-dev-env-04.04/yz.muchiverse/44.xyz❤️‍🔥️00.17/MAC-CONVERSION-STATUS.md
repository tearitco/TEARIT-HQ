# MAC-CONVERSION-STATUS.md — macOS leg status (write-back doc)

**Started:** 2026-08-22 by opencode agent (ox-alpha), per `MAC_COMPAT.md`.
**Machine:** Intel Mac (x86_64), macOS 15.1.1 (24B91). Tree landed intact from Linux.

## Phase A — tree landing: ✅ DONE

- Star-names survived the copy (`ls *.monads` shows star dirs) — no `win-trip.sh` needed, no
  aliasing machinery used (APFS stores `*` natively).
- Case-collision sweep (`find | tolower | sort | uniq -d` over whole house): **no collisions found**.
- Permissions were already open on arrival; no exFAT hop involved.

## Phase B — toolchain: ✅ DONE (mostly pre-existing — do NOT reinstall)

| Tool | Status | Notes |
|---|---|---|
| Xcode CLT / clang | ✅ Apple clang 16.0.0 (`/usr/bin/gcc` = clang shim) | |
| Homebrew | ✅ **already installed** at `/usr/local/bin/brew` (Intel location) v5.x | NOT in non-login shell PATH — scripts must add it or call by full path |
| freetype | ✅ brewed, pkg-config reports 26.6.20 (= FT 2.13.x) | `/usr/local/opt/freetype` |
| pkg-config | ✅ `/usr/local/bin/pkg-config` | same PATH caveat |
| freeglut | ✅ installed | only needed for GL projects (not taskbar path) |
| XQuartz | ✅ preinstalled, DISPLAY live via launchd socket `/private/tmp/com.apple.launchd.*/org.xquartz:0` | Xft + own pkgconfig dir under `/opt/X11/lib/pkgconfig` |
| ripgrep | ❌ not installed | plain `grep` works; `rg` silently-noops in scripts that assume it |

**Key env fact for every build script:** brew's pkg-config does NOT see XQuartz's `.pc` files by
default. Darwin guard pattern that worked (see patched scripts below):

```sh
if [ "$(uname -s)" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"   # else ""
fi
```
`$X11_FLAGS` must go BEFORE `-lX11`/`$LIBS` on link lines — XQuartz's `xft.pc` puts its
`-L/opt/X11/lib` after our raw `-lX11`, and clang has no default `/opt/X11` search path.

## New Mac desktop button: ✅ BUILT + LIVE-TESTED

`$.crypts/mac-start-livedesk.command` — double-clickable in Finder, mirrors the Windows leg's
`win-start-livedesk.ps1` flow (kill old → parse `autostart.pdl` LAUNCH rows → direct-launch each
binary). Bypasses `crypt_autostart.+x` because its process scan reads `/proc` (absent on macOS),
exactly the reason the Windows leg bypassed `crypt_autostart.exe`. Details:
- defaults DISPLAY to the XQuartz launchd socket when unset (Finder spawns have none)
- children get `nohup` + per-component logs under `tmp/livedesk-mac/<name>.log` (a desktop proc
  never exits; inheriting caller stdout hangs any waiting script)
- CRLF-tolerant PDL parsing (matches `crypt_autostart.c`'s `strcspn(line,"\r\n")`) — see pitfalls

## Phase C — compile sweep (critical path first): IN PROGRESS

Baseline on Linux: 44/44 PASS. Table per MAC_COMPAT §C.3 ("passes twice in a row" rule noted;
✅✅ = passed twice).

| Script | Status | Fix applied | Date |
|---|---|---|---|
| `*.monads/*.livedesk-taskbar/ops/build_khtpm_strip.sh` | ✅✅ PASS | Darwin guard block (PKG_CONFIG_PATH + X11_FLAGS); emoji helpers built from wsr-pal source instead of copied prebuilt ELF | 2026-08-22 |
| `*.monads/*.livedesk-taskbar/ops/build_entity_menu.sh` | ✅ PASS ×1 | Darwin guard before CFLAGS assignment; `$X11_FLAGS` on link line | 2026-08-22 |
| `*.monads/*.livedesk-taskbar/ops/build_db_hq.sh` | ✅ PASS ×1 | same guard | 2026-08-22 |
| `*.monads/*.livedesk-taskbar/ops/build_db_hq_manager.sh` | ✅ PASS ×1 | no changes needed (pure logic, no Xlib) | 2026-08-22 |
| `apply_theme_op.c` (ad-hoc gcc) | ✅ PASS ×1 | no changes needed | 2026-08-22 |
| remaining ~40 build scripts | 🔲 pending sweep | — | — |

All 18 binaries in `*.monads/*.livedesk-taskbar/ops/+x/` verified Mach-O after purge+rebuild.
clang accepted all code as-is (only unused-function warnings, pre-existing); zero C edits so far.

## Phase D — livedesk smoke test: ✅ PASSED

Via `$.crypts/mac-start-livedesk.command`:
- taskbar paints + writes real frame data (`#.desktop/strip_frame_changed.txt` 100KB+, cells pdl,
  nav claims, frame history — all freshly timestamped)
- lsof: 35 live X11 unix-socket connections from the taskbar process (XQuartz confirmed)
- all 5 entity windows spawn + stay alive (self, m8_redhorned, m1_ninjadragon, book-stack, asa)
- clean shutdown via existing `$.crypts/button.sh quit` works unmodified on macOS (pgrep|xargs kill)
- visual quality NOT yet eyeballed by a human — XQuartz-functional is the bar this leg

## Findings/pitfalls (also appended to MAC_COMPAT.md §4)

1. **CRLF in autostart.pdl** (from win-trips): canonical C parsers strcspn-strip `\r\n`, bash
   `read` doesn't → launcher-launched entities died instantly with a trailing-`\r` pal-path arg.
   Fixed in the mac launcher; harmless elsewhere.
2. **Stale ELF binaries**: shipped `+.x` files are Linux ELF; "build if missing" checks skip them
   silently. Before any rebuild pass: purge non-Mach-O `+.x` first, then run builders.
3. **brew PATH**: `/usr/local/bin` missing from non-login shells (button/Finder/agent shells).
4. **rg absent** — grep-based tooling only.
5. **Build-script invocation quirk (not Mac-specific)**: `build_khtpm_strip.sh` re-derives
   `$SHARED` from `$(dirname "$0")/../../..` AFTER having already `cd $(dirname $0)` — invoking it
   via a *relative* path from house root double-counts the prefix and fails. Run from inside the
   ops dir or with an absolute path.
6. **setsid//proc sweeps**: still pending house-wide (45 sh / 40 c files) — none hit on the
   taskbar critical path yet.
7. **Duplicate taskbar stacks (pidfile race, fixed 2026-08-22)**: no `/proc` on macOS means
   `ensure_taskbar_running()`'s fallback scan can't save it from a stale/dead pidfile — every
   entity spawned its own strip (6 identical stacks). mac-start-livedesk.command now rm's the
   stale pidfile and waits for the fresh one to be live before launching entities.
8. **Stale-CLOSE poison (fixed 2026-08-22)**: `button.sh quit` writes CLOSE into registered
   pals' `interact_relay.txt`; if the entity was already dead the line sits there and silently
   kills the next entity binding that package (exit(0), zero output — one different victim per
   launch). Launcher truncates all relays under xyzfs/users at startup.
9. **`while read` drops a final unterminated PDL row** (live: ava never launched) — launcher uses
   `|| [ -n "$line" ]`.
10. **Header fit-to-screen (2026-08-22)**: khtpm_strip_parser.c gained an `__APPLE__`
    `present_rgb_fit()` — buffer drawn at natural strlen*8 width, downscaled at present-time to
    the screen-clamped window; header clicks inverse-mapped before hit-testing; popup anchors
    were already proportional (`anchor_x0*hw/layout_w`). Entity tiles got a `_WIN32`-mirror
    work-area clamp in tp_desktop_window_rgb.c (160px tiles were parked at x=1600 on a 1680px
    screen).
11. **HQ menu rows + reset/office respawn dead — `setsid` (fixed 2026-08-23)**: every manager
    spawn used `setsid nohup`; no setsid on macOS → dir/cli/db-hq/events-hq/open-hai/chat-hai and
    the reset/office RESPAWN path all silently failed. `KTB_SETSID` macro (`""` on Darwin)
    spliced through khtpm_taskbar_manager{,_main}.c, khtpm_hq_manager.c, apply_theme_op.c;
    xdg-open→open translation added in run_shortcut (`ktb_portable_darwin`) and in the entity
    action runner (tp_desktop_window_rgb.c ~407). open_cli.sh got a Darwin branch (temp
    `.command` wrapper + `open -a Terminal`). CJK in strip: font fallback chain now tries
    `Heiti SC:pixelsize=13` between Noto and DejaVu (no Noto CJK on this Mac; fc-match confirms).
12. **Book-stack Read chain repaired end-to-end (2026-08-23)**: event.pal's hardcoded Linux
    `/home/no/...` exec path → self-relative `exec ./dispatch.sh`; that exposed THREE more mac
    breaks, each fixed: (a) +18.0G prisc+x lacked 19.00's g_pal_dir exec resolution — backported,
    INCLUDING all three cmd-build branches (first attempt missed the bare/1-arg ones);
    (b) khtpm_show_choices' `/proc/self/exe` readlink → new shared shim
    `&.widgits/tile-picker/ops/self_exe.h`, applied to 5 tile-picker sources; also fixed
    `_POSIX_C_SOURCE 199309L` hiding snprintf under clang, and self_exe.h needing its own
    `<stdlib.h>` for realpath; (c) forked picker inherited show_choices' stdout pipe so callers'
    `$(...)` hung forever — child now dup2's /dev/null onto stdout pre-exec. Plus: picker clamps
    its spawn coords to the real screen (desktop_pos.txt said x=2320 on 1680px), entity startup
    persists the CLAMPED position via write_pos, bible_text/run.sh + tao/run.sh got Mac asset
    roots ($HOME/Desktop/bible]as.DeathNote]0000/book-stack) with loud validation per
    HOUSE_STDS §I.25, tao's GNU `mktemp --suffix` got a portable fallback, and both muta roots'
    prisc+x are Mach-O (menu's `find|head -1` picks +18.0G alphabetically).
    VERIFIED LIVE: full click-path prisc→dispatch→choices window(on-screen)→pick(tao)→
    SHOW_TEXT_FILE injected into entity history same second.
13. **CJK in strip actually fixed (2026-08-23)**: the 08-22 Heiti fallback was DEAD CODE — this
    Mac's fontconfig substitutes missing names (Noto CJK AND DejaVu both resolved to
    ADTNumeric.ttc, zero Han coverage), so the Noto-first open SUCCEEDED and short-circuited the
    chain. Proof via FcCharSetHasChar(U+4E2D): only "Heiti SC" (STHeiti Light.ttc) covers CJK.
    strip_font() now tries Heiti first on __APPLE__; rebuilt + full relaunch; pixel-truth xwd of
    the live header shows rendered glyph pixels in the datetime zone.
14. **Picker click-path re-verified against live desk (2026-08-23)**: user-reported "shows books,
    doesn't open verse" reproduced as transient — synthetic-click test (row hit-test at y>CHROME_H)
    wrote the token instantly and the verse popup mapped on-screen (738x168+938+468). Root cause
    was testing during process churn, not code. XTest is unavailable on XQuartz (no XTEST ext);
    XSendEvent works for programmatic clicks.
15. **Desk-facing binary sweep (2026-08-23)**: tile-picker now 16/16 Mach-O; open-hai pair,
    livedesk-clock pair, START_BUTTON trio, _shared-lib (prisc+x copy, chtpm_parser_pal,
    chtpm_rgb_render, dump_frame_png_op, x11_mirror) all Mach-O — note build scripts that use
    pkg-config need `PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig` (xft.pc),
    and some scripts lack Xft flags entirely (build x11_mirror.c directly with
    -lXft -lfontconfig -lfreetype). Mutaclsym test-harn-same tk_* trio Mach-O.
    REMAINING ELF BACKLOG: ~300 binaries across non-desk apps (LLM experiments, brokers,
    editors, rtp/rpg/zoo ops) — mechanical, not livedesk-blocking.
16. **Harness scripts setsid shim (2026-08-23)**: 7 scripts patched with the
    `SETSID=""` on Darwin header (livedesk stats/settings buttons, run_khtpm_strip.sh,
    openall/run.sh, kpi4/kpi5 scenarios, mutaclsym demo smoke) — all sh -n clean.
17. **GL z-order: EWMH keep-above is OFF the table (empirical 2026-08-23)**: XQuartz ignores
    _NET_WM_STATE_ABOVE (two-window probe: above-flagged window still stacked below a later
    peer). Remaining options for matching the Windows HWND_TOPMOST behavior: periodic XRaiseWindow
    from each GL loop (top of managed stack only), or override_redirect flip (true topmost, loses
    WM frame). USER DECISION PENDING.

## Verified live state (2026-08-23, post item-13 relaunch)

- exactly 1 strip parser + 1 manager + 6 entities; registry 6 rows; 6 tiles visible
- book-stack Read: picker on-screen → pick → verse popup opens same second (synthetic-click verified)
- header datetime zone renders glyphs (xwd pixel check); font = STHeiti Light.ttc via "Heiti SC"
- desktop_pos.txt files persist CLAMPED coordinates after entity start

## Next steps

1. USER DECISION — GL z-order approach (item 17): periodic XRaiseWindow (above managed peers,
   below OR tiles) vs override_redirect flip (true topmost, loses WM frame).
2. Human eyeball pass: CJK datetime glyphs, header fit-to-screen quality, verse popup look.
3. HQ-menu live exercise by hand (dir/cli/db/events/open-hai/chat-hai rows now have Mach-O
   binaries + setsid-free spawns; only a human click remains untested).
4. Remaining ELF backlog (~300 across non-desk apps): purge + `$.crypts/compile-runner.sh`,
   one project at a time with the PKG_CONFIG_PATH guard. Known app bug, out of mac-compat scope:
   mutaclysm ops/mua_compose_frame.c glyph_emoji errors.
5. Phase E harness truth runs against the rebuilt tk_* trio + patched scenario scripts.
6. Flip INDEX.md 🌐 macOS row when sweep completes (currently marked in-progress).
