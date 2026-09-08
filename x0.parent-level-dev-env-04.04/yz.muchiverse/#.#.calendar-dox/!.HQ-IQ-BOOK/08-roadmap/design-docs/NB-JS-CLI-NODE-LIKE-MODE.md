# NB-JS engine as a node/bun-like CLI runner — scope + plan

**Status:** CLI-1 landed 2026-09-07 (in the worker, `cli_main()`); CLI-2
(CommonJS) landed 2026-09-07; CLI-3 (fs-lite + `-i`) landed 2026-09-07;
the ESM rung (CLI-4) stays a hard tail. The node runner shares the binary
with the REPL and the daemon: `duk` (no args) + tty → REPL, `duk file.js`
→ node mode, `duk --browser page.js` → the released DOM page runner,
`duk -i` → REPL even with piped stdin. Trigger for any future rung:
guard-railed functionality exercised by `tests/cli_*.js` runners with
expected exit codes.

**Why the moment is right:** rung 4 (XHR/fetch) just made the worker a
real JS-time runtime. The host installation seam (`install_host()` +
`g_js_prelude` in `ops/nb_host.h`) is the single injection point a node
mode plugs into on the *other side* of the browser behavior. Locking in
the seam's shape now is cheaper than after a browser-only surface grows
more assumptions.

## What already exists (the seed)

- `ops/nb_js_eval <script.js> <outfile>` — one-shot file eval with the
  browser prelude, `OK|1`/`OK|0` + `LOG|*`/`TITLE|` pipe lines; the rung
  test suites already run through it. This is a primitive `node file.js`.
- `read_file()` (512 kB cap, in `nb_host.h`), curl-based http fetch
  (`nb_fetch_sync`), atob/btoa, real microtask queue (`nb_queueMicrotask`)
  and timers in the worker — the pieces node-mode reuses.
- duktape 2.7.0 provides the compiler/evaluator; no async/await or
  native Promise (prelude polyfill covers both).

## The ladder (each rung ships on its own)

### CLI-1 — a real runner (`ops/nb_js_cli.c`, ~150-250 lines)
- argv[1] = script or `-` (stdin), argv[2+] = `process.argv` tail.
- Exit codes: 0 on clean run, 1 on thrown error (message → stderr).
- `process` host object: `argv`, `cwd`, `env`, `stdout`/`stderr`
  (`write`), `exit()` (native; flushes + returns code).
- Console already routes through `native_log` → `pipe_one`; in CLI mode
  it should default to real stdout/stderr (mode flag on install).
- **No** browser prelude: node mode evals a node prelude, browser globals
  stay uninstalled (`window`/`document`/`location` absent — a feature, not
  a bug).

### CLI-2 — CommonJS *(landed 2026-09-07)*
- `require(path)` global in node mode (entry base = the script's own
  directory, absolute or `./`/`../` relative; absolute paths OK).
- Pure-JS loader prelude (`g_cjs_prelude` in the worker) over one host
  hook — `__nb_read_file` (the same 512 kB-capped `read_file`). Module
  files are wrapped `(function(exports, require, module, __filename,
  __dirname){...})` so module-local `var`s do not leak, `this` ===
  `module.exports` like node, and top-level `#!` lines are stripped.
- `module.exports`/`exports` per file; cache keyed by resolved absolute
  path; circular requires serve the partially-initialized `exports`
  (standard CJS, cache entry inserted before eval).
- JSON require: any path ending `.json` → `JSON.parse` of the file.
- No `node_modules`/packages/builtins — a bare specifier throws
  `Cannot find module '<name>' (nbjs has no packages/builtins)`. A
  relative path that misses throws with the resolved path. Both exit 1.
- `console.error` now routes to stderr in node mode (lives on
  `nb_cli_error`); `log`/`info`/`warn` stay on stdout.
- Tests: `cli_test` cases 8-9 (require chain incl. JSON/nested `../`/
  cycle/var-scoping + missing-module error), all green.

### CLI-3 — filesystem lite + REPL *(landed 2026-09-07)*
- `fs`-lite natives over `read_file` + a write/append pair:
  readFileSync / writeFileSync / appendFileSync / existsSync / mkdirSync
  (recursive `mkdir -p` walk, mirroring the manager's `mkdir_p_local`).
  Exposed node-idiomatically as `require('fs')` — a builtin registered in
  the CJS loader (the only builtin; still no node_modules/packages).
- String-only payloads (no Buffer in this Duktape); an encoding arg is
  accepted but ignored so node-style call sites keep working. Reads keep
  the 512 kB cap; miss → `ENOENT`-style error, exit 1.
- REPL: `duk -i` / `--interactive` forces the REPL even when stdin is
  piped (useful for `echo '1+2' | duk -i`). The REPL now also carries
  `require()` + `require('fs')` (added on top of the existing browser
  prelude, so `document` etc. stay available line-by-line).
- `#!` shebang stripping on entry — already handled by
  `DUK_COMPILE_SHEBANG` on the entry compile and by the CJS loader for
  required files.
- Tests: `cli_test` cases 10-12 (fs read/write/append/exists/mkdir,
  `-i` piped REPL eval, REPL require/fs), all green.

### CLI-4 — ESM (defer; explicit non-goal until asked)
- duktape has no `import` syntax support; ESM = a loader that rewrites
  or source-parses `import`/`export` → CJS. Do NOT promise this; note it
  as the hard tail.

## Seam hygiene this depends on (cheap, do now)

- `nb_host.h` prelude blocks must tolerate "no `window`/`document`"
  environments: guard `Object.defineProperty(window, ...)` lines and
  browser-only assumptions with `typeof window !== 'undefined'` so a
  shared host header can be reused by the CLI without divergence.
- Keep permissive-Promise, permissive-fetch decisions in the prelude so
  the same polyfills serve both modes.
- `nb_js_eval` stays as the rung-0/1 fallback; the CLI supersedes it for
  new test tooling rather than replacing it.

## Risks / non-goals
- No async I/O semantics parity with node (no libuv, no evented fs) —
  sync-only, by design, like the house curl pattern.
- No `npm`/`node_modules`/package manager. Never pretend otherwise.
- Bundling: page scripts stay manager-concatenated; module loading is a
  CLI-only concern.

## Entry point into tracking
- OPEN-ITEMS.md #11 (this doc). Roadmap §9 in `NB-JS-ENGINE-ROADMAP.md`.
- The node-mode ladder is done through CLI-3; anything further
  (fs depth, `path`, ESM transpile) is user-requested, not queued.