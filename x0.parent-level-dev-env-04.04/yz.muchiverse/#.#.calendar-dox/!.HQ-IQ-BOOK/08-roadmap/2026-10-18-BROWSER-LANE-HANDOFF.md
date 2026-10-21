# 2026-10-18 — Browser lane handoff

**Branch `opencode`.** Tip is the commit that added this document
(local == remote, verified with `git ls-remote`). Everything below is
measured, not estimated; where something is unproven it says so.

---

## 00. DIRECTION CHANGE 2026-10-20 - read this before section 0

**The owner's goal for this browser is now a fully Chrome-replaceable browser** (use it instead of Chrome, every day, for everything). "Chrome parity is a non-goal" - in the 10-05 roadmap, the 10-11 gap assessment section 1 and section 6 - is **withdrawn**. Nothing is out of scope up front: a feature Chrome has and we lack is an open, sized, queued item.

Read, in this order: **`2026-10-20-CHROME-REPLACEMENT-SIZING.md`** (same folder). It has the sized backlog (groups A-I, about 148-271 sprints, estimates), the four rungs R1-R4, the order of work, and the evidence behind the numbers.

What it changes for you:
1. **JIT is decided** (owner: "we need JIT"). Group F: measured 4-12x slower than V8 on hot JS loops and calls (fib(35) 1.3 s vs 0.1 s). Order: benchmark suite on real pages, then evaluate and adopt a JIT-enabled QuickJS, then a JIT-safe sandbox. Our own JIT only if the adoption fails.
2. **Next after flex and grid** (group A, section 5a below stays your task order): import the Web Platform Tests and a top-50 site sweep as the yardstick, then the cheap compatibility items (send `--compressed`, page ES modules, FormData/Blob, observers, `position:fixed` / `z-index`, `@font-face`), then sandbox the JS worker.
3. **Still true:** one KPI at a time, red before green, never report a number you did not observe. The sizes are estimates; correct the calendar mapping (2-4 days per sprint) when real work lands.
4. **Open owner decision:** protected-video (DRM) streaming. Do not start it.
5. If you cannot find these files: `git pull origin opencode` - they are on `origin/opencode` since commit `acc042035`.

## 0. START HERE (if you are a model with no context, read this and nothing else first)

You are picking up the **house network browser** lane. Everything below is
measured. Do not trust a green number here until you have shown it can go red.

**The task, in order.** §5a is the work: **G2.3 flex row**, three steps that
must happen in this order — a RED test (already written and red, run it first),
then the worker's flex model, then the manager's projection so the renderer
lays the columns out the same way. **Step 2 alone re-opens G2.2** with a
confident wrong number; that is the whole point of §4.

```sh
HR='.../yz.muchiverse/44.xyz.01.00'          # the house root

cd "$HR" && sh '$.crypts/button.sh' build    # MUST be PASS=145 FAIL=0
cd "$HR/_.monads/_.livedesk-taskbar/ops" && sh build_core_render.sh   # separate!
cd "$HR/&.hq-apps/network/tests"
sh nb_flex_test.sh     # G2.3 - RED by design, this is your starting point
sh nb_gbcr_test.sh     # G2.2 - MUST stay 100% (9/9). re-run after every change
sh nb_all_tests.sh     # whole suite: PASS=30 FAIL=0 expected
```

**Four things that will waste your hour if you do not know them now:**

1. **`khtpm_core_render.c` is a separate build** (`build_core_render.sh`), and
   **resident processes do not pick up rebuilds** — restart the manager after
   worker changes, the window after renderer changes. A stale process will
   report a bug you already fixed.
2. **`xyzfs/users` is a git landmine.** Never reset/checkout/clean across a
   branch that tracks fewer user files than your checkout; it deletes wallets
   from disk. §3.
3. **`fprintf(stderr)` is invisible in the renderer** — it never reaches a log.
   Write diagnostics to a **file**.
4. **The checkout is shared with other agents.** Stage and commit only your own
   paths, with an explicit pathspec. Never `git stash`, never `git reset` with
   someone's work staged.

**Do not** wire `nb_flex_test.sh` into `nb_all_tests.sh` until G2.3 is
actually done — it is red on purpose and would turn the suite red on purpose.

---

Companions: `2026-10-14-G1-SPRINT-VERDICT.md`,
`2026-10-17-G2-VERDICT-ADDENDUM.md`, and
`design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md` §9.1–9.14
(the full measured record).

---

## 1. Where the work stands

