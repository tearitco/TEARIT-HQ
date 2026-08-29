> **PATH NOTE (added 2026-08-29, doc-audit pass):** the §4 table's flat
> per-mode history file paths (`db_hq_history.txt`, `events_hq_history.txt`,
> `chat_hai_history.txt`) are superseded as of 2026-08-29 — relay input
> now goes to per-PID files instead: `#.desktop/<mode>_history/<pid>.txt`.
> Writes to the old flat paths are silent no-ops now (a real incident
> this session: one process's test input landed in a different, live
> window of the same mode because of this exact flat-path collision).
> Everything else in this doc is still current.

# Harness Authoring Guide — 2026-08-27

**Purpose**: the single, canonical doc to read before building or
updating ANY test/demo harness in this house. `_.0.aigent-testing-k9.txt`
stays scoped to injection/nav testing PROCEDURE (its own stated rule:
"this file stays scoped to injection/nav testing procedure only... don't
put app-bug/incident writeups here") — this doc is where the broader
"how to actually build a harness" guidance lives: current real
convention, the PAL/events direction, feasibility findings, a priority
list of event commands worth building for harness authoring, per-feature
launch mechanisms, and camera-direction guidance for anything with a
real POV/camera.

---

## 1. Current real convention (unchanged, still correct today)

- **Bash, not Python** — confirmed house convention
  (`_.0.aigent-testing-k9.txt`'s own 2026-08-25 correction). One
  self-contained script per feature, copy-paste-and-adapt from the
  closest existing one, not a `source`d shared library.
- Real, working examples to copy from: `cursword/harnesses/
  events_hq_task3_test_harness.sh`, `*.monads/*.muchi-pet/harnesses/
  common_events_manager_test_harness.sh`,
  `cursword/harnesses/bookmark_badge_contrast_fix_harness.sh`.
- Order of preference for driving/verifying a khtpm-family window: (1)
  the real relay file, (2) a cheap text state dump, (3) PNG dump last —
  full detail in `_.0.aigent-testing-k9.txt`.
- PNG dumps for events-hq/db-hq/chat-hai now force a fresh redraw before
  capture (fixed 2026-08-27, real bug + fix documented in
  `_.0.aigent-testing-k9.txt`), and each has a real frame-history signal
  file (`events_hq_frame_history.txt`/`db_hq_frame_history.txt`/
  `chat_hai_frame_history.txt`) — poll its latest line's STATE FIELDS
  (not just a bumped `seq=`) before triggering a dump.
- **Navigation must be visible, not just before/after** — capture the
  actual focus-stepping/typing sequence (see k9 doc's own "a
  presentation that only shows BEFORE/AFTER STATE looks dead" section).
  Litmus test: if a frame could've been produced by hand-editing a state
  file, it hasn't proven the UI works.
- **Disposable fixtures only** — never edit a pre-existing real entity's
  own files (cursword's `event_pkg/pages/page_1/`, `greet_player`,
  `shop_open`, etc.) to prove a point. Create a new, clearly-named
  disposable entity/common-event/switch, and delete it when done. This
  house has hit this exact mistake twice this session already — don't
  make it a third time.

---

## 2. Camera/POV direction guidance (NEW, 2026-08-27, direct instruction)

Direct instruction: "there is also pov/camera control, which u should
act as director of showing meaningful angles when making test
presentations if possible."

Confirmed real camera control exists in two places:
- **Mutaclysm** (`101.mutaclsym🧟‍♂️️19.00/ops/camera_control.c`, real
  compiled op) — pan/look/mode-switch, gated by the real `interact_mode`
  engagement state (k9 doc Rule 10).
- **board-viewer** (`&.widgits/board-viewer/ops/bv_render_3d.c`) — real
  `camera_mode` (0/1/2, first-person/orbit/etc.), yaw/pitch, and a
  look-down angle — this is the map-view window piececraft-xyz opens
  (see MARKETABLE-FEATURES.md's Toys section).

**When capturing a presentation for anything with real camera control**:
don't just accept whatever angle the camera happens to be at by default
— act as a real director. Concretely:
- Before capturing the "proof" frame for a feature, move the camera (via
  its real relay/key input) to an angle that actually SHOWS the thing
  being demonstrated (e.g., frame the terrain change from mining/
  building once that's real, not a wall-texture close-up; frame the
  xelector/player entity when showing movement, not empty sky).
- If multiple camera modes exist (first-person vs. orbit), pick whichever
  one makes the feature legible to a human viewer, not whichever is the
  default at launch.
- Treat this the same as the "show real navigation" rule above — a
  technically-correct-but-poorly-framed capture is still a weak proof,
  even if nothing is technically wrong with it.
- Real, currently disclosed limitation: since MOVE is the only fully-real
  action in piececraft-xyz today (JUMP/MINE/BUILD are honest stubs, see
  MARKETABLE-FEATURES.md), don't frame a shot implying JUMP/MINE/BUILD
  have a visible effect — frame those captures around the real ledger/
  tick-advance proof instead, and say so honestly in the caption.

---

## 3. PAL/events direction for harness authoring (moved here from
`PAL-VISUAL-SCRIPTING-PLAN.md`, which now just points at this section)

Direct instruction (2026-08-27): "eventually the harnesses will be made
with pal scripts, calling/injecting to events. (and we will even
replacing writing c, with writing events) so if u can move in that
direction anytime, do so" — clarified: "(ops for harnesses etc)",
meaning this covers the OPS harnesses/tests call (relay injectors, PNG
dumpers, state-checkers), not just in-game command logic.

### 3a. Real feasibility check (done 2026-08-27, grounded in actual
`prisc+x.c` code, not assumed)

**Already possible TODAY, with zero VM changes**:
- **Relay injection**: `SYS_OPEN` (mode=2, append) + `SYS_WRITE_LINE` +
  `SYS_CLOSE` (already-real, already-used syscalls,
  `101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x.c` ~lines 712-716) let a PAL
  script append a real relay code (a digit-jump, Enter, or the `112`
  PNG-dump trigger) to any `*_history.txt` file — this is a real
  injection mechanism, not hypothetical.
- **Polling a single-key flat file**: `SYS_GET_KV_INT` (~line 792) can
  already read a `key=value` file and branch on it via the already-built
  Conditional Branch command (Task 3).

**One real gap found**: `SYS_GET_KV_INT` only matches a key at the very
START of a line (`strncmp(line, key, klen) == 0`) — it CANNOT read
today's frame-history lines (`seq=1 focus_nav=1/13 view_mode=0 ...`,
multiple keys per line, none at line-start except the first). Two real
fixes, neither needs a new VM opcode:
1. **Recommended, cheap**: have `evhq_append_frame_history()`/
   `dbhq_append_frame_history()` ALSO write small sibling single-key
   files (e.g. `events_hq_view_mode.txt` containing just `view_mode=N`)
   alongside the existing multi-field log line — directly pollable by
   `SYS_GET_KV_INT` today, no VM change.
2. A new syscall that scans for a key anywhere in a line (not just at
   position 0) — more general, more work, not needed if (1) is done.

**No `SYS_SLEEP` exists** — pacing between injected keys currently needs
either a busy-poll loop (real, works, just burns CPU while waiting) or
falling back to TEMPLATE/`OP_EXEC` with a real `sleep` shell call for
that one step (acceptable per existing PAL-vs-TEMPLATE doctrine: "PAL
genuinely can't express it" is exactly this case until Wait exists).

**Still needs TEMPLATE/exec, and that's fine**: launching the target
render binary itself, and invoking `make_presentation_video.py` at the
end — matches the house's own existing PAL-vs-TEMPLATE doctrine, not a
gap.

### 3b. Priority list — "harness-friendly events" worth building next

From the RPG-Maker vocabulary already staged in
`EVENTS-PAL-BUILDOUT-PLAN.md`, in priority order for UNLOCKING
PAL-native harness authoring specifically (not general game-readiness —
see `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` for that separate
priority order):

1. **Conditional Branch + Control Variables** — already built (Task 3).
   Immediately usable today for "poll until field matches" once 3a's
   sibling-file fix lands.
2. **Loop** — **BUILT (2026-08-27)**: `Loop` / `Break Loop` / `Repeat
   Above` commands landed in `event_commands.registry.pdl` as tier-3
   compile_page() cases (LoopFrame stack in `khtpm_events_hq_manager.c`).
   A "poll until ready" harness step now compiles to a real backward
   `j _loop_N` / forward-break `j _loop_end_N` pair instead of
   hand-unrolled repeated Conditional Branches. Proven live in
   `cursword/harnesses/pal/wait_loop_break_demo.pal` (see 3a-proof3).
3. **Wait** — **BUILT (2026-08-27)**: the `wait` command compiles to the
   VM's EXISTING `sleep <micros>` opcode via `PAL sleep {ms}000` (W-1
   decision — NO new `SYS_SLEEP` syscall was added, zero VM change; ms→µs
   is unit conversion on the numeric literal). Real pacing between
   injected keys, not a busy-poll. Note `sleep 0`/negative no-ops in the
   executor.
4. **"Send Input"/"Inject Key" command** — **BUILT (2026-08-27)**: the
   `send_input` event command wraps SYS_OPEN(append)→WRITE_LINE→CLOSE
   into 10 PAL registry lines (one command per keypress instead of three
   per keypress), using the proven `addi x14,x12,0` fd-stash idiom.
   CLOBBERS x14 — keep no live value in x14 across it.

### 3a-proof. REAL, WORKING proof-of-concept (2026-08-27) — this is not
hypothetical anymore

Built and verified live: `cursword/harnesses/pal/task5_view_tab_switch_
demo.pal` — a hand-authored `.pal` script that drives Task 5's events-hq
view-tab switch END TO END using ONLY real `prisc+x` syscalls, no bash
relay-injection for the actual drive sequence:
1. `SYS_OPEN` (append mode) on `events_hq_history.txt`, saves the fd in
   `x14` (kept safe across subsequent syscalls that clobber `x12`).
2. `SYS_WRITE_LINE` twice (`"50"` then `"13"` — digit '2' then Enter),
   `SYS_CLOSE`.
3. A real poll LOOP (`_poll:` label, `SYS_GET_KV_INT` on the new sibling
   file `events_hq_view_mode.txt`, `beq`/`j`) — waits for `view_mode=1`
   to actually appear, not a fixed sleep.
4. Once ready, re-opens the relay and injects `"112"` (PNG-dump code).

Verified live: ran via `prisc+x /tmp/pal_harness_task5_demo.pal` against
a real running events-hq instance, exit code 0, `events_hq_view_mode.txt`
read back `view_mode=1`, `events_hq_history.txt` contained the exact
`50`/`13`/`112` sequence, and the resulting PNG genuinely showed the
Scratch tab focused and active. Zero stray processes before/after.

**Real, honest limitations of this first proof** (don't oversell it):
- The house-root path had to be hand-embedded as a literal string in the
  `.pal` file itself — no PAL-level path/variable interpolation exists,
  so this script is not portable to a different house root without
  editing it.
- No `SYS_SLEEP` exists, so the poll loop is a real busy-loop (bounded
  only by how fast `prisc+x` can re-read the file) — fine for a quick
  test, would need real pacing for anything longer-running.
- This was hand-authored directly as `.pal` text, NOT authored through
  the real events editor UI (no Loop/Wait/Send-Input commands exist yet
  to make that possible visually) — it proves the underlying VM/syscall
  layer is ready, not that the AUTHORING experience is ready yet. §3b's
  priority list is what closes that gap.

This is real evidence the direction in §3 is achievable now, not just a
future vision — use this file as the template for the next PAL-authored
harness step, rather than starting from a blank file.

### 3a-proof2. SECOND real proof (2026-08-27, same day) — a headless
legacy-harness PORT, not just a UI-driving demo

Built and verified live: `cursword/harnesses/pal/task3_switch_branch_
verify.pal` — a full PAL-authored PORT of `events_hq_task3_test_
harness.sh`'s real runtime check (switch ON -> true branch fires;
switch OFF -> false branch fires), fully self-contained, no bash
orchestration at all:
- `SYS_SET_KV_INT`/`SYS_GET_KV_INT` to set and read the test switch.
- The exact same branch shape the real compiled `task3_if_else.pal`
  uses (`bne`/`j`/labels).
- `SYS_OPEN` (read mode) + checking the returned fd against `-1` to
  verify a marker file's existence, `SYS_SET_KV_INT` to record a real
  PASS/FAIL result (`on_marker_exists=1`, `off_marker2_exists=1`).

Verified live: `prisc+x task3_switch_branch_verify.pal`, exit 0, both
results read back `=1` (PASS), and only the two EXPECTED marker files
existed afterward (on-marker for the ON step, off-marker for the OFF
step) - confirming both branches fired correctly, not just that the
script ran without crashing.

**This is a genuinely different, more significant proof than 3a-proof**:
that one drove a live GUI window (relay injection + polling); this one
is a full, headless PORT of an existing bash test's actual verification
logic into pure PAL - the first real evidence that "replace some legacy
harnesses with PAL" (direct instruction, 2026-08-27) is achievable
today for the simple, linear (no Loop/Wait needed) cases.

**What made this one portable today, and what would block a HARDER
one**: this harness never needed a real loop or a real sleep - two
sequential switch-set/branch-check steps, no waiting for an async
process. `common_events_manager_test_harness.sh` (Task 4, Autorun/
Parallel) was considered as the next port target and REJECTED for now:
it manipulates `switches.txt` with multiple keys via a bash heredoc and
verifies the ledger via `grep -c` PATTERN COUNTING across a whole file
- `SYS_GET_KV_INT` can't count pattern occurrences, only look up one
exact key. Porting that one needs either a new syscall (count matching
lines/pattern) or the real Loop/Wait commands from §3b to iterate a
read-one-line-at-a-time count manually. Correctly deferred, not a
failed attempt.

### 3a-proof3. THIRD real proof (2026-08-27, same day) — the Loop/Wait/Send-Input pass, all compiled shapes

Built and verified live: `cursword/harnesses/pal/wait_loop_break_demo.pal`
— the first harness demonstrating all the new event-command
primitives in their EXACT compiled shapes:
- `wait` → the real `sleep <micros>` opcode (`sleep 100000`), proven by
  wall-clock pacing (0.20s = exactly 2×100ms sleeps = exactly 2
  iterations).
- `loop`/`break_loop`/`repeat_above` → `_loop_1:` + body + `beq`-to-break
  `j _loop_end_1` inside the true branch + `j _loop_1` backward + the
  `_loop_end_1:` label emitted right after (break lands past the loop).
- `control_switch` (post-loop `done=1`), proving the break exits to
  post-loop code, plus a real PASS(1)/FAIL(0) verdict.
Verified live: `done=1 pass=1`, exit 0; independently re-run by Sonnet
at sign-off (`done=1 pass=1`, 0.217s wall — matches).
The same shapes were also proven through the REAL compile+play path on a
disposable `common_events/loop_probe/` fixture (manager `edit:` force
recompile → `event.pal` read-back showed exact `_loop_1:`/`j _loop_1`/
`_loop_end_1:` ordering → real play ran the body 3×, a mid-run `run=1`
flip broke the loop, `done=1` written post-loop). See the handoff's
2026-08-27 ox-alpha EXECUTION RECORD for the full evidence trail.

### 3a-switchvals. Switch values ARE integers (1/0), never "ON"/"OFF" strings

Real convention note (2026-08-27, from Sonnet's review of the
Loop/Wait/Send-Input pass): the switch board (`switches.txt`) stores
INTEGERS — `Control Switch` and `Conditional Branch` are the only
surfaces that speak ON/OFF, and they normalize to 1/0 at the UI/picker
layer, never at storage. If a harness (or any tooling outside that
picker path, e.g. `send_input`-adjacent test code) writes to
`switches.txt` directly, it MUST write `run=1`/`done=0`, NOT
`run=ON`/`done=OFF`: a manually-`echo`'d `run=ON` gets parsed back as 0
by `SYS_GET_KV_INT`, so a `Conditional Branch compare=on` will never
match and a poll-loop will spin forever (hit this live during the
loop_probe runtime test; `run=1` worked). No normalization code is
added at the storage layer on purpose — that would re-hide the exact
string-to-magic-value coercion this doc family already rejected for
Control Switches.

### 3a-comments. Comments are real and required in every PAL harness

Confirmed live (2026-08-27): `prisc+x`'s parser supports both full-line
comments (`# ...` at line start) and inline trailing comments after a
real instruction (`li x15, 1   # SYS_OPEN`) - verified by direct test,
not assumed. Both saved example harnesses (§3a-proof, §3a-proof2) were
rewritten with a comment on every real instruction line plus a header
explaining every register's role and every file touched, then
re-verified to still produce byte-identical results - use them as the
real formatting template. **Comment every line of any new PAL harness**
so a human can read and learn from it without cross-referencing
`prisc+x.c`'s syscall table from memory - this is now the real house
convention for harness `.pal` files, not optional polish.

### 3c. Practical effect on any harness built between now and when 3a/3b land

Build new harnesses (e.g. `cursword/harnesses/marketing_demo_harness.sh`,
being planned as of this doc's writing) as bash for now, per §1 — but
shape each feature's steps as small, separable actions (inject X, wait
for Y, capture Z) so a LATER pass can port individual steps onto real
PAL/event primitives once 3a/3b exist, without redesigning the harness's
overall shape. Note this doc (specifically this section) in the new
harness's own header comment.

---

## 4. Per-feature real launch mechanisms (reference table, verified
2026-08-27 — use this instead of re-deriving launch commands per pass)

