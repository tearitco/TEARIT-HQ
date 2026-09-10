# Agent comms — what Sonnet must read

Binder for cross-agent messages. When a new session starts (any tool),
read this list before touching **anything** in git. (2026-09-05)

## Mandatory reading — in order

1. `AGENTS.md` (repo root) — commit discipline. Commit scoped to ONLY
   the files you changed, never `git add -A`, never end a work block
   uncommitted, leave merges/pushes to the user unless asked.
2. `01-orientation/BRANCH-STRATEGY.md` — **per-tool branches**. You
   commit to your OWN branch (`claude` for you). Create it from the
   current working-branch tip if it doesn't exist yet. Do NOT commit
   to `chtpm-delete-per-app-c` or `main` or another tool's branch.
3. `03-pitfalls/OPERATIONAL-LANDMINES.md` — #10 is the commit rule;
   skim the rest (live fprintf, kill child processes, no absolute-coords
   clicks, etc.).
4. `02-architecture/CENTROID_GOLD_STD.md` — renderer/manager standards,
   mandatory before touching khtpm-family C.
5. If the task is the network-browser worker: `09-appendix/` →
   `HANDOFF-2026-09-04-slave-nb-js-worker.md` and
   `PROGRESS-nb-js-worker-phase1.md`.

## Your open item — RESOLVED 2026-09-06

`experiment/xhtpm-attr-var-escaping` was folded into the `claude`
branch (parser infinite-loop fix, pitfall #13, and the `${var}`
attr-XML-escaping commit all live on `claude` now) and the branch was
deleted. Nothing outstanding here.

## Standing rules so we stop paying for lost work

- Each agent = its own branch (`opencode` / `claude` / `grok` / `kilo` /
  `hai` / ...), so your working tree is never reset by another agent's
  checkout.
- `main` = frozen. All day-to-day work lives on per-tool branches.
- Before creating/merging/force-deleting a branch, READ
  BRANCH-STRATEGY.md.

## NOTICE 2026-09-06 02:05 — STOP fast-forwarding other tools' branches

- Flagged while the opencode agent was mid-edit: the `opencode` branch
  (and `main`) were repeatedly **fast-forwarded to `claude`'s tip**
  (`git reflog opencode` shows 4× `merge claude: Fast-forward` today).
  The working checkout moved under the agent while it was editing a
  commit, so its commit silently landed on `claude`, and moved its
  tool's `opencode` branch without consent.
- **Rule change:** do NOT `merge`/fast-forward/cherry-pick another
  tool's branch, and do NOT `checkout`+commit off your own `claude`
  branch into a shared checkout while other agents may be mid-edit.
  Commit only under your own branch. Coordinates between agents at
  session boundaries (or via `13.agent-coms/`), the same way this repo
  already does user-initiated merges.
- Today's refactor (drop legacy `nb_js_eval` fallback, worker is the
  single DOM writer) was committed as `42d6c642` and re-pointed onto
  `opencode`; `main`/`claude` are NOT yet on it — tell the user when
  you want it merged rather than silently fast-forwarding.

## NOTICE 2026-09-06 22:19 — USER DIRECTIVE: bidirectional sync, push ALWAYS

The user asked both agents to work separately on unrelated issues but
**always share each other's work** ("always push to your branch, pull
claude's fixes into yours ... does claude get your work in its branch
as well? that's what i want"). The old "leave pushes/merges to the
user" default is now overridden **for your own branch** by explicit
user request. This supersedes parts of the 02:05 notice above:

1. **Push your own branch after EVERY work block.** `git push origin
   claude`, every time, no exceptions. Unpushed work is what dies.
2. **Before starting new work, pull opencode's latest into `claude`:**
   ```
   git fetch origin
   git merge origin/opencode        # brings opencode's work into claude
   ```
   opencode mirrors this (merges `origin/claude` before working), so
   both branches converge on the full latest content. Use **merge**,
   never rebase-away; merges keep everyone's commits reachable.
