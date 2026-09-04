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

> **DONE — 2026-09-03**, branch `chtpm-var-substitution`. Only
> `ops/nb_js_eval.c` touched; test `network/tests/rung1_globals.js`.
> Added: `window`/`self`/`globalThis` = real Duktape global (was the
> `localStorage` stub — index bug); `navigator` (userAgent, language,
> languages, platform, onLine, cookieEnabled, doNotTrack);
> `screen` (width/height/availWidth/availHeight/colorDepth/pixelDepth);
> `location` protocol/host/hostname/port/pathname/search/hash/origin
> parsed from the href in C, plus no-op assign/replace/reload;
> `window.name`/`closed`/`length`. Verified `typeof window==="object"`,
> `window===globalThis===self`, `window.x=5;x===5`,
> `"addEventListener" in window` → false w/o throw, full `location.*`
> on a real href. On google's extracted scripts the raw ERROR count is
> unchanged (remaining failures are ES6 syntax + DOM = rungs 2/§4), but
> `page.js.ext1.js` now clears its `window.location.pathname` failure
> and reaches a later DOM-shaped one. Details:
> `09-appendix/PROGRESS-nb-js-rung1.md`.

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

> **PARTIAL — 2026-09-04, branch `chtpm-js-rungs`** (parallel-safe,
> isolated entirely to `ops/nb_js_eval.c`; no manager/fetch changes).
> Premature pure-JS / no-I-O pieces landed so analytics/consent/routing
> scripts stop throwing:
> - `URL` + `URLSearchParams` (parse, resolution, searchParams) — `91819f2e`
> - `history` (pushState/replaceState in-memory stack + state/length,
>   no real navigation) — `b079f0c9`
> - `window.matchMedia` / `getComputedStyle(prop)` / `MutationObserver`
>   no-op stubs — `b079f0c9`
> - `atob`/`btoa` Base64 + `setTimeout`/`setInterval`/`clear*` (return
>   ids, callback never fires — one-shot has no event loop) — `e71232d1`
> - `document.cookie` empty-jar getter/setter (no throw; file jar still
>   a C job later) — `f5f86b4d`
> **Still to do (needs C or the worker):** real `history`/`location`
> navigation pushed to the manager, file-backed `document.cookie` jar,
> a real timer/event loop (rung 3). Tests under `network/tests/rung6_*.js`
> all +OK|1; 5 suites green.


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

See §8 for what "parity" actually means and how the ceiling could be
raised. Rungs 1-5 get "most content sites and light SPAs usable", which
is the near-term goal.

---

## 8. The ceiling — and the DECISION (2026-09-03)

> **DECIDED: path A — hand-built, a few pages of code at a time, our
> own rendering. NO engine embed (not CEF "all of chromium", not
> WebKitGTK).** The GTK/WebKit embed PoC is parked at
> `44.xyz.01.00/&.hq-apps/network/_attic-gtk-embed/` in case that's ever
> revisited. What follows is the "how the ceiling works" reasoning that
> led here.

### Why not embed an engine

- **CEF** = all of Chromium (~200 MB, you ship it). Rejected: "don't
  pull all the chromium."
- **WebKitGTK** (already on the box, 86 MB .so) via XEmbed = the engine
  owns the content rectangle's pixels; khtpm can't overlay on it, and
  it drags a GTK window. Rejected: "use our own [visual]."
- The only "our chrome + real engine, khtpm owns every pixel" option is
  **offscreen render → blit the buffer** (CEF OSR). Still CEF. Rejected.

So: the content pane stays khtpm-drawn `<text>`/`<item>` rows. We make
those rows *smart* (driven by a real DOM after JS runs), not pixel-
faithful. Visual fidelity (real CSS layout) is a later, small,
hand-rolled lift — "when we can", §7.

### What "a few pages at a time" buys, concretely

Each rung below is a few hundred lines of *our* C, on top of Duktape
(one vendored file; swap to QuickJS — also one file — only if ES6
*syntax* becomes the wall, §4). None of it is an engine.

- **Rung 2** (DOM tree in the manager, lazy-exposed to JS): ~300-500
  lines. Unblocks most progressive-enhancement JS.
- **Rung 3** (events + budgeted loop): ~300 lines + a timer heap.
- **Rung 4** (XHR/fetch via manager RPC): ~200 lines.
- **Rung 5** (mutated DOM → `page.state.txt`): ~100 lines of glue —
  the payoff: JS-heavy pages become usable *as readable/clickable
  content*.
- **Rung 6** (history/location/cookies): incremental, as sites demand.

Ceiling of this hand-built path: **the readable web + light SPAs, as
text + links + images in our chrome** — not pixel-parity, not the
application web (Gmail/Figma/WebGL). That is the accepted target.

---

### (background) "Parity" is two different problems

### "Parity" is two different problems

