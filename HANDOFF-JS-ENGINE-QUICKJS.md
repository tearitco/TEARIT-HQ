# HANDOFF — resume the JS engine swap (Duktape → QuickJS)

Single entry file for the resuming instance. Read THIS first, then the
two deep docs in §6, then execute §8. All paths in this doc are
**repo-relative** — they are valid at the root of the cloned repo on
ANY machine (on this machine the clone lives at
`/home/no/Desktop/github/work/NNEST-12.00-opencode`).

## 1. The job (compacted)

Replace the Duktape 2.7 (ES5.1-only parser) engine with **QuickJS
2026-06-04** in the `network` cell so youtube.com's real ES2020+ bundle
can run (roadmap row 31). Two approved decisions:

1. **Native job queue** — delete the C stash-FIFO microtask emulation
   and the prelude's Promise polyfill; drain via `JS_ExecutePendingJob`.
2. **Full worker+host graft, all 44 suites green in-session.**

Scope: pipe protocol, DOM tree, cookie jar, storage jars, `__nb_sha1`,
CSS cache, event-loop C-state (~2100 of the worker's 3912 lines) survive
UNTouched. Only the duk boundary (~750 lines / 54 API names / 105
natives in `ops/nb_js_worker.c` + 19 names/16 natives in `ops/nb_host.h`
+ 11 refs in `ops/nb_js_eval.c`) and the Makefile `nbjs` target change.
The 44 suites are pipe-driven and engine-agnostic — do not modify them.

## 2. Getting the work on a NEW machine (this is the different-machine case)

**Where the code lives (GitHub):**
```
repo:   https://github.com/tearitco/TEARIT-HQ.git
branch: opencode   (ALL of this job's work lives on `opencode`)
```
To pick this up on the new machine:
```
git clone -b opencode https://github.com/tearitco/TEARIT-HQ.git tearit-hq
cd tearit-hq
```
Then open THIS file from the cloned root and follow §6 → §8. Everything
is repo-relative, so paths work at any clone location.

**⚠ PUSH GATE — required before the new machine clones.** As of this
write, `opencode` is **ahead of `origin/opencode` by 8 commits** — the
vendored QuickJS files, the resume docs, probe receipts, and the graft
plan are NOT on GitHub yet. A fresh clone will NOT contain them. The
user MUST push first (`git push origin opencode`) or transport the
worktree by other means (e.g. the desktop copy of this file + simplest
is to push). After the push, the clone above pulls everything.

**This machine's local layout (for reference only, not reproduced on a
new machine):** this repo is used via TWO worktrees of one git store:
`/home/no/Desktop/github/work/NNEST-12.00` = branch `claude` (the other
agent — never commit there) and `/home/no/Desktop/github/work/
NNEST-12.00-opencode` = branch `opencode` (all work). On the new machine
a single clone on `opencode` is correct.

Commit rules (same everywhere): commit ONLY on `opencode`, scoped
`git add <path>` per file (never `-A`); NEVER stage runtime state files
(pdl, logs, pids, `.png`, built binaries `nbjs`/`w*`); never end a
block with uncommitted code (mid-work ok as `wip: <what>`); never
merge/cherry-pick/push without the user's explicit "push"; never commit
to `main`/`claude`/`chtpm-delete-per-app-c`.
Paths contain `&`, `#`, `!` — **always quote them in bash**.

## 3. Exact state at handoff (2026-09-18)

DONE (committed on `opencode`, unpushed):
- Decision + probe receipts + graft plan: `6171d38c` `fe74897b`
  `67e06d22`.
- **QuickJS 2026-06-04 vendored** into `44.xyz.01.00/&.hq-apps/js/`
  (13 files; `duktape.*` still present = rollback) — commit `d8ef8b3a`.
- Resume pack + translation table in the insight doc §8-§9 — commit
  `05306e97`.
- Full read of `ops/nb_js_worker.c` (3912 lines); per-file duk API
  inventories captured.

NOT STARTED: the actual code translation, the build, the 44-suite run.
No transplant line has been written yet.

## 4. Critical facts (do not re-derive)

- Engine file set: `quickjs.c cutils.c libregexp.c libunicode.c dtoa.c`
  + `-lm`. **NO `libbf.c`** (merged into quickjs.c in this release).
- Build flags: **`-std=gnu11`** NOT `-std=c11` (quickjs.c uses the `asm`
  keyword at ~line 60600) + `-D_GNU_SOURCE -DCONFIG_VERSION="2026-06-04"`
  + `-fwrapv`. Our TUs may stay `-std=c11`; only engine TUs break.
- Prelude `g_js_prelude[]` (nb_host.h lines ~339-484, 19,805 bytes)
  stays verbatim EXCEPT the Promise-polyfill chunk (deleted).
