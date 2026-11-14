# Network Browser: ops/fork-exec usage and refactor research (2026-11-13)

Bite-sized research snapshot to answer: **how the house/network browser is built today, did recent work use ops/fork-exec where it could, and what refactor plan makes sense (modular, no monolith rewrite).**

This is a planning/research note, not a behavior change.

## 1) Current architecture (observed)

- **Package:** `44.xyz.01.00/&.hq-apps/network/`
- **Core binaries/processes**
  - `network_browser_manager.c` (~7.6k LOC): manager/orchestrator (fetch/state projection/UI actions, publishes page/state files).
  - `ops/nb_js_worker.c` (~15.5k LOC, ~900 static functions): resident QuickJS-based DOM/JS/page runtime, built as `ops/+x/nb_js_worker.+x`.
  - `nb_dom.c`, `nb_css.c`: DOM parse/serialize and CSS support compiled into worker/manager as needed.
  - Helper ops/binaries in `ops/`: `nb_js_eval.+x`, `nb_media_to_sprite.+x`, `nb_video_player.+x`, `nb_video_pump.+x`, `nb_video_play.+x`, `yt_resolve.+x`, plus many `nb_write_*.sh` action scripts and `nb_print_page.sh`.
- **Build:** `build.sh` compiles manager and ops; worker links QuickJS + wasm3 + nb_dom/nb_css (+ optional GLES).

## 2) Where ops / fork-exec are already used (good boundaries)

- **Manager → worker:** `fork()` + `execl(... nb_js_worker.+x ...)` (resident worker process boundary; line-RPC over pipe/socketpair model). This is the main process split for JS/DOM execution.
- **Video/media helpers:** manager forks/execs `nb_video_play`, `nb_video_player`, `nb_video_pump` (separate ops/processes).
- **UI/action relay scripts:** manager emits actions targeting `ops/nb_write_*.sh` (go/back/reload/tab/click/select/submit/print/autofill helpers etc.), and uses `ops/nb_print_page.sh` for print flow.
- **Resolve/helper tooling:** `yt_resolve` built as separate op.

These align with the house pattern of small ops and process boundaries for distinct responsibilities.

## 3) Where process boundaries are weaker / shelling-out is used

- **Manager:** multiple `system()` / `popen()` call sites (curl/fetch helpers, print/autofill helpers, etc.). Some are pragmatic; each call is an implicit fork/exec boundary but with shell/string-command risks and harder observability.
- **Worker:** contains `system()` / `popen()` usage in places (e.g. fetch/script/media-related paths observed in grep). Worker also embeds very large subsystems in-process.
- **Action model:** many behaviors are delegated to `ops/nb_write_*.sh` (good), but command strings are composed in C; correctness depends on careful quoting/paths.

## 4) Monolith pressure (main risk)

`ops/nb_js_worker.c` is the dominant hotspot (~15.5k LOC). It mixes many concerns:
- wire/protocol loop, lifecycle, crash/debug
- DOM tree/node model wrappers, ownership, mutation/observation
- DOM APIs (elements, text/comment, ranges-ish, traversal, selectors, events)
- CharacterData/Text/collection APIs (recent slices)
- timers/animation/frame, observers (Mutation/Intersection/Resize), media match
- canvas/WebGL, image/media/script loading hooks
- serialization/render projection/layout helpers

Recent DOM API slices (e.g. compareDocumentPosition, CharacterData methods, Text.wholeText/splitText, NodeList/HTMLCollection iteration) were added **in-place** in `nb_js_worker.c` because that is the established runtime. This is consistent with current structure but increases monolith size/coupling; it did **not** introduce new ops/fork-exec boundaries.

## 5) “Did we use ops/fork-exec when we could?”

- **For recent DOM/JS engine features:** generally **no new ops** - appropriate only if a concern is naturally separable (network fetch sidecar, media decode, persistence helper). Core DOM semantics belong in the runtime/engine module, but should be organized into smaller compilation units/modules, not one giant file.
- **Opportunities worth evaluating (not assumed):**
  - Replace fragile `system()` string commands with argument-vector `fork/exec` helpers where feasible.
  - Consolidate repeated file-write/action behaviors behind narrow helpers/ops with clear contracts.
  - Keep long-running or blocking external work in ops/processes (already true for video; review fetch/curl paths case-by-case).

## 6) Bite-sized refactor plan (recommended)

Goal: reduce coupling and change risk without a big-bang rewrite. Each phase must be independently verifiable (build + relevant gates + full `tests/nb_all_tests.sh` run evidence; tail verify if runner stalls per known env pattern).

### Phase 0 — Guardrails (docs/tests only)
- Keep adding focused gates per API slice (current practice).
- Document module boundaries and “where new code goes” before moving code.
- Add a short ownership map comment/doc for `nb_js_worker.c` sections (no behavior change).

### Phase 1 — Mechanical internal modularization of worker (no behavior change)
- Extract coherent subsystems from `nb_js_worker.c` into `ops/nb_worker_*.c/.h` (or `ops/worker/`), compiled into the same binary first:
  - `nb_worker_wire.c` (framing/RPC loop helpers)
  - `nb_worker_dom_nodes.c` (node wrappers, ownership, tree helpers)
  - `nb_worker_dom_api.c` (DOM method/getter implementations)
  - `nb_worker_observers.c` (Mutation/Intersection/Resize/matchMedia)
  - `nb_worker_timers_canvas.c` (timers/raf/canvas/WebGL as applicable)
- Keep public/internal symbol names stable initially; move code, not semantics.
- Verify with existing gates after each extraction commit.

### Phase 2 — Harden process/ops boundaries (targeted)
- Introduce small helpers to replace selected `system()/popen()` uses with `fork/exec` + explicit argv/env (start with highest-risk string-built commands).
- Audit `nb_write_*` invocations for quoting/path safety and centralize command construction.
- No behavior changes to protocols without tests.

### Phase 3 — Manager decomposition (if needed, separate track)
- Identify manager subsystems (tab/history/fetch orchestration/projection/action dispatch) and extract incrementally, preserving file/state contracts.
- Coordinate carefully (manager↔worker handshake is shared; avoid concurrent edits per AGENTS).

## 7) Non-goals / cautions
- **Do not** do a monolithic rewrite or move semantics between processes without tests.
- **Do not** split the DOM runtime into separate processes casually (perf/complexity/ABI/state costs are high).
- Treat each extraction as a behavior-preserving change with fresh build+run evidence.
- Respect lane/ownership guidance and commit discipline (scoped commits, explicit pathspecs).

## 8) Suggested immediate next step
- Create a tiny “module map” section in docs (or header comments plan) identifying first extraction target (`CharacterData/Text/Node prototype helpers` cluster or another low-churn, well-tested area) and execute **one** mechanical extraction as a proving slice.
