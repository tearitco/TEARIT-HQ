# NETWORK-BROWSER GAP ASSESSMENT + KPI PLAN (2026-10-11)

House copy. XO copy: `x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/xo/23.network-browser/` (kept in step).

Written after the 2026-10-11 relaunch-on-merged-binaries run, where the full suite
(14 hermetic suites, `tests/nb_all_tests.sh`) went ALL PASS against a live browser
for the first time — every span/table/hover/bar path proved *in-browser*, not just
in fixtures. This document exists so the next sprint starts from measured state
instead of from the question "is the browser finished?" (it is not; see §2).

Roadmap of record stays `08-roadmap/NETWORK-BROWSER-RENDER-ROADMAP-2026-10-05.md`.
This file is the **gap analysis + KPI layer** on top of it: for each thing that is
explicitly not done, the code-level root cause, the slice breakdown, the
dependency order, and the measurable definition of done.

---

## 1. Honest calibration — what "done" means here

Three different bars, all of which have been confused in conversation:

| Bar | Meaning | Status 2026-10-11 |
|---|---|---|
| **Roadmap definition of done** | "a human can read a real content site without leaving the window" (`NETWORK-BROWSER-RENDER-ROADMAP-2026-10-05.md` header) | **MET** |
| **Expanded scope commitment** (same file, "Maximal ambition mode") | "the full class of modern web pages, including Google-class JS apps (Gmail, GMaps, Photos-grade)" | **NOT MET** — and this is the gap §2 is about |
| **Chrome-replaceable** (was "Chrome-parity", a non-goal until 2026-10-20) | the owner can use it instead of Chrome for everything | **NOT MET, and now THE target** (owner directive 2026-10-20). Sized backlog and rungs R1-R4: `08-roadmap/2026-10-20-CHROME-REPLACEMENT-SIZING.md` |

Anyone reading this later should not take the first row as a claim about the
second. The browser is a readable-content browser with real inline interaction,
not an application runtime.

---

## 1a. PRIORITY (set 2026-10-13) — the two load-bearing blockers

Everything else in this document is polish next to these two. Decided this
date, at the user's direction: **G1 and G2 are P0, worked one sprint at a
time, one KPI at a time**, and nothing else starts until they move.

Why these two and not the rest:

- **G1 (JS apps do not run)** is not "missing a feature", it is *the*
  difference between a readable-content window and a browser. One
  `setInterval` clock stays frozen at tick 0. Every Google-class site is
  downstream of this one item.
- **G2 (no box tree)** means layout is wrong often enough that
  layout-dependent pages misbehave, and every flex/grid/positioning item
  in the backlog queues behind it.

Work order, one KPI per step, each step red-before-green:

| Step | KPI | Done when |
|---|---|---|
| G1-a | harness | a test that measures a live timer and is RED today |
| G1-b | G1.1 live timer tick | >= 6 self-updates in 10 s with no user input, tick monotonic, from fresh frame dumps |
| G1-c | G1.4 focus safety | an unsolicited push never steals cli_io/address-bar focus or the nav cursor mid-typing |
| G1-d | G1.5 SPA click-through pinned | click-driven re-render without a reload, pinned so it cannot regress |
| G1-e | G1.3 push protocol | frames arrive without a request; negative: an idle page produces ZERO unsolicited frames |
| G1-f | G1.2 rAF / G1.6 canvas | >= 10 fps for 3 s; >= 2 distinct canvas PNGs in 5 s |
| G2-a | harness | a test that measures geometry and is RED today |
| G2-b | G2.1 box tree — **CLOSED 2026-10-14** | every visible node carries x/y/w/h; `div > p > span` reports strictly increasing y per depth |
| G2-c | G2.2 gBCR honesty — **CLOSED 2026-10-17, 100 %** | +/-2 px on >= 95 % of sampled nodes; negative: `display:none` still returns 0,0,0,0 |

**STATUS 2026-10-17: G1 and G2 are COMPLETE.** G1 closed 2026-10-14 (all six KPIs). **G2.1 and G2.2 closed 2026-10-17** — G2.2 measures **100 %** (`9/9 comparable rows within ±2 px`) and is wired into `nb_all_tests.sh`; see §9.12 for the three findings that got it there and §9.13 for the instruments that were reporting numbers they were not measuring. G2.3 (flex row) is the next slice.

**STATUS 2026-10-14 (late): G1 IS COMPLETE — all six KPIs done.** G1-a,
G1-b (G1.1), G1-e (G1.3), G1-c (G1.4), G1-d (G1.5), G1-f (G1.6 **and**
G1.2). G1.2 measured 23.6 fps for 3 s at 0.3 % of one core, with a
control proving 0.0 fps without the tick loop. G1.6 measured 5 distinct
canvas PNGs in 5 s, and 1 without the tick loop. The next P0 is **G2**
(box tree): G2-a harness, then G2.1, then G2.2.

