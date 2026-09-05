# HANDOFF SLAVE — NB-JS persistent-worker engine (Phase 1)

**Slave doc** hanging off `handoff-2026-09-04-master.md` (see that file's
naming convention, lines 8-11). This is the **local job** handoff for the
network-browser JS engine work — a distinct, separate job from the master
handoff's khtpm C-deletion/xhtpm pass (that work is tracked in the master;
this job touches only the `&.hq-apps/network` cell and the design docs).

**Last updated:** 2026-09-05 (step 5 committed + pushed; next up: step 6 docs)

**Status snapshot (step 5 pushed):** `68df5763`, `1cb67da3`, `6d32cf64`, `ea864cea` (step 4) and the step-5 guard commit all on `origin/chtpm-js-rungs`. Docs updated in each step's commit.
**Branch:** `chtpm-delete-per-app-c` → pushed to `origin/chtpm-js-rungs`
**Plan doc:** `08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md` (§7 = Phase 1, §8 = Phase 2)
**Roadmap:** `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md` (rung map)
**Session:** https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN

---

## Goal (current job)

Replace the one-shot `nb_js_eval` flow in the network browser with a
**resident persistent JS worker** (`nb_js_worker`), owned by
`network_browser_manager`, speaking line-RPC over a socketpair. Phase 1
lands rung 2 (DOM tree + accessors) plus the rung-5 render glue and the
worker plumbing — 6 commits. Phase 2 (rungs 3/4/5 remainder + real BOM)
is written hand-off-ready in the plan §8.

Constraints honored: worker never bare-spins (node cap 50k + CPU budget);
degrades to the static extractor if the worker dies (never blanks a page);
outward `page.state.txt` → `write_chtpm_projection()` → renderer contract
stays byte-identical; a script-less page never spawns the worker. Never
`git add -A`; stage explicit paths; build via `network/build.sh`.

## State

| step | what | state | commit |
|---|---|---|---|
| — | rung-6 prep (URL/URLSearchParams, history/matchMedia/getComputedStyle/MutationObserver, atob/btoa + timers, cookie stub) | DONE | `91819f2e` `b079f0c9` `e71232d1` `f5f86b4d` |
| — | plan doc + Phase-2 section | DONE | `81223066` `2c2af9ad` |
| — | dbhq_* C deletion (other agent; same branch) | DONE | `dce0f1f4` rev 11 |
| **1** | `nb_dom.c/.h` tolerant HTML parser + pre-order serializer; `do_fetch` writes `fetch.dom` (produced-but-unused) | **DONE** | `68df5763` |
| **2** | `nb_js_worker.c` skeleton + shared `nb_host.h`; manager lazy-spawn on `<script>`, LOAD plumbing, QUIT on exit (no behavior change) | **DONE** | `1cb67da3` |
| 3 | DOM tree in the worker + native accessors (getElementById/getElementsByTagName/querySelector, textContent, children, tagName, getAttribute/setAttribute, classList, appendChild, innerHTML); `tests/worker_dom_test.*` | **DONE** | `6d32cf64` (pushed) |
| 4 | RENDER merge → page.state.txt (visible payoff) | **DONE** | `ea864cea` (pushed) |
| 5 | CPU budget + node cap + SIGKILL/restart guards | **DONE** | step-5 commit (pushed) |
| 6 | Docs: mark rungs done in roadmap | pending | — |

## What's built (steps 1-2)

- **`network/nb_dom.h/.c`** — tolerant tag-tree parse → serialize `fetch.dom`
  in the plan §3 wire format (`N|<tag>|<id>|<cls>|<text-len>|<text>|<attr-len>|<attrs>`
  with `U`/`D` markers). Skips script/style/title/noscript/comments, handles
  void elements, decodes common named+numeric entities, verbatim text
  (no whitespace folding), 50k node cap. Verified against messy real-world
  markup (script skipping, `<b>` inline in `<p>`, `>` in a quoted attr).
