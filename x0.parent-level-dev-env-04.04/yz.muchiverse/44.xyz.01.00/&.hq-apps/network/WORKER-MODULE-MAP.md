# NB JS Worker Module Map (Phase 0)

**Scope:** `ops/nb_js_worker.c` (resident QuickJS DOM/JS runtime for Network Browser).  
**Purpose:** bite-sized ownership/placement guide to reduce monolith growth and prepare mechanical, behavior-preserving extractions. No runtime behavior changes in this document.

## Current build shape
- `build.sh` compiles `ops/nb_js_worker.c` together with `nb_dom.c`, `nb_css.c`, QuickJS sources, and wasm3 into `ops/+x/nb_js_worker.+x`.
- Adding new worker compilation units is feasible: extract code into `ops/nb_worker_*.c/.h` (or similar) and include them in the same link step, preserving behavior.

## High-level subsystem map (approximate, by concern)

> Line ranges are indicative (file currently ~15.5k LOC) and will drift; use section markers and symbol search when editing.

| Area | Approx. region / markers | What lives here | Notes for new code |
|---|---|---|---|
| Wire / RPC / lifecycle | early file, `/* ---- wire framing ---- */`, recv/send/cmd loop, CLI/status helpers | stdin/stdout framing, command parsing/dispatch, page load/reset state, status/error reporting | Protocol changes need manager↔worker coordination; keep framing helpers isolated |
| Node/tree core & identity | `NODEKEY`, node index/orphans, `get_node/get_this`, `push_node`, tree helpers, serialization helpers | NbNode binding, wrapper identity, ownership, tree traversal/mutation primitives | Shared primitives; changes are high leverage/risk |
| DOM parsing/serialization bridges | text/html serialization, selector engine, RENDER/page.state emission | DOM-to-output serialization, selector matching | Touch carefully; behavior often covered by layout/projection tests |
| Element/document factories & core DOM APIs | `nb_dom_*`, `nb_el_*`, attribute/class/id/value/style getters/setters, insertion/removal helpers | Element/document operations, attributes, classList-ish support, child manipulation | Place new element/document APIs near related existing `nb_el_*`/`nb_dom_*` groups |
| Text/Comment/CharacterData APIs | CharacterData data/nodeValue helpers, `nb_cdata_*`, `nb_text_wholeText`, `nb_text_splitText`, ChildNode methods on character data | Text/comment data semantics and mutations | Recent slices live here; good candidate cluster for future mechanical extraction |
| Events / listeners / dispatch | `nb_el_addEventListener/removeEventListener/dispatchEvent`, event object helpers, focus/blur/click | DOM event registration and dispatch plumbing | Cross-cutting; verify event behavior carefully |
| Observers & reactive APIs | MutationObserver, IntersectionObserver, ResizeObserver, matchMedia sections | Observer registration, callbacks, delivery scheduling | Callback/microtask timing is sensitive |
| Layout/geometry/scroll/rects | `getBoundingClientRect`, client/scroll/offset dims, viewport/scroll state | Geometry queries and scroll-related state | Often tied to renderer/projection assumptions |
| Timers / animation / scheduling | `setTimeout/setInterval`, rAF, job queue/microtask helpers (scattered) | Scheduling primitives and task execution | Ordering/timing regressions are subtle |
| Canvas / WebGL / media hooks | `nb_canvas_*`, `getContext`, `toDataURL`, WebGL stubs/impl (if enabled), image/script/media helpers | Graphics/media integration | Optional deps/build flags in some paths (e.g. GLES) |
| Class/prototype installation & globals | `install_dom_classes`, Node/Text/Element/Document/collections prototypes, constructors/constants | JS-visible API surface wiring | When adding APIs, define C impl near peers and wire prototype exposure consistently |
| Main loop / command handling tail | end of file: LOAD/TICK/EVAL/EVENT handling, worker main | Top-level dispatch and process lifecycle | Keep aligned with manager protocol

## Placement rules for new DOM/runtime work (current practice)

1. **Engine semantics stay in the worker runtime** (not moved to ops/scripts) unless the work is clearly an external/helper concern (blocking I/O, media decode sidecar, persistence helper with stable contract).
2. **Add implementation next to related peers** (e.g. CharacterData/Text methods near existing CharacterData helpers; Node prototype methods near Node prototype wiring/helpers; collection methods near collection prototype setup).
3. **Expose JS API in `install_dom_classes` / prototype setup** in the same area as similar APIs (constants on constructor/prototype as appropriate).
4. **Prefer small, test-gated slices** with fresh `./build.sh`, focused gate, affected gates, and full `tests/nb_all_tests.sh` attempt (tail-verify individually if runner stalls in env).
5. **Do not introduce `system()/popen()` string-command paths** for new work if an argv-based helper/fork-exec boundary is feasible; avoid shell-quoting hazards.

## Phase 1 extraction candidates (mechanical, behavior-preserving)

Prioritize clusters with clear boundaries and good test coverage:

1. **CharacterData/Text/Node utility cluster (pilot candidate)**
   - Includes: `nb_cdata_*`, `nb_text_wholeText`, `nb_text_splitText`, related CharacterData/Text helpers and their prototype wiring points.
   - Why: recently modified, cohesive, covered by recent gates (`nb_characterdata_test.sh`, `nb_text_whole_split_test.sh`) plus existing DOM tests.
2. **Observer subsystem extraction** (Mutation/Intersection/Resize/matchMedia)
   - Why: clearly marked sections; isolate callback/scheduling logic.
3. **Collection/prototype convenience layer** (NodeList/HTMLCollection iteration helpers/wiring)
   - Why: localized, recently touched, easy to verify.
4. **Wire/protocol helpers** (recv/send/frame utilities)
   - Why: separable, but protocol-sensitive; extract carefully with targeted tests.

## Non-goals
- No semantic changes or cross-process moves in Phase 1 extractions.
- No monolithic rewrite; each extraction should be one scoped, verifiable commit.
