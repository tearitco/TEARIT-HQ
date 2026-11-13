# NETWORK BROWSER - HANDOFF (2026-10-27)

Branch: `opencode` (HEAD `453790ddd`)  
Nothing pushed. All gates green on fresh build.

## What landed this session
**Commit `453790ddd`** - `fix: survive hostile page scripts instead of silently killing the worker`

### ops/nb_js_worker.c
- Replaced `signal(SIGALRM) + alarm() + _exit(128+sig)` with QuickJS runtime interrupt handler. Slow scripts now abort with `InternalError: interrupted` and are catchable by C, worker survives with DOM intact. 
- Measured: abort is **NOT catchable by page JS** (fixture with 3x `try{while(true){}}catch` ended at first abort with single "interrupted" line).
- `NB_CRASH_BT=1` (env-gated): fatal-signal backtrace written to worker log via `backtrace_symbols_fd` (no gdb on box). 
- `-rdynamic` added in build.sh so backtraces resolve function names.

### network_browser_manager.c  
- Reap unexpected worker death on every main loop tick, publish `script aborted - partial page`, log signal/exit, respawn on next request (no more zombies/silent dead pane).
- `NB_EVAL_BUDGET` from launcher now honored (default still 60).
- `worker_load()` failure no longer discarded - publishes partial page status.

## Verified evidence
- Loopy `while(true){}` → `ready`, worker alive, 1x `interrupted`
- CatchLoop fixture → `ready`, worker alive  
- `kill -SEGV` worker → `script aborted - partial page`, no zombie, `[worker] died unexpectedly (signal 11)`, eval recovers after respawn
- Gates: gBCR 100% (9/9), flex PASS, grid PASS, wasm PASS

## Cloudflare / glassdoor (measured honestly)
Orchestrate script **finishes in seconds** now, throws `TypeError: not a function` at `<script>:1:28370`. Missing browser API = **R4 fingerprint surface**, not budget issue. 4chan not challenged (200, 884 thread rows). To get cf_clearance need R4 API work.

## Current state
- Manager running (PID varies), worker alive
- Status on glassdoor: `ready`, title `Security | Glassdoor` (challenge page loads, scripts run, fail on missing APIs)
- Last commit on branch opencode: `453790ddd`

## Next up (user said "both" - R4 + @media)
**R4 fingerprint surface** is blocking Cloudflare. Need to:
1. Identify exact missing API at orchestrate failure points (errors show native call sites)
2. Add minimal shims for missing APIs (currentScript, URL, etc.) as needed
3. Do @media (G2.5) in parallel where possible

Relay files: `#.desktop/network_browser_request.txt`, `#.desktop/network_browser_status.state.txt`, `#.desktop/network_browser_page.state.txt`, `#.desktop/network_browser_console.txt`, `#.desktop/network_browser_worker.err.log`

Build: `bash &.hq-apps/network/build.sh` from house root or network dir.
Tests: `sh &.hq-apps/network/tests/nb_gbcr_test.sh && sh &.hq-apps/network/tests/nb_flex_test.sh && sh &.hq-apps/network/tests/nb_grid_test.sh && sh &.hq-apps/network/tests/nb_wasm_test.sh`


---

## Session 2026-11-02 — atob was the Cloudflare wall; entropy was broken; CF is NOT a shim gap

Branch `opencode`, committed but **not pushed** (house rule: never push unprompted).

| commit | what |
|---|---|
| `7cc568445` | `atob` must return a binary string, and must flush a padded tail |
| `c9a729fdf` | the fingerprint probe now names the missing element methods |
| `401af2205` | 16 `Element` + 2 `Node` methods every modern page calls |
| `690cbdf61` | `crypto.getRandomValues` returned identical bytes in every process |
| `57f6bc716` | `TextEncoder`/`TextDecoder` + `Blob` readers; the CF verdict |

### 1. The decode failure was OUR `atob` — found and fixed

`atob` was handing raw decoded bytes to `JS_NewStringLen`, which decodes UTF-8,
so **every byte >= 0x80 became U+FFFD**; and it never flushed the final partial
base64 group, so a padded tail lost its last byte. Two independent bugs.

Measured, before -> after:

- `atob('yhXN')`: `[FFFD,15,FFFD]` -> `[CA,15,CD]` (a browser returns the latter)
- Cloudflare's real decoder (atob + rolling-XOR), on three literals lifted from
  a script Cloudflare actually served: `3 / 3 / 211` characters of mojibake ->
  exact reference output, `156`, `1640`, and a 260-char hex BigInt string.

That is why the challenge died at `BigInt(...)` with `invalid bigint literal`:
the deobfuscator's integer strings arrived corrupted, and the string I spent
time chasing (`＊x＊0＊3＊...`) was `U+FFFD ^ 0xFB` — our own corruption, not
theirs. The served script is pure ASCII; that was the tell.

Survived for months because `rung6_atob_test.js` round-tripped **ASCII only**,
and every failure it caused lives **above 0x7F**.

### 2. `crypto.getRandomValues` was deterministic across processes

Nothing calls `srand()`, so glibc seeded from 1. Measured on three separate
worker processes — all identical:

```
67c6697351ff4aec  uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
67c6697351ff4aec  uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
67c6697351ff4aec  uuid=29cdbaab-f2fb-4346-bcc2-54f81be8e78d
```

Now reads the kernel CSPRNG via `getrandom(2)` (`open("/dev/urandom")` was
rejected because this TU already has a `read` symbol colliding with
`<unistd.h>`). `crypto.randomUUID` draws from the same source. A browser whose
"random" values repeat every session is self-identifying, so this may matter
more than any shim — but no single-process test can see it. The new gate runs
the worker **twice**.

### 3. Element / Node methods (the general-capability bite)

`matches`, `closest`, element-scoped `querySelector(All)`,
`getElementsByTagName/ClassName`, `prepend/append/before/after/replaceWith/
replaceChildren`, `remove`, `cloneNode`, `isEqualNode`, `getClientRects`,
`scrollIntoView` — installed on the prototypes so `createElement`-made elements
get them. Verified by **behaviour**, not presence (argument order for the
variadic inserts, element scoping, shallow vs deep clone, attribute-sensitive
equality, and that insertion MOVES a node).

