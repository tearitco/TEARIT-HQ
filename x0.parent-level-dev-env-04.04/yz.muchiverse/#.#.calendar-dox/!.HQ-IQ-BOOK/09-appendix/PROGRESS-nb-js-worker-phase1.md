# PROGRESS — network-browser JS engine: from one-shot Duktape to a resident worker (Phase 1)

**Status:** DONE — 2026-09-05 (phase-1 steps 1-5 committed + pushed)
**Branch:** `chtpm-delete-per-app-c` → `origin/chtpm-js-rungs` (rung-1 work was on `chtpm-var-substitution`)
**Roadmap:** `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md` (§1 rung 1, §3 worker, §6 rung 6)
**Plan:** `08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md` (§7 Phase 1, §8 Phase 2)
**Handoff:** `09-appendix/handoff-2026-09-04-slave-nb-js-worker.md`
**Files touched:** `&.hq-apps/network/nb_dom.[ch]`, `&.hq-apps/network/ops/nb_js_worker.c`, `&.hq-apps/network/ops/nb_host.h`, `&.hq-apps/network/ops/nb_js_eval.c`, `&.hq-apps/network/network_browser_manager.c`, `&.hq-apps/network/build.sh`, `&.hq-apps/network/tests/worker_dom_test.[ch]` (+ 5 rung test suites)
**Tests added:** `tests/worker_dom_test.[ch]`, `tests/rung1_globals.js`, `tests/rung6_url_test.js`, `tests/rung6_bom_stubs_test.js`, `tests/rung6_atob_test.js`, `tests/rung6_cookie_test.js`

---

## The arc

The session started with a one-shot Duktape process (`nb_js_eval`), ES5.1, no
DOM, and ended with a **resident persistent JS worker** (`nb_js_worker`),
owned by `network_browser_manager`, that parses the real DOM tree, runs page
JS against it, and re-serializes the mutated DOM back into `page.state.txt`
— guarded so a hostile script can kill the worker but never the browser.

Commit trail (JS-engine commits only):

```
(none)  rung 1  — duktape one-shot: window/navigator/location made real (chtpm-var-substitution)
91819f2e  nb_js_eval: URL + URLSearchParams ES5.1 polyfill          (rung 6 prep)
b079f0c9  nb_js_eval: history/matchMedia/getComputedStyle/MutationObserver stubs
e71232d1  nb_js_eval: atob/btoa + setTimeout/setInterval/clear* stubs
f5f86b4d  nb_js_eval: document.cookie empty-jar getter/setter
81223066  docs: NB-JS persistent-worker plan (rung 2 DOM + roadmap §3 worker)
2c2af9ad  docs: Phase 2 added to the worker plan (hand-off-ready)
68df5763  phase 1 step 1 — nb_dom: tolerant HTML parser + pre-order serializer
1cb67da3  phase 1 step 2 — worker skeleton + shared host; manager lazy-spawn
6d32cf64  phase 1 step 3 — DOM tree in the worker + rung-2 native accessors
ea864cea  phase 1 step 4 — RENDER merge → page.state.txt (the visible payoff)
bba458ec  phase 1 step 5 — CPU budget + node cap + SIGKILL/restart guards
```

---

## Part 1 — the Duktape-only era (rung 1: "make the browser work")

All we had was one-shot `nb_js_eval`: Duktape 2.7, ES5.1, invoked per page
via `timeout 3 nb_js_eval <page.js> <href> <title>`, writing `LOG|`/`OK|`
effects into `page.state.txt`. Scripts touching the browser globals threw
immediately.

`tests/rung1_globals.js` introduced; `install_host()` rebuilt:

1. **`window` / `self` / `globalThis` = the real Duktape global object.**
   Previously aliased onto the `localStorage` stub — the wrong target, a
   stack-index bug. Now `duk_push_global_object()` lands on a captured index
   and each name is a self-referential property:
   `window === globalThis === self` → `true`; `window.foo=42; foo===42`;
   `"addEventListener" in window` → `false` (does not throw).
2. **`navigator`** — data props only: `userAgent`, `language`, `languages`,
   `platform`, `onLine`, `cookieEnabled`, `doNotTrack`.
3. **`screen`** — `width/height/availWidth/availHeight` 1920x1080, `colorDepth` 24.
4. **`location`** — real getter props parsed from `g_href`: `protocol`,
   `host`, `hostname`, `port`, `pathname` ("/"), `search` ("?y"), `hash`
   ("#x"), `origin`; plus no-op `assign`/`replace`/`reload`.
5. **`window.name/closed/length`** — plain writable props.

**Verified** (`rung1_globals.js`, href with user@host:8443 query+hash):
`LOG|object object object`, `win===global true win===self true`,
full location parse, `navigator`/`screen` props, `getElementById null`.
Real-page check vs 4 extracted google scripts: `ext1.js` advances past the
old `location.pathname is undefined` wall and dies later at
`document.documentElement.lang` — i.e. **the very next failure is DOM
(rung 2)**. Google's webpack bundles (`ext0`/`ext2`) still die at *parse*
time on ES6 arrow functions — Duktape is ES5.1; that is the Duktape→QuickJS
decision point, unchanged.