- Microtask drain: `while (JS_IsJobPending(rt)) JS_ExecutePendingJob(rt,
  &jctx)` in `run_event_loop`; stack-FIFO C code deleted; timers become
  C-held `JSValue`s (dup to fire, free on clear, free all at teardown).
- Rollback anchor: pre-graft commits + `duktape.c/.h` still in `js/`.
  `nb_js_eval.c` is ALSO ported (11 refs) — a Duktape eval against a
  QuickJS `nb_host.h` will not compile.
- `make check` 44 PASS was on the DUKTAPE build; nothing has run on
  QuickJS yet. Never report green without a fresh build + fresh run.

## 5. Region map of `ops/nb_js_worker.c` (the transplant surface)

1-158 framing + `g_live_ctx` decl (→ `JSContext *`); 159-488 DOM/CSS/
selector C-state — untouched; 490-527 `get_node`/`get_this`/ONPROPS;
528-812 CSS + element/document natives; 814-1196 element/classList
natives; 1198-1347 `push_node` + node-identity (stash map → `__nb_idmap`
on the global object); 1349-1390/1722-1840/1841-2200 jars/storage C
helpers untouched, surrounding natives translate; ~2061 `install_dom`;
~2300-2460 timers/RAF natives + event binding (stash → JSValue fields);
2461-2812 `drain_microtasks` DELETE + `run_due_timers`+`run_event_loop`
(native queue); 2825-2874 `__nb_sha1` + `install_events_timers`;
2880-2893 `boot_duk_install`; 2895-2906 alarm CPU budget — stays;
2908-3123 `run_page` (JS_NewRuntime/JS_NewContext/JS_Eval/JS_Call);
3129-3291 `cmd_eval` + REPL; 3293-3448 CLI node-mode natives;
3481-3679 `g_cjs_prelude` — untouched; 3681-3912 `cli_main`+`main`
(`JS_Eval` + strip `#!` shebang manually — no SHEBANG eval flag in this
build).

## 6. Docs to read next (in order)

1. **MAIN handoff doc:** `x0.parent-level-dev-env-04.04/yz.muchiverse/
   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
   JS-ENGINE-QUICKJS-SWAP-INSIGHT.md`
   → §8 RESUME PACK (git use, job, standards, state, next-steps order,
   rollback) and **§9 the full duk→QuickJS translation table** (the
   meat: every API pair + ownership cheat-sheet + worked conversions).
2. **Compact:** `x0.parent-level-dev-env-04.04/yz.muchiverse/
   #.#.calendar-dox/!.HQ-IQ-BOOK/00-compact/browser.md`
   (live browser-cell state, paths, house rules).
3. House rules if in doubt: `AGENTS.md` (repo root), `01-orientation/
   BRANCH-STRATEGY.md`, `03-pitfalls/OPERATIONAL-LANDMINES.md` (#10 =
   commit rule).
Offline backup of all house docs (only exists on this machine):
`/tmp/HQ-IQ-BOOK.7z`.

## 7. Repo-relative key paths (never typed — copy)

- Worker: `44.xyz.01.00/&.hq-apps/network/ops/nb_js_worker.c`
- Host/prelude: `44.xyz.01.00/&.hq-apps/network/ops/nb_host.h`
- Eval CLI: `44.xyz.01.00/&.hq-apps/network/ops/nb_js_eval.c`
- Makefile: `44.xyz.01.00/&.hq-apps/network/Makefile`
- Engine libs: `44.xyz.01.00/&.hq-apps/js/` (quickjs.* etc., new)
- Suites: `44.xyz.01.00/&.hq-apps/network/tests/` (44 pipe-driven)

## 8. Next steps (exact order)

1. Read §6 docs 1+2. Confirm `git log --oneline` shows `d8ef8b3a`
   `05306e97` on `opencode` (vendored + resume pack committed).
2. Rewrite `ops/nb_host.h`: `#include "../js/quickjs.h"`; 16
   registrations → `JS_NewCFunction`+`JS_SetPropertyStr`; accessors →
   `JS_DefinePropertyGetSet` (+magic); strip ONLY the Promise polyfill
   chunk from `g_js_prelude[]`.
3. Rewrite `ops/nb_js_worker.c` top→bottom per §5 using the §9 table
   (host doc).
4. Rewrite `ops/nb_js_eval.c` (11 refs).
5. Makefile `nbjs`: CFLAGS `-std=gnu11` + the two qjs flags; swap
   `duktape.c` → the 5 engine TUs.
6. `make` → iterate compiler errors (mass signature-only fixes).
7. `make check` → 44 suites green from fresh build + fresh run; fix +
   commit each regression (suspects: timer order wlt/wss, microtask
   cadence wfp/wit, exception-string formats).
8. Flip house docs to GREEN: insight doc (graft DONE + commit hashes),
   browser.md (engine-swap line), NB-JS-ENGINE-ROADMAP. Commit scoped.
   Present evidence; await the user's "push".