### 4. VERDICT: the Cloudflare error is not a missing-global gap

Every one of these was implemented and verified working, and **not one changed
the error**:

`atob` (real bug, fixed) · `performance.getEntries*`/`timing` ·
`URL.createObjectURL`/`revokeObjectURL` · `crypto.randomUUID` +
`isSecureContext` · the whole element/Node batch · `TextEncoder`/`TextDecoder`
+ `Blob.text`/`arrayBuffer`.

The orchestrate VM still throws its internal `TypeError: not a function` (one
trace shows it inside its own `call` thunk invoking a non-function) and **no
`cf_clearance` is ever issued**. Stopping the shim search on that evidence.

The one name still unfilled in the VM's own literal table is **`Worker`**. That
is not a shim: it is a real thread and a second runtime. Do not start it
without an explicit decision.

4chan remains unchallenged (200).

### 5. New hermetic gates — now in `nb_all_tests.sh`

`nb_atob_test.sh`, `nb_element_methods_test.sh`, `nb_crypto_random_test.sh`,
`nb_text_blob_test.sh`. All drive the worker directly through the new
**`tests/nb_probe.py`** (framed RPC, no window, no GUI, answers every `FETCH`
with a mismatched id so the worker uses its own curl path and therefore real
TLS). All four are negative-tested.

`nb_probe.py` is the tool that made all of this findable: it runs one page
through the worker with no renderer, so a question about the PAGE can be asked
without a window.

### 6. Method lessons — recorded because each one cost real time

1. **Verify the build with an explicit error grep, never `tail`.** I reported
   "build succeeded" twice from `tail -2` while `nb_host.h` was broken. The
   worker binary was then stale from 2026-10-31, so a previous session's shims
   **had never been in the binary at all** and some of its conclusions were
   about a build that did not exist. `grep -E '\.c:[0-9]+:.*error'`.
2. **Never replace a C-provided global from the prelude.** A prelude block that
   overrode `Blob`/`File` wedged the worker outright (no STATUS, LOAD timeout).
   Additive only: new members and statics.
3. **The prelude runs BEFORE `install_events_timers`.** `crypto` and
   `performance` are built there, so a prelude guard on `getRandomValues` never
   fires. `crypto.randomUUID` had to go in C beside `getRandomValues`.
4. **`Blob` instances are plain objects** (`nb_blob_from_parts` returns one),
   so nothing on `Blob.prototype` is reachable from an instance. `text`/
   `arrayBuffer`/`slice` are installed per instance.
5. **The console capture truncates a long line (~500 chars) and rewrites `|`
   to a space** in EVAL-logged messages. Emit in chunks; anchor assertions to
   `^LOG|` / `^=>` output lines, because the capture also echoes the probe's own
   SOURCE (an unanchored grep matches the source and reports failures the run
   did not have).
6. **awk interval quantifiers are not POSIX.** A correct UUID failed my own
   `{8}-{4}` regex; the test was wrong, not the code. Use `grep -E`.
7. **The live window keeps a RESIDENT worker** across navigations, so it does
   NOT pick up a rebuilt binary. Restart the renderer before trusting a live
   gate's result, or the test is measuring the previous build.
8. `pgrep -f "<pattern>"` **matches the invoking shell's own command line** —
   killing that PID kills your command. Match on `/proc/<pid>/comm` instead.

### 7. Next up

The CFVM path is exhausted as an API hunt; treat further work there as
fingerprint-grade and justify it separately. Recommended direction: back to the
KPI ladder (K1/K2 standing, then G1.4 focus safety, G2.5 `@media`), measuring
against pages that do not gate, so progress is visible rather than blocked on
one hostile endpoint.


### 6a. Attribution run + a correction to section 1 (same day, 2026-11-02)