Earlier status line (kept for the record): G1-a, G1-b and G1-e are DONE** (measured numbers,
design decisions, four silent-failure modes found, and the next bite all
in **[§9](#9-addendum-2026-10-1314--g11-and-g13-landed-measured)**).
The next step is **G1-c / G1.4 focus safety** — the last G1 risk the push
protocol introduced.

**LANE OWNERSHIP — the thing to settle before G1-b starts.** AGENTS.md
splits this repo: the `opencode-fix` lane owns `ops/` worker/fetch/driver
code, `tests/worker_*`, the `nb_js_worker.+x` sides and the FETCH protocol,
and leads `network_browser_manager.c`. The row-projection lane (this one)
owns what lands in the window: page-state, xhtpm, css, and the renderer.

- G1's core (the worker tick loop and the push protocol) is **worker-lane
  code**. The plan has always said "owner must be agreed before starting -
  this is not a row-projection task", and that still holds.
- G2's box tree lives with the DOM/worker side too (`getBoundingClientRect`
  is `nb_js_worker.c`; `nb_dom.c` has zero flex/grid), with the projector
  consuming it.

So the row-projection lane can, and will, do this first: **the red
acceptance harnesses** (G1-a, G2-a) plus the renderer-side work those
reveal (G1-c, G1-d) — none of which touches worker C. That is not
avoidance; a 2-3 sprint rewrite started by two agents in one worktree on
`nb_js_worker.c` is how you get a half-applied protocol nobody can review.
The harnesses are what make the sprint safe to start at all: the next
person to open G1-b inherits a test that says "this is broken, and here is
the number that closes it".

---

## 2. What is explicitly NOT done, and why (code-level)

Every claim below is backed by a file+line observation made on 2026-10-11, not by
recollection. Sizes: `ops/nb_js_worker.c` 8793 lines, `network_browser_manager.c`
6714, `nb_css.c` 439, `nb_dom.c` 483.

### G1 — Real JS applications do not run (no live pushes) ← **the big one**

**Observed:** the worker is a request/response daemon. `LOAD` → `run_page()`
→ scripts → timers drained for at most `MAX_DRAIN_MS` = 800 ms
(`ops/nb_js_worker.c:5486`) → **one** `dom_render_rows()` →
one `RENDER\n<rows>` frame → `STATUS`. Then it blocks in `recv_frame()` waiting
for the next command (`ops/nb_js_worker.c:8764-8790`). There is no tick, no
timer-driven push, no self-wakeup.

Consequences, each observed rather than assumed:
- A `setInterval(…, 1000)` clock stays frozen at tick 0. A 100 ms one renders
  `tick 7` exactly once and then stops (measured 2026-10-11, recorded in the
  roadmap's Milestone-3 note).
- `requestAnimationFrame` exists (`nb_raf`, `ops/nb_js_worker.c:6190`) but
  schedules into that same finite drain — it is a timer, so it inherits the cap
  (`MAX_RAF_FRAMES` bounds total fires per LOAD).
- Canvas/WebGL content can only publish during a render
  (`dom_render_rows` walks dirty canvases → PNG → 5-field `IMG … |canvas` row,
  `ops/nb_js_worker.c:736-750`). No render, no canvas update — which is why
  animation-driven pages look frozen rather than wrong.
- Interaction *does* re-enter the worker: a content-row click sends
  `EVENT\n<selector>\n<type>`, the worker dispatches a real bubbling `Event`,
  drains, and re-renders (`network_browser_manager.c:4515-4531`,
  `ops/nb_js_worker.c` `cmd_event`). So click-driven SPAs half-work; anything
  driven by timers/animation/storage events does not.

**Why it is not a small fix:** it is a protocol change (push + tick), not a
tweak. The worker needs a loop that owns time; the manager needs to consume
frames it did not request; `page.state.txt` and the khtpm incremental-reparse
path need to tolerate unsolicited updates without stealing the `cli_io` focus.

### G2 — Layout engine is partial (no box tree)

**Observed:**
- `nb_dom.c` contains **zero** mentions of flex or grid; `nb_css.c` has exactly
  one, and it is a comment (`nb_css.c:297`). Page content is flattened to row
  kinds by the projector — there is no box to lay out.
- CSS surface is small and explicit: `@media`/`@keyframes` **bodies are skipped**
  (`nb_css.c:215`), and the recognized property set is display, width, height,
  opacity, visibility (+ `collapse`/`hidden`/`none` keywords). No margins,
  padding, border, font, color, position, or float resolution in the page cascade.
- `getBoundingClientRect` is documented in-source as "simple block layout":
  `x` is always 0, `y` is the sum of previous siblings' computed heights, with
  `NB_CONTENT_PX 640` / `NB_CHAR_PX 7` / `NB_LINE_PX 14` as the text model
  (`ops/nb_js_worker.c:930-1000`). Accurate enough that some page scripts
  cooperate; wrong often enough that layout-dependent apps misbehave.

**Dependency:** every flex/grid/positioning item is downstream of a box tree
existing to operate on. The house layout engine (`css_layout_pass` — row, wrap,
grow, gap) is real and healthy for *chrome* toolbars; the work is wiring page
content boxes into it, not writing layout code.

### G3 — Browser chrome gaps

**Observed absent** (grep across template + manager + worker + both CSS files,
2026-10-11): **zoom = 0 hits, autofill = 0 hits, print = 0 hits**. `find` appears
only inside a code comment (`network_browser_manager.c:1139`), and `download`
only in a comment (`network_browser_manager.c:4206`) — neither is a feature.

**Present:** Back, Forward, Reload, Stop, Home, New tab, Close tab, Bookmarks,
Bookmark, History, Clear All History, Console (row labels enumerated from
`network-browser-hq.xhtpm`).

**Also absent/partial:** multi-row selection. `sel_anchor` is a per-element
cursor-range field (`Elem.sel_anchor`, round-tripped as a plain int in the frame
tail), so selection and copy work *within* one text row (inline spans, input
fields) but not across rows or across wrapped lines as a browser user expects.

### G4 — Smaller measured items

- Projection caps: `MAX_LINES` = 4096 lines (`network_browser_manager.c:127`),
  which is the ceiling a very long article hits before rows are dropped.
- `fetch`/`XHR` exist and are Promise-shaped, but block on a curl child during
  the load drain (`ops/nb_js_worker.c:6197`) — fine at load, not viable for a
  live request/response loop, which G1 needs.
- `localStorage` (disk jar) and `sessionStorage` (per-LOAD) are real
  (rung 6, `ops/nb_js_worker.c:2875`); `document.cookie` is a real file-backed
  jar (`:2380`). Storage is not the blocker for G1 — time is.

---

## 3. KPI framework

House verification standard applies to every KPI: **fresh build + fresh run +
real evidence**. A clean compile is not evidence. Every KPI below names the
artifact that proves it, and every behavioral KPI must ship with a suite entry
that has been negative-tested at least once (a passing assertion that cannot
fail is not a KPI).

### 3.1 Standing KPIs (any sprint, any lane)

| # | KPI | Measure | Current (2026-10-11) |
|---|---|---|---|
| K1 | Hermetic suite green | `sh tests/nb_all_tests.sh` → `ALL TESTS PASS` | PASS (14 suites) |
| K2 | Live-browser suite green | same suite against a relaunched browser | PASS (first run 2026-10-11) |
| K3 | Live interaction suite | `tests/nb_interact_test.sh` (needs a running browser; deliberately outside the hermetic runner) | run manually before milestone claims |
| K4 | Frame-protocol compatibility | any frame-tail field addition must not break front-index parsers | enforced since `0071265f2` |
| K5 | Evidence per claim | pixel/state diff artifact named in the commit message | standing practice |
| K6 | No uncommitted work | `git status` clean at session end, own branch only | standing practice |

### 3.2 G1 KPIs — "JS apps actually run"

| # | KPI | Definition of done (measurable) |
|---|---|---|
| G1.1 | Live timer tick | a page with `setInterval(f, 500)` updates its own row **≥ 6 times in 10 s with no user input**, verified from a fresh frame dump (row text differs across dumps, tick counter monotonic) |
| G1.2 | rAF loop | a page driving `requestAnimationFrame` produces a **new frame at ≥ 10 fps** for 3 s (measured by frame-dump mtimes), with no worker CPU runaway (> 60 % of one core) |
| G1.3 | Push protocol | worker→manager frames arrive without a manager request; **negative test**: idle page produces zero unsolicited frames |
| G1.4 | Focus safety | unsolicited re-render never steals the `cli_io`/address-bar focus or the nav cursor mid-typing (assert in a live test) |
| G1.5 | SPA click-through | a click on a rendered content row re-renders **without a reload** — already true today via `EVENT`; pin it in the suite so G1 cannot regress it |
| G1.6 | Canvas liveness | an eval-driven canvas that mutates per tick publishes **≥ 2 distinct PNGs** in 5 s (today: 0 after the first render) |

### 3.3 G2 KPIs — "layout is real"

| # | KPI | Definition of done |
|---|---|---|
| G2.1 | Box tree exists | the DOM node carries computed box geometry (x, y, w, h) for **every visible node**, and a fixture proves it: nested `div > p > span` reports strictly increasing `y` per depth |
| G2.2 | gBCR honesty | `getBoundingClientRect` matches the projected row geometry **within ±2 px on ≥ 95 % of sampled nodes**; negative test: `display:none` subtree still returns 0,0,0,0 |
| G2.3 | Flex row | a page-side `display:flex` container lays children out in a row with correct widths (sampled columns share edges within 1 px) |
| G2.4 | Grid | after flex: `grid-template-columns` honored on a fixture |
| G2.5 | `@media` | a viewport-width-dependent rule switches at the breakpoint; fixture proves both sides |
| G2.6 | Cascade completeness | the recognized-property list in `nb_css.c` covers the set G2.1–G2.5 needs, and the list is documented in-source (no silent drops) |

### 3.4 G3 KPIs — "chrome parity with a real browser"

| # | KPI | Definition of done |
|---|---|---|
| G3.1 | Find in page | `/`-armed find box highlights every match and steps through them; negative test: zero matches reports "not found" rather than hanging |
| G3.2 | Zoom | Ctrl+`-`/`+` changes rendered text size and re-wraps within 1 s |
| G3.3 | Downloads | a download link writes a file and reports size/path; negative: unwritable path reports an error row |
| G3.4 | Print | a page prints its projected rows to a file/PDF via the house print path |
| G3.5 | Multi-row selection | shift-click / drag across rows selects a contiguous run and copy yields the joined text |
| G3.6 | Autofill | form values remembered per-origin and offered on return |

---

## 4. Sprint ladder (dependency order, honest sizing)

Sizing is "sprints" where a sprint = the unit this lane has actually been
delivering (a scoped feature + suite pins + live proof + commit). Cross-lane
items are marked.

| # | Sprint | Depends on | Size | Notes |
|---|---|---|---|---|
| S1 | **Worker tick loop + push protocol** (G1.1–G1.4) | worker lane + manager handshake (`network_browser_manager.c` is the merge point) | 2–3 sprints, cross-lane | Largest single item in the backlog. Everything JS-app-shaped queues behind it. Owner must be agreed before starting — this is not a row-projection task. |
| S2 | Canvas liveness on top of S1 (G1.6) | S1 | 1 sprint | Cheap once frames flow; the publish path already exists and is dirty-flagged. |
| S3 | **Box tree, phase 1** — geometry for every visible node (G2.1, G2.2) — **DONE 2026-10-17** | manager lane owns the tree; projector consumes it | 2 sprints | This is the gate for all layout work. |
| S4 | Box tree phase 2 — flex row (G2.3) — **NEXT** | S3 | 1 sprint | The engine already does row/wrap/grow/gap for chrome; this is wiring, not invention. |
| S5 | Grid (G2.4) | S4 | 1–2 sprints | Roadmap rule stands: grid only after flex. |
| S6 | `@media` + cascade completeness (G2.5, G2.6) | S3 | 1 sprint | Independent of S4/S5; can run in parallel. |
| S7 | Find in page (G3.1) | — | 1 sprint | Fully independent of S1–S6. **Best first pick for the row-projection lane.** |
| S8 | Multi-row selection + copy (G3.5) | — | 1 sprint | Independent; per-element `sel_anchor` already round-trips. |
| S9 | Zoom (G3.2) | S3 (needs re-wrap on width change) | 0.5–1 sprint | Half-independent if scoped to chrome text only. |
| S10 | Downloads (G3.3) / Print (G3.4) | — | 1 sprint each | Independent; shell-command work, not layout. |
| S11 | Autofill (G3.6) | storage jars (already real) | 0.5 sprint | Cheapest G3 item. |

**Total to "Google-class JS apps":** S1–S6 plus a realistic tail of per-site
fixes, i.e. multi-sprint and cross-lane. **Total to full chrome parity:** S7–S11,
which is lane-local and can be taken one at a time.

---

## 5. Rules of engagement for whoever picks this up

1. **Do not re-litigate the non-goals.** No embedded engine (WebKitGTK/CEF
   parked in `_trash/gtk-embed`, CEF rejected — see
   `2026-10-04-BROWSER-REAL-ENGINE-RESEARCH.md`), no new system dependencies for
   the browser surface. The renderer stays khtpm-generic: new page shapes become
   state-file rows / projector output, never per-app C.
2. **Structure, not strings.** No hardcoded per-site selectors. Every fix must be
   expressible as "this shape of DOM", which is also what keeps it from rotting
   when the site changes.
3. **Verify per AGENTS.md.** Fresh build + fresh run + real evidence; a clean
   compile is not evidence; never report a number you did not observe
   (three wrong conclusions in one 2026-10 session traced exactly to that).
4. **Frame-tail changes break end-peel parsers.** `0071265f2` proved it: adding
   two tail fields silently broke `themescan.py` and the select test. Parse frame
   geometry from the FRONT (fields 0..11 are stable for pipe-free labels).
5. **Shared checkout.** Two agents can be live in one worktree; commit with an
   explicit pathspec and read `git diff --cached --name-only` before committing.
6. **Milestone claims require K2/K3 evidence**, not just hermetic PASS.

---

## 6. Explicitly declined (do not re-open without new pain)

- **Full nav-slot renumbering** (every link its own slot): ~40 assignment sites
  and ~80 `g_nav[]` consumers; addressability is already solved three ways
  (digits in cursor, `5.2` dot-direct, arrow-then-Right), and arrows stopping at
  every link would slow primary row nav. Declined as **superseded, not
  deferred** (addendum in `2026-10-08-INLINE-SPANS-PHASE2-HANDOFF.md`).
- ~~**Chrome-pixel parity as a launch bar** — stated non-goal.~~ **Withdrawn 2026-10-20 (owner): the goal is now a fully Chrome-replaceable browser; see `08-roadmap/2026-10-20-CHROME-REPLACEMENT-SIZING.md`.** Do not re-decline it.
- **Reverse-engineering YouTube's Polymer UI** — video stays on the V4
  extraction carve-out.

---

## 7. Copy locations

- House roadmap: `08-roadmap/NETWORK-BROWSER-RENDER-ROADMAP-2026-10-05.md`
- This gap/KPI doc: `08-roadmap/design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md`
- XO copy: `x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/xo/23.network-browser/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md`
---

## 8. Addendum 2026-10-12 — the long-article ceiling, measured then halved

Originally written as "the pool caps the browser at ~40 rows, not fixed
here". **Half of it is now fixed** (same day, next slice), and the rest of
the chain is measured below. Superseded numbers are struck through so the
history of the claim stays readable.

### 8.1 What the ceiling actually was

`network-browser-hq.xhtpm`'s content `<repeat>` emits **24 elements per
content row** — one `<text>`/`<item>`/`<cli_io>`/`<bar>`/`<canvas>` per
content KIND, each hidden by its own `show="${c.is_*}"`. Exactly one is
ever visible.

`parse_element()` allocated an element from the pool **first** and dropped
it **afterwards**, when `show=` said so. So every gated alternative cost a
pool slot to be thrown away: a 124-paragraph article asked for 24x124 =
**2976** elements and got `MAX_ELEMS` = **1024**.

### 8.2 Fixed: gated elements cost no pool slot

The gate is now evaluated **before** the allocation; a gated element and
its subtree are skipped by pointer arithmetic. Measured, same fixture, same
binary path:

| | before | after |
|---|---|---|
| `[POOL] elements` | **1024/1024** (overflow WARN) | **187/1024** |
| content rows in the tree (124-paragraph page) | **37** | **122 of 122 published** |
| renderer `VmRSS` on that page | 56 MB | 28-31 MB |

The RSS number is the reason this beat raising `MAX_ELEMS`: `Elem` is
**29,408 bytes** (measured from the binary: `g_pool` is 0x1e0e000 = 30.1 MB
for 1024 slots), so 4096 slots would have cost 120 MB of BSS **plus** the
same again for the pool's second index array. Freeing the waste was worth
33x more than enlarging the tank.

Honest scope: the gate, the three falsy values and attribute order are
unchanged; the two window-level attributes (`drop_action`,
`drop_highlight`) are still applied from a skipped element; and the peek
**fails open** — an unterminated quote or a missing `>` means "no gate
here", never "drop it". A parser may not guess in the dropping direction.

### 8.3 Closed too: the repeat buffer and MAX_CHILDREN (same day)

With the pool free, the limiter moved twice more. Both were silent.

| link in the chain | symptom on the 1500-paragraph page | fix |
|---|---|---|
| repeat expansion buffer | stopped emitting after **169** of 682 rows, no warning | `rcap` self-grows: the expansion reports that it ran out of room and the caller doubles and retries, to a 64 MB ceiling. A short result is accepted only at the ceiling, and counted |
| `MAX_CHILDREN` = 320 | **319** of 683 rows; 364 children dropped, no warning | raised to **1024**, above the manager's own projection cap for this page |

After both: **683 content rows built of 682 published**, pool 749/1024,
and the renderer prints **no** truncation warning at all.

Costs, measured on a fresh process: `MAX_CHILDREN` 320 -> 1024 grows Elem
29,408 -> 35,040 bytes (the array is inline), i.e. **VmSize 173,828 kB,
VmRSS 19,636 kB** on the small article fixture. Resident cost tracks
elements actually allocated, not the array size.

`KH_MAX_VARS` = 4096 was measured, not assumed: the 682-row page wants
roughly 6 vars/row, so it sits right at the cap and did **not** trip it
(no `[WARN] var table overflow`). It will bite on a larger page and is
left alone until it does, with the WARN that would announce it.

### 8.5 G3.2 ZOOM — the desk-scale route was rejected; a per-window knob shipped (2026-10-13)

> **RESOLVED the same day — read this box before the record below.**
> What follows correctly rejects the *desktop-scale* approach. It is **not**
> a statement that G3.2 is open: a **per-window text zoom** landed hours
> later as the section's own recommended design (`zoomed()` in the renderer),
> committed `b03bb8e7f` with `tests/nb_zoom_test.sh` pinning state, geometry
> and pixels. **G3.2 is DONE.** The rejected route below is kept because the
> measurement (a Ctrl+= that *shrinks* text) is the reason the shipped design
> does not reuse the desk scale.

The obvious implementation was to bind keys to the scale verbs that
already exist: `UI_SCALE_PLUS` / `UI_SCALE_MINUS` are real, the resize and
relayout path is real, and the taskbar settings window has had a "+" row
driving them for weeks. **Nothing bound them to the keyboard**, so
Ctrl+= / Ctrl+- / Ctrl+0 would have closed G3.2 in a dozen lines.

It does not work, and the numbers are why. Same row, same window, pressing
Ctrl+= (zoom **in**) once:

| scale | short row h | long row h | long row lines |
|---|---|---|---|
| 125% | 30 px | 60 px | 2 |
| 150% | 23 px | 46 px | 2 |
| 175% | 27 px | 27 px | **1** |

Text gets **smaller** as the reported scale goes up, and by 175% a wrapped
paragraph has collapsed onto one line. Measured twice, in a hermetic
fixture window and in the live network browser (content row `cx2`: 60 px →
46 px), so it is not an artefact of the fixture. Ink pixel count in the
same row moved the same way: 41071 px → 32304 px.

Cause, as far as the evidence goes: `g_ui_scale_pct` is the **COMBINED**
scale (font_scale x a screen-relative auto factor), and the verb's own
comment warns that stepping it and writing the result back folds the auto
factor into `font_scale`. `desktop_set_font_scale()` does exactly both -
it writes `font_scale` AND assigns `g_ui_scale_pct` - so the reload path
applies the factor a second time. It may be correct for the dock (whose
auto factor is presumably 100%) and wrong for every ordinary window.

**Not shipped.** A Ctrl+= that shrinks the page is worse than no Ctrl+=,
so the key bindings were reverted and the probe suite deleted rather than
committed red. What survives is this record.

The next attempt should NOT reuse the desktop scale. A page zoom wants its
own knob: a per-window percentage applied where the font size is resolved
(`scaled()` / `font_for()`), gated on a container the template marks, so
the browser opts its content pane in and no other window is affected. The
reason it was not done here is budget, not difficulty - the design is in
§4 S9 and the seam (`scaled()` is the single place every font size goes
through) is already known.

### 8.4 Where the browser actually stands now

The renderer is **no longer the limiter** at any measured page size: every
row the manager publishes reaches the tree, and the ceiling is now the
manager's own projection cap (`content_count` 683 on the 1500-paragraph
fixture, with an honest "[showing 683 of 3142 rows]" notice). That cap is
`network_browser_manager.c`'s to raise, in its own lane.

Still true, and worth keeping in view: the structural version of this fix
— one element per row with the kind as data, instead of 24 gated siblings
— is still the better end state. It just stopped being the bottleneck,
because making the gated ones free was worth 33x.

---

## 9. Addendum 2026-10-13/14 — G1.1 and G1.3 landed, measured

**G1.1 (live timer tick) and G1.3 (push protocol) are DONE.** Both halves,
both directions of G1.3, measured rather than argued.

| Step | KPI | State |
|---|---|---|
| G1-a | harness | **done** — `nb_timer_test.sh`, was red by design, went green with the feature |
| G1-b | G1.1 live timer tick | **done** — 20 distinct self-updates in 10.1 s, monotonic |
| G1-c | G1.4 focus safety | **next** — needs key injection into a live window |
| G1-d | G1.5 SPA click-through | not started — pin the existing `EVENT` re-render |
| G1-e | G1.3 push protocol | **done** — positive *and* negative |
| G1-f | G1.2 rAF / G1.6 canvas | not started — G1.6 should be cheap now that frames flow |

### 9.1 What "done" was measured as

Worker side, hermetic (`tests/nb_timer_test.sh`, worker RPC directly):

    watched 10.1s with no input: 20 frames, 20 distinct tick values
    [3, 4, 5, ... 22]
    PASS: G1.1 live timer - 20 distinct self-updates in 10.1s

Manager side, live (`tests/nb_tick_live_test.sh`, scratch house, real
server, real push drained and merged):

    TEXT|tick 10 -> 12 -> 14 -> 16 -> 18     (1 s apart, nobody touched it)
    PASS: a page that updates itself repaints with no input (3 -> 9)
    PASS: the invariant paragraph survived the updates

G1.3 negative (`tests/nb_push_quiet_test.sh`) — both arms in ONE run,
because a zero from a harness that cannot see frames is indistinguishable
from a quiet page:

    idle page:   1 frames during LOAD,  0 unsolicited in 5s
    timer page:  1 frames during LOAD, 15 unsolicited in 3s

Every assertion above was negative-tested: the tick suite drops to **0**
with the `TICK` frame withheld; the quiet suite's zero is paired with a
run that pushes 15.

### 9.2 The design decisions worth keeping

- **The tick slice is not `run_event_loop`.** That drains to *quiescent*
  and sleeps until the next timer, so calling it per tick would hold stdin
  hostage for up to `MAX_DRAIN_MS` and the page could not be clicked. The
  slice runs only what is **due now** — the same `run_due_timers` +
  `drain_jobs` pair, without the waiting.
- **Change detection is a string compare** against the last pushed rows,
  not a DOM dirty flag: one `memcmp` is exact, cannot be forgotten, and
  needs no bookkeeping at every mutation site, which is where a dirty flag
  would rot.
- **Arming and draining land together.** The worker↔manager link is a
  socketpair; a worker blocked writing into a full one stops ticking *and*
  stops answering the next `LOAD`. There is no ordering where `TICK` goes
  out and the drain arrives later.
- **Arming is in `worker_load()`, not `worker_pump_reply()`.** The LOAD is
  what leaves timers pending; nothing sends LOAD through the pump. This
  cost two empty-log runs before it was instrumented.
- **Push is opt-in** (`TICK\n<ms>`, off by default) so a red KPI can only
  ever mean "the loop does not run", never "nobody turned it on".

### 9.3 Four silent-failure modes found on the way

All the same shape: something fails, the page still renders plausibly, and
**nothing says so**. All four are now covered by a test.

1. **A DOM the worker cannot load was dropped silently** — `nb_dom_load()`
   returns NULL for anything that is not `nb_serialize` output, and a
   missing file just fails `fopen`. The page rendered as its title and
   nothing else, with `STATUS ok`. Now an `ERROR|` frame, which the
   manager already handled for worker diagnostics.
2. **The frame reader stole a byte.** Both the worker's
   `recv_frame_ex()` and the manager's `worker_recv_line_to()` read one
   byte unconditionally after a payload and dropped it if it was not a
   newline — eating the next frame's first length digit, and *blocking
   forever* against a peer that omits the newline and goes quiet. The
   comment claimed a pushback that the code did not do. Both now peek with
   a zero timeout and push the byte back.
3. **The manager's worker probe fails silently.** `run_page_scripts()`
   resolves the worker relative to the **house**
   (`g_package_dir = "$house/&.hq-apps/network"`) and returns without a
   word when it cannot open it. A house without the app installed renders
   the static extraction and never ticks, with an empty log.
4. **The id field is not the id.** In the `nb_serialize` wire format
   `nb_attr_get()` parses the raw **attrs blob**; the serializer writes
   `N|p|t||6|tick 0|8|p id="t"`, repeating the tag name and carrying the
   attributes. A hand-written `N|p|t||6|tick 0|0|` loads fine, dumps fine,
   renders its `tick 0` row like a healthy page — and is invisible to
   `document.getElementById`. **Fixtures are generated now**
   (`tests/g1_make_dom.c`), never hand-written.

### 9.4 The method lesson, recorded because it recurred

Four wrong turns in this sprint, and **three were misreading my own
output rather than bad reasoning about the code**:

- A multi-line `fprintf` read with `grep`, which shows only the **first
  physical line** — a full row set read as a bare `TITLE`, which invented
  an entire "the DOM's text is destroyed" theory.
- Debug prints that went to a `DEVNULL` stderr in the harness, so "0
  firings" was read as a finding while the loop was working all along.
- "The `textContent` setter is at fault", argued instead of diffed. One
  experiment against a generated DOM disproved it.
- `nb_serialize` returning a **node count** printed as "bytes", which made
  a good 370-byte fixture look like a 7-byte failure.

Standing rules that came out of it: measure before theorising; never
report a number you did not observe; never read a truncated diagnostic as
if it were whole; and prefer generating a fixture to writing one.

### 9.5 Next bite (G1.4)

G1.4 is the last G1 risk this change introduced: an unsolicited push must
never steal the address bar or reset the nav cursor mid-typing.

**CORRECTION, measured on the live window 2026-10-14: I was wrong about
what G1.4 needs.** I extended `tests/xsendevent.py` with `KeyPress`/
`KeyRelease` believing there was no way to type into a window. There is —
and it was already there:

- **`dispatch_relay_code()` maps relay codes 32..126 to literal
  characters**: `else if (code >= 32 && code <= 126) handle_key(0,
  (char)code);`. So `printf '122\n' >> <relay>` types a `z`.
- What actually blocks the test is **arming**, not typing. Typed
  characters go nowhere until the `cli_io` is armed, and arming happens on
  a real click (`click_focus_then_activate()` → `g_default_input_elem`).
  Verified live: three `z` codes into a running browser changed nothing in
  the mirror except its timestamp. There is no `Ctrl+L`.

So the xsendevent key support (commit `dc7ce76da`) is **not required for
G1.4** and was built on a wrong assumption. It is harmless and correct on
its own terms — the dispatcher reads keys through `XLookupString(&ev->xkey,
…)`, so a synthetic key does need a real keycode — but it is not the
tool this KPI needs.

What G1.4 now needs, concretely:

1. **The address bar's pixel coordinates**, to click it and arm it. The
   `nb_span_drag_test.sh` suite already solves this ("link geometry comes
   from the parked capture's own blue underlines", never hardcoded) — that
   technique is the precedent to copy.
   *Alternative, cheaper:* a relay code that arms the focused `cli_io`
   without a click. That is a real renderer change, so it wants its own
   decision, not a side effect of a test.
2. Then: type a partial URL, let pushes republish, and assert **both**
   that the text survives **and** that the page's tick row advanced in the
   same window. The second half is what stops the test passing vacuously.

### 9.6 Verify a push with `git ls-remote`, never with `origin/<branch>`

Measured 2026-10-14, after being caught out by it.

`git push` printed a confident range line for every commit this sprint and
every one of them landed. Checking them afterwards said the opposite:

    git merge-base --is-ancestor c714a89d5 origin/opencode   -> false
    git rev-list --left-right --count origin/opencode...HEAD -> 0  42

Both were **wrong**, and the reason is this repo's fetch refspec:

    remote.origin.fetch = +refs/heads/claude:refs/remotes/origin/claude

Only `claude` is tracked. There is no refspec for `opencode`, so
`refs/remotes/origin/opencode` is a **stale local ref that `git fetch`
does not update**. It sat 42 commits behind while the real remote was
exactly at HEAD — which is also why `git push` correctly said "Everything
up-to-date": there genuinely was nothing to push.

The trap is the worst kind: the stale ref produces a *specific, plausible,
completely false* answer, and it points at "your work is unsaved" rather
than "your check is broken".

**So:**

    git ls-remote origin refs/heads/opencode    # ground truth, asks the server
    git fetch origin opencode:refs/remotes/origin/opencode   # repair the ref

A push is only *believed* after `git merge-base --is-ancestor <sha>
origin/opencode` — and that is itself only meaningful once the tracking
ref has been repaired. Same family as the other three measurement traps in
§9.4: the instrument, not the code, producing a confident wrong number.

### 9.7 G2.2 groundwork: the correspondence exists, and the dump cannot be parsed positionally

Measured 2026-10-14, ahead of writing G2.2.

**The correspondence is definable.** A 6-paragraph fixture projected to
exactly **6 content rows** in the renderer's frame dump. So "the same node"
across the two independent layout systems — the worker's DOM box tree and
the renderer's painted rows — can be pinned as *the Nth projected content
row corresponds to the Nth DOM element*. That is the definition G2.2
needs, and it is now evidence rather than an assumption. (6 → 6, order
preserved.)

**But the dump is NOT positionally indexable**, which is a trap worth
writing down before someone writes a parser against it:

    1 text          <- tag
    2 cx1           <- id
    3 nb-text,nav-text  <- class
    4 Paragraph 1 of the geometry correspondence fixture.
                     <- LABEL: spans newlines and runs of EMPTY fields
    5 21   6 0   7 200   8 180   9 389   10 30

Labels contain newlines and empty fields, so every numeric column after
the label **shifts per row**. A first attempt that guessed fields 8–11
returned the *sidebar* columns (`cx1..cx6`, all plausible numbers, all
the wrong numbers) and would have produced a confident, entirely wrong
±2 px comparison.

**DONE (same day): `tests/frame_geom.py`, verified against the real dump.**

The emitter is `khtpm_core_render.c`'s frame serializer, a fixed 29-field
pipe-separated row with x/y/w/h at positions 9-12. So the parser counts
from the END (where the field count is known and cannot drift), and
**verifies the count is exactly 29** - a row that does not match is
refused, never guessed. Verified two ways:

  - positives: the window reads **x=0 y=0 w=784 h=504**, which matches the
    real window, and content rows step **y=180, 210, 240, 270 at h=30**.
    The old guessing parser read the same row as `y=200 w=180 h=389`.
  - negative: given a good row plus a short row plus a row with an
    embedded newline, it parses 1 and **skips 3**, printing why.

So G2.2's remaining work is the comparison itself: sample N pairs by
order (Nth projected row ↔ Nth DOM element) and assert ≥ 95 % within
±2 px. The instrument is ready; the tolerance is the easy part.

Note the shape: this is the ninth instance of §9.4's lesson, and the
first where the trap was found **before** it produced a wrong verdict
rather than after.

### 9.8 G2.2 measured: the two geometry systems do not agree, and cannot yet

After restarting the manager so the resident worker actually ran today's
binary (see §9.9), both sides of a 6-paragraph fixture, same page, same
moment:

| row | worker `getBoundingClientRect` | renderer painted row |
|---|---|---|
| 1 | x=0 y=0 w=640 h=14 | x=200 y=180 w=389 h=30 |
| 2 | x=0 y=14 w=640 h=14 | x=200 y=210 w=389 h=30 |
| 3 | x=0 y=28 w=640 h=14 | x=200 y=240 w=389 h=30 |
| 4 | x=0 y=42 w=640 h=14 | x=200 y=270 w=389 h=30 |
| 5 | x=0 y=56 w=640 h=14 | x=200 y=300 w=389 h=30 |
| 6 | x=0 y=70 w=640 h=14 | x=200 y=330 w=389 h=30 |

**0 of 24 values are within ±2 px.** G2.2 as written would score 0 %,
not 95 %.

But read the shape before reading the verdict, because the shape is the
useful part:

- **Both are internally consistent.** The worker steps y by 14, the
  renderer by 30, every row, no drift. Neither is jittery.
- **Both are monotonic and order-preserving.** Row N is row N on both
  sides.
- **They disagree on scale and origin, not on structure.** The worker
  reports DOM-node geometry in its own space, with the renderer's 200px
  sidebar and 180px chrome simply not existing; the renderer lays out
  PROJECTED ROWS in its own column at its own row height.

So the honest conclusion is not "geometry is broken" — it is that **the
two systems have never been one system.** The worker's box tree (§9.7 and
G2.1) is a self-consistent model of the DOM; the renderer is a
self-consistent model of the rows it was handed. Nothing has ever made
them agree, because nothing has ever required it.

Closing that gap is an ARCHITECTURE change, not a tolerance change: one
of the two has to become authoritative for the other, and the choice is a
design decision with real consequences (the worker owns the DOM; the
renderer owns the pixels). G2.2's ±2 px stays red until someone makes
that call — and it should stay red, because a green one here would mean
the number had been tuned rather than earned.

### 9.9 The scale gap is ONE CONSTANT, not a layout disagreement

Follow-up measurement on the same 6-paragraph fixture, reading every row's
height out of the renderer's dump with `tests/frame_geom.py`:

    class                count   h
    nb-text,nav-text         6   30     <- our content rows
    quiet                   12   30
    nb-side                  9   30
    nb-side                 24  150     <- sidebar containers, not rows

and the content y values step **180, 210, 240, 270, 300, 330** — exactly
+30 every row.

So the renderer paints a content row at a **constant height of 30**. The
worker models text at `NB_LINE_PX 14`, documented as "12px text on a 14px
baseline". Those are not the same quantity: the worker is modelling a
**line box**, the renderer a **row** (line + leading/padding). That single
mismatch is the entire scale disagreement in §9.8 — not a disagreement
about layout, about two constants meaning different words.

Which makes the architecture call much smaller than it looked:

1. **Scale first, and it is nearly free.** Decide that the worker models
   ROW height rather than LINE height, and that constant becomes a shared
   one. After that the y-step agrees (30 = 30).
2. **Origin second, and it is a translation, not a conflict.** The residual
   is the content pane's own offset (x +200, y +180 — the sidebar and the
   chrome above it). That is not a disagreement about geometry at all; it
   is two coordinate spaces, and the renderer already knows where its own
   content pane starts. Feeding that origin to the worker (or asking the
   worker for pane-relative coordinates) is plumbing, not redesign.

