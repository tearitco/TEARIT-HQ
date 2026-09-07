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
allowed`) in both CLI and REPL — use `function(){}` callbacks.
Edition is Duktape 2.7.0 with DUK_USE_ES6 on, but arrow syntax is
simply not present in the parser.