**The three red suites are not ours.** Method: build the worker from
`2ed13fdd7` (the last commit before this session - which also lacks the
previous session's shim block), then re-run them. Control first, and it earned
its keep: the swap was verified by md5 (worker differed) and by the source
containing none of the new code.

| suite | pre-session build | HEAD build | verdict |
|---|---|---|---|
| `nb_find_test.sh` | fails, the SAME two assertions | fails, same two | deterministic, pre-existing |
| `nb_span_drag_test.sh` | rc 0,1,1 (3 runs) | rc 0,0,0 then 1,1 earlier (5 runs) | flaky on BOTH, no build correlation |
| `nb_span_select_test.sh` | fails (S2) | pass once, then S6, then S1/S2 | flaky on BOTH, varying failure point |

They also cannot reach the changed code: `nb_find_test.sh` navigates to
`tests/fixtures/mini-article.html`, which is 453 bytes with **no `<script>`
tag** (the worker parses static HTML and nothing else), and the `span_*`
fixtures are `.xhtpm` khtpm windows whose selection, highlight band and
clipboard are C + X11 (driven by `XSendEvent`/`xclipget` via ctypes).

`nb_find_test.sh`'s two failures are therefore a pre-existing defect in
find-in-page, worth its own ticket: "mirror says 3, frame says 2" (the find
system counts a match the frame never paints) and "no wrap (at=3)".

**CORRECTION to section 1.** That section says the `atob` bug "survived for
months because `rung6_atob_test.js` round-tripped ASCII only". The duration is
WRONG, and the causal claim with it. The pre-session build passes the new atob
gate **6/6** - so the engine's `atob`, which is the pure-JS implementation in
`nb_host.h`'s prelude, was already CORRECT.

The buggy C `_nb_atob` (UTF-8 mis-decode + missing padded-tail flush) arrived
with the **R4 shim block** - the uncommitted work sitting in the worktree at the
start of this session (verified: `grep -c "R4 minimal shims"` is 0 at
`2ed13fdd7` and 1 at HEAD). That block installed `_nb_atob` with
`JS_SetPropertyStr(ctx, g, "atob", ...)`, **overriding a working
implementation** and replacing it with a broken one.

So the defect was a regression introduced by a shim, days old - not a
months-old engine bug. The rung6 test's ASCII-only weakness is real, but it is
not why this existed; nothing re-tested `atob` *after* the override. That is
the transferable lesson, and it is the same error made again later in this
session when a prelude block overrode the working C `Blob`/`File` and wedged
the worker (lesson 2 above).

The fix stands: `atob` is now correct at the C layer as well. But the shim that
caused it should not have reimplemented `atob` at all.


### 6b. G3.1 find-in-page: the red was the TEST, not the browser (2026-11-04)

**Correcting 6a.** That section filed this as "a pre-existing defect in
find-in-page, worth its own ticket: mirror says 3, frame says 2". It is not a
product defect at all, and nothing in the browser needed changing. The suite
was measuring the fixture.

Measured, from a diagnostic copy of the suite that printed the frame rows at
the point of failure:

    DIAG-findline:   [FIND] query="the" 1 of 3
    DIAG-raw-count:  3        (rows whose label contains "the")
    DIAG-navgt0:     2        (the same rows, filtered to nav_index > 0)
      nav=21  First paragraph of body text. Enough to force two lines ...
      nav=23  See the docs for more.
      nav=0   >(function(){var u=[...];return Promise.all(...
                                     ... .then(function(r){ ... })()]

The third match is a **console-panel row**, and it matched because `then`
contains `the`. Step 3 of the suite counted only `nav_index > 0` rows, so it
expected 2 while find reported 3. Step 4 then failed as a knock-on: it
hardcoded two matches, so with three the walk 1 -> 2 -> 3 never wrapped, and
the suite reported "no wrap".

Why the code is right and the test was wrong:

  - `kh_find_walk()` (`&.widgits/_shared-lib/khtpm_draw_core.c`) matches
    `item`/`text`/`cli_io`/`text_area`/`bar` and recurses through every
    container, deliberately with no notion of chrome vs content. Making it
    skip "console" rows would put per-project knowledge inside the shared
    renderer - forbidden by `CENTROID_GOLD_STD.md` rule 7.
  - `nav_index` is a red herring for this count twice over: parked off-screen
    rows are nav=0, and the suite's own DEEP case REQUIRES find to match them.
  - The suite's own stated contract is "counts exactly the rows whose label
    contains it, case-insensitively". The nav>0 filter contradicted it.

Fix: compute the expectation the way the feature computes it (no nav filter)
and step against the real hit count instead of a hardcoded 2. The assertion is
not weakened - it still derives its own count from the frame and compares it
to the mirror's independent one - and it gained an assertion.

Evidence: three consecutive clean runs (14 assertions, 0 failures), plus a
negative test that forces `expect=2` and a wrong wrap target and reproduces
the exact original failure text, exit 1.

Lesson worth keeping: a red suite is not evidence of a product defect until the
assertion has been checked against the feature's own definition. Here the
suite had been red for a while against correct code, and the "bug" lived
entirely in the expectation.

---

## 2026-11-04 (evening) — nb_span_drag_test: input was never dropped; the instrument was

Picked up the one item the 2026-11-04 ledger entry left open: the known-flaky
span-drag suite. The whole arc is now dated and reproducible, and the earlier
"synthetic input was dropped" reading is corrected.

### The three-stage diagnosis

1. **Fixed sleeps.** Every dispatch assertion did `sleep 1; grep` against a
   click.log written by an action.sh child spawned by the renderer. Under load
   the child landed after the sleep — the failing T6 message printed
   `log: CLICKED LINK1;`, i.e. the line HAD landed. Three theories were built
   before the failure payload was read.
2. **Stale grep.** The first fix (wait_log greping for the marker) turned a
   sometimes-flake into 0/8: every check matched stale T1/T3 lines. Line counts
   only increase, so the honest receipt is a **delta**, not a pattern. Delivered
   as `wait_count`.
3. **Unsynchronized bursts.** `xe()` now waits for the renderer's own capture
   line in the relay before returning (real press/release are appended
   synchronously inside the dispatcher; FIFO relay ⇒ queued motions are covered
   too). Absence assertions use `settle` instead of a bare `sleep 1`.

After these fixes, **T1–T5 pass deterministically** (7/7 consecutive batches,
including the two runs that used to fail and the T2 band check).

### What remains: a renderer-side race, not delivery

T6 still fails ~1 in 3 runs, always the same shape: press captured, release
captured, `press alone dispatched nothing` passes, and the late release **never**
dispatches (waited 180s; an 8+ iteration clean-room replay of the full T1–T6
sequence never reproduced it). The failing fixture's relay and
`human_input/<pid>.txt` both confirm every event arrived — so the
"synthetic XSendEvent dropped input" theory is dead.

`kh_focus_debug.log` from the failing run shows why: **every dispatch forces a
full content rebuild** (`ANCHOR cleared by content rebuild` +
`SPAN kept ... (content identical)`), and T6's press/release pair brackets
exactly one such rebuild (14:34:49.486 / 14:34:59.602). The deferred span pair
evaluated against the rebuilt tree intermittently declines to dispatch —
deterministically silent afterwards, never merely "slow".

### Consequences

- `tests/nb_span_drag_test.sh` is honest: it waits on receipts and reports the
  failure; it cannot make the renderer's rebuild race impossible. Its red now
  means exactly one thing: **T6's span pair lost to the rebuild cycle** (a
  renderer task), not a product span-drag defect and not dropped input.
- `nb_all_tests.sh` still gates on it (RC=1 on fail) — left as is, deliberately:
  the gate stays honest and red-when-broken. Do not paper over it by reordering
  or sleeping the fixture.
- If picked up next, the fix lives in the renderer: the release-side span pair
  must be evaluated against post-rebuild state (or the fixture stops rebuilding
  per click). It needs its own verification loop — do not "fix" it from the
  test side.

### State after this session

- `tests/nb_span_drag_test.sh`: rewritten to receipt-based waiting
  (`wait_count`, `wc_lines`, `settle`, synchronized `xe`); T1–T5 deterministic.
- KPI ledger: new entry 9.18 records the correction (two 9.17 claims were
  wrong: "dropped input" and "fix target = xsendevent.py"), with the evidence.
- Everything else in the standing suite is unchanged; see S9.17 / nb_all_tests
  results for the full picture.

---

## 2026-11-04 (night) — span_drag: root cause found with receipts, fixed in the renderer

The 2026-11-04 (evening) section left this open as "a renderer-side race -
closing it needs the release-side pair evaluated against post-rebuild state,
or the fixture stops rebuilding per click - a renderer task with its own
verification loop." Done, and the fixture did not change.

### The mechanism, measured

The fixture renders every event with a FULL content rebuild (debug log:
`ANCHOR cleared by content rebuild` + `SPAN kept ... content identical` after
virtually every event). Since INLINE SPANS step 11, the reparse path
UNCONDITIONALLY cleared `g_span_pend.active` - "a rebuild also orphans any
armed span press; the paired release must find nothing to resolve."

T6 arms the press, waits 8 seconds (the absence assertion), then releases.
The rebuild fires ~0.7s after the arm every single time, so the press was
ALWAYS orphaned before the release was parsed: no SPANPAIR receipt, no
dispatch, deterministically silent forever. T1/T3 (release ~0.5s after the
arm) raced the same rebuild and usually won - which is why the failing step
moved between T1/T3/T6 across runs, and why the input "was never dropped":
every event was delivered and consumed; the pair just had nothing left to
pair.

### The fix (mirrors three in-house guards)

Save `g_span_pend` before the rebuild; re-arm after iff `kh_tree_digest` of
the new tree is byte-identical to the old (the same guard the row-run anchor,
span cursor/anchor and hover already use). Identical digest => same tree
order => same nav re-derivation, so the restored press resolves exactly as if
no rebuild had landed; a tree that changed anything leaves it dead - honest,
since its row may be gone or may carry a different action.

### Verification

- nb_span_drag_test.sh: **8/8 consecutive green** (was ~1 in 3 red, and the
  full suite went red on T1 once and T6 twice in recent runs).
- Receipts prove the mechanism: `SPANPRESS kept nav=1 idx=0 ... content
  identical`, then `SPANPAIR phit=1 ... same=1`, dispatch happens.
- **nb_all_tests.sh: ALL TESTS PASS, 38 sections, 31 suites (exit 0)** -
  with the fix in the house-wide renderer.

### Notes

- SPANARM/SPANPAIR breadcrumbs stay in the renderer (same class and volume
  as the existing ANCHOR/SPAN receipts).
- Running renderers keep the old image until restart.
- The full-suite gate in nb_all_tests.sh stays as it is, now honestly green.

---

## 2026-11-04 (night) — G2.6 closed: property list documented, two product bugs fixed, gate registered

The KPI plan's last unverified G2 item is closed. What mattered:

### The list was an undocumented, partly dead table

nb_css.c resolved exactly what the page projection reads (display, the
vertical box model, position family, flex-grow/direction,
grid-template-columns, @media grammar, @font-face, and visibility/
opacity/width/height for getComputedStyle) - but three enum entries
(src, font-weight, font-style) were recognized by prop_id and consumed
by NOTHING, and the header still documented the pre-2026-11-04 engine.
A page writing those keywords got a silent no-op. All three removed
(declarations now drop like any unlisted property; @font-face parses
its own three via parse_font_face_block). The contract is now written
in-source directly above prop_id - complete, grouped by consumption
(layout / computed-style-only / parsed-unread mirrors / @font-face /
not-recognized-with-reasons), with the three-step rule for adding one.

### Two product bugs found by the new gate work, both fixed

- @font-face italic faces could never match: the parser set
  font_style = 0 unconditionally instead of the computed value
  (compiler even warned the local was unused).
- display:none cleared has_display_flex but not display_flex, breaking
  the typed-pair invariant the code itself documents.

### The gate

tests/nb_css_props_test.sh (registered in nb_all_tests.sh): links
nb_css.c + nb_dom.c directly, asserts every recognized property's
resolved value + the drop-totally contract + @media both branches.
45/45 PASS; negative-tested by reintroducing the font-style bug
(exactly one FAIL, then green).

### State

nb_all_tests.sh: ALL TESTS PASS, 39 sections, 31 suites (exit 0), with
the worker rebuilt on the fixed parser. G1.4's suite flaked once in one
run, passed 4/4 isolated and on the re-run; timing-sensitive, recorded
in ledger 9.20.

---

## 2026-11-04 (night 2) — G3.4 closed: Ctrl+P prints the projected page to a PDF

The last unbuilt KPI in the plan. No house print path existed and no
printer is configured; the system carries only groff/ps2pdf (no HTML->PDF
toolchain). Chosen shape:

- ops/nb_print_page.sh generates the document itself: page.state.txt ->
  PostScript (letter, 0.75in margins, TITLE heading / HEAD bold subheads /
  TEXT word-wrapped / LIST dash-indented / Helvetica) -> ps2pdf -> the
  app's own downloads/ dir (same naming as downloads: sanitized TITLE,
  collision-unique, app-owned). Any failure -> rc 1 + "ERROR: ..." and no
  file is produced - never a fake print.
- The manager reports it like a download: success row TITLE|Printed +
  PRINT|<name>|<bytes>|<path> (new projector row kind, mirrors DL|) and
  publish_status("printed"); failure row TITLE|Print failed + the reason.
- Triggers: the manager's request channel (one "print:" line, same
  contract as go:), via ops/nb_write_print.sh (chrome-action contract,
  like nb_write_go.sh) or Ctrl+P in the shared renderer, gated to the
  network-browser template so other apps keep Ctrl+P; relay code 16
  added for agent parity with Ctrl+C/Ctrl+F.