1. **Functional usability** — can a human read the page, click things,
   fill a form, log in, search. This is *bounded and reachable* for a
   large fraction of the web (see §5). Rungs 1-6 + a real block/inline
   CSS layout pass get you: content sites, wikis, docs, blogs, forums,
   most reference/government/library sites, and light SPAs.
2. **App-platform fidelity** — running the modern web *as an application
   runtime*: WebGL/WebGPU, WASM, Service Workers, WebRTC, Web Audio,
   IndexedDB, the full CSS spec (flex/grid/subgrid, containment,
   transforms, filters, `@container`), a JIT JS engine, the HTML5
   parser's exact error recovery, font shaping, a compositor, site
   isolation, HTTP/2-3 + TLS + cache semantics. This is millions of
   lines of C++ per browser, hundreds of full-time engineers. Hand-
   building it in this house's C is genuinely not realistic.

My "forever" applied to #2. #1 is a real project with an end.

### Which sites fall where

| site class | reachable by hand (rungs + CSS layout)? |
|---|---|
| static / server-rendered content, docs, wikis, forums, blogs | **yes — to near-full usability** |
| progressive-enhancement JS (widgets, forms, menus) | **yes** (rungs 2-3) |
| light SPA: fetch JSON → render a list/detail | **yes** (rungs 4-6) |
| heavy SPA (Gmail, Docs, Figma, Discord, Maps, Notion) | **no** — needs #2 |
| canvas/WebGL games, WASM apps, DRM video, WebRTC calls | **no** — needs #2 |

Realistically the hand-built ceiling is **"the readable web + simple
interactive sites"** — which is most of the *informational* web, and
none of the *application* web.

### Three ways to raise or remove the ceiling

**A. Stay hand-built, aim at #1 only.** Rungs 1-6 + add a real CSS
**block/inline/table/basic-flex layout engine** (this is the missing
piece for visual fidelity — right now content renders as a flat text
list). Reference implementations that prove the size: `litehtml`
(~30k lines C++, block+inline+tables+some flex, no JS — used by several
small mail/help viewers), Dillo's own layout (C), NetSurf's
libnslayout, the old KHTML. Pair `litehtml` (or a from-scratch
equivalent) + **QuickJS** + the fetch layer and you have a "small
browser" that renders a genuine fraction of the web correctly. Scope:
multi-month, but *bounded* — it has a done state.

**B. Embed a real engine for the browser widget only.** The nuclear
option, and a legitimate answer to "what can I do to change that":
decide the house is OK taking one large dependency *for this one
widget*.
- **Servo** (`libservo`, Rust) — actively revived since 2023, designed
  to be embeddable, MPL-licensed, no Google/Apple control. Closest to
  "a real modern engine you can embed without shipping Chromium."
- **WebKitGTK** — mature, embeddable, what GNOME Web/epiphany uses.
- **CEF** (Chromium Embedded Framework) — full Chrome, ~200 MB, the
  "it just works" option, but it *is* shipping Chromium.
- Trade-off: instant #2-level parity, but it's no longer "our small C",
  it's a browser engine as a black-box dependency + its update
  treadmill + its attack surface. That's a house-values decision, not a
  technical one.

**C. Hybrid.** Keep the hand-built engine as the default (fast, small,
private, no JS-app attack surface), and add a "open in real engine"
escape hatch (spawn a WebKitGTK/Servo window) for the pages the
hand-built one can't handle. Best of both; the escape hatch is ~a day
of work once you pick an engine, and most browsing never needs it.

### What *you* can do to move this

1. **Pick the target.** "Readable web, fully usable" (A) vs "runs
   everything" (B) vs "hand-built default + escape hatch" (C). Everything
   downstream depends on that one call.
2. If **A/C**: fund the **CSS layout engine** — that's the real
   long-pole for visual parity, more than JS. Decide from-scratch vs
   vendoring `litehtml`.
3. If **B/C**: pick the engine (Servo if "our values matter", WebKitGTK
   if "mature + embeddable", CEF if "just works"), and accept the
   dependency.
4. Regardless: **rung 1 is worth doing now** (see §6) — it's pure win
   under every target.

### Honest bottom line

- "A hand-built browser that renders and lets you use most *content*
  sites and light SPAs" — **yes, reachable**, it's a real project with a
  finish line (rungs + a CSS layout engine).
- "A hand-built browser at parity with Chrome as an app platform" —
  **no**, that specific thing is not realistic without embedding a real
  engine.
- The ceiling is a *choice*, not a law: option B/C removes it entirely
  at the cost of one large dependency.

---

## 6. First concrete step

Rung 1 in `nb_js_eval.c` `install_host()`: make `window`/`self`/
`globalThis` the real global, add `navigator`/`screen`/`location`
(parsed from `argv[3]` href). ~40 lines, no manager changes, kills a
large share of the current `js: script error` rows immediately. Then
decide one-shot-plus vs worker (§3) before starting rung 2.
