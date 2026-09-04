# NB-JS persistent-worker plan — rung 2 (DOM) + roadmap §3

**Status:** PLAN — pending approval / step-by-step execution
**Branch:** `chtpm-delete-per-app-c` (carries JS rung-6 prep + dbhq deletion)
**Date:** 2026-09-04
**Author:** oc
**Roadmap:** `NB-JS-ENGINE-ROADMAP.md` §2 (rung 2 = DOM) + §3 (worker) + §5
**Scope:** `44.xyz.01.00/&.hq-apps/network/` only — `network_browser_manager.c`
(+ new `nb_js_worker.c`), `ops/nb_js_eval.c` (kept as test/fallback).

---

## 0. Why a worker (and why now)

The current JS path is **one-shot**:
`do_fetch` → strip scripts → `system("timeout 3 nb_js_eval.+x page.js js.effects.txt url title")` →
`nb_js_eval` runs Duktape once, writes `js.effects.txt` → exit →
`apply_js_effects()` merges `LOG|`/`TEXT|`/`TITLE|` back into `page.state.txt`.

This cannot host rung 2:
- A DOM tree is a *persistent structure*. A one-shot process that exits
  after one eval cannot hand a living DOM to the next navigation, and it
  cannot re-run JS after user input (rung 3 events) or after a `fetch`
  completes mid-script (rung 4).
- The **DOM tree must live in the JS process** (or be shipped to it), and
  the only way JS can both run *and* pause/await I/O *and* outlive a single
  `document.write` burst is a long-lived worker speaking to the manager.

The user chose **go straight to the worker** (rather than a throwaway
one-shot DOM pass). This plan therefore builds §3's persistent worker and
lands rung 2's DOM inside it. Rungs 3-5 then build on the same process.

## 1. Architecture (from roadmap §3, trimmed to what we ship now)

