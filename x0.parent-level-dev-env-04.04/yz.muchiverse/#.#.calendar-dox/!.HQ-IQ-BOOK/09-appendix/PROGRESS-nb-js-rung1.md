# PROGRESS — network-browser JS engine, Rung 1

**Status:** DONE — 2026-09-03
**Branch:** `chtpm-var-substitution`
**Roadmap:** `08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md` §1 rung 1 + §6
**File touched:** `44.xyz.01.00/&.hq-apps/network/ops/nb_js_eval.c` (only)
**Test added:** `44.xyz.01.00/&.hq-apps/network/tests/rung1_globals.js`

## What changed in `install_host()`

1. **`window` / `self` / `globalThis` are now the real Duktape global object.**
   The old code aliased them onto the `localStorage` stub object (wrong
   target — a stack-index bug). Now the absolute index of
   `duk_push_global_object()` is captured (`g`) and each name is defined
   as a property of that object whose value is that same object.
   - `window === globalThis === self` → `true`
   - `window.foo = 42; foo === 42` → works
   - `"addEventListener" in window` → `false`, does not throw

2. **`navigator`** — plain data props, no functions:
   `userAgent = "Mozilla/5.0 (X11; Linux x86_64) nb_js_eval"`,
   `language = "en"`, `languages = ["en"]`, `platform = "Linux x86_64"`,
   `onLine = true`, `cookieEnabled = false`, `doNotTrack = null`.

3. **`screen`** — `width/availWidth = 1920`, `height/availHeight = 1080`,
   `colorDepth/pixelDepth = 24`.

4. **`location`** — kept the existing `href` getter; added string props
   parsed from `g_href` (argv[3]) in C: `protocol`, `host` (host:port),
   `hostname`, `port`, `pathname` (defaults to `"/"`), `search` (with
   leading `?`), `hash` (with leading `#`), `origin` (`proto//host`, else
   `"null"`). Added no-op `assign` / `replace` / `reload` so a call does
   not throw — real navigation is rung 6.

5. **`window.name = ""`** (writable), **`window.closed = false`**,
   **`window.length = 0`**.

Nothing else touched: DOM (`getElementById` still stub → `null`), events,
timers, XHR/fetch, manager, renderer, templates — all still rungs 2-7.

## Verified

`tests/rung1_globals.js` run with
`href = https://user@example.com:8443/a/b?x=1&y=2#frag`:

```
LOG|object object object
LOG|win===global true win===self true
LOG|assign 42 42
LOG|ua Mozilla/5.0 (X11; Linux x86_64) nb_js_eval lang en langs en
LOG|platform Linux x86_64 onLine true cookieEnabled false dnt null
LOG|screen 1920 1080 24
LOG|ael in window false
LOG|loc https: example.com:8443 example.com 8443 /a/b ?x=1&y=2 #frag https://example.com:8443
LOG|href https://user@example.com:8443/a/b?x=1&y=2#frag
LOG|name "" closed false length 0
LOG|getElementById null
LOG|nav funcs did not throw
OK|1
```

Edge cases: no href → `pathname="/"`, `origin="null"`, `OK|1`.
`https://www.google.com/` → `host=hostname=www.google.com`, `port=""`,
`pathname="/"`, `origin="https://www.google.com"`.
(Known minor: a non-`//` URI like `about:blank` is parsed as
`hostname=about port=blank` — acceptable for rung 1; only http(s) matters.)

## Real-page check — `tmp/page.js` + 4 extracted google scripts, href `https://www.google.com/`

Compared the pre-change binary (built from HEAD) vs the new one:

| script | old | new |
|---|---|---|
| `page.js` | OK\|1, 0 ERROR | OK\|1, 0 ERROR (unchanged) |
| `page.js.ext0.js` | SyntaxError line 3 (arrow fn / ES6) | same — **rung: QuickJS/ES6, not 1** |
| `page.js.ext1.js` | `TypeError: cannot read property 'includes' of undefined` (`window.location.pathname` was undefined) | **now gets past that**; fails later at `document.documentElement.lang` — **rung 2 (DOM)** |
| `page.js.ext2.js` | SyntaxError line 1 (webpack ES6 bundle) | same — **QuickJS/ES6** |
| `page.js.ext3.js` | `getElementsByTagName ... undefined not callable` | same — **rung 2 (DOM)** |

So rung 1 does **not** drop google's raw ERROR count (its remaining
failures are all ES6 syntax or DOM), but `ext1` demonstrably advances:
the `window.location.*` failure is gone and execution now reaches a
later, DOM-shaped failure. Feature-detection against `window` /
`navigator` / `location` no longer throws (synthetic `tmp/fd.js`-style
analytics/consent snippet ran clean end-to-end).

## What rung 2 needs to know

- `document.getElementById` / `querySelector` are still native stubs
  returning `null` (see `install_host`, the `native_null` binding).
  `document` has no `documentElement` / `head` / `body` — the very next
  failures on real pages (`ext1`, `ext3`) are exactly these.
- `window` is now a normal extensible global, so DOM globals can be added
  as ordinary props on index `g` in `install_host`.
- `location` is a data object built once from `g_href`; when rung 6 wires
  navigation it will need getters/setters instead of the current plain
  strings + no-op methods.
- Duktape here is ES5.1-ish: google's bundled scripts (`ext0`, `ext2`)
  die on arrow functions / other ES6 at *parse* time — matches roadmap
  §4. No amount of host-binding work fixes those; that is the
  Duktape→QuickJS decision.
