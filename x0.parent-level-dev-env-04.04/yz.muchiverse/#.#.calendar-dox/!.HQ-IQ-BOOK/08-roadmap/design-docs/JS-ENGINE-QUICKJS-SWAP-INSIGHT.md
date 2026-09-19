# JS engine swap insight — Duktape is the floor, QuickJS is the door (row 31)

**Status:** graft **DONE** — engine transplanted and green (full
`make check` 44 PASS / 0 FAIL on QuickJS 2026-06-04, commits
`5d1e8bd7` engine headers + `07aa2200` full graft). §8's resume pack is
**historical** (it described the pre-graft state); the graft receipts
and engine file list are in §10.
**Date:** 2026-09-17 (updated 2026-09-18)
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

### 2026-09-17 — graft PLAN approved (user go, recorded for handoff)

User decisions on the two graft checkpoints:

1. **Microtask model: NATIVE job queue.** Delete the C stash-FIFO
   (STASH_MICRO/g_micro_*/drain_microtasks scan) + the prelude's Promise
   polyfill; drain via QuickJS's `JS_ExecutePendingJob`. Gains native
   async/await semantics, deletes dead emulation weight; tightens the
   prelude (nb_host.h) and the worker's timing loop. (House Jacobian:
   "native" wins over "port the FIFO 1:1".)
2. **Scope: FULL worker+host graft, suites green in-session.** Port the
   105 natives + heap lifecycle + event loop in `nb_js_worker.c` and the
   16 registrations in `nb_host.h`; vendor QuickJS into `&.hq-apps/js/`;
   rewire `Makefile` `nbjs`; then all 44 suites green from a fresh
   build + fresh run. No half-graft (no two-session split).

**Concrete graft checklist** (what "rung 4-6" means now):

