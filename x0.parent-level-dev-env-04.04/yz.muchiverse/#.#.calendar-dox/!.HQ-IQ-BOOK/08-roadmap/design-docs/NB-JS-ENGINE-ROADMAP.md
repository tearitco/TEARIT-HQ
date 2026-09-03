# network-browser JS engine — roadmap to "real websites work"

**Status:** design / not started
**Date:** 2026-09-03
**Scope:** `44.xyz.01.00/&.hq-apps/network/` — `ops/nb_js_eval.c`,
`js/duktape.*`, and the `network_browser_manager.c` fetch/script/render
pipeline.

---

## 0. Where we are

`nb_js_eval.c` (245 lines) already embeds **Duktape**, a real ES5.1
engine — so the *language* is done (closures, regex, JSON, Array/
String/Object/Math, try/catch, prototypes). What is missing is the
**host environment** a page expects. `install_host()` today stubs only:

| provided | level |
|---|---|
| `console.log/info/warn/error`, `print` | real (→ `LOG\|` rows) |
| `document.title` get/set | real (→ `TITLE\|`) |
| `document.write/writeln` | real (→ `TEXT\|` rows) |
| `document.getElementById` / `querySelector` | **stub → null** |
| `location.href` | getter only, no navigation |
| `localStorage` / `sessionStorage` | no-op stubs |
| `window` / `self` / `globalThis` | **BUG: alias the storage stub, not the JS global** |

Pipeline: `network_browser_manager.c` `do_fetch()` strips `<script>`
bodies out of the HTML (`~line 479-483, 723`), concatenates them into
`tmp/page.js`, runs `nb_js_eval.+x page.js js.effects.txt <href>
<title>` **once**, and merges `LOG\|`/`TEXT\|`/`TITLE\|` back into
`page.state.txt`. There is no DOM tree, no document-order execution, no
event loop, no XHR/fetch, no feedback from JS mutations to what is
rendered.

So a page's scripts run in a vacuum: any `document.getElementById(x).y`
throws `TypeError` (that is the `js: script error` noise — see
`PROGRESS-network-browser-xhtpm.md`).

---

## 1. The ladder (each rung is shippable on its own)

### Rung 1 — fix the global object  *(hours)*
`window` / `self` / `globalThis` must BE the Duktape global object, not
the `localStorage` stub. Then `typeof window === "object"`,
`window.foo = 1; foo === 1`, and `"addEventListener" in window`
feature-detection stops throwing. Also add cheap always-safe globals:
`navigator = {userAgent:"...", language:"en", platform:"linux"}`,
`screen = {width:1920,height:1080}`, `window.name=""`,
`location` with real `protocol/host/hostname/pathname/search/hash`
parsed from the href (still no navigation yet).
**Payoff:** a large fraction of "cannot read property of undefined" on
`window.*` / `navigator.*` disappears with ~40 lines.

### Rung 2 — a real DOM tree  *(the big one — 1-2 focused passes)*
The manager already tokenizes tags. Extend that into a **node tree**
kept in C: `{tag, attrs[], text, parent, first_child, next_sibling}`.
Do NOT serialize the whole tree into JS — expose it lazily through
native Duktape functions that walk the C tree on demand (Duktape
finalizers free the C-side handle).

Minimum node API to implement:
- read: `tagName`, `id`, `className`, `getAttribute`, `attributes`,
  `parentNode`, `childNodes`, `children`, `firstChild`, `nextSibling`,
  `textContent` (get), `innerHTML` (get), `value` (form fields)
- write: `setAttribute`, `removeAttribute`, `textContent` (set),
  `innerHTML` (set → re-parse fragment into the C tree),
  `appendChild`, `removeChild`, `insertBefore`, `replaceChild`,
  `classList.add/remove/toggle/contains`, `style.<prop>` (store on a
  per-node map; no layout)
- `document`: `documentElement`, `head`, `body`, `createElement`,
  `createTextNode`, `getElementById`, `getElementsByTagName`,
  `getElementsByClassName`, `querySelector` / `querySelectorAll`
  (start with: `#id`, `.class`, `tag`, `tag.class`, descendant combos;
  punt on `:nth-child`, attribute selectors, `>` etc. at first)

### Rung 3 — events + the event loop  *(1 pass, needs care for CPU)*
- `EventTarget`: `addEventListener` / `removeEventListener` /
  `dispatchEvent`; an `Event` object (`type`, `target`,
  `preventDefault`, `stopPropagation`); on-property handlers
  (`el.onclick = fn`).
- Timers: `setTimeout` / `setInterval` / `clearTimeout` /
  `clearInterval`, `queueMicrotask`, `requestAnimationFrame` (map to a
  ~16ms timer). Store callbacks in a C min-heap keyed by due time.
- Lifecycle: after the initial parse + top-level script run, fire
  `DOMContentLoaded` then `load` (and `window.onload`).
- **The loop:** drain microtasks, then run due timers, then microtasks
  again, … until the queue is empty OR a budget is hit. See §2.

### Rung 4 — network from JS  *(1 pass)*
- `XMLHttpRequest` (sync form first — trivial: native fn shells `curl`
  and returns), then async.
- `fetch()` returning a real `Promise` (Duktape 2.x has Promise; if the
  build lacks it, ship a tiny polyfill driven by the Rung 3 microtask
  queue).
- The native side calls back into the manager (see §3) to actually do
  the request; responses feed the JS callback via the event loop.
- CORS/same-origin: keep permissive for a local browser; just note it.
**Payoff:** SPAs that fetch JSON then render become usable.