### Verification

- Live browser (relaunched with the new binaries): print: consumed in
  2s -> Mini_article.pdf, valid PDF 1.4, 1 page, correct metadata; pane
  shows the Printed row; page.state.txt carries PRINT|...|2775|<path>.
- New gate tests/nb_print_test.sh (registered): --out produces a valid
  PDF; default names from sanitized TITLE into a scratch house's
  downloads/; hostile chars (parens/backslashes/UTF-8) survive; missing
  state file and missing ps2pdf both fail clean, no file written.
- nb_all_tests.sh: ALL TESTS PASS, 40 sections, 31 suites (exit 0).

### Notes / scope

- No chrome Print button yet - that is a UI design decision (placement,
  icon, label) for its own bite. The plumbing and the op are generic:
  any caller can write "print:".
- PDF goes to downloads/ by the same rule as downloads: app-owned dir,
  never the user's home.
- The shared renderer and the manager were rebuilt; the live browser is
  running them.

---

## 2026-11-07 — Find reveal and scroll state repaired; browser chrome controls finished

Branch `opencode`, commit
`29a3b72c2e8ae2c62c3593cbe5db2e1df1a1e8ef`
(`fix: preserve Find/scroll state while wiring browser chrome controls`).

### What failed, measured rather than inferred

The deep Find clause reported `match still off screen after find
(y=-100000)` even though the ASCII mirror said `[FIND] query="NEEDLE"
1 of 1` and the renderer repeatedly printed `FINDREVEAL ... scroll=43`.
Marker logs put the change inside `layout_scroll_region()`:
`scroll_in=43` became `scroll_after_setup=14` while `max=44`.