- **`network/ops/nb_host.h`** — shared rung-1/6 Duktape host extracted from
  the eval (install_host, URL/history/timers/cookie prelude, native
  accessors). `static`-everything so each TU is self-contained.
- **`network/ops/nb_js_worker.c`** (+ `+x/nb_js_worker.+x`) — resident
  worker; length-prefixed line RPC on stdin/stdout (socketpair dup2'd by
  the manager). Commands: `LOAD\n<page.js>\n<fetch.dom>\n<href>\n<title>`
  → runs page JS → `STATUS ok|err:<msg>`; `QUIT` → clean exit. Stays
  resident across LOADs, never bare-spins.
- **`network/ops/nb_js_eval.c`** — now includes the shared host; behavior
  unchanged (headless-test + rollback path, per plan §2C).
- **`network/network_browser_manager.c`** — worker lifecycle: lazy
  `socketpair`+fork spawn on `<script>` presence, LOAD plumbing in
  `run_page_scripts`, QUIT+reap on shutdown. Page output **unchanged**
  this commit (one-shot eval still drives rendering; effects merge is step 4).
- **`network/build.sh`** — added the worker build line.

## Verified (steps 1-2)

- All 5 rung tests (`rung1_globals.js`, `rung6_url_test.js`,
  `rung6_bom_stubs_test.js`, `rung6_atob_test.js`, `rung6_cookie_test.js`)
  pass through **both** `nb_js_eval` and `nb_js_worker`, each `OK|1`.
- Worker headless: `STATUS ok` on a valid page, `STATUS err:Error: boom` on
  a thrown error, `err:cannot read page.js` on a missing file, resident
  across 4 LOADs, clean exit on QUIT.

## Key decisions

- Worker is the **manager's direct child** (socketpair dup2'd to
  stdin/stdout), not a renderer `<module>`. Line-RPC framing per plan §4
  (6-digit length + payload + `\n`).
- **File-path LOAD** (plan §4 recommendation): worker reads `page.js` +
  `fetch.dom` off disk; manager stays the HTML/DOM owner. LOAD carries
  `href`+`title` as extra lines for `install_host`.
- Step boundaries: worker authoritative render lands at step 4 (RENDER
  merge); until then one-shot eval + static extractor keep the window live.
- Phase 2 (rungs 3/4/5 + real BOM) is queued and hand-off-ready in plan §8.

## Remaining (step 6)

1. Step 6 — Docs: mark rungs done in roadmap (final Phase-1 step).

## Step 5 — DONE (guards: CPU budget, node cap, SIGKILL/restart)

The DOM-node cap was already present (`nb_dom.c` `DOM_MAX_NODES` 50 k at
the parser + serializer); what was missing was the **script side**. Trusting
`while(true){}` page.js to "behave" is not a plan — two layers now bound it:

- **Worker CPU budget.** Every eval runs under `alarm(EVAL_BUDGET_SEC=2)`
  with a deadly-default `SIGALRM` handler (`_exit(128+SIGALRM)`). A runaway
  `while(true){}` kills the worker mid-eval, not the browser. Wrapped both
  the prelude and the page script via `peval_budget()` in `ops/nb_js_worker.c`.
- **Worker node-handle cap.** JS `createElement`/`document.createElement`
  wrappers registered in `g_nodeindex[]` are capped at `NODE_HANDLE_CAP`
  250000 — past that, `node_index()` returns -1 and the wrapper's natives
  no-op through the existing bounds check (`get_this`/`get_node` already
  reject out-of-range handles), so one runaway can't grow the table forever.
- **Manager read timeout.** `worker_recv_line()` now `poll()`s with
  `WORKER_RECV_TIMEOUT_MS` 3000 before every read; a stalled worker makes
  `worker_load` fail instead of hanging the manager forever.
