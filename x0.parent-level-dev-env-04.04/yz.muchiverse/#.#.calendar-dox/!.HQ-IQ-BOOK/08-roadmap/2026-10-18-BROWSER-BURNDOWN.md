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
a named command whose output is pasted into the milestone note — not a new
line in the always-green runner. That gate should exist before the next
milestone claim (sprint A ends with one).

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

*Companions: `2026-10-18-BROWSER-LANE-HANDOFF.md` (START HERE), `2026-10-17-G2-
VERDICT-ADDENDUM.md`, `design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-
PLAN.md`.*