The cause was the preliminary full-width total (`613`, `total=33`,
`max=14`) clamping the entry scroll before the scrollbar's 8px gutter was
reserved. At the final width (`605`), the fixture's filler paragraphs wrap
to `total=62`, `max=44`. The final recomputation corrected the limit, but
the already-clamped 14 was not restored, so the next layout parked the row.

### Renderer changes

- `scroll_region_total()` is reused so the scroll limit is computed at the
  final, scrollbar-narrowed width; the caller's entry scroll is restored
  after that final pass when it fits.
- `g_find_reveal` is cleared with the departing tree and in the no-target
  `kh_find_commit()` path, then reissued against the new tree.
- Find reveal handling is ancestor-gated to its owning scroll region, and
  the visibility answer is recomputed after the reveal's own scroll move.
- `dump_frame_png()` checks for a just-landed browser publish before it
  redraws, so the PNG and ASCII frame sidecar describe the same state.

### Browser chrome

- Network Browser template/CSS now include the Print item/action,
  `user-resizable`, and the raw-X resize grip.
- `tests/xsendevent.py` accepts explicit root coordinates for resize-drag
  tests without changing the span press/move/release contract.

### Verification

- Fresh renderer build: `OK +x/khtpm_core_render.+x`.
- Deterministic manual needle drive produced frame record `cx31 nav=37,
  x=200, y=674, w=605`.
- `sh tests/nb_find_test.sh`: **PASS** — down-reveal to `y=674`, band on
  the revealed row, and scroll-back-up to `y=164`.
- `sh tests/nb_all_tests.sh`: **ALL TESTS PASS**.
- The commit is scoped to renderer, browser CSS/template/manager, and
  `tests/xsendevent.py`; generated browser bootstrap/state files were left
  uncommitted.

### 2026-11-09 — matchMedia (commit `9ad82621f`)

`matchMedia` had been a constant-false prelude stub since rung 6. It is now
a native evaluator over the LOAD viewport (`g_viewport_px` /
`g_viewport_h`, the values `innerWidth` / `innerHeight` already report).

- Real queries: width/height (`min-`/`max-`), level-4 range form, aspect
  ratio, resolution, orientation, color-scheme/reduced-motion/contrast,
  hover/pointer, display-mode, scripting, color/monochrome, comma OR,
  `and`, and leading `not`. Unknown features stay false.
- `MediaQueryList` carries `media`, live-on-read `matches`,
  `add/removeListener`, `add/removeEventListener("change")`,
  `dispatchEvent`, and `onchange`.
- Honest limit: no change **delivery** — the worker has no pane-resize
  channel, so listeners register and dispatch but are not fired by a
  viewport change that never reaches the worker.
- The prelude's `matchMedia` line was deleted (it runs after the C install
  and would shadow the native with the old false stub).
- Gate `tests/nb_match_media_test.sh` **PASS (21/21)**, negative-tested by
  forcing `matches` false (11 failures), then restored.
- Full suite **ALL TESTS PASS**; live browser PID `3875779`.

### 2026-11-10 — insertAdjacentHTML/Text/Element

The three methods were absent entirely. They are now real, built on the same
parser and tree/orphan helpers as `innerHTML`/`appendChild`:

- all four positions (`beforebegin`, `afterbegin`, `beforeend`, `afterend`),
  ASCII case-insensitive;
- `insertAdjacentText` creates a text node; `insertAdjacentElement` moves and
  returns the element, `null` on the no-op cases;
- no parent means no-op for `beforebegin`/`afterend`; unknown position throws
  `SyntaxError`; ancestor-into-descendant throws `HierarchyRequestError`;
- installed per wrapper and on `Element.prototype`;
- childList MutationObserver records fire for every inserted node (a parsed
  fragment yields one record per node rather than one batched record);
- script execution on element insertion is deliberately not claimed.

Gate `tests/nb_insert_adjacent_test.sh` **PASS (20/20)**, negative-tested by
forcing `beforebegin` to append (2 failures), then restored. Full suite
**ALL TESTS PASS**.
### 2026-11-11 — structural navigation getters

`lastChild` was installed nowhere on elements, `previousSibling` existed only
on TreeWalker, and `parentElement`, the element child/sibling accessors, and
`childElementCount` were absent. All are now live-tree getters on element
wrappers, the correct prototypes, and the fragment wrapper. Text nodes take
part in `*Sibling` and are skipped by `*ElementSibling`; `parentElement` is
null for `<html>`; `childElementCount` agrees with `children`.

Known separate limitation surfaced by the new gate: `nb_parse_html`
(`DOMParser` + `innerHTML`) stores character data on the element's own text
field instead of creating child text nodes. The gate builds its mixed tree
with `createTextNode`, so the getters are verified; fixing parsed text nodes
is a DOM-model change with renderer impact and remains open.