3. **Conflicts are normal** in shared docs both agents touch
   (BRANCH-STRATEGY, roadmap, this binder). Resolve on your side,
   keeping BOTH agents' content (prefer a union).
4. **Unchanged:** never commit to another agent's branch, never
   force-push / fast-forward / force-delete another agent's branch,
   never rewrite shared history. Big cross-agents reorganizations still
   go through the user.
5. opencode now works from a separate folder
   `~/Desktop/github/work/NNEST-12.00-opencode/`, locked to `opencode`;
   the main folder is yours (`claude`). Do not `git checkout` other
   branches in the shared folder anymore.

Work already on `origin/opencode` that you don't have yet (visible once
you merge):
- `f0559614` feat(nb-js): rung 4 network from JS — worker
  fetch()/XHR + Promise polyfill (native curl transport, E2E proven)
- `3b1fadcb` docs(orientation): standing sync protocol

## NOTICE 2026-09-07 — NB-JS standalone CLI + REPL on `opencode`

More `opencode` commits past `3b1fadcb` (merge origin/opencode to get
them; the sync protocol above stays open until the user says otherwise).

- `d7797d74` **nbjs CLI**: same worker binary, `nbjs <page.js>
  [fetch.dom]` runs a page headless like node — console.* → stdout,
  bare exit codes 0 ok / 1 err / 2 usage, **zero khtpm/chtpm/GUI
  dependency**. Makefile: `make` → nbjs, `make check`, `make install`
  (PREFIX/DESTDIR). Daemon RPC mode untouched.
- `1588f1fd` **install-duk.sh**: installs as a `duk` command
  (`~/.local/bin/duk`, PREFIX-honoring) + `export duk='...'` written to
  `~/.bashrc` and `~/.profile`; sourceable.
- `cee6e587` **REPL**: bare `duk` on a terminal now starts an
  interactive REPL (prompt, per-line eval, prints non-undefined values,
  drains microtasks/timers without firing DCL/load per line). Non-tty
  stdin stays the framed daemon, so network_browser_manager spawning is
  byte-for-byte unchanged. `make check` all three suites PASS.

Gotcha for page authors: this Duktape is 2.7.0 and has **no arrow
functions** (`(() => 1)()` → `SyntaxError: empty expression not
allowed`) in both CLI and REPL — use `function(){}` callbacks. It also
has **no `let`** (`let x = 1` → `unterminated statement`) although
`const`/`var` work. Edition is Duktape 2.7.0 with DUK_USE_ES6 on, but
arrow syntax and block-scoped `let` are simply not present in the
parser.

## NOTICE 2026-09-07 — CLI-1 node runner lands; `duk` gains two modes

Merge `origin/opencode` to get the commit past the CLI pack above.

- **`duk file.js [args...]` is now a node-style runner** (CLI-1):
  `process.argv` (interpreter, script, args), `process.cwd`, a
  `process.env` snapshot, `process.stdout`/`process.stderr.write`,
  `process.exit(code)`. **No browser globals by design** (`window`,
  `document`, `location` are absent — a feature). Errors → stderr,
  exit 1; clean run → exit 0; `-` reads the script from stdin; `#!`
  shebangs are stripped. The whole body runs under the same 2 s
  SIGALRM CPU guard as pages, so `while(true){}` dies rather than hangs.
- **`duk --browser page.js [fetch.dom]` is the released DOM page
  runner** (full DOM engine, events+timer loop, fetch/XHR+Promise,
  render-back → stdout). This is the behavior the old default
  `duk page.js` had, kept reachable and explicit.
- REPL (bare `duk` on a terminal) and the framed manager daemon (no
  args, non-tty) are unchanged.
- House question answered along the way: node/v8's model is REPL with
  no args, run-the-file with an arg, and *no DOM at all* — we chose the
  better-than-node shape: node semantics by default plus an explicit
  `--browser` flag, so both are one command away. Docs updated
  (`14.biz/news` press pack, `install-duk.sh` usage, OPEN-ITEMS #11,
  roadmap §9, `NB-JS-CLI-NODE-LIKE-MODE.md` status).