The genuinely open question is only step 1's direction — whether the
renderer's row metrics or the worker's line metrics become the shared
definition — because that decides which system is authoritative for
scale. Steps 2 and 3 follow mechanically from it.

ALSO WORTH RECORDING: the resident worker does NOT pick up a rebuild. It
lived from 15:53 while the binary it was running was rebuilt at 19:07, and
`getBoundingClientRect` returned `w=0` — the exact symptom `nb_box_w` was
written to fix. Measured on a stale resident process, that reads as a
geometry bug and is not one. Restart the manager after rebuilding the
worker, or every live geometry measurement is against the wrong binary.
The renderer window has the same property (fixed earlier the same day).

### 9.10 Scale aligned: the y-step now agrees EXACTLY (measured)

Did the §9.9 step 1, and it landed on the prediction to the pixel.

    before:  worker y = 0, 14, 28, 42, 56, 70   h=14   (NB_LINE_PX)
             renderer y = 180, 210, 240, 270, 300, 330   h=30

    after:   worker y = 0, 30, 60, 90, 120, 150   h=30   (NB_ROW_PX)
             renderer y = 180, 210, 240, 270, 300, 330   h=30

Same +30 step on both sides, and identical heights. The residual is now
exactly what §9.9 said it would be:

  - y: a CONSTANT 180 translation on every row (the chrome above the
    content pane). Not a disagreement - an origin.
  - h: agrees on every row (30 = 30).
  - x: a CONSTANT 200 translation (the sidebar).
  - **w: still genuinely wrong — 640 vs 389.** The worker assumes a
    640px content column (NB_CONTENT_PX) while the renderer's pane is
    389px. This one is NOT a translation and is not fixed by anything in
    §9.9: the worker has no idea how wide the pane it lands in will be.

Implementation note worth keeping: this was done by ADDING
`NB_ROW_PX 30` rather than changing `NB_LINE_PX`. They are different
quantities - a line box is text leading, a row is that plus the renderer's
row padding - and overwriting 14 would have quietly redefined the wrap
calculation that uses it. Block stacking now uses the row metric; the
wrap calculation still uses the line metric.

So G2.2 is reduced from "two systems disagree" to two items, one of which
is plumbing:

  1. Feed the worker the pane's real width (and origin), so w and the
     translations stop being guesses. The renderer already knows both.
  2. Decide whether ROW height is now the shared definition - which is
     cheap to answer, since one constant now drives both sides.

Full suite green (ALL TESTS PASS) with NB_ROW_PX in, so nothing that
reads geometry regressed.

### 9.11 The last disagreement closes: feed the viewport, and w agrees

`LOAD` now carries the viewport width as field 6 (js/dom/href/title/css/
**viewport**). Fed the renderer's real pane width, the worker's boxes
report exactly what the renderer paints:

    viewport sent 640   ->  iw=640  w=640  h=30  y=0
    viewport sent 389   ->  iw=389  w=389  h=30  y=0     <- the real pane
    viewport sent 1200  ->  iw=1200 w=1200 h=30  y=0

So the §9.8 disagreement is fully decomposed and each part is now either
solved or explicitly plumbing:

| axis | before | now |
|---|---|---|
| y-step | 14 vs 30 | **agrees** (NB_ROW_PX, §9.10) |
| h | 14 vs 30 | **agrees** |
| w | 640 vs 389 | **agrees when fed the pane width** |
| x, y origin | 200 / 180 off | a translation; two coordinate spaces |

What is still missing is only the SENDER. The worker honours the field
and nobody sends it, because the pane width is the RENDERER's to know -
it lays the pane out. So the remaining work is: the renderer publishes its
content-pane box, the manager reads it and passes field 6. The renderer
already writes both numbers (its receipt records the window; the element
dump has the pane box at x=200 w=389), so this is discovery plus one
field, not new geometry.

Deliberately unchanged until then: absent or unusable (<= 0) leaves the
viewport alone rather than resetting mid-session, and the default stays
640. A wrong-but-supplied number would be worse than an obviously-default
one - the same reason G2.2 stays red rather than tuned.

### 9.12 G2.2 CLOSED: the box tree and the painted pixels agree, 100 %

`9/9 comparable rows agree within ±2 px after translation (100.0 %)`.
Wired into `nb_all_tests.sh`, which it had never been.

Getting there was three findings, in this order, and the middle one is the
one worth remembering.

| axis | §9.11 state | now |
|---|---|---|
| w | agreed, but 8 px too wide on any scrolling page | **agrees exactly** (`dw` 8 → 0) |
| y origin | "a translation; two coordinate spaces" | **agrees exactly** (`dy` 44 → 0) |
| rows below the fold | not accounted for | reported separately, never hidden |

**The pane publisher was never called.** `publish_pane_geometry()` sat at the
*end* of `assign_nav_and_layout()`, which has early returns - and one of them
is the sidebar+panel branch, i.e. exactly this window's shape. So for the
network browser the publisher was never reached: it wrote nothing, silently,
and the renderer's stderr does not reach a log on this leg, so the process had
no way to report its own failure. It is now called on **every** exit path, with
a comment naming the trap so a fourth path cannot silently reintroduce it.

**The 44 px was two things, not one.** I expected to remove the manager's
`page-title` row and be done. Reading the renderer's layout instead of
reasoning about it showed `44 = 14 + 30`:

- 14 px - `layout_scroll_region()` computes `content_y = y + (have_arrows ?
  GENERIC_SBAR_ARROW_H : 0)`, so a column that scrolls starts its first row
  under the scrollbar's up-arrow;
- 30 px - the manager projects `<head><title>` into a `page-title` row as the
  first child of the content scrolllist. A DOM-only model cannot see it,
  because `<title>` lives in `<head>` and takes no body space in any browser.

Both are now modelled, conditional on the same scroll decision as the gutter.
This is the worker mirroring the **house renderer** on purpose - not claiming
`<title>` occupies body space elsewhere.

**The scrollbar gutter is conditional, and that needed the pane HEIGHT.** The
renderer reserves `GENERIC_SCROLLBAR_W` (8, `khtpm_core_render.c:5383`) when a
scrolllist overflows, so painted rows are 381 in a 389 pane. Width alone cannot
decide that, so `LOAD` gained field 7 (`content_h`). There is a real cycle -
wrapping depends on the column and the column depends on whether the wrapped
height overflows - broken by deciding once per page at the full width and
caching. `nb_gutter_test.sh` is the negative control, and it is negative-tested:
a fitting page keeps the full column, an overflowing page loses exactly 8 px.

### 9.13 The 0/12 that was never a result, and the pattern behind it

Worth writing down because the shape recurred far more often than the bugs.

`nb_gbcr_test.sh` reported a confident `0/12 rows agree`. It had been comparing
the wrong elements: it filtered on class `quiet`, believing that was the twelve
paragraphs. `quiet` is the browser **chrome** - 7 nav buttons plus 5 tabs -
which is *also exactly twelve rows*. The count matched by coincidence, so the
test looked self-consistent while measuring nothing.

Three more, all the same shape - a green number that was not measuring what it
claimed:

| instrument | what it actually did |
|---|---|
| class filter | compared chrome, not content (§9.12) |
| `frame_geom.py` stdout | class column truncated to 18 chars, so `from-top,nb-content` was **unmatchable** |
| `browser_pid.sh` | printed the whole pid list, not one; every caller's relay path was garbage |
| `browser_pid.sh --require-one` | counted **lines** in a one-line string, so it was always 1 - a guard that **cannot fail** |
| `xclipget.py` | returns whatever the clipboard holds; a read after Ctrl+C can succeed with the *previous* owner's bytes |
| `nb_gutter_test.sh` (mine) | first "always scroll" injection was an assignment the next line overwrote - a dead statement, so the control broke nothing |

That last one is the honest summary: **on this codebase a green test should not
be trusted until it has been shown to go red.** Four of the six above were
caught only by negative-testing, and the fifth by a byte-identical diff of a
passing and a failing run.