| KPI | state | evidence |
|---|---|---|
| G1 — JS apps actually run | ✅ closed | 6/6 KPIs; timer tick, push protocol, focus safety, SPA click-through, canvas, rAF |
| G2.1 — box tree | ✅ closed | every visible node carries x/y/w/h |
| G2.2 — geometry honesty | ✅ closed, **100 %** | `9/9 comparable rows within ±2 px`, wired into `nb_all_tests.sh` |
| G2.2b — scrollbar gutter is conditional | ✅ closed | fitting page keeps full column, overflowing page loses exactly 8 px |

Current suite state, both verified on this tip:

```
house build    145 scripts — PASS=145 FAIL=0 TIMEOUT=0
hermetic suite PASS=30 FAIL=0   (G2.2 and the gutter control both PASS)
```

**Update 2026-10-19 — the mirror/rendered disagreement is root-caused and
`nb_find` is green.** The symptom §3.2 called "computed against a row set that
momentarily differs" was **not** a race: `dump_frame_png()` redraws *before*
serializing, so the mirror and the frame come from one tree in one tick, and
only a **stale cached count** can disagree. `reparse_chtpm_if_changed()` zeroed
`g_find_hits`/`g_find_at` on every rebuild while its own comment promised
"a content change should re-search", and nothing re-ran `kh_find_commit()`.
Fixed at the two reparse sites (`khtpm_core_render.c` `:3256`, `:3560`, +52/−1);
the reveal had to run **before the caller's layout**, not at `redraw()`'s top,
because `g_layout_fresh` skips that layout (`:12066`). Full write-up, the
deterministic before/after and the pitfalls are in the burndown **§7** — read it
before touching find or reparse.

Measured on the final build (rebuild + window restart), ≥6 runs each, plus a
full `sh nb_all_tests.sh`:

```
nb_all_tests.sh       SOME TESTS FAILED, exit 1 — 30 suites, 3 FAIL lines,
                      all in the mouse-span family (was 3 failing suites)
nb_find_test.sh       8/8 pass, 0 FAIL + PASS in the runner (baseline: FAIL)
nb_selrun_test.sh     2/6 standalone + PASS in the runner (baseline: FAIL)
nb_gbcr_test.sh       6/6 pass, 0 FAIL   (KPI held at 100 %)
nb_span_select_test.sh 5/6 pass          (failed S1/S2 once → 1-in-6 flake)
nb_span_drag_test.sh   3/6 pass          (T2; still undiagnosed)
```

**K1 is not green, but its floor moved**: the mouse-span suites are now the
only reds. Both are hermetic fixtures that never arm find, so they are
provably independent of the fix above — and a single green run of the runner
proves a *rate*, not a fix (a 1-in-6 flake reds the whole verdict).

**If you did not rebuild + restart the window, you are measuring yesterday's
renderer.** The renderer is *not* picked up by resident processes.

**S3 is done.** The plan marked it *"the gate for all layout work"*, so G2.3
is the natural next slice — see §5.

---

## 2. Verify it yourself, in this order

```sh
HR='.../yz.muchiverse/44.xyz.01.00'

# 1. the house build — the one that must be green before anything else
cd "$HR" && sh '$.crypts/button.sh' build

# 2. the browser suite. Hermetic parts run alone; the live parts need a window:
cd "$HR/&.hq-apps/network/tests" && sh nb_all_tests.sh

# 3. G2.2 alone, if you only want the geometry claim
sh "$HR/&.hq-apps/network/tests/nb_gbcr_test.sh"   # expect 9/9, 100.0 %
```

Two things will bite you immediately if you skip them:

- **The renderer is a separate build.** `button.sh build` covers the browser
  app; `khtpm_core_render` is built by
  `44.xyz.01.00/_.monads/_.livedesk-taskbar/ops/build_core_render.sh`.
- **Resident processes do not pick up rebuilds.** After changing the worker,
  restart `network_browser_manager`; after changing the renderer, restart the
  window. A stale process will happily report a bug that was fixed an hour ago.

---

## 3. Landmines that cost real time — please read before you touch git

**`xyzfs/users` is the big one.** The code branches deliberately do not track
it; the data lives on local `user/*` branches and is snapshotted by
`button.sh save-data`. On this merge a fast-forward **did** delete 1,741 files
and all 18 wallets from the live house:

```
ff:      2,004 files 18 wallets  ->  255 files 0 wallets
restore:                              2,004 files 18 wallets
sha256 verify against a pre-reset tarball: exit 0
```

