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
| **Chrome-parity** | feature-for-feature against a desktop browser | **NOT MET, not the target** (stated non-goal in the roadmap) |

Anyone reading this later should not take the first row as a claim about the
second. The browser is a readable-content browser with real inline interaction,
not an application runtime.

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
| S3 | **Box tree, phase 1** — geometry for every visible node (G2.1, G2.2) | manager lane owns the tree; projector consumes it | 2 sprints | This is the gate for all layout work. |
| S4 | Box tree phase 2 — flex row (G2.3) | S3 | 1 sprint | The engine already does row/wrap/grow/gap for chrome; this is wiring, not invention. |
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
- **Chrome-pixel parity as a launch bar** — stated non-goal.
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

### 8.5 G3.2 ZOOM — attempted, measured, REJECTED (2026-10-13)

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
