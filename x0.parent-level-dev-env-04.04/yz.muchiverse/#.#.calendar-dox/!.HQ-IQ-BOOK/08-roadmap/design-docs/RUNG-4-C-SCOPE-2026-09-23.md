# Rung 4 C — XHR (async) — Scope 2026-09-23

**Branch:** `opencode` @ `88f7e42e` parity 0 0, `make nbjs` GREEN.
**Plan:** `NB-JS-ENGINE-WORKER-PLAN.md` §8.2 / §8.5 C (Commit 9) — `XHR (async first)`.
**Worker:** `nb_js_worker.c` · **Prelude:** `nb_host.h` · **Manager:** `network_browser_manager.c`

## Plan spec vs in-tree

**Plan §8.2:** `XMLHttpRequest` async first (worker resident, callback via socketpair) then `fetch()` with Promise. New RPC: `manager <- worker: FETCH <id> <method> <url>` / `manager -> worker: FETCHED <id> <status>\n<len>\n<body>`. Payoff: SPAs that fetch JSON then render.

## In-tree evidence

- **Prelude XHR API — IN-TREE:** `nb_host.h:733` `function XMLHttpRequest()`, `738 addEventListener/removeEventListener`, `740 open(m,u,async){ _async=(async!==false)}`, `745 send(body)` — full XHR surface (readyState, responseText, timeout, etc. at 759-762).
- **Fetch API — IN-TREE:** `nb_host.h:680` `fetch() + XMLHttpRequest over nbFetchSync`, `715 new Promise`, `723 text()/json()` — fetch returns native Promise.
- **Transport — manager RPC (async) SHIPPED 7e55fc8b:** `nb_js_worker.c:3785` `try_fetch_via_manager()` sends `FETCH\n<id>\n<method>\n<url>` via `send_payload` when `!g_cli && !isatty(STDIN)`, waits for `FETCHED` via `recv_frame` (manager-owned network); fallback to direct curl (`file://` fast path, `3818 mkstemp`, `3866 curl -K`) when manager unavailable.
- **Manager RPC — SHIPPED 7e55fc8b:** `network_browser_manager.c:handle_worker_fetch()` handles `FETCH` in both `worker_load@1621` and `worker_eval@1691` loops (file read for `file://`, `curl -L` for `http(s)` with shared cookie handling), replies `FETCHED\n<id>\n<status>\n<body>`.

## Scope verdict — UPDATED 2026-09-23 — SHIPPED 7e55fc8b

**Update:** Manager RPC now landed (`7e55fc8b`): worker `try_fetch_via_manager@3785` + manager `handle_worker_fetch` + loops @1621/1691. Blocking curl remains fallback.

## Scope verdict (original 2026-09-23)

**SHIPPED 7e55fc8b (was partially open):**

- **Shipped (blocking):** XHR + fetch API surface + Promise wrapping + direct curl transport satisfies **payoff table** Phase 2:8.2 — "load more", infinite scroll, simple fetch-JSON SPAs work (blocking curl + microtask drain at `nb_js_worker.c:5000` ensures `.then()` renders before `RENDER`). Verified: `make nbjs` GREEN, worker handles `file://` and `http(s)://` via `resolve_doc_url` + curl.

- **Was open, now SHIPPED 7e55fc8b:** Plan's intended **async manager RPC** (worker resident, non-blocking, manager-owned network: `FETCH <id>` → `FETCHED`) is **not yet**. Current `nb_fetch_sync` blocks the worker thread (alarm disabled at 3805) — no interleaving of timers/events during fetch. True async would let `run_due_timers`/`drain_jobs` interleave while manager fetches.

## Next frontier if async is required

- **Commit 9 refinement:** Add `worker -> manager: FETCH` RPC (non-blocking), manager `do_fetch` handler, `FETCHED` reply, worker async suspend/resume (re-arm timer or microtask) — `≈80 lines` + manager RPC cases. Keep blocking curl as fallback for `file://` / offline.

- **If blocking is sufficient for house SPAs (per payoff), mark §8.5 C as SHIPPED (blocking) with caveat and move next to §8.5 D/E (history/cookies are already shipped per `RUNG-6-REMAINDER-SCOPE`).**

## House law

Verify before claiming: `grep nb_js_worker.c` for `FETCH` RPC vs direct curl, `grep network_browser_manager.c` for `FETCH` handler. One green commit at a time.

*Related scoping docs created this session cover the same pattern — rungs 3/4/5 were all marked shipped after in-tree grep contradicted plan prose.*