Nothing was lost **only** because `save-user-data.sh` had already run and the
restore path had been dry-run and hashed *before* anything was deleted. If you
ever face a branch that tracks fewer user files than your checkout:

1. `sh '$.crypts/button.sh' save-data` — snapshot first
2. prove the restore: `git archive <user/* branch> | tar x` into a scratch dir,
   hash it against a tarball taken **outside** the repo
3. only then move the tree

**Never** `git reset --hard` / `checkout` / `clean` across that boundary. A
restore tested after the fact is a hope.

**Other traps hit this lane:**

- The house build script is `44.xyz.01.00/$.crypts/button.sh` (verbs: `build`,
  `reset`, `status`, `save-data`, `quit`). Not at the repo root.
- `khtpm_core_render.c`'s stderr does **not** reach a log. Any diagnostic
  written to stderr is invisible — write to a **file**.
- `/tmp` is unreliable on this machine; another agent's commit records hard
  crashes with `/tmp` scratch. Long-lived scratch belongs under `$HOME`.
- `button.sh reset` kills every desk process and relaunches from the house you
  run it in. It also runs `save-data` first, which is why it is safe.

---

## 4. The instrument catalogue — the single most useful thing here

Six of the failures this lane were **not** bugs in the code under test. They
were bugs in the thing doing the measuring, and every one produced a confident,
plausible, wrong number.

| instrument | what it actually did |
|---|---|
| `nb_gbcr_test.sh` class filter | compared browser **chrome** (7 nav buttons + 5 tabs = *also exactly 12 rows*), not the 12 paragraphs |
| `frame_geom.py` stdout | class column truncated to 18 chars, so `from-top,nb-content` was unmatchable as a substring |
| `browser_pid.sh` | printed the **whole** pid list, so callers built relay paths like `.../100153 2367044.txt` |
| `browser_pid.sh --require-one` | counted **lines** in a one-line string ⇒ always 1 — a guard that *cannot fail* |
| `xclipget.py` | returns whatever the clipboard holds; read right after Ctrl+C it can **succeed** with the previous owner's bytes |
| my own gutter negative control | an assignment the very next line overwrote — a dead statement, so it broke nothing and "passed" |

Four of those were caught **only** by negative-testing. One was caught by
diffing a passing run against a failing run and finding the traces
**byte-identical**, which eliminated the whole suspected subsystem in one step.

**Two rules worth more than the fixes:**

1. **A green number has not been measured until you have shown it can go red.**
2. **Before theorising about a subsystem, instrument it and diff a pass against
   a fail. If the traces match, the bug is not in the instrumented code.**

Also: **do not trust a count read off a live or conflicted index.** I reported
"17 files under `xyzfs/users`" from `git ls-files` mid-merge; the truth from
`git ls-tree` on a commit was **1,749**.

---

## 5. Open work, precisely scoped

### 5a. G2.3 flex row — and it is **two-sided**, not "wiring"

Measured today, a `display:flex` div with three `<p>` children:

```
worker:    c1 x=0   y=30 w=389    c2 x=0   y=60 w=389    c3 x=0   y=90 w=389
renderer:     x=200 y=180 w=389       x=200 y=210 w=389       x=200 y=240 w=389
```

Both **stack** — both treat the container as a plain block, so both agree
today. The plan calls S4 *"wiring, not invention"*. That is true for the
**worker** (it has a full box-tree engine and `nb_css` already parses
`display`), and **false for the renderer**: the manager ignores the DOM's
`display` entirely and emits a fixed chrome skeleton, so there is no path from
a flex div to a laid-out row.

So the slice is three steps and **only in this order**:

1. **RED test.** G2.3's own words: *a page-side `display:flex` container lays
   children out in a row with correct widths (sampled columns share edges
   within 1 px).* Assert adjacent columns share edges within 1 px. It must be
   red before any code moves.
2. **Worker.** Flex row model in `nb_box_w` / `layout_xy`: children share the
   container's width, each `w = share`, `x` accumulates, all on one `y`.
3. **Manager.** Project a flex container's children so the renderer lays them
   out as columns. The renderer already does row/wrap/grow/gap for `<row>`;
   finding that mapping *is* the wiring.

> **Do not ship step 2 alone.** It would make the box tree report side-by-side
> columns while the renderer still paints them stacked — re-opening G2.2 with a
> new, confident wrong number. Re-run `nb_gbcr_test.sh` after each step.

### 5b. `nb_span_drag_test.sh` — ~60 % fail at T2

