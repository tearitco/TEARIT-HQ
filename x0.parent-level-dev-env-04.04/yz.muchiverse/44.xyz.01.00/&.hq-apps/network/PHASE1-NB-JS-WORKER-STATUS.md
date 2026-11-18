# nb_js_worker Phase 1 extraction status (opencode, 2026-11-18)

## Completed
- Continued mechanical, behavior-preserving Phase 1 single-TU `#include` extractions from `ops/nb_js_worker.c` into `ops/nb_worker_*.c` fragments (one bite at a time), keeping source/install/extract order and behavior.
- Per-bite verification used: fresh `./build.sh` in `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.hq-apps/network` (`BUILD_RC=0`, `OK nb_js_worker`; pre-existing warnings elsewhere observed) and focused probe `bash tests/nb_timer_test.sh` → `PASS` (`PASS: an unloadable DOM is reported, not swallowed`, `PASS: G1.1 live timer - 20 distinct self-updates in 10.1s`).
- Bites were committed on `opencode` with explicit pathspecs only (no runtime/untracked/other-agent files swept). Transient `.git/index.lock` contention in the shared worktree was handled by retrying with staged bite-scoped files only.
- Latest bite in this continuation: `7eb09d75f` `network browser: extract worker preamble block` (`ops/nb_worker_preamble.c`).

## Refactor state
- Post-`7eb09d75f` scan of `ops/nb_js_worker.c`: contiguous non-`#include "nb_worker_*"` blocks with >=10 nonblank lines = **0**. No fully-contained inline function-body definitions remained for mechanical extraction (earlier inspection).
- `install_dom_classes` remains an open wrapper in-file with body/tail/closing brace fragmented across existing `nb_worker_install_*` fragments (not a safe mechanical extraction target).
- Conclusion: established Phase 1 bite-style in-place `#include` modularization of `nb_js_worker.c` is complete by these criteria. Broader candidates in `WORKER-MODULE-MAP.md` (e.g. separate compilation units linked via `build.sh`, other subsystem extractions) remain out of scope for these bites.

## Verification / "back on track" (unresolved)
- **Green (fresh, attributable):** `./build.sh` + `tests/nb_timer_test.sh` for extraction bites (last: `/tmp/opencode/nb_build_preamble.log`, `OK nb_js_worker` at line 2347).
- **Not green / not confirmed:**
  - `tests/nb_all_tests.sh` previously timed out at 150s (`rc=124`) in this session; earlier observed `FAIL: span click-through failed after 3 attempts` in `tests/nb_span_test.sh:267` (environment/test stall/incomplete evidence).
  - Isolated `tests/nb_span_test.sh` run against the live shared browser/X session (`/tmp/opencode/nb_span_test.log`, `RC=1`) failed with environment/visual/live-state-sensitive symptoms (`FAIL: expected 1 span group, got 1000 - paragraphs welded?`, `FAIL: span click-through failed after 3 attempts (no window, or no navigation)` with `no underline band` on captures, `FAIL: no multi-line span tint found`, `FAIL: wrapped-span click did not navigate`), while some checks passed (e.g. keyboard focus/nav). A manual navigation probe did not settle to the requested fixture URL in that shared session, indicating uncontrolled browser state for deterministic diagnosis.
- **Attribution:** nb_js_worker extraction bites were mechanical/single-TU and timer-probe-verified; no code change was made to "fix" span behavior here because root cause was not reproducibly attributable in a controlled, non-interfering environment.
- **To get back on track with confidence:** reproduce `nb_span_test.sh` (and prior suite stall/span failure) in a controlled, isolated environment (dedicated browser instance/house, no shared relay/UI contention, stable X/frame-capture conditions); if reproducible, fix root cause with fresh build + targeted/suite evidence.