## Part 2 — rung-6 prep (BOM stubs, 4 commits)

Still one-shot eval, but the BOM surface grew so feature-detect snippets
stop throwing:

- `91819f2e` **URL + URLSearchParams ES5.1 polyfill** (parse/format, searchParams get/set/append).
- `b079f0c9` **history / matchMedia / getComputedStyle / MutationObserver stubs**.
- `e71232d1` **atob/btoa** (real) + **setTimeout/setInterval/clearTimeout/clearInterval stubs**.
- `f5f86b4d` **`document.cookie` empty-jar getter/setter** (returns "", accepts set).
- `e8d72790` roadmap note that rung 6 is partially built.

Four new rung-6 test suites (`rung6_url_test`, `rung6_bom_stubs_test`,
`rung6_atob_test`, `rung6_cookie_test`) — later the shared-host extraction
makes all 5 rung suites pass through **both** `nb_js_eval` and `nb_js_worker`.

## Part 3 — the plan

- `81223066` **`NB-JS-ENGINE-WORKER-PLAN.md`** — the resident persistent
  worker: line-RPC over a socketpair dup2'd to the worker's stdin/stdout
  (6-digit length + payload + `\n` framing); manager spawns on `<script>`
  presence; worker owns the Duktape heap + DOM tree; `RENDER`/`EFFECTS`
  rows flow back so the worker DOM becomes authoritative for scripted pages.
  Phase 1 = 6 commits; never-guest invariants: no bare-spin, degrade to
  static extractor if the worker dies, no-`<script>` pages stay byte-identical.
- `2c2af9ad` **Phase 2** added (§8): rung 3 = events + event loop, rung 4 =
  XHR/fetch RPC, rung 5 remainder, rung-6 remainder (real history/location/
  cookies) — written hand-off-ready for any later agent.

## Part 4 — Phase 1 step 1 (`68df5763`): `nb_dom.c/.h` parser + serializer

- Tolerant tag-tree HTML parser → pre-order serializer in `fetch.dom` wire
  format: `N|<tag>|<id>|<cls>|<text-len>|<text>|<attr-len>|<attrs>` with
  `D`/`U` down/up markers.
- Skips `script`/`style`/`title`/`noscript`/head; handles void elements,
  common named + numeric entities, verbatim text (no whitespace folding),
  `DOM_MAX_NODES` 50000 cap.
- `do_fetch` writes `fetch.dom` — produced-but-unused at this point.
- Verified against messy real-world markup (script skipping, `<b>` inline in
  `<p>`, `>` inside a quoted attr).

## Part 4 — Phase 1 step 2 (`1cb67da3`): worker skeleton + shared host

- `ops/nb_js_worker.c` — resident worker: `LOAD\n<page.js>\n<fetch.dom>\n
  <href>\n<title>` → run JS → `STATUS ok|err:<msg>`; `QUIT` → clean exit;
  stays resident across LOADs.
- `ops/nb_host.h` — the rung-1/6 Duktape host extracted out of the eval,
  shared by both TUs (static-everything, self-contained).
- Manager (`network_browser_manager.c`): lazy `socketpair`+fork spawn on
  `<script>` presence, LOAD plumbing in `run_page_scripts`, QUIT+reap on
  shutdown. Page output unchanged this commit.

## Part 5 — Phase 1 step 3 (`6d32cf64`): DOM tree + rung-2 native accessors

The tree lives in the **worker** now: `nb_dom_load()` rebuilds it from
`fetch.dom`; native Duktape accessors walk the C tree via a numeric
`_nbnode` handle → `g_nodeindex[]` (reset per page).

Shipped: `getElementById`, `getElementsByTagName`, `querySelector(All)`
(`#id` / `.class` / `tag` / `tag.class` / descendant combos),
`textContent`, `children` (element-only), `childNodes`, `parentNode`,
`firstChild`, `nextSibling`, `tagName`/`nodeName`,
`getAttribute`/`setAttribute`, `classList.add/remove/toggle/contains`,
`createElement`/`appendChild`, `innerHTML` get/set (set re-parses a fragment
into the tree), `document.documentElement`/`body`.

**The Duktape `this`-binding bug — found & fixed (the debug landmark).**
Duktape 2.7 native functions get their args at stack indices **0..n-1** and
the `this`-binding **above** the args — the API is `duk_push_this()`
(`duk_get_this` does not exist). Every element native had been reading
`this` at index 0, which is actually arg[0] (a string) → NULL node → `id=""`,
`getAttribute`=null. Fix:
- `get_this(ctx)` = `duk_push_this` → read `_nbnode` → `g_nodeindex`.
- Element natives use `get_this`; their string args shifted down 1;
  `appendChild`'s child arg moved 1 → 0. Children/traversal/body accessors
  became real **getters** via `duk_def_prop`, not method props.
