# NB-JS engine as a node/bun-like CLI runner — scope + plan

**Status:** CLI-1 landed 2026-09-07 (in the worker, `cli_main()`); CLI-2
(CommonJS) and CLI-3 (fs-lite + `-i`) queued behind it. The node runner
shares the binary with the REPL and the daemon: `duk` (no args) + tty →
REPL, `duk file.js` → node mode, `duk --browser page.js` → the released
DOM page runner. Trigger for CLI-2: a guard-railed `require('./x.js')` +
`process.argv` used by a new `tests/cli_*.js` runner with expected exit
codes.

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

### CLI-2 — CommonJS
- `require(path)`: compile file, wrap as a `module` object, cache by
  resolved absolute path, resolve relative paths via `cwd`, no
  `node_modules`/packages (out of scope for a browser-house tool).
- Circular requires: partially-initialized module export served on the
  second visit (standard CJS behavior, easy in duktape).
- `module.exports`/`exports` globals per-file via wrapper.
- JSON require: `JSON.parse` of the file.

### CLI-3 — filesystem lite + REPL
- `fs`-lite natives over `read_file` + a write/append pair: `readFileSync`,
  `writeFileSync`, `existsSync`, `mkdirSync` (mirror `mkdir_p_local`).
- `-i` REPL over stdin (evaluate line, print result via `console`/`print`).
- `#!` shebang stripping on entry.

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
- Trigger for CLI-1: a guard-railed `require('./x.js')` + `process.argv`
  used by a new `tests/cli_*.js` runner with expected exit codes.