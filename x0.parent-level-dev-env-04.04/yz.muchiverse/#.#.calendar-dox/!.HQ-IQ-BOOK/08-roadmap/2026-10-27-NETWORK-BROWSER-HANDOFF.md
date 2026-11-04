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
