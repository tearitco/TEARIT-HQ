# Rung 4 — Network from JS: XHR / fetch — sync-first delivery

**Scope** (roadmap §1 rung 4): give the resident NB-JS worker a real
`XMLHttpRequest` + `fetch()`, so SPAs that gate content behind an async
data call actually render it. Delivery 1 is deliberately narrow; the
async/relay form is called out at the end as the follow-on.

**Status of the pieces it needs (verified 2026-09-06):**
- Worker already runs a real quiescent event loop: `queueMicrotask` FIFO
  + agenda timers + lifecycle `DOMContentLoaded`/`load` (walk bounded by
  `MAX_DRAIN_MS`, `MAX_TIMER_INVOCATIONS`, SIGALRM backstop). Fetching
  data and resolving a promise inside this loop is *all that is missing*.
- Vendored duktape (2.7.0) is built **without the Promise builtin**
  (0 matches for `Promise` in `js/duktape.h`) → ship a ~50-line ES5.1
  A+-style polyfill driven by the real `queueMicrotask`. (The roadmap
  already anticipated exactly this.)
- Manager owns network at parse time (`collect_scripts` →
  `curl_url_to_file`); per house gut feel, the worker becomes owner at
  JS-time, both shell `curl`. Same-origin vs CORS: permissive, noted.

## Decision

**Sync-under-the-hood, Promise-shaped API, worker-own curl.**

- A single C native `nbFetchSync(method, url, body)` does a blocking
  fetch (curl child, `--max-time 8`, body capped ~256 KB) and returns
  `{status, body}`.
- The JS layer wraps it: `fetch()` returns a *real* `Promise` that is
  resolved via our microtask queue ("promise resolves immediately, chain
  runs during the existing drain").
- `XMLHttpRequest` minimal class: full `readyState` walk ending at 4 on
  `send`, `open`/`setRequestHeader`/`send`, `onload`/`onreadystatechange`,
  `status`/`responseText`/`response`.

Why this is enough now (but not forever): the drain loop already drains
microtasks *to exhaustion*, so `fetch().then().then(...).finally(render)`
chains run synchronously after the call — Babel-rewritten async/await
SPA code (Promise chains, no `async` syntax) renders correctly on the
first pass. What it does **not** do: overlap or reorder network with
timers, or survive a genuinely slow response past the budget — those
need the async form.

## Wiring points (all inside `ops/nb_js_worker.c` + `ops/nb_host.h`)

1. **JS prelude** (`g_js_prelude` in `nb_host.h`, rung-4 block):
   - `Promise` polyfill (`resolve`/`reject`/`then`/`catch`, chained
     callbacks scheduled via `queueMicrotask`, `Promise.resolve`/
     `Promise.reject`/`Promise.all` where cheap).
   - `resolveUrl(base, rel)` mirroring the manager's, plus a `file://`
     special case (base `file:///a/b.html` + `data.txt` → plain local
     path, used by offline E2E fixtures).
   - `fetch(url, opts)` → `new Promise(...)` calling `nbFetchSync` and
     resolving/rejecting on `status`.
   - `XMLHttpRequest` shim (state machine above). `setRequestHeader`
     appends `Header: value` lines consumed by the native curl `-H`.
   - Native is exposed as `nbFetchSync` (underscore-free but
     prefixed-global; replaced by a real async `fetch` in the follow-on).
     Polyfill failure is swallowed — `g_js_prelude` peval is already
     best-effort.
2. **Native** (`nb_js_worker.c`, rung-4 block, installed next to
   `nb_queueMicrotask`):
   - `file://` or absolute disk path → `read_file` directly (no fork);
   - otherwise `popen("curl -sL --max-time 8 ... ")` capturing to
     memory; status = curl rc (`0`→200).
   - **CPU-safety:** the blocking call must not trip the audit alarm —
     wrap with `alarm(0); …; alarm(EVAL_BUDGET_SEC)` so curl runs
     outside the budget window, then the budget re-arms for the rest of
     the drain. (`sigalrm` hard-exits; a slow fetch must *never* get the
     worker killed mid-network.)
3. **No manager changes in delivery 1** — worker owns the fetch path.

## E2E (harness pattern in use)

1. Fixture in `/tmp/rw/house/…`: `gate.html` with inline JS doing
   `fetch('data.txt').then(r => r.text()).then(t => { document.body.textContent = t; })` — the
   page is *gated*: its content only appears after the async call.
2. Request `url: file:///tmp/rw/house/…/gate.html` — manager curls the
   file URL (curl reads `file://`), `data.txt` resolved by the worker's
   `file://` branch.
3. Run `timeout 60 ./man /tmp/rw/house`; assert
   `network_browser_page.state.txt` contains the fetched text (proving
   prelude → polyfill → native → microtask chain → `textContent` →
   RENDER merge all worked), plus `network_browser_status.state.txt`
   `ready`.

## Risks / notes

- Body cap + `--max-time` keep the blocking fetch bounded; a huge JSON
  SPA payload truncates (`responseText` capped) — note it, don't crash.
- Sites that call `fetch` before awaiting the microtask drain (e.g. come
  in a `setInterval` loop) still work because the drain keeps servicing
  timers; only *ordering* vs other network is non-real.
- The em dash contract with the manager (parse-time) vs worker
  (JS-time) curl ownership is recorded; a future "single network owner"
  refactor can give the worker the manager's Stop-aware curl path.

## Follow-on (not this delivery)

**Async relay via manager.** Worker sends `FETCH<id> url` frames on the
socketpair; manager answers mid-`worker_recv_line` with a base64
`FRESP<id> status body`; the worker event loop polls stdin during its
5 ms sleeps and resolves promises as responses land. This adds
real overlap + slow-network tolerance but touches the manager's hot
recv path and framing — deferred until delivery 1 is proven and a site
actually needs it.

## Delivery-1 evidence (2026-09-06, branch `opencode`)

- Manager E2E, `file://` fixture whose content is gated behind
  `fetch('data.txt').then(...).then(render)` + an `XMLHttpRequest`
  `onload` render — both merged into `#.desktop/network_browser_page.
  state.txt`:
  ```
  URL|file:///tmp/rw/house/gate.html
  TITLE|Rung4-Fetch-Gate
  TEXT|fetched:hello-rung4      <- fetch() Promise chain
  TEXT|xhr:xhr-ok               <- XMLHttpRequest onload
  ```
  (`TEXT|xhr:` before the resolveUrl file-join fix — a real trace of the
  bug the fix closed.)
- http curl leg (direct worker LOAD): `LOG|httpStatus=200`,
  `LOG|httpLen=559`.
- Headless regression `tests/worker_fetch_test.c{,.js}`:
  `PASS: worker_fetch_test (rung4 fetch+xhr rendered:
  F200-rung4-data-ok X200-rung4-data-ok)`; exit 0.
- All 5 rung suites green through both `nb_js_eval` and the worker
  (STATUS ok, expected `OK_` markers, 0 errors).
- Transport routing: `file:`/`file://`/`file://localhost` normalize in
  the native; relative URLs resolve with a `file:` special case in JS
  `resolveUrl` (the shared `URL` polyfill's `toString()` collapses
  host-less `file:` URLs, which is why the join lived outside it).