Same symptom as the bug fixed in §9.13 (*"no highlight band"*, whole-label
copy), but a different path: the **mouse-motion** handler, which sets the
cursor without going through `handle_key`. Look at
`khtpm_core_render.c:15597` (the `g_span_pend` / `Button1Mask` motion branch).
The keyboard-side hijack is fixed; this is the pointer side.

Note the variance is large (`1/4, 6/8, 4/10, 1/10` across configurations), so
take ≥10 runs before believing any number here.

**Measured 2026-10-19, final build, 6 runs: 3/6 pass (50 % fail).** Two
corrections to what was believed before, both re-measured:

- `click.log` is **not** always missing. T1 passed 3/6, and two *failed*
  runs printed `FAIL: T6: late release did nothing (log: CLICKED LINK1 …)`
  — the deferred click completes, sometimes. Do not debug this as "the
  action never fires".
- The **stale-fixture-window** theory is **refuted**: no `764x264` window
  exists on screen, and the only two long-lived renderers are legitimate
  house windows (`khtpm_strip_header.xhtpm`, `hotbar-desk.xhtpm`).

**It shares one shape with `nb_span_select_test.sh`** (5/6 standalone that
same day — and *that* suite is what reds the runner, a 1-in-6 flake): the
range never lands and the copy falls back to the whole row text —
`clipboard was [go to ONE then TWO then THREE now]` plus
`no highlight band (0)`. Debug the two **together**: one bug with two entry
points (drag vs. click-range), not two bugs. Neither suite arms find
(`grep -c 'Ctrl+F\|find in page'` → `0` in both), so neither can be caused
by the find/reparse fix in the burndown §7.

### 5c. Also flaky, lower priority

`nb_find_test.sh` (~33 % pass) is **no longer flaky — it was a real bug**:
root-caused and fixed 2026-10-19, 8/8 runs green, see the burndown §7. Treat
that as the warning: "flaky" was a diagnosis, and it was wrong. It was a
stale cached hit count that only looked nondeterministic because the
procedure around it was.

Still flaky: `nb_selrun_test.sh` (2/6 standalone on 2026-10-19, though it
**PASSED** in the runner that same day — one pass proves a rate, not a fix)
and `nb_span_select_test.sh` (5/6). `nb_span_select_test.sh` already got the
copy-receipt fix (45 % → 85 %); the same fix was applied to the other two
and **measured worse on both**, so it was reverted. Do not re-apply it
without ≥10 runs — the principle is sound but the noise is larger than the
effect.

---

## 6. Files that matter

| path | what |
|---|---|
| `&.hq-apps/network/ops/nb_js_worker.c` | the box tree: `nb_box_w`, `nb_content_h`, `layout_xy`, `g_col_px`, `g_content_inset_top`, `tick_slice` |
| `&.hq-apps/network/network_browser_manager.c` | projection + `LOAD` protocol (field 6 = pane w, **field 7 = pane h**) |
| `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` | `publish_pane_geometry()` (on **every** exit path of `assign_nav_and_layout`), `GENERIC_SCROLLBAR_W` 8, `GENERIC_SBAR_ARROW_H` 14, `kh_clipboard_copy`'s COPY receipt |
| `&.widgits/_shared-lib/khtpm_draw_core.c` | `seg_anchor_extend` / `seg_cursor_step` — **house-wide**, every app's inline-span selection |
| `&.hq-apps/network/tests/nb_gbcr_test.sh` | the G2.2 KPI |
| `&.hq-apps/network/tests/nb_gutter_test.sh` | the gutter's negative control |

Constants worth memorising: `GENERIC_SCROLLBAR_W` **8**, `GENERIC_SBAR_ARROW_H`
**14**, `NB_ROW_PX` **30**, `NB_CONTENT_PX` **640**. The frame serializer emits a
fixed **29** pipe-separated fields, x/y/w/h at 9–12 from the left.

---

## 7. House rules that applied here

- Branch `opencode` is this lane's. Never commit to another tool's branch.
- **Stage and commit only your own paths**, and commit with an explicit
  pathspec so another agent's staged work can never ride along. This checkout is
  shared — two agents were live in it.
- Never `git stash` and never `git reset` with someone else's work staged.
- One commit on this branch (`cfe1e567b`) preserves *another* agent's uncommitted
  registry work, because it blocked the fast-forward and discarding it would
  have destroyed 104 authored lines. It is deliberately labelled `wip:` and
  states in its message that it is not my work.
- Never merge, cherry-pick or push unprompted.