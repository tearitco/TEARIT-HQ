# JS engine swap insight — Duktape is the floor, QuickJS is the door (row 31)

**Status:** decision recorded / transplant NOT started
**Date:** 2026-09-17
**Scope:** `44.xyz.01.00/&.hq-apps/network/` — the `nbjs` JS engine
(`&.hq-apps/js/duktape.*`) and its C boundary in `ops/nb_js_worker.c`.
**Companion:** `NB-JS-ENGINE-ROADMAP.md` (where we are), the REAL-SPA
roadmap row 31 (execute youtube's real JS bundle).

---

## 0. The one-line insight

**The engine's parser is ES5.1-only, and modern bundles (youtube's
included) are ES2020+. No prelude polyfill can help — a polyfill runs at
runtime; the parser rejects the source before one byte of page.js runs.
Duktape 3 does not fix this (its changes are string encoding, not
syntax). QuickJS is the only C-embeddable engine that actually parses
the syntax google ships.**

---

## 1. Evidence: the engine is ES5.1, proven, not assumed

The worker's Duktape is the amalgamated `&.hq-apps/js/duktape.c`
(DUK_VERSION 20700 = 2.7.0). Probed by compiling it **standalone**
(`duk_create_heap` + `duk_peval_string`) — zero worker code involved:

| probe | result |
|---|---|
| `class F { constructor(){} }` | `SyntaxError: parse error` |
| `var f = (a)=>a+1; f(1)` | `SyntaxError: parse error` |
| `` var t = `hi ${1+1}` `` | `SyntaxError: invalid token` |
| `var {p,q}={p:1,q:2}` | `SyntaxError: invalid variable declaration` |
| `Math.max(...[1,5,3])` | `parse error` |
| `var x = null ?? 5` | `parse error` |
| `a?.b?.c` | `parse error` |
| `function* g(){}` / `async function f(){}` | both `parse error` |
| `get / set` accessor (ES5) | works |

Forcing `-DDUK_USE_ES6` at compile time changes **nothing**: the
amalgamation's own token table even lacks the parser/compiler paths —
`DUK_TOK_CLASS` (line 3894) exists as a number but **no `case DUK_TOK_CLASS`**
consumes it anywhere in `duktape.c`. This is a reduced build; ES6 cannot
be switched on from the config.

Corollary: **any** put-precompile-transpiling idea would be fighting the
engine; the correct fix is a parser that speaks the language.

---

## 2. Why we didn't hit this sooner (and why nothing was "wasted")

The 2026-09-02/09-03 roadmaps chose Duktape as "a real ES5.1 engine — so
the *language* is done" (NB-JS-ENGINE-ROADMAP §0). That was true for the
generation of receipts we built: skeleton pages, click handlers, XHR,
fetch, DOM, cookies — all written in ES5.1-compatible page JS, all
passing. **Rows 32/34/35 shipped exactly because their page JS stayed
within what the parser accepts.**