Gate `tests/nb_sibling_nav_test.sh` **PASS (27/27)**, negative-tested with
two deliberate inversions (8 failures), then restored. Full suite
**ALL TESTS PASS**.
### 2026-11-11 — parsed text is a real node; RENDER emits it once

`nb_parse_html_nodes` (new, in `nb_dom.c`) makes character data a child
`#text` node. `DOMParser`, the `innerHTML` setter, and `insertAdjacentHTML`
use it; the manager's fetch.dom extraction keeps the folded form so the live
page wire path and row projector are untouched. `innerHTML =` and
`replaceChildren()` now also drop the folded own text a LOAD element carries
(pre-existing bug: old page text survived replacement). RENDER emits a TEXT
row for a text child under its owning element's SEL and suppresses descendant
TEXT under a caption row, so a parsed link renders once.

Gate `tests/nb_parse_text_nodes_test.sh` **PASS (29/29 + rows)**; two negative
tests (fold the parser again: 21 failures; remove caption suppression:
duplicate `TEXT|hi` row), restored and green. Full suite **ALL TESTS PASS**.
`nb_probe.py` has optional `--drain-ms` to catch a RENDER that trails STATUS.

Open: parsed-document `doc.title` still undefined; `textContent =` still
folded (childNodes 0); `innerHTML` does not escape `&`/`<`.
### 2026-11-11 — viewport scroll metrics

`pageXOffset`/`pageYOffset`, `document.scrollingElement`,
`documentElement.scrollTop`/`scrollLeft`/`scrollHeight`/`scrollWidth`, root
`clientHeight`/`clientWidth` (now the viewport, not the unstyled `<html>`
box), and element-level `scroll*` are live. Root heights use the same
`nb_doc_root_h`/`nb_content_h` box model the scrollbar gutter uses; root
offsets are the existing `g_scroll_*` and re-run observer geometry.
Non-root offsets are per-node worker-local values, like `window.scrollTo`.
`<body>` is an ordinary element here, not the scroller.

Found and fixed alongside: `css_hidden` ignored inline `display:none` on a
page with no stylesheet (`g_css == NULL`), which also affected
`offsetWidth`/`offsetHeight`/`getBoundingClientRect` in that case.

Gate `tests/nb_scroll_metrics_test.sh` **PASS (39/39)**; negative A (root
scroller forced false) 11 failures, negative B (`g_css` short-circuit
restored) exactly the hidden-height assertion. Full suite **ALL TESTS PASS**.

Open: non-root offsets are not clamped to a max (no per-element overflow
model); `scrollWidth` stays the client box because the renderer wraps
instead of scrolling horizontally.
### 2026-11-11 — textContent authors a real Text node

`el.textContent = s` now replaces all children with exactly one real Text
node (`''`/`null` add none) instead of storing a folded C string. That makes
`childNodes`, `firstChild`, `cloneNode`, `Range` and childList
MutationObserver agree with a browser for the most common text update.
Text-node self-assignment replaces `data`; `Document` is a no-op; non-string
values convert like a DOMString (`undefined` -> `"undefined"`, `0` -> `"0"`).
The old folded text is freed, so a LOAD element cannot keep stale page text.
One childList record reports the added Text node; removed children are not
reported (single-removedNode record + lifetime safety).

Gate `tests/nb_text_content_test.sh` **PASS (28/28 + rows)**; negatives:
folded setter (structural abort), empty-string node (2 failures), missing
childList record (5 failures). Full suite **ALL TESTS PASS**.

Open, found here: `document.createDocumentFragment` does not exist; fragment
wrappers lack `textContent`; `document.textContent` getter still concatenates;
`innerHTML` still does not escape `&`/`<`.
### 2026-11-11 — DocumentFragment is real and movable

`document.createDocumentFragment()` + the `DocumentFragment` constructor now
exist. The fragment wrapper gained `textContent` (get/set), `append`,
`prepend`, `replaceChildren` and `contains`. Inserting a fragment moves its
children into the target in order and empties it — `appendChild`,
`insertBefore`, `replaceChild` and the ParentNode helpers all expand it —
and no fragment node ever enters the tree. `push_node` returns the cached
fragment wrapper and reports nodeType 11, so `child.parentNode` is the
fragment the page holds (`parentNode.nodeType === 11` works). One childList
record per moved child from the three tree mutators.

Gate `tests/nb_document_fragment_test.sh` **PASS (47/47 + rows)**;
negatives: no appendChild expansion (14 failures), no fragment textContent
(4), no ParentNode expansion (6), no identity caching (1). Full suite
**ALL TESTS PASS**.

Open: fragment-moved `<script>` does not execute; shadow roots are not
movable fragments; parsed documents have no fragment factory; no
`ownerDocument`/`isConnected` on fragments.
### 2026-11-11 — raw-text/RCDATA content is preserved

`script`, `style`, `noscript`, `title` and `textarea` now keep their source
text instead of being skipped. Folded parses (`nb_parse_html`, fetch.dom)
store it on the element; tree parses (`innerHTML`, `insertAdjacentHTML`,
`DOMParser`) author a real `#text` child. `title`/`textarea` decode entities;
the rest stay raw. The renderer prunes the three data tags so captured code
never renders as prose.

Gate `tests/nb_raw_text_test.sh` **PASS (15/15 folded C harness + 17/17
probe + rows)**. Negatives: old skip 7 folded failures; no renderer prune
leaks the script body; no title decode fails exactly the two title checks.
Full suite **ALL TESTS PASS**.

Open: `xmp`/`iframe`/`noembed`/`noframes`/`plaintext` not captured;
`</script >` closing form; comments still skipped; fetch.dom grows with
inline JS/CSS.
### 2026-11-11 — real HTML serialization + writable outerHTML

`innerHTML`/`outerHTML` now escape text (`&`, `<`, `>`, U+00A0), leave
raw-text element bodies (`script`/`style`/...) literal and give void
elements no end tag, so `el.innerHTML = el.innerHTML` is stable and `<br>`
no longer doubles. `XMLSerializer` uses the same walker in XML mode (no
`&nbsp;`, end tags kept). `outerHTML=` parses in the parent context and
replaces the node in order, detaches/orphans the old node, supports
`null`/text/multi-node values, throws `NoModificationAllowedError` without a
parent element, and emits one childList record per moved node plus one for
the removal.