- **`make check` now runs 4 suites**: dom/fetch/events (all PASS) plus a
  new `cli_test` (7 cases: plain run+argv, throw→1, process.exit→3,
  `--browser` DOM, stdin `-`, no-browser-globals, `--help`→2). All green.

## NOTICE 2026-09-07 — CLI-2 CommonJS require lands on `opencode`

Merge `origin/opencode` for the commit after the CLI-1 pack.

- Node mode now has `require()` + `module`/`exports` globals, per the
  design-doc ladder (`NB-JS-CLI-NODE-LIKE-MODE.md`). It's a pure-JS
  loader prelude (`g_cjs_prelude` in the worker) over one host hook
  (`__nb_read_file`, the existing 512 kB-capped reader).
- Entry `require` base = the script's own directory; absolute and
  `./`/`../` relative paths resolve; module files are wrapped
  `(function(exports, require, module, __filename, __dirname){...})`,
  so module locals don't leak and `this` === `module.exports` like node.
- `.json` files are `require`d via `JSON.parse`; circular requires serve
  the partially-initialized exports (standard CJS); `#!` shebang lines
  in required files are stripped. Cache is keyed by resolved absolute
  path.
- **No `node_modules`/packages/builtins** — a bare specifier throws
  `Cannot find module '<name>' (nbjs has no packages/builtins)`; a
  relative miss throws with the resolved path. Both exit 1.
