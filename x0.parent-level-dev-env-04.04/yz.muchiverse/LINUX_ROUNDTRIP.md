# LINUX_ROUNDTRIP.md — Linux return leg status (write-back doc)

**Started:** 2026-08-23 by Kilo agent
**Machine:** Linux (x86_64), kernel 6.x. Tree returned from macOS 15.1.1 (24B91) leg.

## Root cause

The macOS leg rebuilt **~75 binaries as Mach-O** (macOS format) under the house tree. On Linux return these binaries cannot execute (`Exec format error`). Every build script in the house guards compilation with `[ -x binary ] || gcc...`; the Mach-O files retained their executable permission bits, so the guard evaluated to true and rebuilds silently skipped them. Stale poison persisted.

## Phase A — quarantine Mach-O binaries: ✅ DONE

- Permission-bit scan across entire tree (`find ... -perm /111`) flagged **76 Mach-O executables** (75 via name patterns + 1 bare-named in a sessions-adjacent dir).
- All moved to `/tmp/opencode/macho-quarantine/` (preserving relative paths, recoverable, not deleted).
- Verification: `find | file | grep Mach-O` returned **0** post-quarantine.

**Clusters quarantined:**

| Cluster | Count | Location |
|---|---|---|
| livedesk-taskbar ops | 6 | `*.monads/*.livedesk-taskbar/ops/+x/` |
| tile-picker | 16 | `&.widgits/tile-picker/ops/+x/` |
| `_shared-lib` | 5 | `&.widgits/_shared-lib/ops/+x/` and bare `+x/` |
| open-hai | 2 | `&.widgits/open-hai/ops/+x/` |
| events-hq | 1 | `&.widgits/events-hq/ops/+x/` |
| livedesk-clock | 2 | `&.widgits/livedesk-clock/ops/+x/` |
| START_BUTTON | 6 | `*.START_BUTTON/ops/+x/` + `system/` bare names |
| mutaclsym +18.0G | 11 | `ops/+x/` + `system/prisc+x` + 4 bare system binaries |
| mutaclsym 19.00 | 2 | `ops/+x/mua_menu_input.+x` + `system/prisc+x` |
| Mar$.$treetRace | 12 | `xdb/+x/`, `ai/+x/`, `dev/+x/`, `+/+x/` |
| %.harnesses hm_assert | 2 | `ops/+x/hm_assert_file.+x`, `hm_assert_kv.+x` |

**Impact on symptoms:**

- **db-hq opens empty terminal** → `khtpm_hq_manager.+x`, `khtpm_hq_render.+x` Mach-O.
- **mutaclysm halfway opens / empty terminal** → `prisc+x` + bare `system/{renderer,keyboard_input,orchestrator,chtpm_parser_pal,chtpm_rgb_render}` Mach-O.
- **entity menus / book-stack Read dead** → tile-picker ×16, shared-lib ×5, START_BUTTON ×6, open-hai/events-hq/clock ×5.
- **treetRace / neo dead** → 12 treetRace binaries Mach-O.

## Phase B — rebuild as ELF: ✅ DONE

### House-wide compile sweep

**Script:** `$.crypts/compile-runner.sh`
**Result:** 44/44 PASS, 0 FAIL, 0 TIMEOUT.

| Script | Status |
|---|---|
| `*.monads/*.livedesk-taskbar/ops/build_db_hq_manager.sh` | PASS |
| `*.monads/*.livedesk-taskbar/ops/build_db_hq.sh` | PASS |
| `*.monads/*.livedesk-taskbar/ops/build_entity_menu.sh` | PASS |
| `*.monads/*.livedesk-taskbar/ops/build_khtpm_strip.sh` | PASS |
| `&.widgits/tile-picker/scripts/build.sh` | PASS |
| `&.widgits/_shared-lib/ops/build_chtpm_rgb_render.sh` | PASS |
| `&.widgits/_shared-lib/ops/build_dump_frame_png_op.sh` | PASS |
| `&.widgits/_shared-lib/ops/build_x11_mirror.sh` | PASS |
| `&.widgits/open-hai/ops/build_open_hai_manager.sh` | PASS |
| `&.widgits/open-hai/ops/build_open_hai.sh` | PASS |
| `&.widgits/events-hq/ops/build_events_hq_manager.sh` | PASS |
| `&.widgits/livedesk-clock/ops/build_lc_clock.sh` | PASS |
| `*.START_BUTTON/scripts/build.sh` | PASS |
| `101.mutaclsym🧟‍♂️️+18.0G/scripts/build.sh` | PASS |
| `101.mutaclsym🧟‍♂️️19.00/scripts/build.sh` | PASS |
| `102.editor-📄️00.00/scripts/build.sh` | PASS |
| `Mar$.$treetRace.wsr]Q]k32/xsh.compile-all.+x.sh` | manual rebuild (see below) |
| `%.harnesses/file-menu+editor/ops/hm_assert_{file,kv}.c` | manual gcc (see below) |

### Exceptions rebuilt manually