Row 31 (execute youtube.com's *real* bundle) is the first requirement
that forces modern syntax. `desktop_polymer.js`/`base.js` are minified
ES2017+: classes, arrows, template literals, destructuring, spread,
optional chaining. The parser is a **hard floor** — there is no amount of
host-surface work (jars, SHA-1, fetch, signatures) that changes it.

**Not wasted:** every receipt, the pipe protocol, the DOM/CSS/event
state, and the boundary pattern port to the new engine (see §5).

---

## 3. Duktape 3 is NOT the answer

Duktape 3.0 release notes (raia.app mirror of the duktape doc set) center
on **internal string representation — WTF-8**, non-BMP codepoint
handling, `String.fromCharCode` semantics. That is correctness/encoding
work, **not new grammar**. Duktape — across releases — still documents
its post-ES5 status as partial ES6/ES7; it has **no** optional chaining,
**no** nullish coalescing, **no** async/await (deliberately low-priority
in the project's design). Upgrading to Duktape 3 buys nothing for row 31.

---

## 4. QuickJS is the door

| | Duktape (this repo) | Duktape 3 | QuickJS |
|---|---|---|---|
| Baseline | ES5.1, partial ES6/7 | same + WTF-8 | **ES2020–ES2024+, high Test262 pass** |
| class / arrow / template | ❌ | ❌ | ✅ |
| destructuring / spread | ❌ | ❌ | ✅ |
| `?.` `??` | ❌ | ❌ | ✅ |
| async/await | ❌ | ❌ | ✅ (native microtask job queue) |
| inference | refcount+MS | refcount+MS | refcount+cycle |
| embeddable C API | ✅ (value stack) | ✅ | ✅ (`JSValue` handles) |
| bloat | small | small | small (2.7MB source < duktape) |

QuickJS is MIT, integrates as `quickjs.c`/`quickjs.h`, and is already the
sandbox both Frida and Figma stand on — same embeddable-C shape the
worker's `duk_*` boundary takes. Native **microtask job queue** means the
engine's emulated Promise drain actually **gets simpler**.

## 5. Architecture is safe — measured, not guessed

`ops/nb_js_worker.c` is 3912 lines. Broken by coupling to the engine:

| layer | rough size | survives? |
|---|---|---|
| pipe `LOAD/RENDER/STATUS`, DOM tree walk, cookie jar, storage jars, CSS cache, `nb_fetch_sync` socket/curl body, `__nb_sha1`, timer/RAF scheduling | ~2100 (130 house fns, 119 protocol refs) | **unchanged** |
| 105 `duk_ret_t` natives + `duk_push_*`/`duk_get_*`/`duk_put_prop_*` value-stack wrap | ~750 (776 `duk_` refs, 54 distinct API names) | **the transplant** |
| prelude `nb_host.h` (fetch/Promise/XHR/URLSearchParams, 756 lines) | — | mostly survives; Promise polyfill+microtask emulation **deleted** in favor of native |

Only one binary consumes Duktape in the whole tree (`nbjs` Makefile,
`JS_DIR=../js`). The transplant is single-file: replace the contents of
`&.hq-apps/js/`, rewrite the 750-line boundary (arg-in/value-out shapes
only — the native *bodies* are DOM/cookie/CSS house logic that stays).

All **44 test harnesses** drive the worker over pipes and never touch
`duk_*` — they are the regression net and survive as-is.

---

## 6. Decision & responsibility ladder

- **Decision:** move the JS engine from Duktape to QuickJS for row 31.
  This is the only path that parses what youtube actually ships.
- **Do it most carefully, in rungs, receipts before worker edits:**

1. **Vendor QuickJS** into `&.hq-apps/js/` (e.g. `quickjs.c/.h` +
   `quickjs-libc`/`cutils` as needed). No worker changes.
2. **Standalone probe** (`duk_create_heap`-style, same pattern that
   proved ES5.1): compile QuickJS, `JS_Eval` a modern-syntax file
   (`class`, `?.`, `??`, `async/await`) and show parse+run. Print the
   results; record as a receipt in this doc's work log.
3. **Prelude smoke:** load `ops/nb_host.h`'s prelude strings into the
   probe; confirm they evaluate under QuickJS (they are ES5-compatible
   by construction, so expected green; any failure is a porting data
   point before the worker is touched).
4. **Boundary transplant** in `nb_js_worker.c`: map the 54 distinct
   `duk_*` API names to QuickJS equivalents; keep native *bodies*
   untouched; replace Promise polyfill + microtask emulation with the
   native job queue.
5. **Full `make check` (44 suites) on the NEW engine.** No row flipped
   until this is green from a fresh build + fresh run.
6. Only then tackle row 31's real bundle receipt (file:// fixture with a
   real slice, visitor+signature context attached — the fetch()/Promise
   surface we already proved generically).

## 7. What we are NOT doing (scope guardrails)

- Not keeping Duktape and bolting on a transpiler (fighting the parser).
- Not shipping row 31 as "half the bundle runs" — receipts are all-or-
  nothing with real evidence.
- No per-site hardcode in the engine (TPMOS) — QuickJS is a drop-in
  parser, not a youtube shim.
- No uncommitted work: every rung commits scoped to the engine cell.

---

## Work log

### 2026-09-17 — insight recorded, decision taken
- Discovered the ES5.1 floor (standalone duktape probe, §1).
- Confirmed Duktape 3 doesn't add the syntax (§3); QuickJS does (§4).
- Measured the boundary (~750 lines to port, everything else survives,
  §5). 44-suite regression net unchanged.
- Doc written (this file). No engine edits yet. Next rung: vendor
  QuickJS + standalone modern-syntax probe.

### 2026-09-17 — rung 1+2+3 receipts (engine NOT touched)
Official bellard QuickJS **2026-06-04** (`quickjs-2026-06-04.tar.xz`,
https://bellard.org/quickjs/) vendored-side in /tmp (no repo changes).
Probe drivers built against the real sources with upstream's own flags
(`-D_GNU_SOURCE -DCONFIG_VERSION="..." -fwrapv`, engine files:
`quickjs.c cutils.c libregexp.c libunicode.c dtoa.c`; note: no `libbf.c`
in this release, libbf was merged; `-std=c99` breaks the `asm` keyword —
use the Makefile's gnu default).

- **Modern-syntax probe** (`qprobe`): evals one global script containing
  `class`, `?.`, `??`, arrow, template literal, destructuring, spread,
  and `async function`+`await` over a resolving Promise. Result:
  `QJ-PROBE yyyyyy|async=y` (every feature evaluates to the expected
  value; native microtask queue drained via `JS_ExecutePendingJob`). The
  same source is a `SyntaxError` under repo Duktape (§1) — a clean,
  same-input side-by-side.
- **Prelude smoke** (`qprelude`): dumps `g_js_prelude` **verbatim** from
  `ops/nb_host.h` (19,805 bytes, extracted via an include-based C
  dumper that links the real header), loads it under QuickJS against
  stub host globals (`window/document/navigator/location/console` +
  `__nb_ges` + `URL/URLSearchParams` placeholders). Result:
  `PRELUDE-SMOKE LOADED ...` then
  `PRELUDE-CHECK href=https://y.example/a?q=1` — the prelude's URL
  polyfill actually round-trips a URL. Only load-time smoke (no
  `nbFetchSync` native in the probe, so no fetch/Promise-io path yet);
  runtime fetch is exactly what the worker boundary transplant provides.
- **Downloads stayed in /tmp** `/tmp/qjs-src/`, `/tmp/NB_PRELUDE.js`;
  zero repo changes from the probe run. Decision point reached: graft
  starts now (rung 4) or receipts reviewed first.