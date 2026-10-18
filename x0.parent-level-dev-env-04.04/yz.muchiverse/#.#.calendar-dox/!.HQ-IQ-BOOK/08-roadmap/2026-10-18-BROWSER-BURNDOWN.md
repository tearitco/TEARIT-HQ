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

### 3.1 K3 is not wired, so the milestone bar is currently unenforceable

The plan's rule §5.6: *"Milestone claims require K2/K3 evidence, not just
hermetic PASS."* K3 is `nb_interact_test.sh` — the only suite that drives the
**live** window through the relay (typing, submit, nav-jump clicks,
copy/paste). It is **not in `nb_all_tests.sh`**, and it is the only test that
can catch a focus/typing regression in the live window.

So today, every "milestone" claim rests on K2 (live suite), never K3. Wiring
it is cheap and should land **before** the next milestone claim, not after.
(It discovers nav numbers from frame dumps and reports SKIP rather than
FAIL when it cannot, so it is safe to wire.)

### 3.2 Four known-flaky suites are *inside* the runner

`nb_all_tests.sh` calls `nb_span_drag_test.sh` (T2, measured ~60 % failing on
the mouse-motion path), `nb_find_test.sh` (~33 % pass), `nb_selrun_test.sh`
(~50 %), `nb_reach_test.sh`. They read the live clipboard, which has no
read-your-writes guarantee (a copy read can return the previous owner's
bytes).

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

1. Fix §8.5's "rejected" wording with a pointer to `b03bb8e7f` so zoom is not
   re-litigated.
2. Wire `nb_interact_test.sh` (K3) into the runner, or document why not.
3. Decide the four flaky suites' fate (quarantine vs receipt-fix, ≥10 runs).
4. Then start sprint A: `nb_flex_test.sh` is already red —
   worker flex model → manager projection → `nb_gbcr_test.sh` still 100 %
   after each step.

*Companions: `2026-10-18-BROWSER-LANE-HANDOFF.md` (START HERE), `2026-10-17-G2-
VERDICT-ADDENDUM.md`, `design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-
PLAN.md`.*