| Feature | Launch | Input relay | Capture |
|---|---|---|---|
| Taskbar HQ menu | already running (real desktop) | `#.desktop/livedesk_agent_relay.txt`, bare-decimal ASCII | `#.desktop/livedesk_open.txt` registry; live desktop, no dump needed |
| db-hq (+ Task 5 view-tabs) | `*.monads/*.muchi-pet/ops/open_db_hq.sh <house_root>` | `#.desktop/db_hq_history.txt`, bare-decimal ASCII | `dbhq_dump_debug_state()` (code 210) + `dump_frame_png()` (112, forced-redraw-fixed) + `db_hq_frame_history.txt` |
| events-hq | see `cursword/harnesses/events_hq_task3_test_harness.sh` for the exact launch line | `#.desktop/events_hq_history.txt` | same pattern + `events_hq_frame_history.txt` |
| Palettes (incl. chemistry) | runs inside db-hq mode, `livedesk:open-palette:<category>` | `db_hq_history.txt` | same as db-hq |
| Bookmarks | runs inside db-hq mode | `db_hq_history.txt` | same as db-hq |
| Entities/desk-pals | already running (`tp_desktop_window_rgb.+x`, one per pal) | `<pal>/interact_relay.txt`, STRING commands (`OPEN_CONTEXT`, `RUN_METHOD:<label>`, `CLOSE`, `RAISE`, `FOCUS_NAV:<n>`, `ACTIVATE_NAV:<n>`) | per-pal `history.txt` |
| Mutaclysm | `101.mutaclsym🧟‍♂️️19.00/button.sh run` | `pieces/apps/player_app/interact_relay.txt` + `pieces/keyboard/history.txt` (`[TIMESTAMP] KEY_PRESSED: N`), gated by `pieces/display/current_layout.txt` and `interact_mode` | `dump_rgb_png.+x` (reads raw frame buffer directly) |
| h-ai single chat | `&.widgits/open-hai/button.sh <house_root>` | its own history file in the merged binary's open-hai mode | `dump_frame_png()` |
| chat-hai (4-agent) | `&.hq-apps/chat-hai/button.sh <house_root>` | `chat_hai_history.txt` | `chai_dump_frame_png()` (forced-redraw-fixed) + `chat_hai_frame_history.txt` (the ORIGINAL precedent this session's events-hq/db-hq fix was ported from) |
| my-lawyer / my-biotech | `@.apps/my-lawyer/button.sh run` / `@.apps/my-biotech/button.sh run` | `pieces/keyboard/history.txt` (`[TIMESTAMP] KEY_PRESSED: N` — chtpm_parser_pal family, NOT bare-decimal) | `pieces/debug/frames/session_frame_history.txt`; reuse their own real `test-harn-same/demo_*.sh` scenarios |
| piececraft-xyz (Toys) | terminal: `@.apps/piececraft-xyz/button.sh run`; map-view: real separate board-viewer GL window opened via `OPEN_BOARD_WIDGET` | terminal phase reads real stdin; map-view window reads its own `pieces/keyboard/history.txt` (chtpm_parser_pal family), gated by an INTERACT-engaged state (same class as Mutaclysm's `interact_mode`) | board-viewer's own frame dump (see its `ops/bv_compose_frame.c`) |
| Taskbar file/desk switching | already running | `livedesk_agent_relay.txt`, `livedesk:new`/`save`/`load`/`desks` | `#.desktop/livedesk_open.txt` |

**No house-wide multi-binary kill sweep exists** — every current harness
only greps/kills its OWN single binary pattern. A multi-feature harness
(like a marketing-demo one) must build its own aggregate `pgrep`/`pkill`
list covering every binary it touches, in its own cleanup trap.

---

## 5. Index

Indexed in `INDEX.md` — read this doc before designing ANY new harness,
whether single-feature or a multi-feature demo traversal.
