# Rung 6 Remainder — Scope 2026-09-23

**Branch:** `opencode` @ `19c9eb7e` parity 0 0, `make nbjs` GREEN.
**Plan:** `NB-JS-ENGINE-WORKER-PLAN.md` Status RETIRED 2026-09-23, §8.6 SHIPPED-LOG has Rungs 3/4/5 SHIPPED. This doc scopes what remains of Rung 6 remainder (§8.3) and the unmarked §8.5 items.

## Rung 5 — render feedback loop — SHIPPED (no work)
- **Def:** Roadmap Rung 5 DONE 2026-09-05 (ea864cea): re-serialize mutated DOM → `page.state.txt` via `RENDER\n<rows>`.
- **In-tree:** `nb_js_worker.c:447 RENDER_MAX 60000`, emit `4921` / re-emit `4994`, manager `merge_render_rows()@1045`. Post-JS DOM authoritative.
- **Action:** none — glue landed. See `§8.6` entry 3.

## Rung 6 remainder (§8.3) — four bullets

### 1) `document.cookie` file-backed jar — SHIPPED
- **Plan:** `#.desktop/nb_cookies.txt` jar replacing Phase-0 empty stub.
- **In-tree:** `nb_js_worker.c:1849` `cookie_jar_init`, `1870 g_cookie_path`, `2016 read_file`, `2030-2052 write+rename`, manager hands per-house jar at `network_browser_manager.c:1494-1507`.
- **Verify:** cross-LOAD persistence (LOAD runs in fresh engine instance) — already wired.
- **Action:** verify-only; no new code unless persistence test fails.

### 2) `MutationObserver` — SHIPPED as no-op (per plan)
- **Plan:** "can stay a no-op unless a target site needs it."
- **In-tree:** `nb_js_worker.c:2776` stubs + `nb_host.h:628-635` `observe/disconnect/takeRecords` no-ops.
- **Action:** none.

### 3) `history.pushState/replaceState` → address bar without fetch — SHIPPED
- **Plan:** `NAVIGATE-URL` RPC or `#.desktop` marker.
- **In-tree:** prelude `nb_host.h:608-621` `pushState/replaceState` → `__nb_nav_addr`, worker `nb_js_worker.c:1030 __nb_nav_addr`, `54 g_nav_emit`, emit before STATUS `4946/5013`, manager `network_browser_manager.c:1032 g_pending_nav_*`, `1554 NAV\n<kind>\n<url>` capture, `1574 BACK/FORWARD` handling.
- **Action:** verify-only — confirm manager actually advances address bar without fetch on NAV (currently captures; check next-tick consumption at `1597-1625`).

### 4) `location.assign/replace/reload` → manager navigates — SHIPPED (slice 2)
- **In-tree:** `nb_host.h:228-231` `location.* / history.* funnel into pending NAV`, `432 install_location_parts`, manager NAV handling as above covers assign/replace via same NAV frame.
- **Action:** verify-only.

## §8.5 execution order — what is genuinely open

| Plan item | In-tree | Scope |
|-----------|---------|-------|
| A Rung 3 event loop | SHIPPED 1666-3946 | — |
| B `dispatchEvent` + `EVENT <selector>` RPC (Commit 8) | **HALF-SHIPPED**: worker `dispatchEvent@4221`, `g_evl@3011`, but **manager has no EVENT handling** (grep 0 hits) — user click in window does NOT reach scripted el | **OPEN — next frontier** |
| C Rung 4 XHR async | SHIPPED 3716/3783 | — |
| D `fetch()`+Promise | SHIPPED — native Promise `nb_host.h:514-515`, wrapper `715` | — |
| E history/location NAV | SHIPPED (see §3/4 above) | verify-only |
| F file-backed cookies | SHIPPED 1849 | verify-only |
| G docs | pending — this handoff + scope | — |

**Next frontier is §8.5 B — EVENT RPC (Commit 8).** All other Rung 6 remainder bullets are shipped or stub-by-design.

## House law for fresh agent
- Verify in-tree (`grep nb_js_worker.c / network_browser_manager.c / nb_host.h`) before claiming a rung as next.
- One green commit at a time, `make nbjs` GREEN, parity 0 0 → push non-force, re-verify.
- Do not commit runtime/never-source deletions (the `19c9eb7e` diff shows many); stage only `RUNG-6-*.md` / handoff / plan.
