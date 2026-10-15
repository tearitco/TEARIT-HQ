# Sprint verdict — G1: JS apps actually run (2026-10-13 → 2026-10-14)

Two days, one lane, four G1 KPIs closed and verified live. Written up
because the *method* was the hard part, and because the next sprint will
otherwise repeat it.

Plan of record: `08-roadmap/design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md`
§1a (work order) and §9 (what landed, with numbers).

---

## 1. Shipped

| KPI | Result | Where |
|---|---|---|
| **G1.1** live timer tick | 20 distinct self-updates in 10.1 s, monotonic; live window `tick 3 → 12` unaided | `nb_timer_test.sh`, `nb_tick_live_test.sh` |
| **G1.3** push protocol | frames arrive with no request; **and** an idle page produces 0 | `nb_tick_live_test.sh`, `nb_push_quiet_test.sh` |
| **G1.4** focus safety | armed address bar survives 8 unsolicited reparses; cursor stays put | `nb_focus_safety_test.sh` |
| **G1.5** SPA click-through | a click re-renders, never reloads — pinned with a control | `nb_spa_click_test.sh` |

Four real defects found and fixed on the way, all now test-covered:

1. **A DOM the worker cannot load was dropped silently** (`64012e1ee`) —
   `nb_dom_load()` returns NULL for anything that is not `nb_serialize`
   output, and a missing file just fails `fopen`. The page rendered as its
   title and nothing else, `STATUS ok`. Now an `ERROR|` frame, which the
   manager already handled for worker diagnostics.
2. **The frame reader stole a byte, on both sides** (`785407213`) — read one
   byte unconditionally after a payload and discarded it unless it was a
   newline: ate the next frame's first length digit, and *blocked forever*
   against a peer that omits the newline. The comment described a pushback
   the code did not perform.
3. **The manager's worker probe fails silently** — it resolves the worker
   relative to the *house*, and `run_page_scripts()` returns without a word.
   A house without the app installed renders the static extraction and
   never ticks, with an empty log.
4. **The id field is not the id** — `nb_attr_get()` parses the raw attrs
   blob. A hand-written `N|p|t||6|tick 0|0|` loads fine, dumps fine, renders
   its `tick 0` row like a healthy page, and is invisible to
   `document.getElementById`. Fixtures are **generated** now.

Plus one enabling affordance (`e3c038348`): `FOCUS_CLI_IO`, which arms the
address bar without a click.

## 2. Commit shape

22 commits on the browser surface. Only **5** shipped product changes; the
rest were `wip:` snapshots, tooling, and documentation. That ratio is the
honest headline: **most of the sprint was diagnosis, not construction.**

## 3. The method lesson — five instrument failures

Every wrong turn this sprint was the **measuring device** producing a
confident false answer, not bad reasoning about the code:

| Trap | What it claimed | Reality |
|---|---|---|
| multi-line `fprintf` read with `grep` | "the DOM's text is destroyed" | `grep` shows only the **first physical line**; the row set was always full |
| debug prints to a `DEVNULL` stderr | "0 timer firings" | the probe discarded stderr; the loop was working all along |
| `nb_serialize` return printed as "bytes" | "7-byte fixture, generator broken" | it returns a **node count**; the file was 370 bytes and fine |
| `window.__loads` as a reload counter | "no reload" proven | every `LOAD` builds a fresh global — the counter **cannot** read 2 by construction |
| stale `origin/opencode` ref | "42 commits unpushed, work unsaved" | this repo's fetch refspec tracks only `claude`; **every push had landed** |

All five are written down (§9.4, §9.6) so the next session starts knowing
them. The house rule already said it — *never report a number you did not
observe* — and I broke it five times in two days, which is the strongest
argument for having written it into the plan doc rather than leaving it to
memory.

## 4. What worked

- **Bite-sized slices with a red test each.** Every KPI had a suite that was
  red before the feature and green after. Nothing was "done" on inspection.
- **Negative-testing every pass.** Each of the four suites has a control
  proving it can fail. Two of them caught real problems — the G1.4 control
  proved the *text* assertion was toothless (only the armed/cursor check
  had teeth), and the G1.5 control proved my own fixture was vacuous.
- **Hermetic by default.** `browser_pid.sh` for PID selection; the push,
  focus and click suites need no browser, no X, no other agent's window.
- **Checking the ground truth.** When a number looked wrong, ask the
  instrument's source: `/proc/<pid>/exe` for a binary, `git ls-remote` for
  a push, `/tmp/dom_tree` for a DOM.

## 5. What did not work

- **Reasoning where I should have measured.** The `textContent`-setter
  theory was argued instead of diffed; one experiment killed it. Two days
  of the sprint went into theories that a ten-line probe answered.
- **Building a tool before checking whether one existed.** `xsendevent.py`
  key support was built on the belief that typing was impossible; the relay
  already mapped codes 32–126 to characters. Correct code, wrong premise,
  and it had to be corrected in the record afterwards.
- **Pixel-hunting a value the code could be asked for.** Proposed deriving
  the address bar's coordinates from a screenshot capture when a relay verb
  could simply arm the field.

## 6. Next sprint

**Take G1.6 (canvas) before G1.2 (rAF).** Both need work, but G1.6 is
smaller, and it pushes *PNG frames* rather than text rows through the same
pipeline — which is exactly what breaks when a path is only ever tested
with one kind of payload. G1.2 is a genuine feature: `requestAnimationFrame`
needs a frame source, i.e. a second cadence beside the timer queue.

Then **G2-a** (harness) → **G2.1** (box tree), the other P0 blocker.

**Standing advice for the next lane, earned the hard way:** before
concluding *why* something fails, spend one cheap measurement proving it.
Every expensive hour this sprint bought a theory that measurement gave away
for free — and twice, measurement proved the theory wrong.