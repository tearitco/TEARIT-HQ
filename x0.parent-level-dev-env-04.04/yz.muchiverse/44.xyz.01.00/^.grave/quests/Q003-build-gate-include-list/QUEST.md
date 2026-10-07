# Q003 — build gate misses files the renderer #includes (stale-binary bug)

| field | value |
|---|---|
| status | open |
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
2. Add them (with the right `$SHARED/` prefixes) to `CR_SRCS`. Do the same check for the other gated binaries in `build_khtpm_strip.sh` (`MGR_SRCS`, etc.).
3. First build after the change will recompile (about 25 s) because the combined hash changes; that is expected.

## Acceptance

- [ ] Touching (a trivial comment edit in) `khtpm_nav_echo.c` makes the next `sh build_khtpm_strip.sh` recompile the renderer; an unchanged tree does not (a no-op run stays under a second).
- [ ] Same proof for one header (`house_wait.h`).
- [ ] Output of both runs pasted in `## Result`; revert the trial edits.

## Rules

Everything in `^.grave/README.md`. Do not restart the owner's desktop; build only.

## Log

## Result