A separate measurement error worth keeping: I reported "17 files under
`xyzfs/users`" from a *conflicted, uncommitted* index. The real number was
**1,749** (`ls-tree` on a commit, not `ls-files` on a live index), and the bad
instrument nearly caused the wrong call on the `origin/claude` merge.

### 9.14 House state after the merge: the reset deleted 1,741 user files

`origin/claude` (199 commits) merged into `opencode`. The live tree could not
be fast-forwarded at first - 241 modified files and 328 deletions from other
agents in flight - so the merge was built and tested in a scratch worktree and
parked, then landed on the branch without moving anyone's working tree.

Checked before touching anything, because AGENTS.md documents this exact
landmine: `origin/claude` tracks **0** files under `xyzfs/users` while the live
branch tracked **1,749**, so a fast-forward would have **deleted them from
disk**, wallets included. Measured before and after:

    ff: 2,004 files 18 wallets -> 255 files 0 wallets
    restored from user/* branches: 2,004 files 18 wallets
    sha256 verify against a pre-reset tarball: exit 0, all files identical

So the data was never at risk - but only because `save-user-data.sh` had
already snapshotted it, and only because the restore path was **proved** (a
dry run through `git archive` per branch, hashed against a tarball taken
outside the repo) *before* anything was deleted. A restore tested after the
fact is a hope.

Two things that went wrong here, both mine: `browser_pid.sh` sent five stale
renderer windows' worth of leftovers into a relay path (fixed in 9.13), and my
own negative control for the gutter was a dead statement (fixed in 9.13).

---

## 9.15 2026-10-29 — GL-OS stalled direction & wraith-alpha path

**Problem:** GL-OS (in `pieces/apps/gl_os/`) stalled due to bad direction — over-engineered as a desktop host with hardcoded project paths, parser tag limitations, and broken sovereignty sync.

**Observation:** The `.xhtpm` layout files (e.g. `network-browser-hq.xhtpm` in `&.hq-apps/network/`) define the row-projection UI chrome — one `<text>`/`<item>`/`<cli_io>`/`<bar>`/`<canvas>` per content kind per row, hidden by `show="${c.is_*}"`. These are the terminal-side UI overlay for the browser window.

**Path forward (turning GL-OS into "wraith-alpha for TPMOS"):**
- Wraith-alpha (`projects/wraith-alpha/`) already solves GL-OS's problem — a thin GL frame consumer that mirrors TPMOS content via `current_frame.rgba32` + `rgb_frame_changed.txt` polling
- Repurpose `gl_desktop.c` as a wraith-style frame consumer instead of a project-mirroring host
- Reuse wraith-alpha's `wraith_gl.c` GLUT setup (1024x640, frame buffer polling, input receipts)
- Use `.xhtpm`/`.chtpm` layouts for the terminal-side UI overlay (same pattern as network-browser-hq)
- Strip `gltpm_parser.c`, `gl_os_project_scan.c`, and per-project manager plugins

**Also updated:** x86-64 build path fixed for `http_server.c` in the tomom project; `button.sh` RENDERER path corrected.

**Docs:** See `xo/HANDOFF_DOCS_SUMMARY.md` and `xo-cal/oct-29-2026/` for full relay testing report.


---

## 9.16 2026-11-04 — G2.5 (@media) CLOSED, and two defects found but NOT fixed

### G2.5 closed: `@media` fires against the viewport

`nb_css.c` skipped every `@media` block wholesale, on the stated ground that
"the engine has no viewport, so a media query can never fire" (`nb_css.h`).
The worker DOES have one: LOAD carries the pane's content width, and
`window.innerWidth` is exported from that same number.

- `nb_css_parse_vp(text, len, viewport_px)` evaluates `@media` against it;
  `nb_css_parse()` keeps the old behaviour through the SAME code path
  (viewport 0 = unknown = skip), so the two cannot drift.
- Grammar supported: `[only|all|screen]` with `(min-width: N)` /
  `(max-width: N)` / `(width: N)`, N in px/em/rem, top-level commas as OR.
  Everything else (print, orientation, prefers-*, `not`) does NOT match —
  chosen deliberately: skipping is a missing nicety, firing wrongly moves a
  page's layout.
- Matching bodies parse recursively into the same list, in source order, so
  the cascade order survives; nested `@media` works for free.

Gate `tests/nb_media_test.sh` (hermetic, registered in `nb_all_tests.sh`):
4 widths straddling the boundary (700/767/768/900), TWO channels each — the
computed display AND the element's rect, because `display:none` must
collapse it to 0x0 and that is the consequence a page observes. A
property-only assertion is a self-report. Negative-tested.

### Found, measured, NOT fixed (each wants its own bite + evidence)

1. **Selector-list specificity is SUMMED, not MAXed.** `.a, .b {}` scores two
   classes, so it outranks a single-class rule inside an `@media` block and
   the override never applies. Measured, same stylesheet either way:

       comma-list base      vp=700  wide.display='block'   <- override defeated
       separate base rules  vp=700  wide.display='none'    <- correct

   CSS takes the MAX over the list. This is pre-existing; fixing it moves the
   cascade for every page, so it needs its own evidence run. The new gate
   avoids the trap and says so in its header.

2. **Projected rows do not skip `display:none` for page-created elements.**
   Both branches appear in the worker's RENDER rows at both widths, so the
   computed style and the painted rows disagree for JS-created nodes.
   Measured while building the fixture above; not touched.

### Also this session: two flake classes closed at the renderer

`reparse_chtpm_if_changed()` cleared per-window state UNCONDITIONALLY on every
rebuild. For the row-run anchor that was already fixed (2026-10-19, with a
`kh_tree_digest` content-identity guard); the same defect was still live for
the inline-span cursor/anchor and for hover:

- span selection: `nb_span_select_test.sh` ~6/10 -> **10/10**; receipts show
  `SPAN kept nav=2 idx=1 anchor=0 (content identical, digest …)`.
- hover: `nb_hover_test.sh` 3/4 -> **8/8**; receipts show
  `HOVER kept nav=1 idx=0 (content identical, digest …)`.

Both use the same shape as the 2026-10-19 fix, which is the pattern to copy
for any future per-window state that must survive a rebuild.

### Standing KPIs at the end of 2026-11-04

`nb_all_tests.sh` -> **ALL TESTS PASS**, 35 sections, 30 suites, 0 failures.

A pre-existing build break was also found and fixed to get there: commit
`a7b6084a5` added `extern int g_win_w, g_win_h` reads in
`khtpm_render_core.c` without lifting the matching `static` in
`khtpm_core_render.c`, so HEAD had not compiled since 2026-10-21 and the
running renderer was two days stale — which is exactly what hides this class
of bug.


### 9.16a CORRECTION (same day, 2026-11-04) — the specificity claim in 9.16 was wrong

9.16 recorded, as an open deviation, that "selector-list specificity is
SUMMED, not MAXed". **That was wrong.** `specificity()` already took the MAX
per comma segment; the header comment in `nb_css.h` said "summed" and I
believed the comment instead of reading the loop.

Chasing the real cause of a `.narrow, .wide {}` base rule defeating both
`@media` branches turned up two different, both-real defects (fixed in
`f06d16b42`):

1. **The last rule was silently DROPPED** when a stylesheet's final character
   is `}`. `skip_block()` returned `end` for both "unterminated" and "closing
   brace is the last character"; the rule loop reads `close == end` as
   unterminated and breaks. Measured: `.aa{...} .xx{...}` (no trailing
   whitespace) -> 1 rule; with a trailing newline -> 2 rules; a single-rule
   sheet ending in `}` -> **0 rules**. Most stylesheets end with a newline,
   which is why this stayed invisible.

2. **`n_type` counted alphabetic characters**, so a class NAME contributed its
   own length: `.narrow` scored n_type=6 and `.wide` scored 4. Equally
   specific class rules were therefore ordered by name LENGTH, so a later
   override lost to a longer name. Measured, element carrying both classes:
   `.aa{block} .xx{none}` -> block (later rule lost);
   `.aaaaaa{block} .xx{none}` -> '' (longer name won).

A type selector is now counted only where one can begin, so `.cls` is
(0,1,0) and `p.cls` is (0,1,1). Ties resolve by source order, as CSS says.

`tests/nb_media_test.sh` keeps the realistic comma-list base rule so both
defects fail the suite if they return, and its header carries the
measurements. Full standing suite after both fixes: **ALL TESTS PASS**, 35
sections, 30 suites — which is the evidence that mattered, since a cascade
change can move every page's layout and nothing else in the suite would have
noticed.

Lesson, and it is the same one as 9.16's other correction: **read the
implementation, not the comment.** The comment was stale/wrong in exactly the
way that sent this investigation down the wrong path first.


### 9.17 2026-11-04 — G1.4 was ALREADY green; the real gap was three unregistered suites

I picked `G1.4` as "the last untouched G1 KPI". **Wrong** - it has its own suite,
`tests/nb_focus_safety_test.sh`, it is registered in `nb_all_tests.sh`, and it
passes, including the assertion that matters:

    after typing: addr=hello
    PASS: the address bar is armed and holds the typed text
    PASS: the projection really was reparsed under the armed field
    PASS: the typed text survived 8 unsolicited republishes
      after typing one more char: addr=helloX     <- cursor still at the end

The lesson is the same one this session keeps charging for, one step earlier in
the process: **check what is already covered before choosing work.** I read the
KPI table (which lists the goal) but not the runner (which lists the coverage),
so I proposed a KPI that was already closed.

### The gap that was actually there: suites that exist but are not run

`comm` of the suite files against `nb_all_tests.sh`:

    nb_flex_test.sh        G2.3 display:flex      NOT registered
    nb_grid_test.sh        G2.4 grid              NOT registered
    nb_wasm_test.sh        WebAssembly            NOT registered
    nb_interact_test.sh                          deliberately outside (K3 says so)

So **K1 did not cover flex, grid or wasm.** The ledger said G2.3/G2.4 were green
while the standing gate could not see either: a flex or grid regression would
have passed `ALL TESTS PASS` silently. Registered in `51584cec2` - flex and grid
beside G2.2 (they need the live browser), wasm in the hermetic group (it needs
only python3). Suite is now 38 sections, and each new section prints its PASS in
the runner.

### Known flake, recorded so nobody re-investigates it

`nb_span_drag_test.sh` is the one non-deterministic suite. Measured 2026-11-04:

| sample | result |
|---|---|
| 6 isolated runs | 4 pass / 2 fail - failures at **T6** ("late release did nothing") and **T1** ("expected LINK1, log empty") |
| T2 specifically, all 6 isolated runs | `T2: drag copy is exact` - passed every time |
| whole-suite runs | 38-section run 1 = red (T2, `clipboard was [ONE then TWO]`); 38-section run 2 = ALL TESTS PASS |

A failure that lands on a DIFFERENT step each run, in a suite whose input is
synthetic `XSendEvent` press/motion/release, is dropped input rather than state
- and it predates this session (then: 3/4, and 0,1,1 across builds). It is NOT a
product regression, and the suite's red should be read as "the synthetic input
driver dropped an event", not as "span drag is broken".

If someone wants it green, the target is `tests/xsendevent.py`'s delivery
guarantee (a retry/ack around each synthetic motion, or driving the relay the
way the keyboard suites do instead of XSendEvent), not the renderer.

### Where the KPIs actually stand at 2026-11-04

`nb_all_tests.sh`: **ALL TESTS PASS, 38 sections, 31 suites** with flex, grid and
wasm now included. G1.1-G1.6, G2.1-G2.5, G3.1-G3.6 all have registered suites
except `nb_interact_test.sh` (K3, by design).

### 9.18 2026-11-04 (evening) — span_drag: the input was never dropped; the timing instrument was

The 9.17 note said nb_span_drag_test's red meant "synthetic XSendEvent dropped an
event" and that the fix target is `tests/xsendevent.py`'s delivery guarantee. Both
were wrong, and the correction is recorded here so nobody re-investigates from
those premises again.

**What was actually wrong, measured in three stages**

1. **Fixed sleeps.** Every dispatch assertion did `sleep 1; grep 'CLICKED LINK1'`
   (or a line count against a stale baseline) against a fixture whose action.sh
   is a CHILD the renderer spawns. Under load the child lands after the sleep -
   the failing T6 message literally printed `log: CLICKED LINK1;`, i.e. the line
   HAD landed, just after the window closed. Failure messages are evidence; they
   were not being read.
2. **Stale grep.** The first fix (a `wait_log` that grepped for the marker) made it
   worse (0/8): every check matched the STALE lines from T1/T3 already in
   click.log and returned instantly, so every subsequent count assertion failed.
   Counts only ever increase, so the honest receipt is "more lines than the
   baseline", never pattern presence. Delivered as `wait_count`.
3. **Fire-and-forget bursts.** Events were sent with no acknowledgment that the
   renderer processed them. `xe()` now waits for the renderer's OWN capture line
   in the relay (every real press/release is appended synchronously inside the
   dispatcher, and the relay queue is FIFO, so the receipt also proves any queued
   MotionNotify ahead of it was processed). Absence assertions use `settle`.

After those fixes, **T1-T5 are deterministic green** (7/7 consecutive batches),
and no batch ever produced a T1/T3/T5/T2-clipboard failure again.

**What remains, and why it is not delivery**

T6 fails in ~2 of 6 runs, always identically: press captured, release captured,
`press alone dispatched nothing` passes, and the pair then **never** dispatches
(waited 180s; the replay lab reproducing the full T1-T6 sequence 8+ times never
reproduced it). Hard evidence that delivery is complete: the relay files from the
failing runs contain every press and release verbatim, and the fixture's
`human_input/<pid>.txt` logs all five real presses with nav=1.

The failing runs all show the same signature in `kh_focus_debug.log`: **every
dispatch triggers a full content rebuild** (`ANCHOR cleared by content rebuild`
+ `SPAN kept ... (content identical, digest ...)`), and T6's press/release pair
brackets exactly one such rebuild (14:34:49.486 and 14:34:59.602 in the failing
run). The deferred span pair is evaluated against whatever the rebuild leaves
behind and intermittently declines to dispatch - deterministically silent
afterwards, never merely "slow".

**This is a renderer-side race** (the release's span pair vs the post-dispatch
content rebuild), in the fixture's every-event-rebuild cycle. It cannot be
closed from the test without touching the renderer's most-loaded path; closing
it means either the release-side pair is evaluated against post-rebuild state,
or the fixture stops rebuilding per click - a renderer task with its own
verification loop, not a test tweak. The test now waits honestly and reports the
failure; it cannot make the race impossible.

**Corrections to 9.17**

- ~~"synthetic XSendEvent dropped an event"~~ - wrong. Events were always
  delivered; the instrument was reading stale data and fixed windows.
- ~~"fix target is tests/xsendevent.py's delivery guarantee"~~ - implemented
  exactly that class of fix (relay capture receipt in `xe()` + count-based
  waits); T6 still fails. Delivery was never the binding constraint.
- The suite's red should now be read as: **one span pair sometimes loses to the
  rebuild cycle inside the renderer** - not as a product span-drag defect, and
  not as dropped input. T1/T2/T3/T4/T5 are honest green.

**Lessons**

1. Read the failure's own payload first. "log: CLICKED LINK1;" was the whole
   diagnosis; three theories were built on top of it before it was read.
2. Never assert on a marker inside an ever-growing file; assert on a delta.
3. A test that fires a burst of input into an event-driven app must either
   serialize on the app's own receipts or be expected to race with everything
   the app does between events - here, a full content rebuild per click.

Current status of the flake: **T6 only, ~1 in 3 runs, renderer-side race, known
and documented; the rest of the suite is deterministic.**

### 9.19 2026-11-04 (night) — span_drag root cause FIXED in the renderer (T6 was one rebuild away)

9.18 said the residual was "a renderer-side race - closing it needs the
release-side pair evaluated against post-rebuild state (or the fixture stops
rebuilding per click)." That is now done, and the fixture needed no change.

**Mechanism, measured (not inferred)**

The fixture renders every event with a FULL content rebuild; the debug log
shows one after virtually every event (`ANCHOR cleared by content rebuild` +
`SPAN kept ... (content identical, digest cba5db244267da69)`). The reparse
path (since INLINE SPANS step 11) cleared `g_span_pend.active`
UNCONDITIONALLY at the top of every rebuild - "a rebuild also orphans any
armed span press (its row may be gone); the paired release must find nothing
to resolve."

T6 sends press, waits 8 seconds (the absence assertion), then releases. The
rebuild fires ~0.7s after the arm every single time (T6's armed press is the
only pair with an 8-second window), so the press was ALWAYS orphaned before
the release was parsed - deterministically silent: no SPANPAIR line, no
dispatch, never. T1/T3 (release ~0.5s after arm) raced the same rebuild and
usually won; the failing steps moved between T1/T3/T6 across runs because the
loss depends on which rebuild lands inside the press..release window.

**The fix**

Mirror the house's three existing content-identity guards (ROW-RUN anchor
2026-10-19, span cursor/anchor 2026-11-03, hover 2026-11-04 - all built on
`kh_tree_digest`): save `g_span_pend` before the rebuild; re-arm it afterwards
iff the tree is byte-identical. Identical digest => identical tree order =>
identical nav re-derivation (assign_nav_and_layout indexes from tree order),
so the restored press resolves exactly as if no rebuild had landed. A tree
that changed anything leaves the press orphaned, which is honest (its row may
be gone, may have moved, or may carry a different action).

**Verification**

- 8/8 consecutive runs of nb_span_drag_test.sh green (previously ~1 in 3 red,
  failing at a random step; the full suite had failed on T1 in one run and T6
  in two others).
- Receipts confirm the mechanism: SPANPRESS kept ... content identical after
  each rebuild; SPANPAIR phit=1 ... same=1 at the release; dispatch happens.
- **nb_all_tests.sh: ALL TESTS PASS, 38 sections, 31 suites (exit 0).**

Notes: diagnostic breadcrumbs (SPANARM/SPANPAIR receipts) stay in the
renderer - same class and volume as the existing ANCHOR/SPAN receipts. The
running renderers are unchanged until restart. The digest guard is untouched;
no new untested mechanism was added - the failing leg itself reproduced
reliably and now logs the keep/dead outcome on every rebuild.

### 9.20 2026-11-04 (night) — G2.6 closed: the recognized-property list is real, documented, and gated

G2.6's definition of done: the recognized-property list in nb_css.c
covers the G2.1-G2.5 set, the list is documented in-source, no silent
drops. Reality before this session: the list was an undocumented table,
the header still described the pre-2026-11-04 engine
("only display/visibility/opacity/width/height; @media can never
fire"), and three enum entries (src, font-weight, font-style) were
recognized by prop_id but consumed by NOTHING - a declaration with
those keywords was accepted and silently discarded, the textbook
silent drop G2.6 exists to prevent.

**What was done**

1. The contract is now written in-source, directly above prop_id:
   the complete list grouped by how each property is consumed (layout /
   getComputedStyle-only / parsed-but-unread mirrors / @font-face-only /
   not-recognized-at-all with the reasons), the hard rule that every
   property the projection reads MUST resolve here, and the three-step
   rule for adding one (NbCssStyle + apply_decl + the worker, plus the
   gate below).
2. The dead entries were REMOVED (nothing else referenced them;
   @font-face parses src/font-weight/font-style through its own block,
   which is where they are legal). A declaration of those keywords now
   drops like any other unlisted property - loudly unambiguous, never
   half-applied.
3. nb_css.h's stale module comment rewritten to match the engine that
   actually exists (pointing at the documented list instead of
   duplicating a stale enumeration).

**Two genuine product bugs the gate caught before it ever passed**

- parse_font_face_block computed font-style (italic/oblique) into a
  local, then stored font_style = 0 UNCONDITIONALLY - the compiler
  warned "variable 'style' set but not used". Italic @font-face faces
  can never match. Fixed: font_faces[idx].font_style = style.
- display:none cleared has_display_flex but left display_flex stale
  (2 from display:grid), violating the typed-pair invariant the code
  itself documents ("never disagree"). Fixed: display_flex = 0 too.

**The gate**

tests/nb_css_props_test.sh (registered in nb_all_tests.sh): links
nb_css.c + nb_dom.c directly (zero JS/engine deps, no window, no net),
45 checks, one per recognized property + the drop-silently contract +
@media both branches + inline-style-beats-sheet + @font-face. Negative
tested: reintroducing the font-style bug reproduces exactly one FAIL;
restoring it is green. PASS: 45/45.

**Verification**

- nb_css_props_test.sh: 45/45 PASS (and the deliberate negative test).
- nb_all_tests.sh: **ALL TESTS PASS, 39 sections (was 38), 31 suites,
  exit 0** - with the worker rebuilt on the fixed nb_css.c.

**Flake observation (not fixed, recorded)**

nb_focus_safety_test.sh (G1.4) failed once in the full-suite re-run at
2026-11-04 ("the next keystroke went nowhere"); 4/4 isolated passes and
PASS on the immediate re-run. Same timing-sensitive synthetic-input
class as the span_drag family; no focus-path code changed by this work.
If it recurs, instrument the arm/release receipts the way span_drag was
bisected.

### 9.21 2026-11-04 (night) — G3.4 closed: Ctrl+P prints the projected page to a PDF

The last unbuilt KPI. "A page prints its projected rows to a file/PDF via
the house print path" — no house print path existed, no printer is
configured, and the system carries groff/ps2pdf but no HTML-to-PDF
toolchain. Chosen implementation:

- **Manager invokes the op; the op does the work.** ops/nb_print_page.sh
  reads the projection (network_browser_page.state.txt), emits PostScript
  itself (state -> PS -> ps2pdf; letter size; 0.75in margins; TITLE heading,
  HEAD as bold subheads, TEXT word-wrapped to the margin, LIST indented
  dashes, Helvetica), and writes the PDF into the app's own downloads/ dir
  (same location and same naming discipline as downloads: sanitized TITLE,
  collision-unique, app-owned, never user homes). On any failure it exits 1
  with "ERROR: <why>" and produces no file - never a fake print.
- **Pane reports the outcome like a download**: TITLE|Printed +
  PRINT|<name>|<bytes>|<path> on success (new row kind handled in the
  projector, mirrors DL|), TITLE|Print failed + TEXT|<why> on failure;
  publish_status("printed"|"error: print failed").
- **Triggers**: the manager's own request channel ("print:" line, same
  truncate-one-line contract as go:) - written by ops/nb_write_print.sh
  (chrome-action contract, mirrors nb_write_go.sh) or by Ctrl+P (ch==16),
  added to the shared renderer, gated to the network-browser template -
  other apps keep Ctrl+P. Relay code 16 added for agent parity (mirrors
  Ctrl+C/F).
- **Ghostscript escapes are hand-rolled**: parens/backslashes in page
  content are escaped in the PS prologue - the failure mode that eats a
  PostScript page.

**Verified**

- Live browser: PRINTED Mini_article.pdf (2775 bytes, 1 page, valid
  PDF 1.4, correct title metadata), pane shows the Printed row,
  page.state.txt carries PRINT|<name>|<bytes>|<path>.
- New hermetic gate tests/nb_print_test.sh (registered; 40 sections):
  --out produces valid %PDF- with correct page count; default mode names
  the file from the sanitized TITLE into the scratch house's downloads/;
  hostile page (parens/backslashes/UTF-8) yields a valid PDF; missing
  state file -> rc 1 and NO file; PATH without ps2pdf -> rc 1 with a
  clear ERROR; nb_write_print.sh writes exactly "print:".
- nb_all_tests.sh: **ALL TESTS PASS, 40 sections (was 39), 31 suites,
  exit 0.**

**Scope note**: no "Print" button was added to the browser chrome -
that is a design decision (where it sits, icon, label) and belongs to a
UI bite, not this one. Ctrl+P is the universal print gesture; the
request plumbing and the op are generic (any caller can write "print:").
- PDF lands in downloads/ by design (same rule as downloads - app-owned,
  never user homes).

### 9.22 2026-11-07 — Find down-reveal restored: preliminary width clamped the scroll away

`nb_find_test.sh`'s deep-match clause was red again, and this time the
product-side root cause was located rather than inferred from the failing
assertion.

**What the receipts showed**

- On the `scroll-article.html` fixture the needle is at row 60. A correct
  reveal should land with `scroll=43`, but the live receipts alternated:
  `FINDREVEAL row=60 ... scroll=43`, then a later layout read `scroll=14`,
  then the needle was parked at `y=-100000` again, while the per-PID ASCII
  mirror still reported `NEEDLE ... 1 of 1`.
- Marker logs inside `layout_scroll_region()` isolated the change to setup:
  entry `scroll_in=43` became `scroll_after_setup=14`, with `max=44`.
- Walking the setup statements showed the transition happened at the
  **preliminary full-width clamp** (`width 613`, `total=33`, `max=14`).
  After the scrollbar was reserved, the final narrower width (`605`)
  recomputed the same 29 filler paragraphs as one line wider each, giving
  `total=62` and `max=44` - but the entry scroll had already been clamped
  down to 14 and was never restored.

**The fixes in `khtpm_core_render.c`**

1. Total/scroll-limit calculation now uses the final scrollbar-narrowed
   width, and the caller's entry scroll is restored after that final
   computation when the final range still permits it. This stops the
   preliminary width from silently discarding a real scroll position.
2. A pending Find reveal is cleared when the tree it points into is
   discarded, and in the no-target branch of `kh_find_commit()`; the
   end-of-reparse `kh_find_commit()` then reissues it against the new tree.
3. The one-shot reveal is ancestor-gated to its owning scroll region, so a
   sidebar/layout pass cannot consume a pending reveal belonging to the
   content panel. Visibility is recomputed after the reveal's own scroll
   adjustment.
4. The dump path checks for a just-landed page publish before capturing so
   its PNG and its ASCII sidecar use the same tree/scroll state.

**Chrome wiring included in the same commit**

Network Browser gained its guarded Print action, its raw-X resize grip /
`user-resizable` class, and the final-width scroll repair; `xsendevent.py`
accepts root coordinates so the resize drag delta can be driven honestly.
Generated bootstrap/UI state was left out of the commit.

**Verification**

- `sh build_core_render.sh` — clean build (`OK +x/khtpm_core_render.+x`).
- Deterministic manual needle drive produced the frame record:
  `cx31 ... nav=37, x=200, y=674, w=605`, matching the per-PID ASCII
  mirror's `[FIND] query="NEEDLE" 1 of 1`.
- `sh tests/nb_find_test.sh` — **PASS**, including:
  `needle y -100000 -> 674`, painted band `676..689` on row `674..704`, and
  the opposite-direction match returning to `y=164`.
- `sh tests/nb_all_tests.sh` — **ALL TESTS PASS** (the 40-section/31-suite
  suite visible in the run output, including all previously registered gates).
- Commit: `29a3b72c2e8ae2c62c3593cbe5db2e1df1a1e8ef`
  (`fix: preserve Find/scroll state while wiring browser chrome controls`),
  scoped to the renderer, browser CSS/template/manager, and `xsendevent.py`,
  on `opencode`.

### 9.23 2026-11-09 — matchMedia was a constant-false stub; now a real viewport evaluator

The rung-6 prelude had always installed

```js
Object.defineProperty(window,'matchMedia',{value:function(q){
  return {matches:false, media:String(q||''), ...};
}});
```

so **every** media query answered "no". A 784px window claimed not to be
wider than 600px, and the mobile branch in real bundles
(`matchMedia("(pointer:coarse)").matches`) was silently unreachable. The
stub was not merely incomplete: it was a false statement about the window,
which is exactly the kind of inert surface the house rules reject.

**The slice**

- A native evaluator in `ops/nb_js_worker.c` over the same
  `g_viewport_px` / `g_viewport_h` that `innerWidth` / `innerHeight` report
  (LOAD fields 6 and 7), so `matchMedia` and `innerWidth` can never disagree.
- Supported honestly: media types (`all`/`screen`; `print` and unknown types
  false), `width`/`height`/`device-*` with `min-`/`max-`, the level-4 range
  form `(width >= 600px)`, aspect ratio, resolution, orientation,
  `prefers-color-scheme`, `prefers-reduced-motion`, `prefers-contrast`,
  `forced-colors`, `hover`/`any-hover`, `pointer`/`any-pointer`,
  `display-mode`, `scripting`, `update`, `color-gamut`, `color`/`monochrome`/
  `color-index`, `grid`, `scan`, and `overflow-block`/`overflow-inline`.
- Comma is OR, `and` is AND, leading `not` negates the whole query, `only`
  is accepted and ignored.
- Unknown features answer false rather than guessing. That is deliberate:
  this worker will not claim a renderer capability it does not have.
- `MediaQueryList` is a real object: `media`, `matches` (recomputed on every
  read), `addListener`/`removeListener`, `addEventListener`/
  `removeEventListener("change")`, `dispatchEvent`, and `onchange`.
- The prelude's `matchMedia` line is **deleted** and replaced by a comment;
  it runs after the C install and would otherwise shadow the native with the
  old stub.

**Scope, stated honestly**

- `.matches` is live on read, so polling code is correct.
- Change **delivery** is not implemented: there is no pane-resize channel
  into the worker (LOAD carries one viewport, then the worker lives in it
  until the next navigation), so a listener waiting for a change waits
  forever. The listener surface is real and dispatchable; it is not wired to
  a viewport event that does not exist. This is recorded here rather than
  implied away.
- `prefers-color-scheme: light` is true because there is no OS preference
  channel to consult and light is the defensible default; the evaluator does
  not pretend to know the user's dark-mode setting.

**Verification**

- New gate `tests/nb_match_media_test.sh` at a pinned 800x600 viewport:
  **PASS (21/21)** — width boundary, exact width/height, range form,
  landscape/portrait, light scheme, `screen` vs `print`, comma OR, negation,
  unknown-feature false, `media` echo, resolution, pointer, and
  add/remove/dispatch/onchange.
- Negative test: forcing `matches` to constant false failed 11 assertions
  (`min-width-true`, `width-exact`, `comma-or`, ...); restored and green
  again.
- `sh tests/nb_all_tests.sh`: **ALL TESTS PASS** (PASS=30 FAIL=0).
- Live browser relaunched (PID `3875779`) on a fresh build.
- Commit `9ad82621f` (`feat: evaluate matchMedia against the real viewport`),
  scoped to `ops/nb_js_worker.c`, `ops/nb_host.h`, the new gate, and the
  runner, on `opencode`.


### 9.24 2026-11-10 — insertAdjacentHTML/Text/Element were absent; now real DOM insertion

`element.insertAdjacentHTML`, `insertAdjacentText`, and
`insertAdjacentElement` did not exist on any element, on the live or parsed
document, or on `Element.prototype`. Real pages reach for them wherever they
want one snippet without replacing the host's children, so the absence was a
hard stop rather than a graceful degradation.

**The slice** (`ops/nb_js_worker.c`)

- All three methods at the four spec positions: `beforebegin`, `afterbegin`,
  `beforeend`, `afterend`. Position matching is ASCII case-insensitive.
- Insertion uses the same `nb_parse_html` and tree helpers that
  `innerHTML`/`appendChild` already use, and the same orphan/detach path, so
  a node never appears in two places.
- `insertAdjacentHTML` parses a fragment and inserts each resulting node in
  source order. `insertAdjacentText` creates a real text node.
  `insertAdjacentElement` moves an existing node, returns it, and returns
  `null` for the no-op cases.
- `beforebegin`/`afterend` on a node with no parent is a no-op (its parent is
  `#document`, which has no siblings), exactly as a browser does.
- An unknown position throws `SyntaxError`. Inserting an ancestor into its
  own descendant throws `HierarchyRequestError` instead of corrupting the
  tree.
- Installed per element wrapper and on `Element.prototype`, so elements made
  by `createElement`/`DOMParser` and feature detection both see them.
- ChildList MutationObserver records are emitted for every inserted node. A
  parsed fragment therefore yields one record per node, where a browser
  batches them into one record with multiple `addedNodes`; both describe the
  same real insertions.
- Not claimed: script execution on `insertAdjacentElement` (the separate
  dynamic `<script>` path in `appendChild` is not duplicated here).

**Verification**

- New gate `tests/nb_insert_adjacent_test.sh`: **PASS (20/20)** — all four
  positions in one tree, uppercase position, text nodes at both ends, element
  move + return value + second move, invalid position for HTML and text,
  detached no-op, detached element returns `null`, ancestor guard, prototype
  presence, and two childList records whose target/added node are correct.
- Negative test: forcing `beforebegin` to append failed `body-order` and
  `elem-moved`; restored and green again.
- `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**, with the new gate running
  in the hermetic group.
- Built from the same source as the run; no browser relaunch was needed
  because the slice only adds element methods (the resident worker gates are
  hermetic and spawn their own fresh worker).

### 9.25 2026-11-11 — sibling/child navigation accessors were missing or uninstalled

Writing the 9.24 gate exposed the gap: `lastChild` had a C function but was
never installed on elements, `previousSibling` did not exist outside the
TreeWalker wrapper, and `parentElement`, `firstElementChild`,
`lastElementChild`, `nextElementSibling`, `previousElementSibling`, and
`childElementCount` were absent entirely. Reads returned `undefined`, which
breaks traversal code without an exception to point at.

**The slice** (`ops/nb_js_worker.c`)

- `lastChild` and `previousSibling` now read from the live C tree; the
  previous sibling is found by walking the parent's child chain, so text
  nodes participate exactly as in a browser.
- `parentElement` returns the parent only when it is an element; the parent
  of `<html>` is `#document`, so it answers `null`.
- `firstElementChild`/`lastElementChild`/`nextElementSibling`/
  `previousElementSibling` skip text nodes; `childElementCount` counts only
  elements and agrees with the existing `children` collection.
- Every getter reads current state on each access, so a later append,
  insert, or removal is visible immediately with no cache.
- Installed per element wrapper, on `Node.prototype` (`lastChild`,
  `previousSibling`, `parentElement`), on `Element.prototype` (the
  element-only five), and on the fragment/ShadowRoot wrapper.

**A separate limitation this gate found, not fixed here**

`nb_parse_html` (used by `DOMParser` and the `innerHTML` setter) still folds
character data into the current element's own `text` field instead of
creating child text nodes. `parseFromString('<div>x<i></i>y</div>')` reports
`childNodes.length` of 1 and `innerHTML` of `<i></i>`; the text survives in
the element's `textContent`, not as nodes. This gate therefore builds its
mixed tree with `createTextNode`/`insertBefore`/`appendChild`, which is the
same shape script-generated DOM has, and the getters are verified against
that. Parsed text nodes are a DOM-model change with renderer impact and are
left as a named open item rather than half-fixed inside a getter slice.

**Verification**

- New gate `tests/nb_sibling_nav_test.sh`: **PASS (27/27)** — text vs element
  siblings in both directions, first/last child across mixed content,
  detached nulls, live append of element then text, fragment accessors,
  parentElement of a text node and of `<html>`, `childElementCount` vs
  `children.length`, and prototype presence on the correct interfaces.
- Negative test: making `previousSibling` read forward and making
  `childElementCount` count text produced **8 failures**
  (`prev-sibling-text`, `text-has-prev`, `count-a`, `count-agrees-children`,
  `live-append-element`, `live-append-text`, `fragment-count`,
  `fragment-prev-sibling`); restored and green again.
- `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**, new gate registered in the
  hermetic group.

### 9.26 2026-11-11 — parsed character data is a real #text node

The 9.25 gate found this: `nb_parse_html` folded every text run into the
current element's own `text` field. `parseFromString('<div>x<i></i>y</div>')`
reported one child, `innerHTML` lost the text, and sibling/child traversal,
Range and cloneNode could not see it. It was true for every script-facing
parse surface.

**The slice**

- `nb_dom.c` parses through one implementation with a `text_nodes` flag.
  `nb_parse_html_nodes` (new export) makes each non-empty run a real child
  `#text` node; text between top-level tags belongs to the parse root, which
  is the fragment `innerHTML`/`insertAdjacentHTML` return. `nb_parse_html`
  is unchanged and still folds.
- Only the script-facing surfaces use the new form: `DOMParser.parseFromString`,
  the `innerHTML` setter, and `insertAdjacentHTML`. The manager's fetch.dom
  extraction keeps the folded form on purpose — the wire format and the row
  projector were built around element-owned text, and changing the live page
  path in the same slice would be a different, much larger change.
- Two pre-existing replacement bugs surfaced by real rows:
  `innerHTML = ...` and `replaceChildren()` cleared children but left the
  element's folded own text, so a LOAD element appeared to keep its old page
  text. Both now drop it; `textContent` setter already did.
- RENDER: a `#text` child contributes a TEXT row whose SEL names its owning
  element (events bubble to it), not a synthetic id on a node no selector can
  match. A caption row (TITLE/LINK/IMG) already carries the element's whole
  `textContent`, so descendant TEXT is suppressed underneath it — otherwise a
  parsed `<a>` rendered `LINK|href text` and then `TEXT|text` a second time.
- Entities decode inside a text node (`a &amp; b` -> `a & b`), and
  `innerHTML` round-trips the mixed content (`x<i>MID</i>y`).

**Verification**

- New gate `tests/nb_parse_text_nodes_test.sh`: **PASS (29/29 + rows)**.
  Covers child count/nodeType, first/last value, innerHTML round-trip,
  textContent, sibling text on both sides, entity decoding, element queries
  still element-only, Range over parsed text, deep clone of a mixed tree,
  `innerHTML`/`insertAdjacentHTML` shapes, folded-text removal, and real
  post-EVAL RENDER rows: `LINK|/x hi`, `SEL|#t` + `TEXT| tail`, no duplicate
  `TEXT|hi`, no stale `TEXT|tick 0`.
- `nb_probe.py` gained an optional `--drain-ms` window because the post-JS
  RENDER can trail the EVAL STATUS frame; default behaviour is unchanged.
- Negative A (make `nb_parse_html_nodes` fold again): **21 assertion
  failures** plus the row assertions (missing `SEL|#t`/`TEXT| tail`).
- Negative B (parser correct, caption suppression removed): every DOM
  assertion passed and the row check failed with exactly
  `duplicate TEXT|hi from the captioned link`.
- Restored and green again; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**
  with the new gate registered.

**Open, found here and not papered over**

- A parsed document's `doc.title` is `undefined`: the live document's title
  accessor reads the global `g_title`, and parsed document wrappers never got
  a title member. Independent of text nodes.
- `textContent = 'x'` still stores folded own text rather than creating one
  Text child, so `childNodes.length` is 0 where a browser says 1.
  `textContent` reads correctly either way.
- Text serialization does not escape `&`/`<` in `innerHTML` output (already
  true for folded text; not introduced here).

### 9.27 2026-11-11 — the viewport scroll surface reads the real box model

The worker had `window.scrollTo`, `scrollY`/`scrollX` and `scrollIntoView`,
but the properties pages read back were missing or CSS-only:
`window.pageXOffset`/`pageYOffset` did not exist, `document.scrollingElement`
did not exist, `documentElement.clientHeight`/`clientWidth` returned an
unstyled `<html>` box (0), and `scrollTop`/`scrollLeft`/`scrollHeight`/
`scrollWidth` were absent on every element. Infinite scroll, lazy loading and
sticky headers therefore compared against `undefined` or 0.

**The slice**

- `window.pageXOffset`/`pageYOffset` are live aliases of the same
  `g_scroll_x`/`g_scroll_y` the worker already moved.
- `document.scrollingElement` is the root element on the live document and
  on parsed documents.
- Root client box is the viewport: `documentElement.clientHeight` is
  `g_viewport_h`; `clientWidth` is `g_col_px`, the same content column the
  layout uses (so it narrows when the document overflows and the gutter
  exists). Non-root `client*` stays the CSS-declared box.
- `documentElement.scrollHeight` is the document height
  `nb_doc_root_h` already computes for the scrollbar decision, raised to
  `clientHeight` as CSSOM requires. An element's `scrollHeight` is
  `nb_content_h`, likewise raised. `display:none` reports 0.
- `documentElement.scrollTop`/`scrollLeft` read and write the viewport
  offset and re-run observer geometry through `nb_obs_recheck`, so
  `getBoundingClientRect` and IntersectionObserver see the move. Other
  elements get per-node worker-local offsets, clamped at zero, exactly the
  way `window.scrollTo` is worker-local: the renderer exposes no channel to
  scroll an inner pane.
- `<body>` is deliberately NOT the scrolling element. In this
  standards-mode document `body.scrollHeight` is its own content height and
  `body.scrollTop` is a per-node value, so the legacy body alias cannot
  hijack the viewport. Code that insists on the body alias needs
  `documentElement` or `window`.
- Widths report the client box for both root and elements. This renderer
  wraps content rather than scrolling it sideways, so no horizontal extent
  is claimed; `scrollWidth == clientWidth` here is the renderer's truth,
  not a measurement that was skipped.
- Found while gating: `css_hidden` short-circuited on `g_css == NULL`, so a
  page with no stylesheet did not count inline `display:none`. It now calls
  `nb_css_hidden` unconditionally (`nb_css_resolve` is NULL-safe), which
  also corrects `offsetWidth`/`offsetHeight`/`getBoundingClientRect` for
  that case.

**Verification**

- New gate `tests/nb_scroll_metrics_test.sh`: **PASS (39/39)**. Starts at
  0/0, then `scrollTo(0,120)` moves `scrollY`, `pageYOffset`,
  `documentElement.scrollTop` and the observed rect by exactly 120;
  `documentElement.scrollTop = 45` writes through; `body.scrollTop = 7`
  stays on the body and does not move the viewport; negative offsets clamp;
  the options form moves both axes; an inner `<div style=height:100px>`
  reports `clientHeight 100`, `scrollHeight 100` and independent
  `scrollTop`/`scrollLeft`; `display:none` reports 0; growing the document
  from one paragraph to 26 raises `documentElement.scrollHeight` from 600
  (the client minimum) to 780; both prototypes carry the surface.
- Negative A (force the root-scroller test false): **11 failures** —
  `root-client-*`, `short-doc-scroll-height`, `scrollto-root-top`,
  `set-root-top`, `body-top-does-not-scroll`, `negative-clamps`,
  `options-root-left`, `grown-client-height`.
- Negative B (restore the `g_css` short-circuit): exactly
  `hidden-scroll-height got=30 want=0`, and nothing else.
- Restored and green again; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**
  with the new gate registered.

**Open, kept honest**

- Non-root scroll offsets are not clamped to `scrollHeight - clientHeight`
  because the engine has no per-element overflow model; they clamp at zero
  only. A page that over-scrolls reads back the value it set.
- `scrollWidth` does not model deep horizontal overflow (see above).
- `body.clientHeight` remains the CSS-declared box (0 for an unstyled body),
  the same pre-existing limitation as every other auto-height element.
- `pageXOffset`/`pageYOffset` are getters only, matching `scrollY`/`scrollX`.

### 9.28 2026-11-11 — textContent assignment authors a real Text node

9.26 made parsed character data a real `#text` child, but the script-facing
setter still wrote one C string onto the element (`n->text`). So
`el.textContent = 'x'` left `childNodes.length` at 0, `firstChild` at null,
`cloneNode` with nothing to copy, `Range` unable to address the text, and a
childList MutationObserver silent — for the single most common text update a
page performs.

**The slice**

- An element (or fragment) setter now clears its children and, when the
  value is not the empty string, appends exactly one real Text node. Only
  `null` maps to `""`; `undefined` becomes `"undefined"`, `0` becomes
  `"0"`, per DOMString conversion. Markup in the value stays literal text —
  this is `textContent`, not `innerHTML`.
- A Text node's own `textContent` replaces its `data` and adds no child;
  `Document` is a no-op, as in a browser.
- The old folded `n->text` is freed and cleared, so a LOAD element's page
  text cannot survive the assignment (the same class of stale-text bug
  innerHTML=/replaceChildren() had).
- The added Text node raises one real childList record (`addedNodes` holds a
  nodeType-3 node). Removed children are not reported: one record carries a
  single removed node and a consumer could retain it after its C node is
  gone. The added record is true on its own; the omission is documented.
- RENDER already emits a TEXT row for a text child under its owner's SEL
  (9.26), so the authored text renders once with no extra work.

**Verification**

- New gate `tests/nb_text_content_test.sh`: **PASS (28/28 + rows)**.
  Covers the folded starting state, one-child shape, nodeType/data/owner,
  old text dropped, element-children replacement, `''`/`null`/`undefined`/`0`,
  literal markup, Text-node self-assignment, clone, a Range inside the new
  node, the childList record, and post-EVAL rows `SEL|#t` + exactly one
  `TEXT|observed` with no stale `TEXT|tick 0`.
- Negative A (folded setter restored): the gate fails at the structural
  assertions — `p.firstChild` is null, so the probe aborts before the rest.
- Negative B (always author a node, even for `""`): exactly
  `empty-string got=1: want=0:` and `null-is-empty got=1: want=0:`.
- Negative C (drop the childList record): 5 failures, all in the observer
  block.
- Restored and green again; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**
  with the new gate registered.

**Open, found here**

- `document.createDocumentFragment()` does not exist at all (only
  `template.content` and shadow roots make fragments). Found while probing
  fragment assignment; not added in this slice.
- Fragment wrappers have no `textContent` accessor, so the C setter works on
  the node but script cannot reach it through `template.content` /
  shadow root. Needs the wrapper pair.
- `document.textContent` still concatenates the document instead of
  returning null (pre-existing getter behaviour, untouched).
- `innerHTML` serialization still does not escape `&`/`<` (pre-existing).

### 9.29 2026-11-11 — DocumentFragment is a real, movable node

`document.createDocumentFragment()` did not exist: only `<template>.content`
and shadow roots produced fragments. The shared insert paths also had no
fragment step, so the most common templating move — build a fragment, fill
it, append it once — could not run at all. A fragment that did reach
`appendChild` would have been stored as a child node the DOM tree can never
hold.

**The slice**

- `document.createDocumentFragment()` and the `DocumentFragment` global
  constructor return a detached `#document-fragment` node (nodeType 11),
  kept on the orphan list so teardown still frees it.
- The fragment wrapper now carries `textContent` (get/set), `append`,
  `prepend`, `replaceChildren` and `contains` in addition to the existing
  insert/query/child getters.
- Inserting a fragment **moves its children** into the target in order and
  leaves the fragment empty: `appendChild`, `insertBefore`, `replaceChild`,
  and the ParentNode helpers (`append`/`prepend`/`replaceChildren`) all
  expand it. `appendChild`/`insertBefore` return the fragment (the spec
  value); `replaceChild` returns the replaced child.
- A fragment is never inserted as a node, so the renderer and tree walkers
  cannot meet nodeType 11 in the tree.
- `push_node` now returns the real cached fragment wrapper for a fragment
  node (and reports nodeType 11 on the generic path), so a child's
  `parentNode` **is** the fragment the page holds and
  `parentNode.nodeType === 11` is finally true — the canonical fragment
  check.
- `appendChild`/`insertBefore`/`replaceChild` emit one childList record per
  moved child. The ParentNode helpers stay record-free, matching their
  existing behaviour for ordinary nodes.

**Verification**

- New gate `tests/nb_document_fragment_test.sh`: **PASS (47/47 + rows)**.
  Covers the factory/constructor, fresh shape, fragment-scoped
  `getElementById` vs the live document, `textContent` get/set,
  `appendChild`/`insertBefore`/`replaceChild` expansion and ordering,
  empty-fragment no-op, all three ParentNode helpers, parent identity +
  `contains`, one observer record per moved child, clone shape/identity, and
  a post-EVAL frame with `TEXT|rtext`/`TEXT|visone`/`TEXT|vistwo` emitted
  exactly once and no fragment node row.
- Negative A (drop the `appendChild` fragment branch): 14 failures
  (`move-host-count got=1`, `move-fragment-empty got=2`, order becomes
  `#document-fragment,...`, observer sees one record).
- Negative B (drop the fragment `textContent` accessor): 4 failures.
- Negative C (drop fragment expansion from the ParentNode helper): 6
  failures across append/prepend/replaceChildren.
- Negative D (drop wrapper identity caching): exactly
  `parent-identity got=false want=true`.
- Restored and green; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS** with
  the new gate registered.

**Honest limits**

- Scripts moved inside a fragment are not executed (the fragment branch
  returns before the direct-child script fetch/execute path). Direct
  `appendChild(script)` still executes as before.
- `#shadow-root` is not treated as a movable fragment; only
  `#document-fragment`.
- Parsed documents (`DOMParser`) still have no `createDocumentFragment`
  factory, and no `createElement`/`createTextNode` either.
- `DocumentFragment` has no `ownerDocument`/`isConnected` surface yet.

### 9.30 2026-11-11 — raw-text elements keep their content

The parser kept the raw-text ELEMENT but threw its body away: `parse_open`
advanced past the closing tag and stored nothing. `getElementById` still
found a `<script>`, so the gap was invisible until a page read it — which is
exactly what every modern framework does. `document.getElementById(
'__NEXT_DATA__').textContent` was empty, `<script type="application/json">`
data islands were empty, `<style>` rules were unreachable from a page, and
`innerHTML`/`DOMParser` inherited the same hole. `<title>` text only existed
in the manager's separate regex extraction, and `<textarea>` content was
parsed as markup.

**The slice**

- `parse_open` now captures the source text between the open and closing tag
  instead of skipping it, on **both** parse paths:
  - folded (`nb_parse_html`, the manager's fetch.dom input): stored on the
    element's own `text` field, exactly like folded prose;
  - tree (`nb_parse_html_nodes`, used by `innerHTML`, `insertAdjacentHTML`,
    `DOMParser`): authored as one real `#text` child (nodeType 3).
- `title` and `textarea` are RCDATA: character references decode
  (`A &amp; B` -> `A & B`). `script`/`style`/`noscript` are raw text:
  `&amp;` and `<` stay literal and never become markup.
- `textarea` joined the raw-text set (it was previously parsed as HTML).
- The renderer now prunes `script`, `style` and `noscript` subtrees at the
  top of `dom_walk_render`, so captured data can never become a visible
  TEXT row now that fetch.dom carries it.
- The wire serializer needed no change: it already length-prefixes text, and
  the round-trip test proves the data island survives fetch.dom.

**Verification**

- New gate `tests/nb_raw_text_test.sh`: **PASS (15/15 folded + 17/17 probe
  + rows)**. It compiles a C harness against `nb_dom.c` for the folded path
  and drives the real worker for the tree path:
  - folded: script/style text exact; title entities decoded; noscript raw
    with no `<p>` child; textarea RCDATA with no `<b>` child; serialize ->
    `nb_dom_load` round-trip keeps the script text.
  - tree: LOAD `script#sc` textContent, `body.textContent` includes it,
    `innerHTML` gives one Text child (nodeType 3) and serializes the script
    body raw; style; title decode; textarea RCDATA; noscript raw; DOMParser
    script/style text.
  - rows: visible `TEXT|tick 0` and a `TITLE|` row are present; no script,
    style or noscript body leaks into RENDER.
- Negative A (restore the old skip): **7 folded checks fail**, including
  `folded-script-text got=[(null)]`.
- Negative B (remove the renderer prune): rows leak
  `TEXT|if (a < b && c > d) { go(); }` under `SEL|#sc`.
- Negative C (don't decode title): exactly `folded-title-entities` and
  `ih-title-entities` fail.
- Restored and green; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS** with the
  new gate registered.

**Honest limits**

- The closing-tag scan is still the simple `</tag>` form: `</script >` with
  whitespace is not recognised (pre-existing).
- A literal `</script>` inside a JS string ends the element early — the HTML
  parser does the same, so this matches browsers.
- `xmp`, `iframe`, `noembed`, `noframes` and `plaintext` are not captured yet
  (not in `is_rawtext`).
- Comments are still skipped, and captured script/style text makes fetch.dom
  larger by the size of a page's inline JS/CSS.

### 9.31 2026-11-11 — innerHTML is HTML serialization; outerHTML can write

The serializer behind `innerHTML`, `outerHTML` and `XMLSerializer` emitted
text verbatim. A browser escapes it, and pages depend on that: the canonical
`el.innerHTML = el.innerHTML` (template extraction, sanitizers, editors)
re-parsed raw user text as markup. Void elements also gained end tags, and
`</br>` re-parses as `<br>`, so a round-trip silently added a line break.
`outerHTML` was a getter only — `el.outerHTML = ...` did nothing, while
libraries use it to replace a node in place.

**The slice**

- Text in HTML mode escapes `&` -> `&amp;`, `<` -> `&lt;`, `>` -> `&gt;`
  and U+00A0 -> `&nbsp;` (both the element's folded text and `#text`
  children).
- Raw-text elements (`script`, `style`, `xmp`, `iframe`, `noembed`,
  `noframes`, `plaintext`, `noscript`) keep their data literally, matching
  the parser. `title` and `textarea` are RCDATA, so their text IS escaped
  (and round-trips through the parser's entity decode).
- Void elements (`area`, `base`, `br`, `col`, `embed`, `hr`, `img`, `input`,
  `link`, `meta`, `param`, `source`, `track`, `wbr`) no longer get an end
  tag. `<br><img src="/x"><input type="text">` round-trips byte-for-byte.
- `XMLSerializer` runs the same walker in XML mode: it escapes `&`/`<`/`>`
  but not U+00A0 (XML has no `&nbsp;` entity) and keeps end tags for void
  elements (`<br></br>`, well-formed XML).
- `outerHTML` is now writable: the value is parsed in the parent's context
  and replaces the element with the parsed nodes, in order. The replaced
  node is detached and orphaned (its wrapper stays valid); assigning `null`
  removes it; assigning text inserts an escaped-on-read text node. A node
  with no parent, or whose parent is the `#document`, throws
  `NoModificationAllowedError`.
- Replacement emits one childList record per moved node and then one for the
  removed node, matching the per-node record model of the tree mutators.

**Verification**

- New gate `tests/nb_html_serialize_test.sh`: **PASS (36/36 + rows)**.
  Covers escaped text + NBSP, parse/serialize/parse stability, raw
  script/style, escaped title/textarea, all three void cases and a stable
  `<br>` round-trip, `outerHTML` getter, single/multi/text/null replacement
  and ordering, detached/documentElement errors, the `+,+,-` record shape
  with the removed node identity, XML escaping/no-`&nbsp;`/void end tag, and
  a post-JS RENDER with `TEXT|newvis` exactly once under `SEL|#newvis` and
  no stale `#t`.
- Negative A (no escaping): **8 failures** (`esc-text`, `esc-nbsp`,
  `roundtrip-html`, `roundtrip-stable`, both RCDATA checks, `oh-text-escaped`,
  `xml-amp`).
- Negative B (always close void elements): **5 failures** (`void-br`,
  `void-img`, `void-input`, `void-markup`, `void-roundtrip-markup`).
- Negative C (read-only outerHTML): **13 failures** across the setter, error
  and record checks.
- Negative D (escape raw-text bodies too): exactly `raw-script` and
  `raw-style` fail.
- Restored and green; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS** with the
  gate registered.

**Honest limits**

- Attribute blobs are emitted as stored, not re-escaped. Parser/load blobs
  are source markup, so they are already correct; a value set with
  `setAttribute('title', 'a & b')` appears raw (valid here), but a value that
  contains `"` or entity-like text can round-trip differently from a
  browser. Attribute escaping is its own slice.
- `outerHTML` nodes parsed from a fragment do not execute `<script>` (same
  limitation as fragment insertion).
- `outerHTML` remains an Element accessor; Text/Comment wrappers do not get
  it.
- The parser recognises only the plain `</tag>` closing form; `</script >`
  remains uncaptured (pre-existing).

### 9.32 2026-11-11 — attribute values are markup-safe; entities are UTF-8

9.31 left one documented hole. InnerHTML/outerHTML emit the attribute blob
verbatim (correct, because parser blobs are already source markup), but
`setAttribute` stored its value raw. A value containing `"` terminated the
quoted attribute and broke the blob itself; a value containing `&` could
start an entity on the next parse; `getAttribute` then disagreed with the
serialized markup. On top of that, `decode_entities` mapped `&nbsp;` to a
plain space and `&mdash;`/`&copy;` to single Latin-1 bytes, so the
serializer's own `&nbsp;` escape did not round-trip and named entities
produced invalid UTF-8.

**The slice**

- `attrs_set` (the single attribute-blob writer, used by `setAttribute`,
  `dataset`, `img.src`, auto ids and now the `id`/`className` properties)
  escapes values in HTML attribute mode: `&` -> `&amp;`, `"` -> `&quot;`,
  U+00A0 -> `&nbsp;`. `<` and `>` stay literal, per spec. The blob is
  therefore always valid markup, so the serializer keeps emitting it
  verbatim and parser-sourced attributes are never double-escaped.
- `el.id = ...` and `el.className = ...` now update the attribute blob as
  well as the selector field, so `innerHTML`/`outerHTML` show the new value
  (`el.id=''` serializes `id=""`, like a browser).
- `decode_entities` now decodes named references to their real code points
  and emits UTF-8 through a shared encoder: `&nbsp;` is U+00A0 (two bytes),
  `&mdash;` U+2014, `&ndash;` U+2013, `&hellip;` U+2026, `&copy;` U+00A9,
  `&reg;`, `&times;`, `&divide;` correct; numeric references are UTF-8
  encoded with NUL/surrogates/out-of-range mapped to U+FFFD (HTML input
  stream rules). An entity is at least three source bytes and emits at most
  four, so existing `n*4`/1200-byte caller buffers remain safe.
- Everything is byte-for-byte stable across
  `outerHTML -> innerHTML -> outerHTML` and `getAttribute` returns the
  original string before and after the round-trip.

**Verification**

- New gate `tests/nb_attrs_serialize_test.sh`: **PASS (34/34 + rows)**.
  Covers escaping of `&`/`"`/NBSP and literal `<`/`>`, literal `&amp;`
  surviving as `&amp;amp;`, all five values round-tripping through a
  reparse, neighbour preservation and `removeAttribute`, `dataset`,
  `id`/`className` serialization and empty-string values, parser-sourced
  attributes keeping their source escapes with decoded getters, numeric
  references, and `&nbsp;`/`&mdash;`/`&copy;`/numeric decoding to the exact
  code points (`97,160,98,32,8212,32,169,32,8212,32,65`) with a stable
  serialize/reparse loop. The RENDER frame shows `TEXT|A & B` once under
  `#t`, with no entity text.
- Negative A (store values raw again): **12 failures**, including
  `set-quote-get got=say` (blob truncated), `set-literal-get got=&` and
  every unescaped outer check.
- Negative B (previous decoder): **8 failures** — NBSP decodes to space
  (32, not 160), numeric `&#8212;` becomes U+FFFD, and text codes read
  `97,32,98,32,65533,...`.
- Negative C (no id/className blob sync): exactly the four serialization
  checks fail, including `empty-id-class got=<b></b>`.
- Negative D (escape `&`/NBSP but not `"`): **5 failures**, all quote
  related.
- Restored and green; `sh tests/nb_all_tests.sh`: **ALL TESTS PASS** with the
  gate registered.

**Honest limits**

- Parser-sourced attribute blobs are still emitted as authored, not
  canonicalised: a raw `&` in the fetched source stays raw (a browser would
  write `&amp;`), and single quotes/valueless attributes keep their source
  form. `setAttribute` values are canonical.
- Named references outside the small table (`&euro;`, `&middot;`, ...) stay
  literal; the table is the pre-existing house subset.
- Attribute names are not validated, so `setAttribute('a b', ...)` can still
  create a malformed blob.
- Numeric references still accept values above U+10FFFF but clamp them to
  U+FFFD rather than implementing the full HTML replacement table.

### 9.33 2026-11-12 — parsed DOMParser documents have a Document surface

`DOMParser.parseFromString` returned a `push_node()` wrapper: element-scoped
queries worked, but the wrapper had no Document members. The host installs
`document.title` on the live global only, so a parsed document's title read
`undefined`, and `createElement`/`createElementNS`/`createTextNode`/
`createDocumentFragment` were missing entirely. `document.textContent` also
read `undefined` on the live host object and, for parsed documents, fell
through to the element getter and concatenated the whole tree instead of
returning `null` as the Document spec requires.

**The slice**

- `document.textContent` is now `null` for Document nodes: the element/Node
  getter returns `JS_NULL` for `#document` wrappers, and the live host
  `document` (a plain object with no `NODEKEY`) gets an own accessor that
  reads `null` and treats assignment as a no-op.
- Parsed document wrappers install `title` as a getter/setter: the getter
  returns the first `<title>` element's text content (`&nbsp;` decodes to
  U+00A0 on the parse path), the setter replaces that element's content. If
  no `<title>` exists, the setter creates one under `<head>`; `<head>` is
  synthesized only under an `<html>` root element (prepended so it stays
  first). For a fragment-like parse (`<div>x</div>`) with no `<html>` root,
  the `<title>` is attached to the document root instead of being inserted
  inside the fragment root, so the fragment content is left intact.
- `createElement`, `createElementNS`, `createTextNode`, and
  `createDocumentFragment` are installed on parsed document wrappers and on
  the live document through a shared factory helper. Nodes created by a
  parsed document append into that parsed tree, serialize through
  `innerHTML`/`outerHTML`, and remain detached from the live rendered page.
- `textContent` on a `Text` node still replaces its data (the setter's
  `!n->tag` character-data branch runs before the Document no-op).

**Verification**

- New gate `tests/nb_parsed_doc_test.sh`: **PASS (37/37 + rows)**. Covers
  live and parsed `document.textContent === null` and assignment no-op,
  parsed `nodeType`/`documentElement`/`body`, `doc.title` reading RCDATA
  entities, all four factories (plus `createElementNS`), created element and
  text node types, appending created nodes/text/fragments into the parsed
  body with exact `innerHTML`, `getElementById` over nodes created by the
  parsed document, title replacement and clearing, `<head>` synthesis only
  under `<html>`, fragment-like root preservation, and a document consisting
  only of `<title>`. The post-JS RENDER frame keeps the live `TEXT|tick 0`
  and `TITLE|Attrs` rows and contains no parsed-document text.
- Negative A (no Document `textContent` null case): `parsed-tc-null got=false`.
- Negative B (title setter does not replace old content): **5 failures**
  including `title-set got=P TNew & Title`.
- Negative C (no live `document.textContent` accessor): **2 failures**,
  `live-tc-null` and `live-tc-set-noop`.
- Negative D (synthesize `<head>` under any root): `frag-root-intact`
  `got=<head><title>Frag</title></head>x want=x`.
- Correct `Document.textContent === null` exposed that `Range`
  `cloneContents`/`extractContents` returned a `#document` root instead of a
  `DocumentFragment`; they now return a real fragment, and
  `nb_range_selection_test.sh` is green again.
- Restored and green; full suite **ALL TESTS PASS** with the gate
  registered.

**Honest limits**

- `doc.title` setter replaces content but does not remove an extra existing
  `<title>` element beyond the first it finds.
- Parsed documents still lack several Document members (`implementation`,
  `documentURI`/`URL`, `contentType`, `compatMode`, `doctype`,
  `createComment`, `createAttribute`, `importNode`, `adoptNode`).
- `createElement`/`createTextNode` do not set `ownerDocument` (not tracked
  on house nodes).
- Live `document.title` remains the host `g_title` accessor; setting it does
  not rewrite the `<title>` element node in the live DOM.

### 9.34 2026-11-12 — live document.title syncs with the <title> element

The host mirrored `document.title` in `g_title` only. `dom_render_rows`
prefers the `<title>` element's text over `g_title`, so a JS
`document.title = "x"` updated `g_title` but left the element (and the
rendered `TITLE` row) unchanged, and the getter could disagree with the DOM.

**The slice**

- `native_get_title` (host) returns the `<title>` element's text content when
  one exists in `g_dom_root`, else `g_title`.
- `native_set_title` keeps `g_title`/`g_title_set` in sync for the no-DOM
  fallback and, when `g_dom_root` exists, updates the `<title>` element's
  text (clearing children so folded and tree shapes agree). If no `<title>`
  element exists, it creates one under `<head>`; `<head>` is synthesized only
  under an `<html>` root, otherwise the title is attached to the document
  root.
- `nb_dom.h` is now included by `nb_host.h` and `g_dom_root` lives in the
  host global state so the host natives can reach the live DOM. Small local
  tree helpers (`nbh_find_tag`, `nbhsb_node_text`, `nbh_clear_children`,
  append/insert-before) implement the DOM writes with the public `NbNode`
  layout and `nb_node_free`.

**Verification**

- New gate `tests/nb_live_title_test.sh`: **PASS (13/13 + 6/6 + rows)**.
  Case 1: getter reads element text "Orig" (not the LOAD `g_title`), setter
  updates getter, the `<title>` element and the RENDER `TITLE` row (including
  empty string), and after removing the title element the setter recreates it
  under `<head>` so the final RENDER shows `TITLE|Created` once with the body
  unchanged. Case 2: a page with no `<title>` element starts empty, and the
  setter creates `<title>` under `<head>` and renders `TITLE|Made`.
- Negative A (getter ignores the element): `get-initial got=live:fixture
  want=Orig`.
- Negative B (setter does not update the DOM): **9 failures** in case 1 and
  **3** in case 2, including `el-after-set got=Orig want=Changed` and missing
  rendered title.
- Negative C (setter does not create a missing title): **3 failures** in each
  case (`created-exists`/`made-exists` false).
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- The live `document.title` setter replaces content in the first `<title>`
  element it finds; it does not deduplicate multiple `<title>` elements.
- The getter returns the element's concatenated text content; the LOAD
  `--title` argument is authoritative only when no `<title>` element exists.

### 9.35 2026-11-13 — permissive raw-text/RCDATA end tags

Raw-text and RCDATA elements (`script`, `style`, `noscript`, `title`,
`textarea`) were closed only when the end tag had an immediate `>` after the
tag name. HTML allows whitespace (space, tab, LF, FF, CR) before `>`, so
`</script >`, `</style\t>` and `</TITLE >` were not recognized: the parser
kept scanning to EOF and the element swallowed following markup as data.

**The slice**

- `skip_to_close` in `nb_dom.c` now accepts optional HTML whitespace between
  the end-tag name and `>` (case-insensitive tag name unchanged). Content
  boundaries and the returned parse position account for the consumed
  whitespace. Applies to both parse paths (folded fetch parse and the
  tree/innerHTML parse) because both share `skip_to_close`.

**Verification**

- New gate `tests/nb_rawtext_close_test.sh`: **PASS (12/12)**. Covers
  `<script>` closed by space, tab and newline (including upper-case close
  tag name), `<style>` space close, RCDATA `<title>` with entity decoding
  (`Hi &amp; bye` -> `Hi & bye`) and a preserved sibling, and `<textarea>`
  keeping literal content (`<b>&amp;</b>` -> `<b>&</b>` as RCDATA-decoded
  text) with a following sibling.
- Negative (require immediate `>`): **10 failures** (every whitespace-close
  case fails; the element consumes the trailing sibling/content).
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- Only the standard HTML whitespace set is accepted before `>`; attributes or
  other junk in an end tag are still not recognized (matching the prior
  behavior).
- `xmp`, `iframe`, `noembed`, `noframes`, `plaintext` are still not treated
  as raw-text elements by the parser.

### 9.36 2026-11-13 — xmp/iframe/noembed/noframes/plaintext parsed as raw text

`xmp`, `iframe`, `noembed`, `noframes`, and `plaintext` were parsed as normal
elements, so markup inside them became child elements and entities decoded.
In HTML they are raw-text elements: content is data until the matching end tag
(`plaintext` has no end tag and consumes the rest).

**The slice**

- `is_rawtext` in `nb_dom.c` now includes `xmp`, `iframe`, `noembed`,
  `noframes`, `plaintext`; `parser_add_raw_text` stores their bodies as raw
  text (no entity decoding), on both parse paths. `skip_to_close` already
  handles permissive end-tag whitespace (9.35); `plaintext` consumes to EOF.
- Serializer `nb_ser_raw_tag` already listed these tags, so `innerHTML`
  round-trips their literal content. The renderer prunes these subtrees (like
  script/style/noscript) so raw embedded markup never becomes visible `TEXT`
  rows.

**Verification**

- New gate `tests/nb_rawtext_elements_test.sh`: **PASS (32/32 + rows)**.
  For `xmp`/`iframe`/`noembed`/`noframes`: markup stays literal
  (`<b>&amp;</b>` not parsed), no entity decoding, one text child, closing tag
  preserved, following sibling intact; permissive close (`</XMP >`) works;
  `plaintext` consumes the rest of the fragment; `innerHTML` reparse is
  stable. RENDER (folded wire form) leaks none of the raw bodies and keeps
  `TEXT|tick 0`.
- Negative (elements not treated as raw text): **13 failures** in the JS
  assertions (markup parsed/entities decoded/structure differs) while render
  remained non-leaking for that fixture.
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- `xmp` content is no longer rendered as visible text (pruned with other
  raw-text containers); DOM `textContent`/serialization are correct.
- `plaintext` always consumes to end-of-input on parse; there is no way to
  close it.
- `noscript` remains in script-enabled/raw-text form (content captured as
  data, not as rendered fallback markup).

### 9.37 2026-11-13 — cheap document metadata on live and parsed documents

Live and `DOMParser` documents lacked basic Document properties, so libraries
probing `contentType`, `URL`/`documentURI`, `compatMode`, encoding fields,
`doctype`, or `implementation` saw `undefined`.

**The slice**

- `nb_install_doc_metadata` sets `contentType`, `URL`, `documentURI`,
  `compatMode` (`"CSS1Compat"`), `characterSet`/`charset`/`inputEncoding`
  (`"UTF-8"`), `doctype` (`null`), and an `implementation` object
  (`createHTMLDocument`, `createDocument`, `createDocumentType`,
  `hasFeature`) on a document wrapper.
- `DOMParser.parseFromString` installs metadata with the validated MIME type
  and the inert URL `about:blank`. The live document installs metadata with
  `text/html` and `g_href` (page URL), replacing the previous ad-hoc
  implementation-only setup.

**Verification**

- New gate `tests/nb_doc_metadata_test.sh`: **PASS (26/26)**. Parsed
  `text/html` and `application/xml` documents report correct
  `contentType`/`URL`/`documentURI`/`compatMode`/`doctype`/encoding fields and
  a working `implementation`; the live document reports `text/html`, the page
  href, `CSS1Compat`, `null` doctype, `UTF-8`, and a working implementation.
- Negative (no metadata installed): probe fails to complete
  (`TypeError` on missing `implementation`), so the gate fails.
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- `doctype` is always `null` (the parser does not build DocumentType nodes).
- Parsed document `URL`/`documentURI` is always `about:blank`; `compatMode`
  is always `CSS1Compat` regardless of parse mode.
- `implementation` methods reuse the existing minimal document/doctype
  constructors; they are not a full DOMImplementation surface.

### 9.38 2026-11-13 — ownerDocument propagation (live + parsed documents)

**What changed**

- `nb_node_ownerDocument` getter in `ops/nb_js_worker.c`: Document nodes
  (including the live global `document`) return `null`. For in-tree nodes the
  getter walks to the top root: `g_dom_root` maps to the live global
  `document`, a parsed `#document` root maps to its parsed document wrapper.
  Detached nodes report the creating document recorded at creation time
  (`__nb_owner`); fallback is the live global document.
- `Node.prototype` exposes `ownerDocument`; `make_fragment_wrapper` installs
  the getter on plain fragment wrappers.
- `createElement`/`createElementNS`/`createTextNode`/`createDocumentFragment`
  record the creator document as owner when invoked on a recognized document
  (live global document, parsed `#document` wrapper, or a contentType-bearing
  document-like object).
- Live host `document` explicitly sets `ownerDocument = null`.

**Verification**

- New gate `tests/nb_owner_document_test.sh`: **PASS (18/18)**. Live document
  is `null`; live-tree elements and nodes created by live factories report the
  live document. Parsed `DOMParser` documents are `null`; elements in parsed
  trees and nodes created by parsed factories/fragments report the parsed
  document; different parsed documents have distinct ownership.
- Negative probe (`document.ownerDocument === document`) returns `false`
  (correctly `null`), so the `null`-for-document contract is observable.
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- No `importNode`/`adoptNode` API yet. In-tree ownership follows the current
  tree root (a parsed-created node inserted into the live tree will report the
  live document as owner), which is not a substitute for explicit adopt/import
  semantics or cross-document insertion errors.
- Parsed document trees from `nb_dom_load`/wrappers are identified by a
  `#document` top root; nodes without a recorded creating document and without
  a document root fall back to the live global document.
- `DocumentFragment` wrappers are not `Node.prototype` instances; the getter
  is installed directly on them.

**9.38 follow-up (import/adopt blocker fix, 2026-11-13)**

- `ops/nb_js_worker.c`: implemented `Document.importNode(node, deep)` and
  `Document.adoptNode(node)` on live and parsed documents. `importNode` clones
  (shallow/deep, including fragments) into the target document and records
  target ownership on the clone root; `adoptNode` detaches an attached node and
  retargets its owner to the target document (detached-subtree root owner
  propagates to descendants). Both throw `NotSupportedError` for document
  nodes.
- `nb_node_ownerDocument` now also inherits detached-subtree owner from the
  detached root wrapper, so imported/adopted subtree descendants report the
  correct owner without per-node wrapper stamping.
- Gate `tests/nb_import_adopt_test.sh`: **PASS (22/22)** (shallow/deep import,
  cross-document import, adopt detached/attached, subtree ownership, fragment
  import, NotSupportedError). Negative probe confirms imported node is not
  owned by source document. Full suite **ALL TESTS PASS**.

### 9.39 2026-11-13 — comment nodes (createComment, parse/preserve, serialize)

**What changed**

- `ops/nb_js_worker.c`: added `document.createComment(data)` on live and
  parsed documents. Comment wrappers report `nodeType` 8, `nodeName`
  `#comment`, `nodeValue`/`data`/`textContent` as comment data (settable),
  correct `ownerDocument`, and can be inserted/removed like character data.
- Comment nodes serialize in HTML as `<!--data-->` (innerHTML/outerHTML/XML
  serializer path) and are skipped by the renderer walk and excluded from
  element `textContent` concatenation. `node_text_content`/`push_node`/
  `nodeValue`/`data`/`textContent` paths treat `#comment` as character data.
- `nb_dom.c`: the node-mode parser (`nb_parse_html_nodes`, used by
  `DOMParser`/`innerHTML`) now preserves `<!-- -->` as `#comment` nodes with
  their content; the folded extraction parser still skips comments.

**Verification**

- New gate `tests/nb_comment_node_test.sh`: **PASS (20/20)**. Covers
  `createComment`, nodeType/name/value/textContent get/set, append/owner,
  innerHTML serialization, comments excluded from `textContent`, parsed
  comments preserved (`innerHTML`/`textContent`/child type), and
  clone/import of comment nodes. Render leak check: no harness errors and no
  comment content leaked into render frames.
- Negative probe: `div.innerHTML='a<!--b-->c'` yields `textContent==='abc'`
  **false** and `textContent==='ac'` **true** (comment excluded).
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- Comments are preserved only in node-mode parsing (`nb_parse_html_nodes` /
  DOMParser/innerHTML); the folded `nb_parse_html` fetch/extraction path still
  skips comments.
- Comment data is not escaped/sanitized for `--` sequences in serializer
  output (emitted as stored), and malformed comment endings are not
  normalized.
- No `createComment`-specific wire-format/parser guarantees beyond node-mode
  tree parsing and in-memory DOM operations.

### 9.40 2026-11-13 — parser attribute canonicalization in HTML serialization

**What changed**

- `ops/nb_js_worker.c`: HTML serializer now canonicalizes parser/load
  attribute blobs (`n->attrs` in tag-prefix/raw form) by parsing attributes and
  re-emitting them with decoded-then-escaped values (`name="..."`, bare attrs
  preserved when valueless). Raw `&` and source entity forms are serialized
  safely without changing attribute semantics.
- Normalized blobs produced by `setAttribute`/`attrs_set` remain emitted as
  stored (already escaped), avoiding double-escaping.
- `nb_dom.c`/`nb_dom.h`: exposed `nb_decode_entities` for shared entity
  decoding used by canonicalization.

**Verification**

- New gate `tests/nb_attr_canonical_test.sh`: **PASS (20/20)**. Parser-sourced
  attributes with `&amp;`, raw `&`, quoted values, bare attributes, and empty
  values serialize safely/canonically while preserving `getAttribute`/presence
  semantics; `setAttribute`-written values remain correctly escaped with no
  double-escaping.
- Negative probe: parsed `data-b="a&b"` no longer serializes with a raw
  `a&b"` sequence (`LOG|NEG false`).
- Restored and green; full suite **ALL TESTS PASS** with the gate registered.

**Honest limits**

- Canonicalization applies to parser-form blobs; normalized blobs are trusted
  as stored. Attribute ordering/spacing may normalize for parser-form output.
- Present-empty `getAttribute` semantics were pre-existing at this slice and
  corrected in 9.41 (now returns `""` when present, `null` only when absent).


### 9.41 2026-11-13 — present-empty getAttribute semantics

**What changed**

- `ops/nb_js_worker.c`: `element.getAttribute(name)` now returns `null` only
  when the attribute is absent; for present attributes (including empty-valued
  attributes such as `data-empty=""`) it returns the attribute value string
  (possibly `""`), matching DOM semantics. Presence is checked via
  `attrs_has`/attribute lookup rather than treating empty decoded values as
  absent.

**Verification**

- Updated `tests/nb_attr_canonical_test.sh`: **PASS (20/20)** with
  `getAttribute('data-empty') === ""` and `hasAttribute('data-empty') === true`.
- `tests/nb_attrs_serialize_test.sh`: **PASS (34/34 + rows)** (absent-after-
  remove still `null`).
- Negative probe (fresh worker): `hasAttribute(missing)=false`,
  `getAttribute(missing)=null`, `getAttribute(a)=""` for `a=""`.
- Full-suite evidence: targeted/negative gates above passed on a fresh build;
  `tests/nb_all_tests.sh` monolithic reruns in this session stalled after
  `nb_span_select_test` without a final summary line, so the post-span_select
  live tests (`nb_raf_test.sh`, `nb_geometry_test.sh`, `nb_gbcr_test.sh`,
  `nb_gutter_test.sh`, `nb_flex_test.sh`, `nb_grid_test.sh`,
  `nb_reach_test.sh`) were run individually and all passed (RC=0/PASS);
  `nb_bar_test.sh` produced no completion marker in that tail run (environment
  stall).

**Honest limits**

- Attribute getter/setter semantics otherwise unchanged; this slice only fixes
  present-empty value reporting for `getAttribute`.


### 9.42 2026-11-13 — element attribute helpers (getAttributeNames/hasAttributes/toggleAttribute)

**What changed**

- `ops/nb_js_worker.c`: implemented `Element.getAttributeNames()`,
  `Element.hasAttributes()`, and `Element.toggleAttribute(name, force)`
  on element wrappers. Names are enumerated from the attribute blob with the
  same parser-form/normalized handling as existing attribute iteration (tag
  token excluded, names lowercased). `toggleAttribute` adds/removes via the
  existing attribute set/remove paths and supports the optional `force`
  argument; added attributes use an empty string value.

**Verification**

- New gate `tests/nb_attr_api_test.sh`: **PASS (24/24)**. Covers empty and
  populated elements, parser-sourced attributes (including bare
  `disabled`), `getAttributeNames` content/exclusion of tag token,
  `hasAttributes`, and `toggleAttribute` add/remove/force semantics
  (including present-empty value).
- Negative probe: parsed `span` tag name not returned by
  `getAttributeNames`; `toggleAttribute('x', false)` on absent attribute does
  not create it (`LOG|NEG tag=false created=false`).
- Full-suite evidence: gate registered and passing on fresh build; monolithic
  `nb_all_tests.sh` rerun in this session stalled after `nb_span_select_test`
  without a final summary (environment stall pattern). Post-span_select live
  tests were run individually and all passed RC=0/PASS: `nb_raf_test.sh`,
  `nb_geometry_test.sh`, `nb_gbcr_test.sh`, `nb_gutter_test.sh`,
  `nb_flex_test.sh`, `nb_grid_test.sh`, `nb_reach_test.sh`; `nb_bar_test.sh`
  tail run inconclusive/no completion marker.

**Honest limits**

- Attribute name casing follows existing lowercased iteration behavior.
- `NamedNodeMap` remains the lightweight array-like object already exposed by
  `element.attributes` (not a fully live spec NamedNodeMap interface).