### Rung 5 — render feedback loop  *(glue, ~1 pass)*
After scripts + the event loop go quiescent, **re-serialize the
mutated DOM tree** → `page.state.txt` (TITLE / TEXT / LINK / IMG rows),
exactly the format the projector already consumes. Now `innerHTML =`,
`appendChild`, `document.write` after load, `el.textContent = ...`
actually change what the window shows. This is the visible payoff of
rungs 2-4.

### Rung 6 — BOM odds & ends  *(incremental, as sites demand)*
`history.pushState/replaceState` (→ manager updates the address bar
without a fetch), `location.assign/replace/reload` (→ manager
navigates), `document.cookie` backed by a `#.desktop/nb_cookies.txt`
jar, `window.matchMedia` stub, `window.getComputedStyle` returning a
plausible stub, `MutationObserver` (can no-op then improve),
`URL` / `URLSearchParams` (pure JS polyfill).

### Rung 7 — CSS/layout awareness  *(optional, large, defer)*
`getBoundingClientRect`, `offsetWidth/Height`, `display:none`
visibility. Needed by carousels / lazy-loaders / sticky headers.
Big; only worth it once rungs 1-6 land and real sites are close.

---

## 2. CPU safety (house rule — non-negotiable)

The JS engine is the one place in this app that can spin forever. Every
rung must respect:
- **Per-page wall-clock budget** (e.g. 2000 ms total for parse + top
  script + event loop). Duktape supports an exec-timeout via
  `DUK_USE_EXEC_TIMEOUT_CHECK` — build with it and a deadline check.
- **Timer iteration cap** (e.g. 5000 callback invocations per page) —
  kills `setInterval` busy loops.
- **DOM node cap** (e.g. 50k nodes) — kills runaway `appendChild`.
- **`requestAnimationFrame` cap** (e.g. 120 frames ≈ 2s) then stop
  calling it back.
- The **manager kills a worker that overruns** its budget (SIGKILL,
  render whatever `page.state.txt` has).
- The one-shot / worker process still `usleep`s its idle loop; never
  a bare spin. (`aigent-testing-k9.txt`, `!.HOUSE_STDS.md §C`.)

---

## 3. Architecture: one-shot vs persistent worker

Today `nb_js_eval` is a **one-shot** (`main()` → eval → write → exit).
That cannot support an event loop that needs to `fetch()` mid-run
(the data has to come from somewhere while JS is paused).

**Recommended: a persistent JS worker process.** Started by the
manager (its own child, NOT a renderer `<module>`), talking over two
FIFOs / a socketpair with a tiny line RPC:

```
manager -> worker :  LOAD <href>\n<html-len>\n<html bytes>
worker  -> manager:  FETCH <id> <method> <url>        (async XHR/fetch)
manager -> worker :  FETCHED <id> <status>\n<len>\n<body>
worker  -> manager:  NAVIGATE <url>                   (location.assign)
worker  -> manager:  RENDER\n<page.state rows>        (quiescent, rung 5)
manager -> worker :  EVENT click <selector>           (user clicked a scripted el)
```

The **worker** owns: Duktape heap, the DOM tree, the event loop, timers.
The **manager** owns: curl / network, tabs / history, and writing
`page.state.txt` from the worker's `RENDER` payload. One worker per
tab (or one shared, keyed by tab id). Kill + restart the worker on
navigation or on budget overrun.

Keep the current one-shot `nb_js_eval.+x` around as the rung-0/1
fallback and for tests.

---

## 4. Engine choice — Duktape vs QuickJS

Duktape is ES5.1 + a little ES6 (let/const, arrow fns, TypedArrays if
configured). **Modern bundled sites (webpack/Babel-to-ES2017+, or
untranspiled ES2020) will fail on *syntax* before any DOM work
matters** — optional chaining `?.`, nullish `??`, `async`/`await`,
class fields, spread in calls, `BigInt`, top-level `await`.

If language coverage becomes the wall:
- **QuickJS** (Fabrice Bellard) — one `.c` + `.h`, ~ES2020 complete
  incl. `async`/`await`, generators, Proxy, BigInt, modules. Bigger
  than Duktape but still embeddable, still no deps. Same host-binding
  shape (`JS_NewCFunction`, `JS_SetPropertyStr`), so rungs 1-6 port
  with mechanical changes.
- Recommendation: build rungs 1-3 on Duktape (fast, already vendored).
  When a real target site fails on syntax, swap to QuickJS behind the
  same `install_host()` seam before investing in rungs 4-7.

---

## 5. Realistic outcome per rung

| after rung | what works |
|---|---|
| 1 | feature-detection scripts stop throwing; analytics/consent snippets no-op cleanly; static pages with a bit of JS look right |
| 2 | scripts that read/annotate the DOM (add classes, read data-attrs); server-rendered pages with progressive-enhancement JS |
| 3 | click handlers, tab widgets, accordions, form validation, dropdowns |
| 4 | "load more", infinite scroll, search-as-you-type, simple SPAs that fetch JSON |
| 5 | the above actually *re-render* in the window |
| 6 | client-side routing, cookie-gated content, `history` back/forward |
| 7 | carousels, lazy images, sticky UI, anything that measures layout |

Full parity with a real browser is out of scope forever. Rungs 1-5 get
"most content sites and light SPAs usable", which is the goal.

---

## 6. First concrete step

Rung 1 in `nb_js_eval.c` `install_host()`: make `window`/`self`/
`globalThis` the real global, add `navigator`/`screen`/`location`
(parsed from `argv[3]` href). ~40 lines, no manager changes, kills a
large share of the current `js: script error` rows immediately. Then
decide one-shot-plus vs worker (§3) before starting rung 2.
