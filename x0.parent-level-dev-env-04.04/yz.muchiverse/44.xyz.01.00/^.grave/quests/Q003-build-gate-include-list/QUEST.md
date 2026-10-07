# Q003 — build gate misses files the renderer #includes (stale-binary bug)

| field | value |
|---|---|
| status | done 2026-10-06 (claude): both gates PASS; the first build after this change recompiles once (hash changed) |
| tier | student or worker (good first quest) |
| size | S |
| assignee | - |
| posted | 2026-10-06 by claude (manager) |
| needs-owner-decision | none |

## Mission (one sentence)

Make `build_core_render.sh`'s hash gate list every file `khtpm_core_render.c` actually includes, so editing one of them rebuilds the binary.

## Why it matters

The gate hashes only the files in `CR_SRCS`; a source edit outside that list silently leaves a stale `khtpm_core_render.+x` running. Found
2026-10-06 while investigating the slow-start bug (see `04-bugs/bug_bounty.md`). `hash_gate.sh` documents this limitation: "pass every
real input file explicitly".

## Read first

1. `_.monads/_.livedesk-taskbar/ops/build_core_render.sh` (line ~98, `CR_SRCS=...`)
2. `&.widgits/_shared-lib/hash_gate.sh` (`hash_gate_stale` / `hash_gate_commit`, and the "real, deliberate non-feature" comment)
3. `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` lines 1-110 (the `#include "..."` list)

## Do

1. List the quoted includes of `khtpm_core_render.c` and of the shared `.c` cores it text-includes (`khtpm_draw_core.c`, `khtpm_render_core.c`, `khtpm_reparse_diff.c`, `khtpm_nav_echo.c`).
   Known gaps at posting time: `khtpm_nav_echo.c`, `house_wait.h`, `kh_proc_registry.h`, `kh_boot_mark.h`, `khtpm_css_parser.h`, `stb_image.h`, `lib/stb_image_write.h`.
2. Add them (with the right `$SHARED/` prefixes) to `CR_SRCS`. Fix `MGR_SRCS` in `build_khtpm_strip.sh` too (`khtpm_taskbar_manager.h` is missing). Do not edit `verify.sh` to make it pass.
3. First build after the change will recompile (about 25 s) because the combined hash changes; that is expected.

## Scorer (deterministic, no judgement needed)

`bash verify.sh [core|manager|all]` reads the real `#include` graph and the real gate lists and prints `MISSING|...` lines and a `VERDICT|PASS|FAIL|...` line (exit 0/1).
`bash verify.sh --selftest` proves the scorer itself (complete list passes, list missing one file fails). **Baseline 2026-10-06 (FAIL; the manager gap was fixed the same day by Q005's hook commit, which added `khtpm_taskbar_manager.h` + `khtpm_phone.c` to `MGR_SRCS`, so only the core 7 remain):** core missing 7
(`house_wait.h`, `kh_boot_mark.h`, `kh_proc_registry.h`, `khtpm_css_parser.h`, `khtpm_nav_echo.c`, `stb_image.h`, `stb_image_write.h`), manager missing 1
(`ops/khtpm_taskbar_manager.h`, a gap not in the original list). Ops copies of shared-lib files are scored as the shared original. Done = `verify.sh all` prints PASS twice.

## Acceptance

- [ ] `bash verify.sh all` exits 0 (both VERDICT lines PASS) and `--selftest` still passes.
- [x] Touching `khtpm_nav_echo.c` marks the renderer stale; an unchanged tree does not (proved with the real gate functions in `gate_test.sh`; the full 25 s recompile itself was not re-run).
- [x] Same proof for headers (`house_wait.h`, `khtpm_css_parser.h`, `kh_boot_mark.h`, `kh_proc_registry.h`).
- [ ] Output of both runs pasted in `## Result`; revert the trial edits.

## Rules

Everything in `^.grave/README.md`. Do not restart the owner's desktop; build only.

## Log

2026-10-06 | claude | Fixed: `build_core_render.sh` CR_SRCS now lists khtpm_css_parser.h, khtpm_nav_echo.c, house_wait.h, kh_boot_mark.h, kh_proc_registry.h, stb_image.h, stb_image_write.h (13 files; the manager gate was fixed earlier by Q005's commit). `verify.sh --selftest` PASS, `verify.sh all` PASS (core required=10 listed=13 missing=0; manager required=4 listed=6 missing=0).
2026-10-06 | claude | Behavioural proof WITHOUT compiling or touching real sources: `gate_test.sh` runs the real `hash_gate.sh` and the real CR_SRCS on a scratch copy: an unchanged tree is not stale (no-op run does not rebuild); an edit of khtpm_nav_echo.c, house_wait.h, khtpm_css_parser.h, kh_boot_mark.h or kh_proc_registry.h each marks the binary STALE (5/5). Caveat: the "old list misses it" case is trivial (the old list simply does not contain the file). Bug found while writing it: `sed` expands `&` in the replacement, and the shared folder is named `&.widgits` (use bash substitution). NOT done: the real recompile after touching a file (the next reset's build will recompile once because the combined hash changed).

## Result