Gate `tests/nb_html_serialize_test.sh` **PASS (36/36 + rows)**. Negatives:
no escaping (8 failures), void end tags (5), read-only outerHTML (13),
escaping raw-text bodies (2). Full suite **ALL TESTS PASS**.

Open: attribute blobs are not re-escaped; fragment-parsed scripts in
`outerHTML=` do not execute; `</script >` closed form not captured.
### 2026-11-11 — attribute values are markup-safe; entities decode as UTF-8

`setAttribute`/`dataset`/`id`/`className` now store values escaped for HTML
attribute mode (`&`, `"`, NBSP; `<`/`>` literal), so a quote can no longer
truncate the blob and a literal `&amp;` survives as `&amp;amp;`. Parser
blobs are still emitted as authored (no double escaping). `id=` and
`className=` now sync the attribute blob, so outerHTML/innerHTML reflect
them. `decode_entities` emits real UTF-8: `&nbsp;` -> U+00A0, `&mdash;` ->
U+2014, `&copy;` -> U+00A9, numeric references UTF-8 (invalid -> U+FFFD).

Gate `tests/nb_attrs_serialize_test.sh` **PASS (34/34 + rows)**. Negatives:
raw storage (12 failures), old decoder (8), no id/class sync (4), no quote
escape (5). Full suite **ALL TESTS PASS**.

Open: parser-sourced attrs are not canonicalised; named-entity table still
small; attribute names not validated; numeric out-of-range clamps to U+FFFD.
### 2026-11-12 — DOMParser documents get a real Document surface

`DOMParser.parseFromString` wrappers now have `title` (reads first `<title>`,
decodes RCDATA entities; setter replaces content, creates `<title>` under
`<head>` when missing, synthesizes `<head>` only under `<html>` and never
inside a fragment root) and the node factories `createElement`,
`createElementNS`, `createTextNode`, `createDocumentFragment`. Nodes they
create append/serialize in the parsed tree and do not leak into the live
RENDER. `document.textContent` is `null` for Document nodes (parsed wrapper
and live host document) and assignment is a no-op.

Gate `tests/nb_parsed_doc_test.sh` **PASS (37/37 + rows)**. Negatives:
Document concat (1), title no-replace (5), no live textContent accessor (2),
head synthesized into fragment root (1). Also fixed Range
`cloneContents`/`extractContents` to return a `DocumentFragment` (exposed by
correct Document `textContent`). Full suite **ALL TESTS PASS**.

Open: parsed docs still miss `implementation`/URL/contentType/doctype/comment/
importNode; `ownerDocument` not tracked; live `document.title` setter does not
sync the `<title>` DOM node.
### 2026-11-12 — live document.title syncs with the <title> element

`document.title` getter now reads the live `<title>` element's text when
present (falling back to `g_title`), and the setter updates the element
(creating `<title>` under `<head>` when missing, synthesizing `<head>` only
under `<html>`). RENDER, getter and DOM agree. Implemented in the host
natives with local NbNode helpers; `g_dom_root` moved into host global state.

Gate `tests/nb_live_title_test.sh` **PASS (13/13 + 6/6 + rows)**. Negatives:
getter ignores element (1), setter skips DOM (12 across cases), setter
no-create (6). Full suite **ALL TESTS PASS**.

Open: first `<title>` only (no dedupe); multiple head/title edge cases follow
browser parsing, not forced normalization.
### 2026-11-13 — permissive raw-text/RCDATA end tags

`skip_to_close` now accepts HTML whitespace before `>` in raw-text/RCDATA
end tags (`</script >`, `</style\t>`, `</TITLE >`, newline forms), so closing
no longer swallows following content. RCDATA entities still decode; trailing
text becomes a sibling.

Gate `tests/nb_rawtext_close_test.sh` **PASS (12/12)**. Negative (immediate
`>` required): 10 failures. Full suite **ALL TESTS PASS**.

Open: junk in end tags not accepted; `xmp`/`iframe`/`noembed`/`noframes`/
`plaintext` not parsed as raw-text.
### 2026-11-13 — xmp/iframe/noembed/noframes/plaintext raw-text capture

`is_rawtext` now includes `xmp`, `iframe`, `noembed`, `noframes`, `plaintext`.
Their bodies are captured as literal raw text (no markup parsing, no entity
decoding) on both parse paths; `plaintext` consumes to EOF. Serializer
round-trips; renderer prunes them from visible TEXT rows.

Gate `tests/nb_rawtext_elements_test.sh` **PASS (32/32 + rows)**. Negative
(normal parsing): 13 assertion failures. Full suite **ALL TESTS PASS**.

Open: `xmp` not rendered visibly (DOM-correct but not preformatted display);
`plaintext` uncloseable; `noscript` remains script-enabled/raw-text form.
### 2026-11-13 — cheap document metadata on live and parsed documents

Live and parsed documents now expose `contentType`, `URL`/`documentURI`,
`compatMode` (`CSS1Compat`), `characterSet`/`charset`/`inputEncoding`
(`UTF-8`), `doctype` (`null`), and `implementation`. Parsed docs use the
requested MIME type and `about:blank`; live doc uses `text/html` and `g_href`.

Gate `tests/nb_doc_metadata_test.sh` **PASS (26/26)**. Negative (no metadata):
probe aborts on missing implementation. Full suite **ALL TESTS PASS**.

Open: `doctype` always null; parsed URL always `about:blank`; compatMode
fixed to standards mode; implementation surface minimal.

### 2026-11-13 — ownerDocument propagation (live + parsed)

Document and node wrappers now expose `ownerDocument`: documents return `null`;
live-tree nodes and live-factory creations report the live `document`; parsed
`DOMParser` trees and parsed-factory creations report the parsed document;
detached creations keep the creating document recorded at creation time.
Fragment wrappers get the getter directly.