- **apply_theme_op.c** — no build script existed; compiled ad-hoc:
  `gcc -Wall -O2 -o ops/+x/apply_theme_op.+x ops/apply_theme_op.c $(pkg-config --cflags --libs freetype2 x11)`
- **treetRace** (`Mar$.$treetRace.wsr]Q]k32`) — lives outside the house root; compiled all `.c` files per source directory (`./`, `ai/`, `dev/`, `xdb/`, `+/,` `$.m$rr.🔘️.®™]x2]ON!/`) into their local `+x/` output dirs using the original `xsh.compile-all.+x.sh` link line flags.
- **hm_assert pair** — sources in `%.harnesses/file-menu+editor/ops/` but no build script; compiled directly with `gcc -Wall -O2`.
- **apply_theme_op.c** — no build script existed; compiled ad-hoc:
  `gcc -Wall -O2 -o ops/+x/apply_theme_op.+x ops/apply_theme_op.c $(pkg-config --cflags --libs freetype2 x11)`
- **khtpm_show_text.c** — no build script; compiled ad-hoc after book-stack Read chain regression reported:
  `gcc -Wall -O2 -o ops/+x/khtpm_show_text.+x ops/khtpm_show_text.c`

## Phase C — verification: ✅ DONE

- `find | file | grep Mach-O` → **0 Mach-O remain** across entire tree.
- Spot-checked critical paths post-rebuild:
  - `khtpm_hq_manager.+x` — ELF
  - `khtpm_hq_render.+x` — ELF
  - `khtpm_entity_menu_render.+x` — ELF
  - `apply_theme_op.+x` — ELF
  - `emoji_xtract.+x` — ELF
  - `emoji_gen_atlas.+x` — ELF
  - tile-picker `khtpm_show_choices.+x` — ELF
  - shared-lib `chtpm_rgb_render.+x`, `x11_mirror.+x` — ELF
  - open-hai `khtpm_open_hai_manager.+x`, `khtpm_open_hai_render.+x` — ELF
  - events-hq `khtpm_events_hq_manager.+x` — ELF
  - livedesk-clock `lc_clock.+x`, `lc_reminder_popup.+x` — ELF
  - START_BUTTON trio + `system/prisc+x` — ELF
  - mutaclsym +18.0G `prisc+x` + bare system binaries — ELF
  - mutaclsym 19.00 `prisc+x` — ELF
  - treetRace `xdb/+x/` suite + `ai/+x/` trio — ELF
  - hm_assert `hm_assert_file.+x`, `hm_assert_kv.+x` — ELF
  - `khtpm_show_text.+x` — ELF (rebuilt ad-hoc; no build script; book-stack Read chain depends on it)

## Phase D — livedesk smoke test: ✅ PASSED

Via `$.crypts/button.sh reset`:
- strip parser + taskbar manager + 6 entities all spawned (rc=0 each).
- Process census: 8 live khtpm processes (1 strip parser, 1 taskbar manager, 6 entities).
- All entity windows reachable via autostart.pdl LAUNCH rows (self, m8_redhorned, m1_ninjadragon, book-stack, asa, ava).

## Ruled out (not contributing to breakage)

- **PDL paths** — `autostart.pdl` uses relative paths; fine on Linux return.
- **CRLF** — `crypt_autostart.c` strips `\r\n` via `strcspn`; `autostart.pdl` CRLF tolerated.
- **Relay CLOSE-poison** — Linux `crypt_autostart.c` truncates `interact_relay.txt` on launch (item 8 fix from Mac leg applies here too).
- **Hardcoded old-tree paths** — zero refs to `NNEST_CLEAN_PARENT/x/NNEST-11.17` or `/Users/` in build scripts/binaries.
- **book-stack asset roots** — `bible_text/run.sh` and `tao/run.sh` use multi-level fallbacks ending on the mounted Linux SHARE] path; Mac-only `~/Desktop/bible]as.DeathNote]0000/book-stack` is just one candidate in the chain.

## Next steps

1. **Human click-path exercise:** db-hq menu rows, mutaclysm full flow, book-stack Read (picker → verse), START_BUTTON, open-hai, events-hq.
2. **CJK datetime glyphs + header fit-to-screen** — ported from Mac, need Linux eyeball pass.
3. **HQ-menu live exercise by hand** — Mach-O rebuilt, setsid-free spawns now live.
4. **Remaining ELF backlog** — ~300 non-desk-app binaries were already ELF and untouched; no action needed.
5. **GL z-order** — deferred; X11 `_NET_WM_STATE_ABOVE` behavior unchanged from Mac findings (not Linux-specific).
6. **Remove quarantine** once confidence is high: `rm -rf /tmp/opencode/macho-quarantine` (or keep as rollback snapshot for 1–2 sessions).
7. **Windows compile coverage:** generated `compile-runner.ps1` + 33 missing `build.ps1` twins (5 projects already had them). All auto-generated from bash originals — needs Windows/MSYS2 verification before trusting. See `ROUNDTRIP_FIX.md` for the full list.