- **Manager SIGKILL + reap.** `worker_close()` (new swift) sends `SIGKILL`
  and blocks on `waitpid` — no leftover or zombie workers. On a dead/
  stalled worker, `worker_load` returns 0 and the next `<script>` page
  respawns a fresh worker.
- **SIGPIPE ignored** at `main` top, so a dying worker's write can't take
  the manager down.

**Verified end-to-end** (real worker + manager binaries in a scratch house,
local HTTP server): a `while(true){}` page — manager survives, no hang, no
zombie workers, `page.state.txt` keeps the static rows
(degrade-without-blanking preserved). The **following** scripted page
(`document.getElementById().textContent=` + `appendChild`) still
RENDER-merges through a respawned worker: `TEXT|after-js`, `TEXT|appended`.
`worker_dom_test` PASS vs the debug worker; all 5 rung suites green through
both eval and worker; full `build.sh` (O2) exits 0. Note: during testing an
earlier manager instance that had picked up the worker path only *after*
the house already lacked the worker binary silently downgraded to the
legacy eval — the house must contain `&.hq-apps/network/ops/+x/nb_js_worker.+x`
for the worker path to engage.

## Step 3 — DONE (uncommitted; bug found & fixed)

**Root cause found & fixed.** Duktape 2.7 native functions get their args at
stack **0..n-1** and the `this`-binding ABOVE the args (API = `duk_push_this`,
which exists; `duk_get_this` does NOT). All element natives were reading
`this` at index 0 (which is actually arg[0], a string → NULL node → `id`=""
and `getAttribute`=null). Fix applied to `ops/nb_js_worker.c`:
- New helper `get_this(ctx)` = `duk_push_this` → read `_nbnode` → `g_nodeindex`
  → `NbNode*`. `get_node(ctx,idx)` kept for argument-position wrappers
  (e.g. `appendChild`'s child arg, which DID move from index 1 → 0).
- All element natives use `get_this`; their string args shifted down 1
  (`getAttribute`/`setAttribute`/`classList` etc.); `appendChild` child arg
  now at 0.
- `children`/`childNodes`/`parentNode`/`firstChild`/`nextSibling` and
  `document.documentElement`/`document.body` are now **getters registered via
  `duk_def_prop`** (real-DOM semantics), not method props. `children` is
  element-only (skips text nodes).
- Instrumentation used to find it: threw `b._nbnode` (proved index binding
  correct) then C-trace `duk_get_type(ctx,0)` = 5 (STRING) for
  `getAttribute` and 0 (NONE) for the `id` getter → identified the convention
  mismatch.

**Result:** `worker_dom_test → PASS: STATUS ok, exit 0` (all 81 assertions:
banner id/className/textContent, setAttribute data-x, parentNode/firstChild/
nextSibling, getElementsByTagName li count=3, querySelector .item +
descendant, querySelectorAll count, classList add/remove/toggle/contains,
createElement/appendChild, innerHTML get/set, document.body,
documentElement.tagName==html).

**Full regression PASSED (O2, real `build.sh`, both binaries):** all 5 rung
tests (`rung1_globals`, `rung6_url_test`, `rung6_bom_stubs_test`,
`rung6_atob_test`, `rung6_cookie_test`) pass through BOTH `nb_js_eval` AND
`nb_js_worker`; DOM test passes against the O2 worker binary too. Rung-test
runner = `/tmp/rung_regress.sh` (framed `<len>\n<payload>\n` LOAD/QUIT; for a
rung test file, page.js=the test, fetch.dom empty).

**Remaining before commit:** commit step 3 (files: `nb_dom.c/h`,
`ops/nb_js_worker.c`, `build.sh`, `tests/worker_dom_test.*`, this handoff)
with the house footer, then push `origin/chtpm-js-rungs`. Do NOT touch the
other modified docs (master handoff etc. — other agent's khtpm work, 73 dirty
paths repo-wide, staged explicit paths only, never `git add -A`).

**Handoff-debug notes if it ever comes back (the reference case):**
`STATUS err:OBS null=false node=div id= ga=null` with `_nbnode`=0 means the
wrapper+index are fine and the accessors read the wrong stack slot — recheck
`this` handling, not the index binding. Duktape arg 0 is the FIRST ARG.

## Step 4 — DONE (committed; RENDER merge → page.state.txt)

The visible payoff: the worker's post-JS DOM now drives the rendered page.

**Wire contract (implemented).** After `duk_peval` succeeds the worker
builds `page.state.txt`-format rows from its DOM and sends a **single
frame** `RENDER\n<rows>` (rows = `TITLE|`, `TEXT|`, `LINK|`, `IMG|`;
`TITLE` first) and only then `STATUS ok`. Rows are budget-capped at 60 k
bytes so the frame always fits both sides' buffers. The manager's
`worker_load()` (in `network_browser_manager.c`) now loops frames: a
`RENDER` payload lands in `g_worker_render[]`, a `STATUS` ends the call.

**Merge (`merge_render_rows()`).** Overlays the RENDER rows onto
`page.state.txt` using the `apply_js_effects` atomic pattern:
non-content rows (`URL|` …) pass through, content rows (TITLE/TEXT/LINK/IMG)
are replaced wholesale, commit is atomic. When RENDER rows merge, the
legacy one-shot effects merge is **skipped** — the DOM-less eval would
otherwise inject a `TEXT|js: script error …` noise line on
DOM-touching scripts. A script that errors in the worker sends no RENDER,
so the static rows stay (degrade-without-blanking preserved).

**Serializer.** `dom_render_rows()` walks the `fetch.dom` C tree in
document order. The fetch.dom wire form carries each element's **direct
text inline on the element node**, so element text is emitted directly
(whitespace-only runs skipped, 88-col wrap like the static extractor).
`TITLE` comes from `g_title` (the LOAD title / `document.title` setter via
`nb_host.h`) since the parser drops `<title>`; `LINK|<href> <label>` for
`<a href>`; `IMG|<src> <alt>`.

**Bug caught during step 4:** `nb_attr_get()` returns a **shared static**
decoded buffer — calling it twice in one statement
(`src`, then `alt`) clobbered the first result (worker printed
`IMG|pic pic` for `src="http://e/i.png" alt="pic"`). Fixed by copying each
attribute into its own local buffer before the next call. (`nb_attr_get`
itself is correct; only the two-call-per-statement usage bit us.)

**Verified end-to-end (real manager binary + local HTTP server):**
- scripted page (`document.title="JSTitle"`, `el.textContent="after-js-mutation"`,
  `createElement`+`appendChild`) → `page.state.txt` shows
  `TITLE|JSTitle`, `TEXT|after-js-mutation`, `TEXT|three-appended`,
  `URL|...` header preserved, no whitespace rows, no `js:` noise.
- no-script page → unchanged static output (worker never spawned).
- log-only script page → worker-authored rows, clean title/text.
- Script-error page → `STATUS err` only, static rows kept.

**Regression (all green):** full-cell `build.sh` (O2) exit 0; all 5 rung
tests PASS through both eval AND new worker; `worker_dom_test` PASS vs the
new worker (`tests/worker_dom_test.c` now skips the RENDER frame and
reads frames exactly — the trailing `\n` after each payload must be
dragged or the next frame parses as length 0).

**Remaining before commit:** commit step 4 (files:
`network_browser_manager.c`, `ops/nb_js_worker.c`,
`tests/worker_dom_test.c`, roadmap, worker plan, this handoff) with the
house footer, then push `origin/chtpm-js-rungs`. Do NOT touch the other
modified docs; stage explicit paths only, never `git add -A`.

## Cross-links

- Master: `handoff-2026-09-04-master.md`
- Plan: `08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md`
- Roadmap: `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md`