Gate `tests/nb_owner_document_test.sh` **PASS (18/18)**. Negative
(`document.ownerDocument === document`) returns `false`. Full suite **ALL
TESTS PASS**.

Open: no `importNode`/`adoptNode`; in-tree ownership follows root walk (not an
adopt/import API or cross-document insertion guard); unowned detached fall back
to live document.

**2026-11-13 — importNode/adoptNode blocker fix (on 9.38)**

Implemented `document.importNode`/`adoptNode` (live + parsed): import clones
into target doc with target ownership; adopt detaches and retargets owner;
detached-subtree owner now propagates from root wrapper to descendants.
`tests/nb_import_adopt_test.sh` **PASS (22/22)**; negative confirms imported
node not source-owned. Full suite **ALL TESTS PASS**.

Open: no automatic cross-document insertion guard/throw on append (adopt/import
APIs now available); `importNode` deep defaults false per current signature
handling.

### 2026-11-13 — comment nodes (createComment, parse/preserve, serialize)

Added `document.createComment` and comment-node support: `nodeType 8`,
`#comment`, `nodeValue`/`data`/`textContent` get/set, correct owner, insertable,
serialized as `<!--...-->`, excluded from rendered text/element `textContent`.
Node-mode parser (`DOMParser`/`innerHTML`) now preserves HTML comments.

Gate `tests/nb_comment_node_test.sh` **PASS (20/20)**. Negative:
`textContent==='abc'` false / `==='ac'` true for `a<!--b-->c`. Full suite **ALL
TESTS PASS**.

Open: folded extraction parser still skips comments; comment `--` content not
sanitized in serializer.

### 2026-11-13 — parser attribute canonicalization

HTML serializer now canonicalizes parser/load attribute blobs (decode+re-escape)
so raw `&`/entity forms serialize safely; normalized `setAttribute` blobs remain
as stored (no double-escape). Exposed `nb_decode_entities` for shared decoding.

Gate `tests/nb_attr_canonical_test.sh` **PASS (20/20)**. Negative: raw
`data-b="a&b"` no longer emitted unsafely (`NEG false`). Full suite **ALL TESTS
PASS**.

Open: parser-form attr order/spacing may normalize; pre-existing
`getAttribute` returns `null` for present-empty attrs (hasAttribute true)
unchanged.


### 2026-11-13 — present-empty getAttribute semantics

`element.getAttribute` now returns `""` for present-empty attributes (e.g.
`data-empty=""`) and `null` only when absent, via presence check instead of
empty-value-as-missing.

Updated `tests/nb_attr_canonical_test.sh` **PASS (20/20)** (empty attr now
`""`, hasAttribute true). `nb_attrs_serialize_test.sh` **PASS (34/34 + rows)**.
Negative: missing -> `null`, present-empty -> `""`.

Full-suite note: monolithic `nb_all_tests.sh` reruns stalled after
`nb_span_select_test` without final summary in this session; post-span_select
live tests run individually (`nb_raf`, `nb_geometry`, `nb_gbcr`, `nb_gutter`,
`nb_flex`, `nb_grid`, `nb_reach`) all RC=0/PASS, `nb_bar` tail run
inconclusive/no marker.


### 2026-11-13 — element attribute helpers

Implemented `getAttributeNames()`, `hasAttributes()`, and
`toggleAttribute(name, force)` on elements (attribute-blob iteration consistent
with existing parsing/normalization; `toggleAttribute` uses set/remove paths,
optional force supported).

Gate `tests/nb_attr_api_test.sh` **PASS (24/24)**. Negative:
`getAttributeNames` excludes parser tag token, `toggleAttribute(x,false)` on
absent attr does not create (`NEG tag=false created=false`).

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.


### 2026-11-13 — cloneNode deep/default semantics

Fixed `DocumentFragment.cloneNode(deep)` to honor `deep` (default false):
shallow returns empty fragment, deep clones children. Element/text/comment
defaults unchanged.

Gate `tests/nb_clone_deep_test.sh` **PASS (18/18)**. Negative:
`fragment.cloneNode(false).childNodes.length===1` -> `NEG false`.

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.


### 2026-11-13 — comment serialization safety

HTML comment serialization now escapes premature-close sequences in comment
data (`-->` -> `--&gt;`, `--!>` -> `--!&gt;`) so serialized comments don't
terminate early/leak text on reparse.

Gate `tests/nb_comment_serialize_test.sh` **PASS (9/9)**. Negative: comment
`a--b-->c` reparses as 1 child with empty element textContent.

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.


### 2026-11-13 — Node.isSameNode

Added `Node.isSameNode(other)` (Node prototype) returning identity for the same
underlying node and `false` for null/undefined/non-node/missing args. Exposed
`isSameNode`/`isEqualNode` on DocumentFragment wrappers.

Gate `tests/nb_issamenode_test.sh` **PASS (15/15)**. Negative:
`element.isSameNode(element.cloneNode(true))` -> `NEG false`.

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.


### 2026-11-13 — XMLSerializer comment well-formedness

Made comment serialization mode-aware: HTML keeps prior behavior (escape
premature-close `-->`/`--!>`, preserve safe readable `--`), while XMLSerializer
breaks `--` sequences and avoids trailing `-` in comment data for well-formed
XML output.

Gate `tests/nb_xml_comment_serialize_test.sh` **PASS (9/9)**. Existing gates:
`nb_comment_serialize_test.sh` **9/9**, `nb_serializer_test.sh` **6/6**,
`nb_html_serialize_test.sh` **36/36 + rows**, `nb_comment_node_test.sh` **20/20**.

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.


### 2026-11-13 — Node.hasChildNodes

Exposed `Node.hasChildNodes()` on Node prototype (implementation already used
by DocumentFragment wrappers), returning whether `firstChild` exists.

Gate `tests/nb_haschildnodes_test.sh` **PASS (12/12)**.

Full-suite note: monolithic `nb_all_tests.sh` rerun stalled after
`nb_span_select_test` without final summary (env stall). Post-span_select live
tests run individually all RC=0/PASS: raf, geometry, gbcr, gutter, flex, grid,
reach; bar tail run inconclusive.