- [x] Vendor official QuickJS 2026-06-04 into `&.hq-apps/js/`:
      `quickjs.c/.h`, `cutils.c/.h`, `libregexp.*`, `libunicode.*`,
      `libunicode-table.h`, `dtoa.c/.h`, `list.h`. Keep duktape/* in
      place until the transplant is green (nb_js_eval.c eval path can
      outlive the worker swap for rollback).
- [ ] Makefile `nbjs` target: swap `$(JS_DIR)/duktape.c` -> quickjs
      component list; keep `-lm`; add the two qjs compile flags privacy
      guards from upstream (`-D_GNU_SOURCE -DCONFIG_VERSION="..."`),
      NOT `-std=c99` (breaks `asm`).
- [ ] Rewrite `nb_js_worker.c` glue. Per-API map is the ~45 `duk_*`
      names listed in this doc's survey (put_prop_string/push_string/
      push_c_function/push_global_stash/peval/...). Natives become
      `JSValue name(JSContext*, JSValueConst this, int argc, JSValueConst* argv)`
      with `JS_DupValue/JS_FreeValue` discipline; native *bodies* (DOM,
      cookie, storage, sha1, css) stay untouched.
- [ ] **Delete**: STASH_MICRO FIFO, prelude Promise polyfill, the
      `enqueue`/microtask-drain emulation. **Add**: `JS_ExecutePendingJob`
      loop in the drain slot (CPU-budget kept via alarm/EVAL_BUDGET_SEC;
      g_pending_err capture via `JS_GetException`).
- [ ] `nb_host.h`: registrations from `duk_push_c_function`+`
      duk_put_prop_string` to `JS_NewCFunction`+`JS_SetPropertyStr`;
      prelude text stays verbatim except the Promise-polyfill chunk.
- [x] `nb_js_eval.c` decision FLIPPED 2026-09-18: port it too (only 11
      `duk_` refs / 7 APIs) because it `#include`s `nb_host.h`, which
      must become QuickJS-typed. Rollback anchor becomes the pre-graft
      commit + duktape files kept on disk (not a Duktape nb_js_eval).
- [ ] Fresh `make check` -> all 44 suites PASS on the QuickJS build;
      fix/commit each regression as it appears (expected suspects: timer
      ordering in wlt/wss, microtask cadence in wfp/wit, exception-string
      formats).
- [ ] House docs flip when green: this doc (graft DONE receipt, commit
      hashes, engine file list), compact browser.md (engine swap line),
      NB-JS-ENGINE-ROADMAP (ES5.1 "language done" framing -> corrected).

**Rollback anchor:** until `make check` is green on QuickJS, the old
tree stays recoverable via the pre-graft commit + duktape files kept in
place. The graft never forces working-tree noise into a commit: runtime
state files are not staged (house rule #10).

## 8. RESUME PACK — handoff to a NEW instance (read first, cold start)

Write date: **2026-09-18**. This section is the whole handoff: how to use
the same git, what the job is, the standards, the exact in-flight state,
and the translation table that turns the remaining work into mechanical
copy.

### 8.1 Same git — two worktrees, two branches

- One repository. The `AGENTS.md` house-rules file lives at the repo
  root of `/home/no/Desktop/github/work/NNEST-12.00`.
- The repo has **two worktrees** of the same git store:
  - `/home/no/Desktop/github/work/NNEST-12.00` — branch `claude`
    (the OTHER agent's tree). **Never commit here.**
  - `/home/no/Desktop/github/work/NNEST-12.00-opencode` — branch
    `opencode` (THIS job's tree). **All work goes here.**
- So: do NOT set your working dir to the bare `NNEST-12.00`; open files
  under `NNEST-12.00-opencode/x0.parent-level-dev-env-04.04/yz.muchiverse/
  44.xyz.01.00/&.hq-apps/network/...`.
- Paths contain `&` and `#` and `!` — **always quote them in bash**,
  or the shell will interpret them.
- The turf of this job is the `network` cell: `ops/nb_js_worker.c`,
  `ops/nb_host.h`, `ops/nb_js_eval.c`, `Makefile`, `&.hq-apps/js/`.
- Commit rules (see also AGENTS.md, BRANCH-STRATEGY.md,
  OPERATIONAL-LANDMINES.md #10): commit ONLY on `opencode`, scoped
  `git add <path>` per file (never `-A`); runtime state files (pdl,
  logs, pids, `.png`, built binaries `nbjs`/`w*`) are NEVER staged;
  never end a work block with uncommitted code (mid-work ok as
  `wip: <what>`); never merge/cherry-pick/push without the user's
  explicit "push"; never commit to `main`/`claude`/`chtpm-delete-per-app-c`.

### 8.2 The job (compacted)

Replace the Duktape 2.7 (ES5.1-only parser) engine with **QuickJS
2026-06-04** so youtube.com's real ES2020+ bundle can execute (roadmap
row 31). Two approved decisions:

1. **Native job queue** — delete the C stash-FIFO microtask emulation
   and the prelude's Promise polyfill; drain via `JS_ExecutePendingJob`.
2. **Full worker+host graft, 44 suites green in-session.**

Scope discipline: pipe protocol, DOM tree, cookie jar, storage jars,
`__nb_sha1`, CSS cache, event loop C-state (~2100 lines) survive
**untouched**. Only the duk-boundary (~750 lines / 54 API names / 105
natives in the worker + 19 names/16 natives in nb_host.h + 11 refs in
nb_js_eval.c) and the Makefile `nbjs` target change. The 44 suites are
pipe-driven and engine-agnostic — they are the regression net, do not
modify them.

### 8.3 Exact state at handoff (2026-09-18)

DONE (committed on `opencode` unless noted):
- Insight + decision doc, probe receipts, approved graft plan —
  commits `6171d38c`, `fe74897b`, `67e06d22` (all unpushed).
- Probe evidence: modern-syntax parse+run (`QJ-PROBE yyyyyy|async=y`)
  and verbatim prelude load under QuickJS (`PRELUDE-SMOKE LOADED`,
  `PRELUDE-CHECK href=...`).
- **QuickJS vendored into `&.hq-apps/js/`** — 13 files (`quickjs.c/.h`,
  `cutils.c/.h`, `libregexp.c/.h`, `libregexp-opcode.h`,
  `libunicode.c/.h`, `libunicode-table.h`, `dtoa.c/.h`, `list.h`).
  `duktape.*` + `stb_image.h` still in place (rollback).
  **UNCOMMITTED → commit these first.**
- Full read of `nb_js_worker.c` (3912 lines). Region map in §8.4.
- Per-file duk API inventories captured (§9 table built from them).

NOT STARTED:
- The actual code translation (host, worker, eval, Makefile), the
  build, and the 44-suite run. Transplant has not touched a line yet.

### 8.4 Region map of `ops/nb_js_worker.c` (line refs; on-disk file is authoritative)

- 1-158: includes, framing (g_rbuf/recv_frame/split_lines), `g_live_ctx`
  decl. Only line ~62 `static duk_context *g_live_ctx` → `JSContext *`.
- 159-488: DOM tree C-state, node_index, SB builder, selector engine,
  RENDER rows — **no duk calls; untouched**.
- 490-527: `get_node`/`get_this` helpers + ONPROPS (duk) — translate.
- 528-812: CSS/getComputedStyle + element/document natives (duk) —
  translate values only; bodies are DOM logic.
- 814-1196: element + classList natives (duk) — translate.
- 1198-1347: `push_node` + node-identity wrapper (duk + **global stash**
  map) — translate; stash → C-side `JSValue` map keyed on the global
  object (`__nb_idmap`), created in run_page, freed at teardown.
- 1349-1390 + 1722-1840 + 1841-2200: cookie jar / storage C helpers —
  **untouched**; the GET/SET natives around them (duk) translate.
- ~2061: `install_dom` — translate entire registration block.
- ~2300-2460: timers/microtasks/RAF natives (duk) + event-binding —
  translate; **microtask FIFO deleted**; timer callbacks become C-
  held `JSValue`s (dup/free instead of stash arrays).
- 2461-2812: `drain_microtasks` (**DELETE**) + `run_due_timers` +
  `run_event_loop` — rewrite drain to `while (JS_IsJobPending(rt))
  JS_ExecutePendingJob(rt,&jctx)` (jctx = ctx; `ctx2 && jctx == ctx`
  pattern from the probe loop; break on `<0` + `JS_GetException`).
- 2825-2874: `__nb_sha1` native + `install_events_timers` (duk) —
  translate.
- 2880-2893: `boot_duk_install`: `install_safe` — translate the
  duk_pcall wrapper to `JS_Call(ctx, fn, JS_UNDEFINED, argc, argv)`.
- 2895-2906: alarm-based CPU budget — **stays** (sigalrm → _exit).
- 2908-3123: `run_page` — heap create (`JS_NewRuntime`/`JS_NewContext`),
  eval (JS_Eval), global bootstrap, callbacks `JS_Call`.
- 3129-3291: `cmd_eval` + REPL (`repl_main`) — translate.
- 3293-3448: CLI node-mode natives (read_file/fs/cli install) — translate.
- 3481-3679: `g_cjs_prelude` JS string — **untouched**.
- 3681-3912: `cli_main` + `main` — translate (`duk_pcompile_lstring_*`
  → `JS_Eval` + strip a leading `#!...` shebang line in C, since this
  QuickJS build ships no SHEBANG eval flag).

### 8.5 Next steps (exact order)

1. Commit the 13 vendored `&.hq-apps/js/` files + this doc + browser.md
   update, scoped, on `opencode`. (House rule: never hand off with
   uncommitted work.)
2. Rewrite `nb_host.h` (756 lines): `#include "../js/quickjs.h"`; the 16
   registrations go `duk_push_c_function`+`duk_put_prop_string` →
   `JS_NewCFunction`+`JS_SetPropertyStr`; accessors → `JS_DefineProperty
   GetSet` with magic get/set; `g_js_prelude[]` (339-484, 19,805 bytes)
   stays verbatim EXCEPT the Promise-polyfill chunk gets deleted.
3. Rewrite `nb_js_worker.c` top→bottom per §8.4 using §9's table.
4. Rewrite `nb_js_eval.c` (11 refs, 7 APIs — trivial) so the shared
   nb_host.h compiles.
5. Makefile `nbjs` target: CFLAGS → **`-std=gnu11`** (NOT `-std=c11` —
   quickjs.c uses the `asm` keyword at ~line 60600) and add
   `-D_GNU_SOURCE -DCONFIG_VERSION=\"2026-06-04\" -fwrapv`; replace
   `$(JS_DIR)/duktape.c` with `quickjs.c cutils.c libregexp.c
   libunicode.c dtoa.c` (**NO libbf.c — merged in this release**); keep
   `-lm`. Our TUs can stay `-std=c11`; only the engine TUs break.
6. `make` → iterate compiler errors (expect mass signature-only fixes).
7. `make check` → all 44 suites green from a fresh build + fresh run;
   fix+commit each regression (expected suspects: timer ordering in
   wlt/wss, microtask cadence in wfp/wit, exception-string formats).
8. Flip house docs to GREEN: this doc (graft DONE + commit hashes +
   engine file list), compact browser.md (engine-swap line), and
   NB-JS-ENGINE-ROADMAP ("ES5.1 language done" framing → corrected).

### 8.6 Rollback anchor (adjusted 2026-09-18)

`duktape.c/.h/duk_config.h` are gone from the working tree (deleted in the
graft cleanup commit) — they remain recoverable from git history, as does
the pre-graft `nb_js_worker.c`/`nb_host.h`/`nb_js_eval.c`. `nb_js_eval.c`
is QuickJS; there is no Duktape build to fall back to. `stb_image.h` in
`&.hq-apps/js/` stays (media ops include it).

### 8.7 Where to prove / other pointers

- Probe recompile kits live in `/tmp/qjs-src/` (`qprobe.c`, `qprelude.c`)
  and `/tmp/NB_PRELUDE.js` — disposable; not needed for the graft.
- `git log --oneline` on `opencode`: `67e06d22` = approved graft plan.
- Load the `khtpm-house-standards` skill before touching any
  _manager.c/_render.c pair or taskbar UI (not needed for nbjs cells).
- Never report green without `make check` from a fresh build + fresh
  run — a clean compile is not evidence.

## 9. Translation table — the meat (duk → QuickJS 2026-06-04)

Built from the 2026-09-18 API inventories. Natives become:

```c
static JSValue nb_node_name(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    struct node *n = get_node(ctx, this_val);
    return JS_NewString(ctx, n && n->id ? n->id : "");
}
```

Return `JS_UNDEFINED`/`JS_NULL`/`JS_TRUE`/`JS_FALSE`/built values or
`JS_NewInt32/NewFloat64/NewString/NewStringLen/NewObject/NewArray`;
on error `JS_ThrowTypeError/RangeError/InternalError(ctx,"fmt",...)`
then `return JS_EXCEPTION;`.

| duk (Duktape 2.7) | QuickJS |
|---|---|
| `duk_create_heap(NULL,NULL,NULL,NULL,fatal)` | `rt=JS_NewRuntime(); ctx=JS_NewContext(rt);` (fatal cb N/A) |
| `duk_destroy_heap(ctx)` | `JS_FreeContext(ctx); JS_FreeRuntime(JS_GetRuntime(ctx));` |
| `duk_context *`/`duk_ret_t`/`duk_idx_t` | `JSContext *`/`JSValue`/`int` |
| `return 0` from native | `return JS_UNDEFINED;` (or value) |
| `duk_get_string(ctx,i)` (no coercion) | if `JS_IsString(argv[k])`: `JS_ToCStringLen(ctx,&len,argv[k])`; else treat as NULL — matched |
| `duk_safe_to_string(ctx,i)` (coerce, no throw) | `JS_ToCString(ctx,argv[k])`; if NULL → clear exc + `JS_PrintValue` dump; caller frees |
| `duk_get_int/get_number` (no coercion) | `JS_IsNumber ? JS_ToInt32/JS_ToFloat64 : default` |
| `duk_to_boolean` | `JS_ToBool(ctx,v)` — returns `int`, `-1`=exception → treat `<=0` as 0 |
| `duk_is_object/callable/number/string` | `JS_IsObject/JS_IsFunction/JS_IsNumber/JS_IsString` |
| `duk_push_undefined/null/boolean/int/number/string/lstring` | `JS_UNDEFINED/JS_NULL/JS_NewBool/JS_NewInt32/JS_NewFloat64/JS_NewString/JS_NewStringLen` |
| `duk_push_object/array/global_object` | `JS_NewObject/JS_NewArray/JS_GetGlobalObject` (latter = OWNED, free) |
| `duk_push_this` | `this_val` (native param) |
| `duk_push_global_stash` | **gone** — microtask FIFO deleted; idmap → `__nb_idmap` on global object |
| `duk_push_c_function(ctx,fn,nargs)` | `JS_NewCFunction(ctx,fn,"name",nargs)` |
| `duk_set_magic` + `duk_get_current_magic` | `JS_NewCFunctionMagic(ctx,fn,"name",len,JS_CFUNC_generic_magic,i)`; native gains trailing `int magic` |
| `duk_get_global_string(ctx,"k")` | `JS_GetPropertyStr(JS_GetGlobalObject(ctx),"k")` (free the glob) |
| `duk_put_prop_string(ctx,o,"k")` | `JS_SetPropertyStr(ctx,o,"k",val)` — **takes ownership of val** |
| `duk_put_prop_index(ctx,a,i)` | `JS_SetPropertyUint32(ctx,a,i,val)` — **takes ownership** |
| `duk_get_prop_string/prop_index` | `JS_GetPropertyStr/JS_GetPropertyUint32` (owned; free after) |
| `duk_has_prop_string` | `JS_HasProperty(ctx,o,JS_NewAtom(ctx,"k"))` + `JS_FreeAtom` |
| `duk_del_prop_index` | `JS_DeleteProperty(ctx,o,JS_NewAtomUInt32(ctx,i),0)` + free atom |
| `duk_def_prop(ctx,o,idx,DUK_DEFPROP_HAVE_GETTER|...SETTER|...ENUMERABLE|ENUMERABLE)` | `JS_DefinePropertyGetSet(ctx,o,JS_NewAtom(ctx,"k"),get_fn,set_fn,JS_PROP_HAS_GET|JS_PROP_HAS_SET|JS_PROP_HAS_ENUMERABLE|JS_PROP_ENUMERABLE)` — takes ownership of fns; magic accessors via NewCFunctionMagic |
| `duk_dup(ctx,i)` | `JS_DupValue(ctx,v)` |
| `duk_pop`/`duk_pop_2` | `JS_FreeValue(ctx,v)` (free owned refs) |
| `duk_remove` | stack idioms → locals with Dup/Free discipline |
| `duk_pcall(ctx,n)` | `r=JS_Call(ctx,f,JS_UNDEFINED,n,argv)`; `JS_IsException(r)` → `JS_GetException`+save pErr; free f |
| `duk_pcall_method(ctx,n)` | `JS_Call(ctx,f,this_val,n,argv)` |
| `duk_pnew(ctx,n)` | `JS_CallConstructor(ctx,f,n,argv)` (1 use) |
| `duk_peval_string(ctx,src)` / `duk_peval` | `JS_Eval(ctx,src,len,"<nbjs>",JS_EVAL_TYPE_GLOBAL)`; check `JS_IsException` |
| `duk_pcompile_lstring_filename` (CLI) | `JS_Eval(ctx,src,n,filename,JS_EVAL_TYPE_GLOBAL);` + manually strip `#!` shebang line |
| `duk_error(ctx,DERR,"fmt",...)` | `JS_ThrowTypeError/RangeError/InternalError/...` + `return JS_EXCEPTION` |
| `duk_uarridx_t` | `uint32_t` |
| `DUK_VARARGS` natives | fixed length 0 (QuickJS passes real argc always; length is cosmetic) |
| microtask FIFO + prelude Promise polyfill | **delete**; `queueMicrotask` native → `JS_EnqueueJob`; drain `while(JS_IsJobPending(rt)) JS_ExecutePendingJob(rt,&jctx)` in the event loop |
| stash-held timers | C `JSValue` fields; dup to fire, free on clear/overwrite, free all at teardown |

Ownership cheat-sheet: `JS_SetPropertyStr/Uint32` and
`JS_DefinePropertyGetSet` CONSUME the value(s); `JS_GetProperty*`,
`JS_GetGlobalObject`, `JS_Eval`, `JS_Call` RETURN owned refs you must
`JS_FreeValue`. `JS_ToCString*` returns memory freed with
`JS_FreeCString`.

Worked conversions worth copying: the sha1 native and the DOM
getElementById native are the two cleanest duk→QuickJS hand ports in
the worker (translate via the table above and mirror shape).

## 10. Work log (appended by session)

### 2026-09-18 — graft DONE (receipts, commits `5d1e8bd7` + `07aa2200`)
- **Engine files** live in `&.hq-apps/js/` (committed `5d1e8bd7`):
  `quickjs.c/.h`, `quickjs-atom.h`, `quickjs-opcode.h`, `cutils.c/.h`,
  `libregexp.c/.h`, `libregexp-opcode.h`, `libunicode.c/.h`,
  `libunicode-table.h`, `dtoa.c/.h`, `list.h`. `duktape.c/.h` + `duk_config.h` + the old
  `install-duk.sh` were REMOVED in the 2026-09-18 cleanup (rollback now =
  git history, not on-disk files); `stb_image.h` stays (nb_media_to_sprite
  still includes it).
- **Boundary transplant** (`07aa2200`, 5 files): nb_js_worker.c +
  nb_host.h fully duk→QuickJS (105 natives, heap lifecycle, event loop,
  registrations); microtask FIFO + prelude Promise polyfill **deleted**,
  drain = `while(JS_IsJobPending(rt)) JS_ExecutePendingJob(rt,&jctx)`;
  stash timers → C-held `JSValue` dup/free; nb_js_eval.c ported (shares
  nb_host.h). Makefile `nbjs` + build.sh ops lines: 5 engine TUs,
  `-std=gnu11 -D_GNU_SOURCE -DCONFIG_VERSION=\"2026-06-04\" -fwrapv
  -pthread`.
- **Bug fixes found by the transplant + suites:**
  - `JS_IsCallable` is not in public quickjs.h — use `JS_IsFunction`.
  - 5 leaked `JS_GetGlobalObject` refs (run_page `__nb_ges`, install_fs
    `__nb_fs`, repl/cli `__nb_read_file`, cli `process`) + the
    `esmPrepare` handle (`prep` freed only in the else branch) — all
    fixed; a single leaked handle makes `JS_FreeRuntime` assert
    (`list_empty(&rt->gc_obj_list)`, quickjs.c ~2464). Diagnosed with a
    `-DDUMP_LEAKS` build (prints "Object leaks: …").
  - **wps slice bug:** QuickJS's lexer peeks `input[input_len]` for EOI
    checks, so `JS_Eval` must receive NUL-terminated input. Script
    slices are mid-malloc-buffer; the byte after a slice was `/` (start
    of the next boundary), manufacturing bogus
    "unexpected end of string"/`'<<'` SyntaxErrors. Fix:
    `((char*)p)[slice] = 0;` before each slice eval in
    `run_scripts_slices` (slices own one contiguous buffer).
- **Evidence:** `make check` exit 0 — 44 suites, 60 PASS lines, 0 FAIL
  (fresh `make nbjs` + fresh run; wps repro pages `/tmp/nbjs-check/
  caseA.js`/`caseB.js` byte-verified). Deployable ops binaries rebuilt
  via `build.sh`; eval op probe `print("eval-op-ok")` → `OK|1`.
- Safe to resume §8.5 step 8's row-31 real-bundle receipt.

### 2026-09-18 — vendor DONE + full API map + resume-pack handoff
- Vendored official QuickJS **2026-06-04** into `&.hq-apps/js/` (13
  files; duktape kept for rollback). *Commit pending — the vendored
  files are uncommitted working-tree additions as of this write.*
- Re-read `nb_js_worker.c` end-to-end (3912 lines): confirmed §8.4
  region map; full API inventories (54 duk names / 776 refs / 105
  natives in worker; 19 / 151 / 16 in nb_host.h; 7 / 11 in
  nb_js_eval.c); read `nb_host.h`, `nb_js_eval.c`, `Makefile`,
  `quickjs.h`; locked the quickjs build flags (§8.5 step 5) and the
  microtask/timer translation (stash-FIFO → JS_ExecutePendingJob;
  stash timers → C JSValue fields).
- Decision: `nb_js_eval.c` ported too (rollback anchor adjusted, §8.6).
- This RESUME PACK (§8) + translation table (§9) written so a fresh
  instance can restart the graft cold. Next action on resume: commit
  vendored files, then run §8.5 steps 2→8.

### 2026-09-18 — row-31 real-bundle receipts (graft proven on youtube's code)
- Fetched real youtube bundle files to `/tmp/yt` (`home.html`, `spf.js`,
  `network.js`, `scheduler.js`, `web-animations.min.js`, `kevlar_base.js`)
  and ran them under `./nbjs --browser <file> [fetch.dom]`. spf/network/
  scheduler/web-animations eval clean; `kevlar_base.js` (10,790,631 bytes,
  one IIFE) is loaded and parsed whole.
- Commits on `opencode` (scoped, `make check` re-run green each time):
  - `0c5a24a7` DOM class hierarchy + `createElementNS` + canvas 2D stub +
    `NB_STACK` traces. Root-cause chain for web-animations: missing
    `document.createElementNS` → `Element` global undefined
    (`Element.prototype` read at load) → `<canvas>.getContext` absent.
  - `fddc91b7` `read_file_big` page-only stream load (64MB ceiling; 512KB
    `read_file` kept for fs-lite/CJS) + `NB_EVAL_BUDGET` override. Kevlar
    whole-file load: 8.3s wall, 104MB peak RSS.
  - `18943d95` constructors (quickjs.c:17642 gives C constructors
    `new_target` as `this_val`; they must build the instance) +
    `customElements`/`CSSStyleSheet` + standard element/event globals +
    `hasAttribute` + prelude `MessageChannel` / `<template>.content`.
    Note: `add_global_class` deliberately does NOT clobber the prelude's
    `Event` (nb_el_click builds clicks through it — clobbering broke rung-3
    events, caught by `make check`).
  - `b6129605` computed-style `fontSize` (kevlar font metrics).
- **Where kevlar stops:** line 26741 `querySelector('ytd-app')` with the
  minimal DOM (fixture-content gap); with a `fetch.dom` built from the real
  `home.html` it boots its Polymer element system (real Polymer console
  output) and stops at line 1314's bundle-URL assertion (`Error: Tc`,
  `_F_jsUrl` mismatch vs the file-loaded script) — app/config-specific, not
  an engine gap. `make check` remains 60 PASS / 0 FAIL.

### 2026-09-18 — kevlar boots + renders (the `_F_jsUrl` stop cleared)
- Diagnosed the line-1314 byter (`Nkz`, closure module loader): it computes
  the bundle URL as `D = O.src ? O.src : O.getAttribute("href")` where
  `O = getElementById("base-js")`, then requires `Vsi(D)` (URL must match
  `/(_/js/|_/ss/)…/k=/`). `window._F_jsUrl` (`_.$c._F_jsUrl`) is unset in
  the fetched page, so the `base-js` script element is the source of truth —
  and the HTML parser was dropping it (is_skip covered `script`+`head`).
- Fix `0063797e`: `nb_dom.c` keeps raw-text elements (script/style/title/
  noscript) as DOM nodes and no longer skips `<head>`; `push_node` exposes
  `.src`/`.href` from the raw attribute (closure reads these directly).
- **Receipt:** `./nbjs --browser kevlar_base.js fetch.home.dom`
  (fetch.dom from the real `home.html`, 419 nodes incl 42 scripts) now runs
  kevlar to completion: Polymer boots, the app renders the youtube footer
  (15 `LINK|` frames + `© 2026 Google LLC` `TEXT|`), exit 0. `make check`
  60/0.
- Follow-up `09fc27b9`: the one logged `TypeError` came from youtube's
  error reporter (`C2y`) doing `script.src.indexOf("/debug-")` on inline
  scripts (no src attr → our accessor was undefined). `.src`/`.href` are
  now strings (`""` when absent) on the tags that own them; kevlar runs with
  ZERO logged errors. `native_log` also appends a caught `.stack` under
  `NB_STACK=1`.