A **persistent worker process** `nb_js_worker.+x`, owned by the manager
(manager's direct child — NOT a renderer `<module>`), talking over a
**socketpair** with a tiny **line-based RPC**. One worker per browser cell
(single-tab is fine today; key by tab id when we support >1 scripted tab).

Line RPC framed as: `LEN\n<JSON-or-escaped-payload>\n` (see §4 for
simplicity). The initial command set is deliberately minimal:

```
manager -> worker :  NAVIGATE <url>\n<len>\n<html>
worker  -> manager:  EFFECTS <n>\n<TITLE|...>\n<TEXT|js: ...>\n...(n rows)
worker  -> manager:  RENDER <len>\n<page.state rows>     (after JS quiesces)
manager -> worker :  EVENT <selector>                    (rung 3+, later)
manager -> worker :  HALT                                (shutdown/kill)
```

`RENDER` and `EFFECTS` both flow back as `page.state.txt` rows so the
existing `write_chtpm_projection()` → renderer contract is untouched.
`EFFECTS` is the "immediate document.write/title during script run"
channel; `RENDER` is the "final serialized DOM after the event loop goes
quiescent" (rung 5). For **this** step we ship `RENDER` (the rung-2 payoff)
and keep `EFFECTS` as the current-chrome title sink.

## 2. Deliverables for this step

### A. New: `ops/nb_js_worker.c` (the worker)
- Owns: Duktape heap (`js/duktape.*`), the DOM tree, install_host
  (reuse the rung-1/6 host from `nb_js_eval.c`, shared via a small
  `nb_host.c` or `#include`).
- Reads the manager DOM + scripts over the socketpair.
- Implements **rung 2 DOM**: the manager builds the *tag parse* and ships
  it; the worker builds the node tree in C and lazily exposes it to JS via
  native Duktape accessors (roadmap §2 minimum API).
- Runs top-level scripts via Duktape `duk_peval`, then drains microtasks /
  immediate timers once (rung 3 loop comes later), then `RENDER`s.
- CPU safety (roadmap §2, non-negotiable): `DUK_USE_EXEC_TIMEOUT_CHECK`
  wall-clock deadline; node cap; the manager SIGKILLs on overrun.
- `usleep` idle between RPC polls — never a bare spin (§2).

### B. Edit: `network_browser_manager.c`
- **Add a DOM parse** during `do_fetch` that turns `fetch.html` into a
  compact node tree, replacing the flat linear scan where JS-aware output
  matters. Keep the existing static `TEXT|/MEDIA|/IMG|/VIDEO|/LINK|`
  extractor **as-is for the non-JS path** (or derive it from the tree —
  do that only if it's clean; otherwise keep both for the first cut).
- **Replace `run_page_scripts()` one-shot `system()` call** with: spawn
  worker on first need (lazy), then send `NAVIGATE <url>\n<len>\n<html>`.
- **Merge worker `RENDER`/`EFFECTS`** rows into `page.state.txt` (this
  replaces `apply_js_effects()`), then `write_chtpm_projection()` as today.
- **Manage worker lifecycle**: spawn on first `NAVIGATE`, restart on
  crash/navigation, SIGKILL on budget overrun, reap on manager exit
  (SIGTERM/atexit). Guard so a missing/dying worker degrades to the old
  static extractor (never blank the page).

### C. Keep / tests
- Keep `ops/nb_js_eval.c` + the one-shot binary as the **headless test +
  rollback** path (roadmap §3 "keep the one-shot around for tests").
- New tests: `tests/worker_dom_test.*` — assert `document.getElementById`,
  `textContent`, `children`, `querySelector('#x')`, classList, etc. on a
  small canned HTML page, via the worker (no real network).

## 3. The DOM node tree (rung 2, server-side C)

Manager side (`nb_dom.h`/in manager): parse won't live in JS. Compact C
tree:

```c
struct NbNode {
    const char *tag;          /* "div","a","span",... lowercase */
    char *id, *cls;           /* first/space-joined class (expand later) */
    char *attrs;              /* raw attr blob (name="v" ...) for getAttribute */
    char *text;               /* inner text (decoded) for textContent */
    struct NbNode *parent, *first_child, *next_sibling;
    int nav;                  /* reserved */
    /* JS handle: uintptr index when exposed via Duktape finalizer */
};
```

- **Parse:** a small tag tokenizer marked in the manager during the same
  pass that strips scripts (skip script/style/title/comment/noscript;
  handle void elements img/br/hr/input/meta/link/source; auto-close on
  `</tag>` and on mismatched close). HTML is messy — the roadmap says
  punt on full error-recovery (HTML5 parser is a different project). First
  cut = **well-formed-ish** + common real-page tolerance (`<br>`,
  unclosed `<p>`,`<li>`,`<div>` for a fixed few).
- **Cap:** 50k nodes (kill runaway appendChild both in parse and in JS
  `appendChild`).
- **Wire format to worker:** serialize the tree compactly, e.g. pre-order:
  `N|<tag>|<id>|<cls>|<text-len>|<text>|<attr-len>|<attrs>\n` with `U` up /
  `D` down markers, single flat stream. The worker rebuilds the linked
  tree in its own heap (no shared memory — simpler, no ABI coupling).

## 4. RPC framing (kept dead-simple)

Length-prefixed lines are the only framing (no JSON dependency):

```
send(fd, msg):
  snprintf(lenbuf, "%.6d\n", payload_len);
  write(lenbuf); write(payload); write("\n");
recv(fd):
  read lenbuf until '\n'; read payload_len bytes.
```

Payloads are `\n`-safe by escaping any embedded newline as `\x0a` on the
sender and unescaping on the receiver (or the manager always puts raw
HTML in a separate file and sends just a path — **simpler: ship the HTML
file path in NAVIGATE, worker reads the file**. Avoids a giant single
`write` and keeps the payload small. Decide in step A; file-path variant
is the recommendation.)

If file-path variant: `NAVIGATE <url>\n<path-to-fetch.html>\n` and the
worker parses the HTML itself OR reads a `.dom` file the manager wrote.
Recommendation: **manager writes `fetch.dom` (serialized tree); worker
reads it + reads `page.js`.** Worker stays a "JS process", manager stays
the "network/HTML owner" — cleanest split and least traffic.

So the actual RPC for this step reduces to:

```
manager -> worker :  LOAD\n<path-page.js>\n<path-fetch.dom>
worker  -> manager:  RENDER <n>\n<rows>
worker  -> manager:  STATUS ok|err:<msg>
manager -> worker :  QUIT
```

`LOAD` carries two file *paths*; the worker reads them. This is nearly
the current one-shot argv contract (`page.js` + tmp dir) but delivered to
a *resident* process — minimal new plumbing, maximum reuse of how the
manager already prepares `page.js` and (new) `fetch.dom`.

## 5. Keeping the outward contract identical

The renderer only ever sees `#.desktop/network-browser-hq_ui.txt` (from
`write_chtpm_projection()`) fed by `page.state.txt`. Steps:

1. `do_fetch` writes static rows to `page.state.txt` **exactly as today**.
2. Worker `RENDER`/`EFFECTS` rows are merged in by the manager
   (title-wins + `TEXT|js: ...` prefixing, same as `apply_js_effects`).
3. `write_chtpm_projection()` unchanged.
So no renderer/`.chtpm` change. A page with no scripts renders exactly as
before (worker not even spawned unless a `<script>` exists).

## 6. House rules / discipline

- Never `git add -A` — stage the specific files in the network cell.
- Commit after each shippable step (see §7); footer
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` +
  `Claude-Session: <session url>`.
- `+x/` binaries are git-ignored; rebuild via network cell `build.sh`
  (add the worker build line there).
- CPU: worker must never bare-spin; budget + node cap enforced (roadmap §2).
- If the worker dies/misses, **degrade to the static extractor without
  blanking the page** — a JS regression must not nuke content.
- End of each step: run all `network/tests/*.js` through the worker/test
  harness + keep `rung1_globals.js` green.

## 7. PHASE 1 — execution order (each commit = green, shippable)

1. **`nb_dom.c/.h` parse + serialize** in the manager; emit `fetch.dom`
   alongside `page.js` in `do_fetch`. (No behavior change yet — the dom
   file is produced but unused.) Verify: dump a sample page's `fetch.dom`.
   *Commit 1.*
2. **`nb_js_worker.c` skeleton**: Duktape heap + host (reuse), read
   `LOAD`-delivered files, run `page.js`, write nothing yet but print
   `STATUS ok`. Manager: spawn on `<script>` presence, `QUIT` on exit.
   Verify headless via a test fixture. *Commit 2.*
3. **DOM tree in the worker** + native accessors (rung 2 API:
   `document.getElementById/getElementsByTagName/querySelector`,
   `textContent`, `children`, `tagName`, `getAttribute`, `setAttribute`,
   `classList`, `appendChild`, `innerHTML` get/set). Verify with
   `tests/worker_dom_test.*`. *Commit 3.*
4. **`RENDER` merge** back into `page.state.txt` (rung 5 glue) —
   mutated DOM now visibly changes the window. Verify with a real
   `innerHTML=`-style fixture + live window. *Commit 4.*
5. **CPU budget + node cap** harden + `usleep` idle-loop discipline +
   manager SIGKILL/restart guards. *Commit 5.*
6. **Docs**: mark rung 2 (+ rung 5 glue + §3 worker plumbing) done in
   the roadmap; note what rungs 3/4/7 remain. *Commit 6.*

## 8. PHASE 2 — rungs 3, 4, 5 + the rung-6 remainder (the "what's next" / hand-off)

> This is the natural continuation once Phase 1 lands. It is written to be
> **hand-off-ready**: any agent can pick up any commit-level rung below and
> ship it green. "Rung 3" is NOT one thing to a newcomer — clarify the
> roadmap rung numbering for anyone reading this:
>
> | roadmap rung | what | lands in |
> |---|---|---|
> | 2 | DOM tree + accessors | **Phase 1** (this doc §7) |
> | 3 | **events + event loop** (EventTarget, timers, DOMContentLoaded/load, microtask drain) | **Phase 2** |
> | 4 | XHR / `fetch()` via manager RPC | **Phase 2** |
> | 5 | re-serialize mutated DOM → `page.state.txt` | **Phase 1 step 4** (glue already pulled in) |
> | 6 | BOM odds & ends (part done: URL/URLSearchParams, history, matchMedia, getComputedStyle, MutationObserver, atob/btoa, timers stubs, cookie stub) | **partial now; real version in Phase 2** |
> | 7 | CSS/layout awareness (getBoundingClientRect, offsetWidth, display:none) | deferred — separate effort |
>
> So: **"run/rung 3" = events + the event loop.** It is the first thing in
> Phase 2, and the single biggest unlock after rung 2 (click handlers,
> tab widgets, accordions, form validation become possible).

### 8.1 Rung 3 — events + the event loop (first Phase-2 chunk)

`EventTarget`-style `addEventListener/removeEventListener/dispatchEvent`,
an `Event` object (`type`/`target`/`preventDefault`/`stopPropagation`),
on-property handlers (`el.onclick = fn`). Timers:
`setTimeout/setInterval/clearTimeout/clearInterval` (now really fire —
replace the Phase-1/§7 step-3 no-op stubs with a C min-heap keyed by due
time), `queueMicrotask`, `requestAnimationFrame` (~16ms timer).
Lifecycle: after top-level run, fire `DOMContentLoaded` then `load` /
`window.onload`. **The loop:** drain microtasks → run due timers →
drain microtasks → repeat until empty or budget hit (§2 per-page
wall-clock + timer-invocation cap + node cap). The worker already runs in
a `usleep` RPC loop, so this slots straight in. *≈300 lines + timer heap.*

### 8.2 Rung 4 — network from JS (XHR / fetch via manager RPC)

`XMLHttpRequest` (async first — the worker is resident, so a callback can
come back over the same socketpair) then `fetch()` returning a real
Promise (Duktape 2.7 has Promise; else a microtask-queue polyfill).
New RPC: `manager <- worker: FETCH <id> <method> <url>` and
`manager -> worker: FETCHED <id> <status>\n<len>\n<body>`. CORS /
same-origin stay permissive (local browser). **Payoff:** SPAs that fetch
JSON then render become usable. *≈200 lines + RPC cases.*

### 8.3 Rung 6 remainder — the real, not just stub, BOM

- `history.pushState/replaceState` → manager updates address bar without a
  fetch (new `NAVIGATE-URL` RPC or `#.desktop` marker echo-lifo).
- `location.assign/replace/reload` → manager navigates (same channel).
- `document.cookie` **file-backed** `#.desktop/nb_cookies.txt` jar — now
  trivially doable in the resident worker (read/write on get/set), replacing
  the Phase-0 empty-jar stub.
- `MutationObserver` can stay a no-op unless a target site needs it.

### 8.4 When is Phase 2 "done"?  (the §5 payoff table)

| after | what works |
|---|---|
| Phase 1 (rung 2 + render glue) | scripts read/annotate DOM; progressive-enhancement JS, and the mutated DOM **shows** in the window |
| Phase 2: 8.1 (rung 3) | click handlers, tab widgets, accordions, form validation |
| Phase 2: 8.2 (rung 4) | "load more", infinite scroll, search-as-you-type, simple fetch-JSON SPAs |
| Phase 2: 8.3 (rung 6 real) | client-side routing, cookie-gated content, back/forward |
| Phase 3 (roadmap rung 7, optional) | carousels, lazy images, sticky UI, anything that measures layout |

### 8.5 Phase-2 execution order (each green + shippable)

A. Rung 3 event loop + timers + lifecycle — *Commit 7*
B. `dispatchEvent` + on-property handlers + `EVENT <selector>` RPC
   (user click in the window reaches a scripted el) — *Commit 8*
C. Rung 4 XHR (async) — *Commit 9*
D. `fetch()` + Promise — *Commit 10*
E. Rung 6 real: history/location navigation RPC — *Commit 11*
F. Rung 6 real: file-backed cookies — *Commit 12*
G. Docs: phase 2 status in the roadmap — *Commit 13*

Each of A–G is independently hand-off-able.

## 9. Explicit non-goals (Phase 1 milestone)

- No `fetch()`/XHR (rung 4), no real event loop / timers firing callbacks
  (rung 3 — we run top-level scripts + one microtask drain only), no
  `page.js.extN.js` ordering beyond today's concat, no HTML5 full error
  recovery, no CSS/layout (rung 7), no `history` navigation to manager.
- `document.cookie`/`matchMedia`/`getComputedStyle` remain the rung-6
  stubs already landed — fine for this milestone.
- **All of the above are Phase 2 work (§8), deliberately walled off so
  Phase 1 ships a working DOM+render loop without scope creep.**

## 10. Risks / mitigations

| risk | mitigation |
|---|---|
| HTML parse entangles the manager | keep it a separate `nb_dom.c`; first cut tolerant-but-simple; never block `do_fetch` on parse failure (degrade to old extractor) |
| Worker leaks / doesn't die | budget + node cap + SIGKILL-on-overrun + reap on manager exit |
| DOM<->JS handle leak | Duktape finalizers free the C handle (roadmap §2) |
| Split-brain doc trees (manager vs worker) | manager owns parse + static output; worker owns mutable tree + JS; only the worker's `RENDER` feeds `page.state.txt` after scripts |
| Scope creep | §9 non-goals are hard walls; each of Phase-1 §7 / Phase-2 §8.5 commits is independently shippable |

## 11. Open questions for the user (before step 1)

1. Ship the HTML/dom to the worker via **file paths** (recommended) or
   inline payloads? → I'll take file paths unless told otherwise.
2. One shared worker for all tabs, or keyed per-tab? → start single,
   key-by-tab later.
3. Should the static non-JS extractor stay as-is (both paths) for the
   first cut, or be re-derived from the new DOM immediately? → keep both,
   reconcile later (lower risk).
