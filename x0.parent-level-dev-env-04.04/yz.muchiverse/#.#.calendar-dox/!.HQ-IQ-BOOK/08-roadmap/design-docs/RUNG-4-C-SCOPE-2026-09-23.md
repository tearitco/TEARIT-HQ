# Rung 4 C — XHR (async) — Scope 2026-09-23

**Branch:** `opencode` @ `88f7e42e` parity 0 0, `make nbjs` GREEN.
**Plan:** `NB-JS-ENGINE-WORKER-PLAN.md` §8.2 / §8.5 C (Commit 9) — `XHR (async first)`.
**Worker:** `nb_js_worker.c` · **Prelude:** `nb_host.h` · **Manager:** `network_browser_manager.c`

## Plan spec vs in-tree

**Plan §8.2:** `XMLHttpRequest` async first (worker resident, callback via socketpair) then `fetch()` with Promise. New RPC: `manager <- worker: FETCH <id> <method> <url>` / `manager -> worker: FETCHED <id> <status>\n<len>\n<body>`. Payoff: SPAs that fetch JSON then render.

## In-tree evidence

- **Prelude XHR API — IN-TREE:** `nb_host.h:733` `function XMLHttpRequest()`, `738 addEventListener/removeEventListener`, `740 open(m,u,async){ _async=(async!==false)}`, `745 send(body)` — full XHR surface (readyState, responseText, timeout, etc. at 759-762).
- **Fetch API — IN-TREE:** `nb_host.h:680` `fetch() + XMLHttpRequest over nbFetchSync`, `715 new Promise`, `723 text()/json()` — fetch returns native Promise.
- **Transport — BLOCKING curl, not manager RPC:** `nb_js_worker.c:3716` header `blocking curl child, Promise-shaped`, `3783 nb_fetch_sync`, `3805 alarm(0)` watchdog, `3818 mkstemp /tmp/nbfetch`, `3866 curl -sS -K`, `resolve_doc_url()@3742` for relative URLs, `file://` fast path. Worker does **direct curl**, not `FETCH` RPC to manager.
- **Manager RPC — NOT IMPLEMENTED:** `network_browser_manager.c` has 0 hits for `FETCH` from worker (only its own `do_fetch@754` and `RENDER@1045`/`NAV@1554` handling). No `FETCH <id>` handler, no `FETCHED` reply.

## Scope verdict

**Partially SHIPPED, partially OPEN:**

- **Shipped (blocking):** XHR + fetch API surface + Promise wrapping + direct curl transport satisfies **payoff table** Phase 2:8.2 — "load more", infinite scroll, simple fetch-JSON SPAs work (blocking curl + microtask drain at `nb_js_worker.c:5000` ensures `.then()` renders before `RENDER`). Verified: `make nbjs` GREEN, worker handles `file://` and `http(s)://` via `resolve_doc_url` + curl.

- **Open (spec async):** Plan's intended **async manager RPC** (worker resident, non-blocking, manager-owned network: `FETCH <id>` → `FETCHED`) is **not yet**. Current `nb_fetch_sync` blocks the worker thread (alarm disabled at 3805) — no interleaving of timers/events during fetch. True async would let `run_due_timers`/`drain_jobs` interleave while manager fetches.

## Next frontier if async is required

- **Commit 9 refinement:** Add `worker -> manager: FETCH` RPC (non-blocking), manager `do_fetch` handler, `FETCHED` reply, worker async suspend/resume (re-arm timer or microtask) — `≈80 lines` + manager RPC cases. Keep blocking curl as fallback for `file://` / offline.

- **If blocking is sufficient for house SPAs (per payoff), mark §8.5 C as SHIPPED (blocking) with caveat and move next to §8.5 D/E (history/cookies are already shipped per `RUNG-6-REMAINDER-SCOPE`).**

## House law

Verify before claiming: `grep nb_js_worker.c` for `FETCH` RPC vs direct curl, `grep network_browser_manager.c` for `FETCH` handler. One green commit at a time.

*Related scoping docs created this session cover the same pattern — rungs 3/4/5 were all marked shipped after in-tree grep contradicted plan prose.*
