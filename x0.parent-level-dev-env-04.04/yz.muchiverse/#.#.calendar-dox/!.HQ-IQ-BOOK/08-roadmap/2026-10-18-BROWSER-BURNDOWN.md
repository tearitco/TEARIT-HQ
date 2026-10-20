# 2026-10-18 — Browser lane: re-baseline + burndown

**Why this file exists.** A burndown drawn from the KPI plan's §4 sprint
ladder would be fiction. That ladder was written 2026-10-11 and the
`origin/claude` line landed G3.1/G3.2/G3.3/G3.5/G3.6 on 10-12/13; the ladder
was never updated, so it lists five shipped features as future work. This
document re-derives **done vs open** from evidence (a wired suite + a plan §9
measurement, not from the ladder's prose), then sizes what is actually left.

Source of truth for the numbers below: `design-docs/2026-10-11-BROWSER-GAP-
ASSESSMENT-AND-KPI-PLAN.md` §3 (definitions) and §9 (measured record); the
suites in `44.xyz.01.00/&.hq-apps/network/tests/`; and `git log` on those
suites. Where a claim is not backed by one of those, it says so.

---

## 1. The re-baseline: 18 KPIs, 13 done, 5 open

| KPI | state | evidence |
|---|---|---|
| G1.1 live timer tick | ✅ | `nb_timer_test.sh` + `nb_tick_live_test.sh`; plan §9.1 (20 distinct updates in 10.1 s) |
| G1.2 rAF loop | ✅ | `nb_raf_test.sh` |
| G1.3 push protocol | ✅ | `nb_push_quiet_test.sh`, **both** arms in one run (§9.1) |
| G1.4 focus safety | ✅ | `nb_focus_safety_test.sh` |
| G1.5 SPA click-through | ✅ | `nb_spa_click_test.sh` |
| G1.6 canvas liveness | ✅ | `nb_canvas_test.sh` |
| G2.1 box tree | ✅ | `nb_geometry_test.sh` |
| G2.2 gBCR honesty | ✅ **100 %** | `nb_gbcr_test.sh` — 9/9 within ±2 px (§9.12) |
| G2.2b conditional gutter | ✅ | `nb_gutter_test.sh`, negative-tested both directions |
| **G2.3 flex row** | ❌ | `nb_flex_test.sh` written, RED by design (handoff §5a) |
| **G2.4 grid** | ❌ | no suite; roadmap rule: grid only after flex |
| **G2.5 `@media`** | ❌ | no suite; OPEN-ITEMS item 11: "`@media` never fires (no viewport)" |
| **G2.6 cascade completeness** | ❌ | no suite; needs the property list defined by G2.1–G2.5 |
| G3.1 find in page | ✅ | `nb_find_test.sh` (`cdc514b98`, 2026-10-13) |
| G3.2 zoom | ✅ | `nb_zoom_test.sh` (`b03bb8e7f`, 2026-10-13, per-window text zoom) |
| G3.3 downloads | ✅ | `nb_download_test.sh` (`1754df49e`, 2026-10-13) |
| **G3.4 print** | ❌ | no suite and no feature — `grep nb_print` finds nothing |
| G3.5 multi-row selection | ✅ | `nb_selrun_test.sh` (`7ee6ee3c7`, 2026-10-13) |
| G3.6 autofill | ✅ | `nb_autofill_test.sh` (`71cd4e89b`, 2026-10-12) |

### What §4's ladder got wrong

| ladder said | reality |
|---|---|
| S7 find-in-page — "best first pick", not started | **shipped** 2026-10-13 |
| S9 zoom — "not started"; §8.5 says **rejected** | **shipped** `b03bb8e7f` the same day, as the per-window knob §8.5 recommended |
| S10 downloads — not started | **shipped** 2026-10-13 |
| S11 autofill — "cheapest G3 item, 0.5 sprint" | **shipped** 2026-10-12 |
| S8 multi-row selection | **shipped** 2026-10-13 |

§8.5's "rejected" wording is the trap: the *desktop-scale* approach was
rejected, and that record stands, but a different implementation landed
hours later. Anyone reading §8.5 alone would re-attempt or re-reject a
solved problem. Worth a pointer in §8.5 itself (done in §5 below).

---

## 2. Burndown — KPIs remaining

Counted on the 18 G-KPIs. The un-KPI'd items in §3 are **not** on this
line; they are real work that no KPI currently scores, which is itself a
finding.

```
18 |*                                    <- today: 5 open (G2.3 .4 .5 .6, G3.4)
   | *                                   sprint A: flex + test baseline
15 |
   |  *
   |   *
12 |    *                                sprint C: grid
   |     *
 9 |
   |      *
   |       *
 5 |        *   (M3 reflow is multi-sprint and runs alongside)
   |         *
 0 +---A----B----C----D----E---------->
       flex  media grid print  M3
```

| sprint | closes | KPI count after |
|---|---|---|
| **A (next)** | G2.3 flex + make the test baseline trustworthy (§3) | 6 |
| B | G2.5 `@media` + G2.6 cascade completeness (one sprint, independent of flex) | 4 |
| C | G2.4 grid | 3 |
| D | G3.4 print | 2 |
| E+ | Milestone 3 JS↔box-tree reflow (cross-lane, multi-sprint) | — |

**Sprint A is the recommendation**: it is the ladder's own NEXT, its
contract is already pinned by a red test, and it is the gate the roadmap set
for all remaining layout work ("grid only after flex").

---

## 3. The pre-sprint findings — these block a trustworthy burndown

Three things surfaced while building this baseline. All three are about the
**measuring equipment**, which is the failure mode this lane has hit more
than any other.

### 3.1 K3 is unwired *by design* — and so the milestone bar has no gate

The plan's rule §5.6: *"Milestone claims require K2/K3 evidence, not just
hermetic PASS."* K3 is `nb_interact_test.sh` — the only suite that drives the
**live** window through the relay (typing, submit, nav-jump clicks,
copy/paste).

It is **not** in `nb_all_tests.sh`, and correctly so: §3.1 records it as
"deliberately outside the hermetic runner". It is not hermetic — it needs a
live window *and* the network (it submits a form to Wikipedia), so putting it
in the pass/fail bar would make K1 red for environmental reasons rather than
code reasons. **Do not wire it into K1.**

The real gap is the other end: the rule says "run it manually before
milestone claims", and nothing enforces that. The fix is a **milestone gate** —
`tests/nb_milestone_gate.sh`, now written — a named command whose output is
pasted into the milestone note, not a new line in the always-green runner.

**Measured the moment the gate existed:** K3 is **red today**.

```
== milestone gate ==
== 1. type into search field, submit, land on Wikipedia
FAIL: field commit missing
== 2. clipboard round trip (copy row, paste into field)
SKIP: bookmark row not visible
K3 FAIL (rc=1)
```

So the bar the plan set for milestone claims is **not met right now**, and no
amount of green hermetic suites would have said so. Given plan §9.5 (typing
does nothing until the `cli_io` is armed, and arming needs a real click), the
first thing to check is whether K3 can arm its field at all — that is a test
mechanism question before it is a product question. Recorded, not yet
diagnosed.

**Diagnosed 2026-10-19 — both bugs were in the test, not the product.**
Two independent defects, each sufficient on its own to produce
`FAIL: field commit missing` no matter what the renderer did:

1. **`digits()` sent raw characters instead of ASCII values.** Relay codes
   are ASCII *values*; codes below 32 other than the named ones are no-ops
   in `dispatch_relay_code()`. So `printf '%s\n' "2"` delivered code `2`,
   not the digit — the nav jump never landed, focus stayed on the already
   focused "Go", Enter dispatched the submit instead of arming the field,
   and the form submitted empty. The landmine is documented verbatim in
   `nb_selrun_test.sh`'s header, and `nb_selrun`'s own `key()`/`ascii()`
   idiom is the correct form. Fix: `printf '%d\n' "'$d"`.

2. **The commit check could never match a correct commit.** The test grepped
   `grep -q "^search\tBlockly$"` against `fields.txt`. Inside double quotes
   bash keeps the backslash and a grep BRE matches a literal
   backslash-t — there is no tab in the pattern. Measured: the file held
   `search<TAB>Blockly` and the grep still failed; with a real tab
   (`TAB="$(printf '\t')"`) it matches.

Evidence the product was never at fault: with only those two test lines
changed, `kh_focus_debug.log` shows `GRAB key=ci3 rc=0`, the seven
`KEYPRESS key=ci3` lines for `B l o c k l y`, then Enter and Escape — and
`network_browser_fields.txt` holds `search\tBlockly` exactly. Run
`sh nb_milestone_gate.sh` twice, back to back:

```
PASS: field commit
PASS: submit navigates
K3 PASS
```

Step 2 still SKIPs on `bookmark row not visible` — a skip, not a failure.
**The milestone bar's K3 requirement is now met by the gate's own output.**

Two operational traps cost real time in this diagnosis and are worth
recording: (a) `nb_interact_test.sh` restarts the window via `button.sh`
when `BROWSER_PID` is unset, so a manual experiment driven at a stale pid
talks to a dead relay and produces *no* log lines at all — read artifacts
by identity (`browser_pid.sh`), never by an old pid; (b) a renderer **or
worker** rebuild is not picked up by a resident process — the worker at
03:37:21 kept the pre-rebuild binary until the window was restarted at
03:55, which made a correct `nb_css.c` change look like a no-op.

### 3.2 The runner is RED right now — measured, not inferred

**`sh nb_all_tests.sh` at `f6bd309aa` returned `SOME TESTS FAILED`, exit 1.**
30 suites ran; 3 failed:

```
FAIL  span mouse drag-select   (T2: no highlight band)
FAIL  find in page             (no wrap at=0)
FAIL  row-run selection + copy (no [SEL] line; band not on every selected row)
```

Every other suite passed, including G2.1, G2.2 (100 %), the gutter control,
all six G1, zoom, downloads, autofill and reach. So the KPI statuses in §1
stand — but **standing KPI K1 ("suite green") is FAILING today**, and the
three red suites are exactly the flaky ones below. That is the honest
baseline, and it is the reason hygiene comes before feature work in sprint A.

> **2026-10-19 update:** `find` was fixed (§7.4) and `nb_selrun` is now
> fixed too (`24c8ac325`, §7.4 boxed note) — **10/10 twice**, from a
> 4–5-of-10 baseline. The row-run row in the table below was therefore
> never a flake: the ~50 % failure rate was the anchor reset firing on
> unchanged-content reparses, which is timing-dependent and so *looked*
> like noise. K1's floor is now **the three mouse-span suites only**.

`nb_all_tests.sh` calls `nb_span_drag_test.sh` (T2, the mouse-motion path),
`nb_find_test.sh`, `nb_selrun_test.sh`, `nb_reach_test.sh` (this one passed).

**Rates, measured (6 runs each, in sequence):**

| suite | in-sequence | standalone | note |
|---|---|---|---|
| span drag-select | 1/6 | — | T2, no highlight band |
| find in page | **0/6** | ~3/5 | see below |
| row-run selection | 2/6 | — | selection does not extend |

**The 0/6 was not "find is broken" — it is the mirror.** A single standalone
run passed **13/13**. In a *failing* run the disagreement is precise:

```
PASS: Ctrl+F arms find
FAIL: query/matches wrong: line='[FIND] query="the" 0 of 0 -- ...'
FAIL: count mismatch: mirror says 0, frame says 2
PASS: Enter advances to match 2        <- find itself works
PASS: band is painted and tracks the match line
```

The renderer has **2 matches** — the band is painted, Enter advances — while
the mirrored `[FIND]` line reports **0 of 0**. And both are written into the
*same* dump (`open_memstream` at `khtpm_core_render.c:11839`, the `[FIND]`
line at `:11896`), which is the part worth keeping: **the mirrored hit-count
is computed against a row set that momentarily differs from the rows the same
dump renders.** It self-corrects on a later frame, so the suite's verdict
depends on *when* it reads — which is why the same suite is green standalone
and red in sequence.

That reframes the whole cluster. It is not "flaky tests" and it is not only
"the live clipboard": the **mirrored state can disagree with the rendered
state in the same artifact**, and `[SEL]` shows the same shape (`[SEL] rows
2..3` when the run should be longer). Any consumer of the mirror — agent or
suite — inherits that gap. This is the thing to fix, and it is a renderer
finding, not a test finding.

Consequence: **K1 "hermetic suite green" is not deterministic.** The runner's
final line can be `SOME TESTS FAILED` for reasons unrelated to any code
change, which means a red runner cannot be trusted as signal and a green one
cannot be trusted as proof. Either they get quarantined into an explicitly
"flaky, informational" section or their clipboard reads get a receipt check
(the fix `nb_span_select_test.sh` already has, 45 % → 85 %). **Do not**
apply that fix blindly — it was tried on `find`/`selrun` and measured
**worse**, so it needs ≥10 runs per suite before believing either direction.

### 3.3 The "PASS=30" mislabel — pin it so it does not recur

`grep -E '^PASS=|^FAIL='` on a runner log returns `PASS=30 FAIL=0`, which was
reported this session as "hermetic suite PASS=30 FAIL=0". That line is a
**sub-suite's fixture count** (a snapshot diff over 30 fixtures), not a count
of passing suites. `nb_all_tests.sh` prints exactly one verdict:
`ALL TESTS PASS` or `SOME TESTS FAILED`. **Read that line, not a grep.**
Same family as §9.13: a green number that was not measuring what it claimed.

---

## 4. What is genuinely open, in one list

**Scored KPIs (5):** G2.3 flex · G2.4 grid · G2.5 `@media` · G2.6 cascade
completeness · G3.4 print.

**Real work no KPI scores:** the roadmap's *Milestone 3 — JS↔box-tree reflow*
(the manager owns the box tree; `textContent`/`appendChild` reflow the
subtree; `el.style` writes reflow). This is the item that makes the whole box
tree worth having, it is cross-lane, and it is multi-sprint. It is also the
hardest thing on this page and the most likely to deserve a KPI of its own.

**Small, unowned:** `file://` worker image resolution (roadmap Milestone 2
remaining); the "1 element still drops" fixed chrome cost (§ roadmap,
Milestone 12); `nb_bar_test.sh`/`nb_reach_test.sh` reliability.

---

## 5. Do this before the sprint claims a milestone

1. ~~Fix §8.5's "rejected" wording~~ — **done in this block**: §8.5 now opens
   with a RESOLVED box pointing at `b03bb8e7f`.
2. Add the **milestone gate** (run `nb_interact_test.sh`, paste its output into
   the milestone note). Keep it out of K1.
3. Fix the **mirror/rendered disagreement** (§3.2) — the three red suites are
   a symptom, not the bug. Determine why the mirrored `[FIND]`/`[SEL]` state
   can disagree with the rows in the same dump, and fix *that*. Do not
   quarantine the suites to make K1 green: that would hide a renderer bug
   every mirror consumer inherits.
4. Then start sprint A: `nb_flex_test.sh` is already red — retain the display
   keyword in `nb_css.c`, worker flex model, then the renderer-half design
   decision (§6), keeping `nb_gbcr_test.sh` at 100 % after each step.

---

## 6. Sprint A reconnaissance — what flex actually needs (read before coding)

Read the code rather than the ladder's "wiring, not invention". **The ladder
is wrong on both halves.** Four findings, each a file+line.

**6.1 The very first change is in `nb_css.c`, not the layout code.** The CSS
layer does *not* keep the display keyword:

```c
/* nb_css.h:48 */  char display[16];  /* "none" if display:none, else "" */
/* nb_css.c:311-319 */ if (strcasecmp(cv,"none")==0) ... else out->display[0]=0;
```

So `display:flex` and `display:block` are **the same value today**. The
handoff's "`nb_css` already parses `display`" is half true — it parses it
only to detect `none`. Step 0 is to retain the keyword.

**6.2 The worker has no x-axis at all.** `layout_xy()` ends `*out_x = 0`
(`ops/nb_js_worker.c:1251`) — unconditionally. That is *why* a flex div
reports `x=[0,0,0]`: not a missing flex branch, a missing horizontal axis.
Flex is the first x-accumulation the model will ever have.

**6.3 Three worker changes, all small.** `nb_box_w()` fills the parent's
width for block children (`:1176-1180`) → a flex child needs a *share*;
`layout_xy()` accumulates sibling heights → a flex parent must accumulate
*widths* and hold `y`; `nb_content_h()` sums children (`:1193`) → a flex
container's height is the *max*, not the sum. The red test's four clauses map
onto exactly these.

**6.4 The renderer's row engine is CHROME-scoped, so the manager half is not
wiring.** `<row>` is only laid out when it carries a chrome class —
`class="toolbar"` (`khtpm_core_render.c:6189, 6224`), `tabbar`, or
`pal-grid-row` — via the shared `css_layout_pass()` in
`khtpm_render_core.c`. `network_browser_manager.c` has **no** flex handling
whatsoever (`grep flex` → nothing). And the content pane is a fixed per-kind
`<repeat>` in `network-browser-hq.xhtpm` (§8.1: 24 gated siblings per row).

So a flex content container has **no path** to the row engine as things
stand. The manager half is one of (a) a new content row *kind* in the
template that is a `<row>`, or (b) generalizing the layout pass past the
chrome classes. Both are design decisions, and (b) touches every window.

**Consequence for sizing:** the plan's S4 "1 sprint, wiring not invention" is
optimistic. The worker half (6.1–6.3) is a day. The renderer half (6.4) is a
design decision that deserves its own note before code — which is exactly the
kind of call the plan's §5.5 reserves for the owner. **Do not ship the worker
half alone** (it would re-open G2.2 with a confident wrong number).

> **SUPERSEDED 2026-10-19 — the owner chose (b), and reading the real code
> changed the answer.** The design note is
> `design-docs/2026-10-19-G2.3-FLEX-6.4-DESIGN.md`. Headline: **neither (a)
> nor (b) is what is missing.** `css_layout_pass()` **already is** a complete
> general flexbox engine (display:flex, flex-direction, flex-grow,
> flex-wrap, padding/gap, absolute out-of-flow), the CSS layer already parses
> and cascades all of it (`khtpm_css_parser.h:42-45`), and it is already
> reachable from CSS — `<page style="display:flex; flex-direction:row">`
> routes sidebar+panels through it, generalized live since 2026-09-11. The
> browser does not get flex because **its `<page>` never asks for it**
> (verified: 0 occurrences of `flex` in the 182-line template), not because
> of a chrome-class gate. So "touches every window" was too pessimistic: the
> generalization already happened, and its only consumer is this browser.
>
> **What is actually missing is the *worker* half** — `nb_flex_test.sh`
> reads `getBoundingClientRect()` out of a worker `eval`, so it measures the
> worker's box tree, and the two halves must land so worker and renderer
> agree or `nb_gbcr` breaks. That sequencing is the real risk, not the layout
> engine. See the note's §2 for the per-step plan and §3 for the gate
> (`CENTROID_GOLD_STD` rule 7: no per-app branch in the shared engine).


**6.5 Progress 2026-10-19 — step 6.1 landed, nothing else.** `nb_css.c` now
retains the display keyword instead of clearing it to `""`
(`commit e19a0f0bd`, branch `opencode`). Measured live after a fresh
`sh build.sh` **and a window restart** — see the trap in §3.1, a resident
`nb_js_worker` keeps the pre-rebuild binary, so a rebuild alone looks like
a no-op:

```
getComputedStyle().display -> f=flex g=block h=EMPTY
```

`h=EMPTY` is the guard that matters: an element with no `display`
declaration still computes to `""`, so pages that never set `display` are
byte-for-byte unaffected. `nb_gbcr_test.sh` PASS (9/9 within ±2 px, 100 %),
`nb_flex_test.sh` still FAILs with the children stacked
(`y=[30,60,90]`, `x=[0,0,0]`) — correct, because 6.1 changes no geometry;
it only makes the request *visible* to the code that will consume it.

**Still not done, in order:** 6.2 (`layout_xy()` accumulates sibling
*widths* for a flex parent), 6.3 (`nb_box_w()` gives a flex child a
*share*), and 6.4 — the design decision that gates shipping either half.
`nb_flex_test.sh` is deliberately **not** wired into `nb_all_tests.sh`
until G2.3 is complete, so K1's verdict line does not change from this
work.

**6.6 Progress 2026-10-20 — steps 6.2 and 6.3 landed; G2.3's worker half is
green.** Branch `opencode`, commit `543de563c`. The worker now has a
horizontal axis and flex semantics:

- `nb_css.{h,c}` — the style model gained `has_display_flex`/`display_flex`,
  `has_flex_direction`/`flex_row`, `has_flex_grow`/`flex_grow`, plus
  `flex-direction`/`flex-grow` in `prop_id` and the cascade. This mirrors the
  renderer's shared parser (`khtpm_css_parser.h:42-45`) field-for-field, so
  the two halves agree by construction.
- `ops/nb_js_worker.c` — `layout_xy()` accumulates *widths* for a flex-row
  parent and holds `y` (instead of always stacking and ending `*out_x = 0`);
  `nb_box_w()` gives a flex-row block child a *share* of the row (equal, or
  weighted by `flex-grow`); `nb_content_h()` takes the **max** of a flex
  row's children, not their sum. Every branch is gated on the parent being a
  flex row, so a non-flex page is byte-for-byte unchanged.

Measured, on binary md5 `57d43be35f968d04b1b7aab301fb96f2` (fresh
`sh build.sh` **and a window restart** — the §3.1 trap again):

```
nb_flex_test.sh PASS, twice, identical numbers:
  c1 x=0   y=30 w=130
  c2 x=130 y=30 w=130
  c3 x=259 y=30 w=130
one row (shared y=30); edges share within 1px (gaps [0,1]);
x advances; widths fit.
nb_gbcr_test.sh PASS — 9/9 rows within ±2px, 100.0% (K2 holds).
```

**Two bugs were found and fixed while proving it** — both worth recording
because neither was visible by reading:

1. **The first cut segfaulted the worker.** `layout_xy()` dereferenced
   `n->parent` in the new flex check with no NULL guard; the document root
   has no parent. `worker_css_test.c` crashed with `signal 11`, and ASAN
   named the line (`layout_xy:1343`). This is the §3.1 class of error in
   reverse: a *rebuild* is required to see it, and the resident-worker trap
   hid it from the live test until a restart.
2. **`flex-direction` defaults to ROW, not column.** The first cut required
   an explicit `flex-direction:row`, so a bare `display:flex` kept stacking —
   which is *exactly* the page `nb_flex_test.sh` uses. Found by probing
   `nb_css_resolve` in isolation (`fx` resolved `display_flex=1` but
   `has_flex_direction=0`), not by guessing. The renderer's own parser has
   the same reversal latent (`has_flex_direction ? flex_row : 0`), which is a
   separate finding for 6.4's wiring step.

`worker_css_test.c` runs 10/10 and is identical to HEAD; its one remaining
failure (`wcs[px-size]`, `B6b stack=60`) **reproduces on the unmodified HEAD
worker**, so it is pre-existing and not from this change.

**Still not done:** 6.4 — but the design note
(`design-docs/2026-10-19-G2.3-FLEX-6.4-DESIGN.md`) found the renderer's flex
engine *already exists* (`css_layout_pass()`) and is already reachable from
CSS; the browser's `<page>` simply never declares `display:flex`. So 6.4 is
wiring, not invention — the renderer half is authoring flex into the
template, not a new layout branch.

---

## 7. Sprint A, first bite — the mirror/rendered disagreement: root cause, fix, evidence

*Worked 2026-10-18 → 2026-10-19. Branch `opencode`, file
`44.xyz.01.00/_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` only
(52 insertions / 1 deletion, verified `git diff --stat`), rebuilt with
`build_core_render.sh` and re-tested against a restarted window each time.*

### 7.1 It was not a race, and it was not two different row sets

§3.2 read the symptom as *"computed against a row set that momentarily
differs from the rows the same dump renders"*. **That reading is wrong, and
the wrongness was load-bearing** — a race implies you can't fix it
deterministically, so this one took the long way round to disprove.

`dump_frame_png()` (`:12617`) calls `redraw()` **first**, `redraw()` writes
the ASCII mirror at the end of its own body (`:12576` →
`kh_write_ascii_frame()`), and only then is `g_window` serialized to the
frame. Same tick, same thread, same tree. A disagreement between `[FIND]`
and the frame therefore **cannot** be two row sets: it can only be a
**stale cached count**.

And that is exactly what it is:

```
khtpm_core_render.c:3364-3370  reparse_chtpm_if_changed()
    /* ... The QUERY text is deliberately NOT cleared here: the user typed
     * it, and a content change should re-search rather than silently
     * forget the ask. */
    g_find_hits = 0;            <- :3370, what shipped
    g_find_at   = 0;
```

The comment states the intent — **re-search** — and only the *drop*
shipped. `kh_find_commit()` (`:10488`) had **four** call sites before this
fix — the two keystroke sites (`:12970` backspace, `:12975` append) and two
inside `kh_find_step` (`:10535`, `:10544`) — **every one of them keyed on
user input**; nothing re-ran it after a rebuild. So on every full `.chtpm` rebuild the query
survived, the hit list (which holds **pointers** into the tree being
discarded) was zeroed, and the window stayed "query=`the` **0 of 0**" until
the next Enter forced a commit. The band still painted, Enter still
advanced, the frame still showed the rows — the *count* was the only stale
thing, which is the precise §3.2 signature.

`incremental_reparse=0` live in `hq_ui.pdl`, so the full-rebuild path is
the one that runs.

### 7.2 The fix — finish the sentence the comment already wrote

Three parts, all at the reparse sites rather than in the mirror (fixing the
mirror would make the *reporter* agree with a wrong value):

1. **Re-search at the end of both reparse paths** — `:3256` (incremental,
   unexecuted, labelled as reasoned-not-observed in its own comment) and
   `:3560` (full rebuild), each guarded on `g_find_query[0]`:
   `if (g_find_query[0]) kh_find_commit();`
2. **Do not zero `g_find_at`** in the reset (`:3370-3371`). The query
   survives a content change; the ask includes *which match the user was
   on*. Zeroing it dropped the cursor back to match 1, which is what made
   one Enter read as "did not advance".
3. **Order matters: it must run before the caller's layout.** The reparse
   callers do `assign_nav_and_layout(); g_layout_fresh = 1; redraw();`, and
   `redraw()` **skips** its own layout when `g_layout_fresh` is set
   (`:12066`). A re-search left to run at the top of `redraw()` therefore
   set `g_find_reveal` after the last layout of the tick, and the reveal was
   never consumed — while the same reparse had just put a
   `<scrolllist class="from-top">` back to scroll 0. Observed directly:
   `y=434` right after a dump, `y=-100000` two seconds later.

That third point is the reason the fix moved during the session. The first
working version (re-search at the top of `redraw()`) fixed the **count**
(8/8 runs) but *still* failed the reveal 2/8 — because ordering, not
presence, was the problem. Measured before/after on the same 4-attempt
manual procedure:

| | reveal held? |
|---|---|
| first version (hook in `redraw()`) | 2/4 then reverted to `y=-100000` |
| final version (hook at end of reparse) | **5/5 hold at `y=434`**, stable 3 s later |

### 7.3 Evidence

**Deterministic before/after** — same procedure, arm find on one article,
then replace the content with find still armed (no new keystroke):

| | mirrored `[FIND]` | rows in the *same* frame |
|---|---|---|
| before | `query="the" 0 of 0` | 4 visible / 31 total carry "the" |
| after | `query="the" 1 of 31` | **31** total (independent recount) |

The 31 is an independent `awk` over all serialized rows in that same dump —
mirror and frame now agree on the number *and* it is the right number.

**`nb_find_test.sh`, 8 consecutive runs after the final build:**

```
run 1..8: PASS=13 FAIL=0
==== nb_find: total FAIL across 8 runs: 0 ====
```

(baseline §3.2: **0/6** in-sequence, ~3/5 standalone.)

**No regression in the KPI suite:**

```
nb_gbcr_test.sh: 6 runs, total FAIL = 0   (G2.2 must stay 100 %)
```

**Negative test.** The suite carries its own: clause 6 types
`zzzznotpresent` and *requires* `0 of 0`, so a fix that merely inflated
counts would fail it — it still passes. The pre-fix red runs above are the
other half: the same clauses failed 3 of 8 with the exact same pair
(`Enter did not advance (at=1)` / `no wrap (at=2)`), and 0 of 8 after.

### 7.4 The runner verdict on the final build — and what is still red

**`sh nb_all_tests.sh` → `SOME TESTS FAILED`, exit 1: 30 suites, 3 FAIL
lines.** All three FAIL lines are in the *mouse-span* family. The two
suites §3.2 listed red for this report are green:

| suite | baseline §3.2 (2026-10-18) | in the runner, final build | standalone ≥6 runs |
|---|---|---|---|
| `nb_find_test.sh` | **FAIL** | **PASS** ✅ | **8/8 pass** × 13 asserts, 0 FAIL |
| row-run selection (`nb_selrun`) | **FAIL** | **PASS** ✅ | **10/10 pass**, twice (was 2/6) |
| `nb_gbcr` (K2, KPI) | PASS | PASS | **6/6, 0 FAIL** |
| span mouse drag-select (T2) | FAIL | FAIL | 3/6 pass |
| span range selection (S1/S2) | PASS | **FAIL** | 5/6 pass → **flaky, not a regression** |

So K1 is *closer*, not green: **the floor is now the mouse-span suites**,
not find. The runner runs each suite **once**, so a 1-in-6 flake
(`span range selection`) is enough to turn the whole verdict red — that is
exactly what happened here (it failed S1+S2 in the runner, then 1 of 6
standalone).

**`nb_selrun` is fixed** (`24c8ac325`) — see §7.4's boxed note. It is now
**10/10 twice** against a 4–5-of-10 baseline, and the ~50% flake rate was
never a flake: it was the anchor reset firing on unchanged-content
reparses, which is timing-dependent, which is what a "flake" looks like
from outside. With that gone, the row-run table's 2/6 was a *rate of a
real bug*, not noise — worth remembering the next time a suite is called
flaky.

**Both remaining reds are independent of this fix.** Proven, not asserted:
`grep -c 'Ctrl+F\|find in page'` → **0** in `nb_span_select_test.sh` *and*
in `nb_span_drag_test.sh` (and 0 in `nb_selrun_test.sh`), so all three run
their own hermetic fixture renderer with `g_find_query` empty — the new
re-search is behind `if (g_find_query[0])` and never executes there.

**`nb_selrun`** — failures are two shapes, `no [SEL] line` and a
`[SEL] rows 2..3` where `1..3`/`2..4` was expected. `[SEL]` is computed
**live** from `g_sel_run_anchor`/`g_focus_nav` at frame time (`:11956`), not
cached, so the value is a fact about state, not a stale buffer. `2..3`
instead of `1..3` says the anchor was set while focus was already **2**,
i.e. the *first* of two `223` codes did not extend. Candidates, none yet
measured: `g_focus_nav`/`g_n_nav` invalid at dispatch (`kh_row_run_extend`
returns 0 without moving — `:8613`), or a focus change landing between
`home()`'s mirror read and the first code. `dispatch_relay_code` itself
sets `g_key_shift = 1` *before* `handle_key` and clears it after
(`:13772`), so the shift plumbing on that path is not obviously wrong.
**Mechanism UNDIAGNOSED — do not treat the candidates as findings.**

> **CLOSED 2026-10-19 (`24c8ac325`). The candidates above were all
> wrong.** The mechanism is the anchor reset in
> `reparse_chtpm_if_changed()`: `g_sel_run_anchor` was zeroed on **every**
> content rebuild, including one whose content did not change. A
> spontaneous reparse landing between two Shift+Downs destroyed the
> anchor the first press had just set, so the second press re-anchored at
> the current focus — `2..3`, exactly the observed shape — and a reparse
> landing just before the mirror read lost the run outright (`no [SEL]
> line`). The ANCHOR receipts added in `f37d4c919` show it on every
> failing run:
>
> ```
> ANCHOR set anchor=1 focus=1
> ANCHOR cleared by content rebuild     <- 0.5-1.0s later, no key
> ANCHOR set anchor=2 focus=2           <- re-anchor, too late
> ```
>
> **All four observed failure shapes were that one mechanism**, so they
> were never four bugs. The fix mirrors the 2026-09-14 decision already
> made in the same function for in-field selections: capture a digest of
> the page before the tree is discarded, and keep the anchor only if the
> newly installed tree digests identically.
>
> **Evidence** (binary md5 `5577f762`, strings-checked):
> 10/10 clean `nb_selrun_test.sh` runs, twice, against a 4–5-of-10 dirty
> baseline; `ANCHOR kept` fired 9 and 17 times. Negative test (same ids,
> same row count, every label rewritten): `ANCHOR content changed (digest
> 8fe0df4c… -> a9e91cbf…)`, the run collapses, and the next Shift+Down
> re-anchors at the live row (`3..4`), not a stale `1..3`.
>
> **Trap for the next reader:** the first attempt matched rows by element
> id through `kh_nav_elem()` and restored 0 of 23 rebuilds — **nav does
> not exist mid-reparse** (`g_n_nav` is 0 until the caller's
> `assign_nav_and_layout()` runs, so every lookup returned NULL). The
> fix compares content because content is what exists there. Any future
> restore inside a reparse must respect that ordering.
>
> **A second trap, mine, which cost a whole battery:** an earlier
> "the fix does not work" measurement (4/10) was actually the
> **pre-change binary** — I had built HEAD into the same path to compare
> suites and copied the wrong file back. `strings "$BIN" | grep 'ANCHOR
> kept'` is what caught it. Verify the artifact by content (a hash or a
> string it must contain), never by the copy you believe you made.


**`nb_span_drag` / `nb_span_select` share one shape**: the range never
lands, and the copy falls back to the *whole* row text —
`clipboard was [go to ONE then TWO then THREE now]` with
`no highlight band (0)`. Two corrections to an earlier draft of this
section, both from re-measurement:

1. `click.log` is **not** always absent. This batch: T1 passed 3/6, and
   two *failed* runs printed `FAIL: T6: late release did nothing (log:
   CLICKED LINK1 …)` — the deferred click **does** complete, sometimes.
2. **A stale-fixture-window theory was checked and refuted**: no `764x264`
   window exists on screen, and the only two long-lived renderers are
   legitimate house windows (`khtpm_strip_header.xhtpm`,
   `hotbar-desk.xhtpm`). Do not assume it is this.

Consequence for K1: **the runner is not green yet.** The honest floor for
the next session is the table above; re-measure ≥6 runs after any change,
and remember a single runner pass proves a *rate*, not a fix. With
`nb_selrun` fixed, the floor is **exactly the three mouse-span suites**
(`nb_span_test`, `nb_span_select`, `nb_span_drag`) — every one of them
already proven pre-existing by running them against the pre-change
binary.

### 7.5 Pitfalls reported

1. **My own probe lied before the product did.** The first attempt at a
   deterministic repro got **6/6** "arm → dump → type → `0 of 0`". Its
   `dump()` watched a receipt path that is never written, so every dump
   silently slept 10 s instead of 1 s, and the entire result was an
   artifact of my harness — repeating with the correct receipt path gave
   `1 of 2` immediately. Exactly §"never report a number you did not
   observe": **negative-test your own instrumentation before drawing a
   conclusion from it.**
2. **Idle time was a third suspect and a dead end** — 1 s / 4 s / 12 s gaps
   between arming and typing all gave the correct count (2× each). Ruling
   this out is what forced the switch from *when* to *what*.
3. **`pgrep -f 'network_browser_manager'` matched my own shell** and
   printed the shell's argv as the manager's — the `browser_pid.sh`
   comment (`nb_find_test.sh:44-49`) warns about exactly this.
4. **A timed-out test command is a truncated run, not a verdict.** The
   first full-runner attempt was killed at 120 s and left a partial log
   with no verdict line; only re-running it in the background produced a
   real one.
5. **I measured the wrong binary and believed the number**
   (2026-10-19, and it cost a full battery). To prove the span failures
   were pre-existing I built HEAD's source into the live path, then
   "restored" by copying back a file that had itself already been
   overwritten — so a battery I read as "4/10, the fix does not work"
   was in fact the *pre-change* binary, and a battery I read as "10/10"
   had run the fix. Two opposite readings of "the same binary" that were
   never the same binary. What caught it: `strings "$BIN" | grep 'ANCHOR
   kept'` → 0, and `md5sum` showing the installed file byte-identical to
   the HEAD build. **Verify the artifact by its content, never by the
   copy you meant to make** — this is §"never report a number you did
   not observe" wearing a new hat, and the reason the fix's evidence
   above names its binary hash.
6. **Nav does not exist inside a reparse**, and the code will not tell
   you. The first fix attempt restored `g_sel_run_anchor` by looking up
   element ids with `kh_nav_elem()` and restored **0 of 23** rebuilds.
   No error, no warning — `kh_nav_elem()` simply returns NULL, because
   `g_n_nav` is 0 until the caller's `assign_nav_and_layout()` runs
   *after* `reparse_chtpm_if_changed()` returns. One temporary receipt
   (`na=NULL nf=3 nfelem=NULL`) answered it in a single run. Any state
   about *rows* must be decided on **content** at reparse time, or
   deferred past the layout call — never on nav indices.

*Companions: `2026-10-18-BROWSER-LANE-HANDOFF.md` (START HERE), `2026-10-17-G2-
VERDICT-ADDENDUM.md`, `design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-
PLAN.md`.*