- Found via instrumentation: `b._nbnode` proved index binding correct;
  C-trace `duk_get_type(ctx,0)` = 5 (STRING) for `getAttribute` and 0 for
  the `id` getter → convention mismatch, not a binding bug.

**Verified:** `worker_dom_test` → `PASS: STATUS ok` (all 81 assertions);
5 rung suites pass through both eval AND worker; full O2 `build.sh` green.

## Part 6 — Phase 1 step 4 (`ea864cea`): RENDER merge → page.state.txt (the payoff)

The worker's post-JS DOM drives the rendered page.

- **Wire contract:** after a successful eval the worker sends a **single
  frame `RENDER\n<rows>`** where rows are `TITLE|` (first), `TEXT|`, `LINK|`,
  `IMG|` — the exact `page.state.txt` format — budget-capped at 60 k bytes
  so the frame always fits, then `STATUS ok`.
- **Manager:** `worker_load()` loops frames — `RENDER` payload → 
  `g_worker_render[65536]`, `STATUS` ends it; `merge_render_rows()`
  overlays rows onto `page.state.txt` atomically (content rows replaced
  wholesale; `URL|` passes through). When RENDER merges, the **legacy
  one-shot effects merge is skipped** — the DOM-less eval would otherwise
  inject `TEXT|js: script error …` noise. Script error → no RENDER → static
  rows stay (degrade-without-blanking preserved).
- **Serializer:** `dom_render_rows()` walks the tree in document order;
  element text is inline on the node in fetch.dom wire form, so it's emitted
  directly (whitespace-only runs skipped, 88-col wrap). `TITLE` comes from
  `g_title` / `document.title` (the parser drops `<title>`); `LINK|<href>
  <label>`; `IMG|<src> <alt>`.
- **Bug caught:** `nb_attr_get()` returns a **shared static** decoded buffer
  — calling it twice in one statement (`src`, then `alt`) clobbered the
  first (`IMG|pic pic` for `src="http://e/i.png" alt="pic"`). Fixed by
  copying each attr into its own local buffer before the next call.
- **Tests updated:** `worker_dom_test.c` now skips the RENDER frame and
  drains frames exactly (each frame payload is followed by a trailing `\n` —
  miss it and the next frame parses as length 0).

**Verified end-to-end** (real manager binary + local HTTP server): scripted
page → `TITLE|JSTitle`, `TEXT|after-js-mutation`, `TEXT|three-appended`
(appendChild), no whitespace rows, no `js:` noise; no-script page
byte-identical; script-error page keeps static rows.

## Part 7 — Phase 1 step 5 (`bba458ec`): CPU budget + node cap + SIGKILL/restart guards

The DOM-node cap already existed (`DOM_MAX_NODES` 50 k, enforced at the
parser/serializer in `nb_dom.c`). What was missing was the **script side**:

- **Worker CPU budget.** Every eval runs under `alarm(EVAL_BUDGET_SEC=2)`
  with a deadly-default `SIGALRM` handler (`_exit`). A `while(true){}`
  page.js kills the worker mid-eval — not the browser. Both the prelude and
  the page script go through `peval_budget()`.
- **Worker node-handle cap.** JS `createElement` wrappers registered in
  `g_nodeindex[]` are capped at `NODE_HANDLE_CAP` 250000 — past that
  `node_index()` returns -1 and the wrapper's natives no-op via the existing
  bounds check in `get_this`/`get_node`.
- **Manager read timeout.** `worker_recv_line()` `poll()`s with
  `WORKER_RECV_TIMEOUT_MS` 3000 before every frame read — a stalled worker
  fails `worker_load` instead of hanging the manager forever.
- **Manager SIGKILL + reap.** `worker_close()` now `SIGKILL`s and blocks on
  `waitpid` — no strays, no zombies. Worker death ⇒ next `<script>` page
  respawns a fresh worker.
- **SIGPIPE ignored** at `main` top — a dying worker's write can't take the
  manager down.

**Verified end-to-end** (real worker + manager binaries in a scratch house,
local HTTP server): hostile `while(true){}` page — manager survives, no
hang, no zombie workers, `page.state.txt` keeps the static rows; the
**following** scripted page still RENDER-merges through a respawned worker
(`TEXT|after-js`, `TEXT|appended`); resident worker persists between LOADs.
5 rung suites + `worker_dom_test` + full O2 `build.sh` all green.

## Phase 1 — done, Phase 2 — queued

Steps 1-5 committed + pushed to `origin/chtpm-js-rungs`; step 6 (docs:
mark rungs done in roadmap) is the only Phase-1 item left. Rungs 3 (events +
event loop), 4 (XHR/fetch RPC), 5 remainder, and the rung-6 real-BOM
remainder (`createTextNode`, `getElementsByClassName`, `removeChild`/
`insertBefore`/`replaceChild`, `removeAttribute`, `style.*`, form `value`,
real `history`/`location`/cookies, ES6/QuickJS) are all written up
hand-off-ready in plan §8.