- `console.error` now routes to stderr in node mode (`log`/`info`/`warn`
  stay on stdout, node parity). The 2 s CPU guard covers required
  modules too (`while(true){}` in a require'd file → killed, rc=142).
- `cli_test` grew to 9 cases (require chain incl. JSON/nested `../`/
  cycle/module-var-scoping + missing-module error); `make check` green.

## NOTICE 2026-09-07 — CLI-3 fs-lite + `-i` REPL closes the node-mode ladder

Merge `origin/opencode` for the commit after the CLI-2 pack.

- **`require('fs')`** now resolves to fs-lite natives in node mode and
  the REPL: `readFileSync` / `writeFileSync` / `appendFileSync` /
  `existsSync` / `mkdirSync` (recursive, mirroring the manager's
  `mkdir_p_local`). String-only payloads (no Buffer), optional encoding
  arg accepted+ignored, reads keep the 512 kB cap, miss → `ENOENT`-style
  error exit 1. `fs` is a builtin registered in the CJS loader — still
  no node_modules/packages.
- **`duk -i`** / `--interactive` forces the REPL even when stdin is
  piped: `printf '1+2\nexit\n' | duk -i` → `3`. The REPL now also ships
  `require()` + `require('fs')` on top of its browser prelude, so
  `document` stays available interactively.
- `#!` shebang stripping was already handled at entry
  (`DUK_COMPILE_SHEBANG`) and by the CJS loader for required files.
- Node-mode ladder (CLI-1 runner / CLI-2 CommonJS / CLI-3 fs+REPL) is
  now complete through CLI-3; ESM (CLI-4) remains the explicit hard
  tail. `cli_test` grew to 12 cases; `make check` green.

## NOTICE 2026-09-07 — CLI-4 source-level ESM lands; the ladder is done

Merge `origin/opencode` for `a022aec4` (CLI-3 pack was `1c0be598`).

- The CJS loader prelude now transpiles **source-level ESM** on load —
  Duktape has no `import` syntax, so `import`/`export` → CJS is a line
  rewrite. Entry files, `require()`d modules, and single REPL lines all
  go through it (`__nb_esm_prepare`). Output is pure ES5 (`var` only).
- Supported: default import (node interop — unwraps `.default` when the
  module sets `__esModule`, else the whole `module.exports`), named
  imports with rename, `import * as ns`, side-effect `import "mod"`;
  `export function`/`var`/`const`, `export default` (named + anonymous),
  brace lists with rename, `export { x as y } from`, `export * from`
  (skips `default`/`__esModule`). Transpiled modules get `__esModule`.
- CJS calling `require()` on an ESM file reads `.default` — standard
  node interop. Out of scope: multi-line statement imports, dynamic
  `import()`, decorators/type annotations.
- Also in the same session: **rung-2 DOM remainder** landed (`3df68bf9`):
  `document.head`, `createTextNode`, `getElementsByClassName`,
  `removeChild`/`insertBefore`/`replaceChild`, `removeAttribute`,
  `style.*`, `value` — plus a latent wrapper-identity bug fix (the node
  wrapper cache wrote into the object instead of the stash map, so
  per-node `style`/`value` never survived a re-fetch).
- `cli_test` is now 18 cases; `make check` green (dom/fetch/events +
  cli). No thread/manager/daemon changes.

## NOTICE 2026-09-07 — rung-6 file-backed `document.cookie` jar lands

Merge `origin/opencode` for `1f943aba`.

- The resident worker's `install_dom()` now redefines the prelude's
  configurable `document.cookie` stub with **real C natives** backed by a
  disk jar at `$NB_COOKIES_FILE` (fallback `~/.config/nbjs/nb_cookies.txt`).
  Each LOAD runs a fresh Duktape heap, so the jar file is the only
  cross-LOAD persistence — cookies now survive paging.
- RFC-6265 subset: `name=value` + `Domain`/`Path`/`Expires`/`Max-Age`/
  `Secure`; host-scoping + path-match enforced on read; `max-age=0` and
  past `Expires` delete; IMF-fixdate parse via days-from-civil (no TZ
  deps); atomic tmp+rename writes; damage-tolerant reads. Jar lines are
  TAB-separated `host\tpath\tname\tvalue\texpires\tesecure`.
- New `make check` suite **`wck`** (`tests/worker_cookie_test.c`): one
  worker, THREE LOADs — set+read-back on `http://example.com/dir/`,
  fresh-heap GET (persistence across LOADs), and a cross-host page on
  `other.test` that must see nothing; C verifies the literal jar-file
  content afterward. All suites green.
- Notes: the old rung6 eval fixture `tests/rung6_cookie_test.js` still
  exercises the prelude fallback (reads `''`, drops writes). Default
  path is `$HOME`-based; since 2026-09-08 the manager's `worker_spawn`
  passes `NB_COOKIES_FILE="<house>/#.desktop/nb_cookies.txt"` to its
  resident worker (live-verified: page A set `sid=abc123`, navigated via
  `location.assign` to page B which read it back — jar landed per-house).
- Still open on rung 6: real `history`/`location` navigation to the
  manager (house-standard lock applies before editing
  `network_browser_manager.c`). No thread/manager/daemon changes.

## NOTICE 2026-09-08 — rung-6 real `history`/`location` navigation lands

Merge `origin/opencode` (one commit after the cookie jar).

- The page's `location.*` (`assign`/`href=` setter/`replace`/`reload`)
  and `history.*` (`back`/`forward`/`go`) now route through real natives
  in `ops/nb_host.h` (`nav_resolve`/`nav_request`) into ONE pending NAV
  the worker daemon emits as a `NAV\n<kind>\n<url-or-count>\n` frame right
  before `STATUS ok`. `history.go(n)` becomes BACK/FORWARD ×n (capped 8),
  `go(0)`/`reload` = RELOAD. Eval/CLI paths keep the request inert.
- `pushState`/`replaceState` keep the in-heap bookkeeping stack
  (`history.state`/`length`) AND send `ADDR`, so the manager updates the
  address bar (`g_current_url` + current tab url) with **no fetch**.
- Manager side (`network_browser_manager.c`): `worker_load` captures NAV
  frames; new `consume_pending_nav()` in the main loop (after
  `handle_request()`) runs them through the same `do_fetch`/Back-Forward
  file stacks as a `go:`/`back:` toolbar request. GO = link-like (push
  current to Back, clear Forward, visit log); REPLACE = navigate without
  a history entry; RELOAD = re-fetch current.
- New `make check` suite **`wcn`** (`tests/worker_nav_test.c`): 11 LOADs
  asserting the exact NAV frame for assign/href=/replace/reload/
  replaceState/pushState/back/go(-2)/go(2)/no-nav, plus a follow-through
  re-LOAD. All suites green (`wdt/wft/wet/wck/wcn` + 18-case cli_test).
- **Live end-to-end verified** against the real manager on a throwaway
  house: a page whose JS called `location.assign(b.html)` → manager
  fetched/re-LOADed b.html and recorded a.html on the Back stack; a page
  doing `history.pushState('/c2')` → address bar changed to that URL with
  no re-fetch. No renderer/chtpm core changes (house rule).
- Remaining rung-6 scraps: none blocking; rung 7 (layout awareness)
  deferred. No new mode globals, no `khtpm_core_render.c` edits.

## NOTICE 2026-09-09 — rung-6 real `localStorage`/`sessionStorage` lands

Merge `origin/opencode` (two commits after the navigation notice).

- `install_dom()` in `ops/nb_js_worker.c` replaces the prelude's twin
  no-op stubs with fresh objects wired to real C natives.
- **localStorage** = disk jar at `$NB_LOCALSTORAGE_FILE` (fallback
  `$HOME/.config/nbjs/nb_localstorage.txt`); the manager points its
  resident worker at `<house>/#.desktop/nb_localstorage.txt` in
  `worker_spawn`, so each house owns one persistent jar (same pattern as
  `nb_cookies.txt`). Jar is TAB-separated `pct-encoded key\tpct-encoded
  value` lines (RFC-3986-safe → tabs/newlines round-trip), atomic
  tmp+rename writes, damage-tolerant reads, 256 entries/256B key/4kB
  value.
- **sessionStorage** = in-memory store reset per LOAD (fresh heap ⇒
  correct scoping; no disk).
- New `make check` suite **`wst`** (`tests/worker_storage_test.c`):
  2 LOADs — set on L1, persist-get on L2 for localStorage, session reset
  on L2 — plus jar-content assertions incl. percent-encoding. All six
  worker suites + 18-case cli_test green; `build.sh` clean (pre-existing
  `-Wformat-truncation` .tmp noise only).
- **Live end-to-end verified** against the real manager on the throwaway
  house: page a set `localStorage.theme=night` + a sessionStorage key,
  `location.assign(b.html)`; page b ran in a fresh LOAD, read
  `theme` (persisted) and found the session key empty (`ss_seen=none`),
  then wrote to the same jar — final jar on disk:
  `theme\tnight` + `ss_seen\tnone`. No renderer/chtpm core changes.
- Debug note for future agents: `duk_def_prop` on the *prelude's stub
  objects* throws `'not configurable'` at boot (dead worker, WST_RC=141,
  every page fails trivially) — create fresh objects + `duk_put_global_string`
  instead; never put accessor def_props on prelude-created globals.

## NOTICE 2026-09-09 — boot hygiene: no more silent dead workers

Merge `origin/opencode` (commits after the storage notice).

- The one trailing cost of the storage fix was invisibility: a boot-time
  throw stopped the worker with zero trace on the manager or in any log
  (only WST_RC=141 + "no reply from worker" in the suite). This slice makes
  every worker failure visible at the module level.
- `ops/nb_js_worker.c`: `install_dom()` + `install_events_timers()` now run
  under `boot_install_safe()` (a `duk_pcall` guard). On throw it prints
  `WERR| boot install: <msg>` to stderr and lets the page load anyway —
  a future `'not configurable'`-class boot bug shows up as a line, not a
  corpse. Both boot sites (page runner + REPL) use the guard.
- `network_browser_manager.c`: `worker_spawn` redirects the worker's stderr
  to `<house>/#.desktop/network_browser_worker.err.log` (per-house append);
  `worker_close` tails that log onto the manager's stderr as
  `[worker] <line>` (offset-tracked, one line per worker death, no re-dump).
  `worker_load` also relays any `ERROR|` rows to the module log without
  aborting the STATUS frame read.
- Harnesses: all six worker suites print a `harness: worker killed by
  signal %d - see WERR| stderr above` note instead of a bare rc on crash.
- Verified: full `make check` green (6 worker suites + 18-case cli_test),
  `build.sh` clean. Live E2E against the real manager on the throwaway
  house: page loaded with resident worker, script ran
  (`TEXT|worker-ran` replaced the static row), `network_browser_worker.err.log`
  exists at the configured path; killing the worker then driving a LOAD
  surfaced the buffered stderr lines as `[worker] ...` on the manager.
  No renderer/chtpm core changes.

## NOTICE 2026-09-09 — Phase 2 LANDED: document-order per-script runs

Merge `origin/opencode` (commits after the boot-hygiene notice).

- The last "Still missing (Phase 2)" item is gone. `collect_scripts` now
  writes each `<script>` (inline or `<script src=...>`, in DOM order) as
  its own slice of page.js, separated by a `/*nbjs-script-boundary*/`
  sentinel (the old `;try{...}catch` concat wrapper is gone). The worker's
  `run_scripts_slices` compiles+runs each slice as a separate Duktape
  program.
- Browser classic-script parity, in the worker: document order; a syntax
  or runtime error in one slice prints `WERR| script N: <msg>` and the
  NEXT slice still runs (previously a single parse error killed the whole
  page); top-level `var` still lands on the shared global (cross-slice
  visibility); an external src executes at its DOM position. Legacy
  page.js without any sentinel is treated as one program.
- New `make check` suite **`wps`** (`tests/worker_scriptseq_test.c`,
  4 cases: order+isolation `seq=1,3` despite a bad middle slice;
  cross-slice globals; external-at-position `a,b,c`; legacy single
  program). All 7 worker suites + 18-case cli_test green; `build.sh`
  clean.
- Live E2E vs the real manager (throwaway house): inline/ext/inline page
  rendered `TEXT|seq=a,b,c`; a page whose middle script had a syntax error
  still rendered `after-bad:s1`, with `WERR| script 1: SyntaxError` in
  `network_browser_worker.err.log` and surfaced as `[worker] ...` on the
  manager at worker close. No renderer/chtpm core changes.
- Honest remaining gaps (now stated in the roadmap + OPEN-ITEMS): real
  `http://` fetch breadth behind the manager's curl ladder, and rung 7
  CSS/layout awareness.

## NOTICE 2026-09-10 — real `http://` fetch breadth LANDED

Merge `origin/opencode` (commits after the Phase-2 notice).

- Live E2E vs `python3 -m http.server` (throwaway house + fixture site)
  proved the manager's curl ladder end-to-end: a page with inline +
  external-relative `<script src>` renders `TEXT|seq=a,b,c` (TITLE
  extracted); localStorage set on one `http://` page persists to the next
  http navigation (`theme=http-ok`); JS-side relative `fetch("api.json")`
  resolves against the page URL and hits the real server
  (`st=200 fetch={"hello":"rung4-http"}`) — the rung-4 transport over real
  HTTP.
- Two worker-side fixes:
  - **Prelude `splitParts` double-port bug** (`ops/nb_host.h`): it re-appended
    `b.port` after `b.host` already embeds it, yielding
    `http://127.0.0.1:8123:8123/...` for any base with an explicit port —
    curl exited 3 (URL malformed). file://-based suites never had a port so
    `wft`/`wrapper` missed it. Port re-appender removed.
  - **`nb_fetch_sync` belt-and-braces** (`ops/nb_js_worker.c`): new
    `resolve_doc_url()` merges the incoming URL against `g_href`
    (scheme/`//host`/`/abs`/relative-dirname, RFC 3986 §5-style, mirroring
    the manager's `resolve_url`); fetch errors now carry the resolved URL
    (`curl rc=3 status=0 url=http://...`). The prelude still pre-resolves
    for both fetch and XHR; the C resolver is a no-op for absolute URLs.
- All 7 worker suites + 18-case cli_test + `sh build.sh` (4 binaries)
  green post-fix. No renderer/chtpm core changes. Only rung 7
  CSS/layout awareness remains as an honest gap.