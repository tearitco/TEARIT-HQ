# HANDOFF SLAVE — NB-JS persistent-worker engine (Phase 1)

**Slave doc** hanging off `handoff-2026-09-04-master.md` (see that file's
naming convention, lines 8-11). This is the **local job** handoff for the
network-browser JS engine work — a distinct, separate job from the master
handoff's khtpm C-deletion/xhtpm pass (that work is tracked in the master;
this job touches only the `&.hq-apps/network` cell and the design docs).

**Last updated:** 2026-09-04 (step 3 fully verified — build + regression + DOM test all green; step-3 files uncommitted, ready to commit & push)
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
| 3 | DOM tree in the worker + native accessors (getElementById/getElementsByTagName/querySelector, textContent, children, tagName, getAttribute/setAttribute, classList, appendChild, innerHTML); `tests/worker_dom_test.*` | **DONE (uncommitted)** — full-cell `build.sh` OK; all 5 rung tests PASS through eval AND worker; DOM test PASS vs O2 worker; ready to commit | — |
| 4 | RENDER merge → page.state.txt (visible payoff) | pending | — |
| 5 | CPU budget + node cap + SIGKILL/restart guards | pending | — |
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

## Remaining (steps 4-6)

After step 3 (worker DOM) is committed:
1. Step 4 — RENDER merge → `page.state.txt` (visible payoff): worker sends
   the merged page state; manager writes it; renderer contract stays
   byte-identical.
2. Step 5 — CPU budget + node cap + SIGKILL/restart guards.
3. Step 6 — Docs: mark rungs done in roadmap.

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

## Cross-links

- Master: `handoff-2026-09-04-master.md`
- Plan: `08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md`
- Roadmap: `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md`
