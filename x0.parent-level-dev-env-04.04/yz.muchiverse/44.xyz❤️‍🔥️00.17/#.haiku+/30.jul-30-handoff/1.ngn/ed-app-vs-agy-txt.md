# ed-app (widget-pair) -vs- agy-txt (self-contained) — Phase T6

Real comparison of two now-both-green, level-2-harness-proven text
editors, built the same week, from the same reuse base, to close out
`102.agy-txt/PLAN.md`'s own Phase T6. This is the SMALL, CONCRETE
rehearsal `todo-j30.txt` §5 names directly (line 251-253: "found while
building 102.agy-txt as this exact phase's own smaller-scale
rehearsal") for the much bigger, still-unstarted rpg-xyz (Track A) vs
rtp-xyz (Track B) decision. Read this before starting either track —
every number and finding below is real, not projected.

--------------------------------------------------------------------------------
## 0. The two things being compared

| | ed-app (widget-pair) | agy-txt (self-contained) |
|---|---|---|
| Location | `102.editor-📄️00.00` + `&.widgits/file-menu`, registered together as `@.apps/text-editor-xyz` | `102.agy-txt`, one project |
| Process model | **2 separate sessions/processes** (own `pieces/sessions/` each) | **1 session/process**, all 4 layouts |
| Layouts | 1 (`editor.chtpm`) + 1 (file-menu's own, buttonless) | 4 (`editor.chtpm`, `file_menu.chtpm`, `file_browser_save.chtpm`, `file_browser_load.chtpm`) |
| Tool-switch mechanism | cross-process wake/focus (`'4'=FM` keystroke → the already-running widget) | same-process `href`/digit-jump nav, `current_layout.txt` changes, no relaunch |
| Op LOC | 1058 (editor) + 1425 (file-menu) = **2483** | **1220** |
| Harness scenario LOC | 473 (`demo_save.sh` + `demo_load.sh`) | 257 (`demo_save_load.sh`) |
| Harness result | 2 scenarios, both green | 1 scenario, 8/8 assertions green |

This is Track A's shape (separate widget processes per tool) vs Track
B's shape (one process, in-session layout switch) at 1/10th scale —
confirmed by re-reading `todo-j30.txt` §4 PHASE B2's own description
of rtp-xyz after agy-txt was already built: it is the same "`<module>`
tags... switching via href/F-key WITHIN ONE SESSION" architecture,
just for map/event editing instead of text.

--------------------------------------------------------------------------------
## 1. Reuse: the real numbers, not the plan's estimate

PLAN.md §2 proposed reusing 4 ed-app ops. What actually happened:

- **`agy_widget_cmds.c`** — copied from `editor_widget_cmds.c`. Real
  diff: 3 lines (a comment header, a usage-string, and
  `project_id=agy-editor`→`agy-txt`). Zero logic changes. This op
  never referenced file-menu at all — it only touches its own
  `project_root`, so cross-project reuse was genuinely free.
- **`agy_edit_key.c`** — copied from `editor_menu_input.c` (441 lines)
  and extended to 604 lines (187 diff-lines: the file_menu/
  file_browser dispatch layer, PASTE mode's `on_browser` branch, path-
  buffer handling — all genuinely new work agy-txt needed that ed-app
  didn't).
- **`agy_compose_view.c`** — copied from `editor_compose_frame.c`, 74
  diff-lines (adds `compose_file_browser()`).
- **`agy_compose_stub.c`** — Phase T2 scaffold, editor's own
  equivalent shape, not a byte-for-byte copy (small, throwaway).

Net: roughly half of agy-txt's 1220 op-LOC is inherited near-verbatim,
half is genuinely new (the file_menu/file_browser layer ed-app's own
architecture never needed, because ed-app delegates that entire
concern to a SEPARATE, SEPARATELY-PROCESSED widget instead).

**Direct read for Track A/B**: rpg-xyz and rtp-xyz share far MORE
underlying data model (switches.pdl, event.pal command IR, per B2's
own instruction) than ed-app and agy-txt did (ed-app's own file-menu
is a generic, project-agnostic widget; agy-txt had to grow its OWN
file-menu-shaped logic inline). Expect Track B's real reuse percentage
against Track A's already-built pieces to be HIGHER than the ~50%
seen here, not lower — the harder, newer part (event interpreter,
data shape) is being built ONCE and shared by design, whereas here the
newer part (inline file-menu logic) was accidental duplication, not
planned sharing.

--------------------------------------------------------------------------------
## 2. Two real bugs, found independently, fixed the same way — proof of B3

`todo-j30.txt` PHASE B3 predicts: "Track B should get CHEAPER than
Track A over time... the second track paying down the first track's
own design cost." This happened in miniature, twice, while building
agy-txt:

**Multi-char typing must go through PASTE, not raw per-character
relay.** `test-harn-ed-app`'s own `demo_save.sh` hit this FIRST — a
marker string typed character-by-character through `interact_relay.txt`
came out truncated even with a 50ms/char throttle (a second bottleneck
at the relay hop, beyond the throttle). Fixed there with `ed_paste()`,
calling `editor_menu_input.+x PASTE "<text>"` directly. Because
`agy_edit_key.c` was COPIED from `editor_menu_input.c`, it had already
inherited that exact PASTE branch, unmodified, before agy-txt's own
harness was ever written. When agy-txt's harness hit the identical
symptom (0 characters landing under full automation), the fix was
`ag_paste()` — same shape, same op, same call convention — and it
passed on the first try. The fix was paid for once, by ed-app; agy-txt
inherited it for free.

**Digit-key `jump_to()`, not counted arrow presses, for deterministic
nav.** This one agy-txt found FIRST, that ed-app's own harness never
needed to: `chtpm_parser_pal.c`'s own ARROW_UP/DOWN nav WRAPS AROUND
cyclically for buttoned layouts (confirmed by direct source read,
`if (focus_index < 0) focus_index = element_count-1;`), it does not
clamp. Ed-app's harness never hit this because its own layout only
ever has ONE focusable button (`EDIT TEXT (INTERACT)`) — no multi-item
nav to get wrong. Agy-txt's 4-layout, multi-item nav surfaced it
immediately. This is now documented (PLAN.md, this project's harness
header comment) as a reusable finding for BOTH future tracks — any
CHTPM-buttoned layout with 2+ focusable elements should use digit-jump,
never counted arrows, for harness determinism.

**Read for Track A/B**: expect this pattern to repeat. Whichever track
starts SECOND on a given sub-feature will find some of its bugs
already fixed by the first track's own harness discovering them —
literal cost-sharing, not a hope. Track A is currently ahead on
individual proven pieces (file-menu, tile-picker, map-picker already
have level-1 proof per `todo-j30.txt` §5) — expect Track B to inherit
fixes from those FIRST, the same way agy-txt inherited PASTE from
ed-app.

--------------------------------------------------------------------------------
## 3. The finding that matters most: level-2 synthetic injection has a real gap

This is new since `todo-j30.txt` §5 was written, and changes its own
"both harnesses must be level-2 from the START" guidance in one
important way.

**What happened**: real human keyboard testing of agy-txt (not either
project's harness — genuine physical typing) showed every character
duplicated: "testing" landed as "tteessttiinngg". Root-caused by direct
source read plus one live isolation test (a single manual write to
`interact_relay.txt` produced exactly one character — the read side,
`agy_edit_key.c`'s main loop, was innocent). The real cause: TWO
independent capture paths both deliver the same physical keystroke into
`interact_relay.txt` whenever a real `gl_mirror` GL window is open —
(1) `gl_mirror.c`'s own GLUT keyboard callbacks append directly, and
(2) `system/keyboard_input.c`'s terminal capture → `pieces/keyboard/
history.txt` → `chtpm_parser_pal.c`'s `process_key()` INTERACT branch
→ `inject_raw_key()`, a second, independent forward of the same key.
`gl_mirror.c` already had the intended fix mechanism sitting unused —
`pieces/system/gl_focus.lock`, written while it's alive, with its own
header comment claiming "keyboard_input.c was updated to check the
same lock" — confirmed false, no reader anywhere actually checked it.
Fixed 2026-07-30 in the shared `chtpm_parser_pal.c` (skip the
`inject_raw_key()` forward when the lock is held), rebuilt into both
agy-txt and 102.editor's own `system/` from the same wsr-pal source,
confirmed fixed by the user via live re-test.

**Why this matters for Track A/B specifically**: this bug is in
`chtpm_parser_pal.c` and `gl_mirror.c` — SHARED system binaries every
project in the house compiles from the same source, including
whatever rpg-xyz's widgets and rtp-xyz's modules end up using for
INTERACT typing (item names, save-file names, chat/dialogue text
entry, anything typed while a real GL window is open). It was
INVISIBLE to both ed-app's and agy-txt's own "level-2" harnesses,
despite both being real key-injection, because synthetic injection
(writing directly into `interact_relay.txt`, or calling an op's own
PASTE mode) only ever produces ONE write per intended character,
deliberately — it can't reproduce a race between two independent OS-
level capture processes. A harness can be fully level-2 by this
house's own existing definition and still never see this class of bug.

**Recommendation for both tracks' own full-loop harnesses (§5)**: keep
level-2 synthetic injection as the default (it's fast, deterministic,
CI-friendly, and it's what caught both bugs in §2 above) — but budget
at least one genuine OS-level input test per track, for whichever
feature involves real INTERACT typing with a real GL window open
(`xdotool key`/`xdotool type` against the actual `gl_mirror` window, or
literal human testing before calling a save/load or event-editing
feature done). Synthetic-only level-2 proof is necessary, not
sufficient, for anything that types while GL is active.

--------------------------------------------------------------------------------
## 4. Verdict on `todo-j30.txt` §5's own predictions

§5 predicted: "Track A... reach individual pieces faster... but Track
B to potentially close faster on FULL-LOOP once started, since B2's
own module-switching risk, once proven once, de-risks map/event/save
all at once within a single session — Track A's separate-widget-
processes model pays a re-launch cost per tool switch that Track B's
in-session layout-switch doesn't."

This rehearsal supports it directly. Ed-app's own harness needs TWO
session lookups (`find_editor_session` + `find_fm_session`) and an
explicit cross-process wake keystroke before file-menu can even be
addressed — real, measurable extra harness surface area versus
agy-txt's single `find_session()`. Once agy-txt's own single-session
nav model was proven (§2's digit-jump finding), the SAME session
handled edit, save, new, and load with zero re-launch cost, confirmed
via `ps aux` showing exactly one live process for the entire 8-
assertion scenario. This is the concrete, measured version of the
prediction — not a re-guess, an actual observation at small scale.

Per §5's own closing line, this does NOT pre-decide the winner for the
real rpg-xyz/rtp-xyz pair — B2's own module-switching risk (real
`<module>` respawn-on-href behavior, already found and documented in
PLAN.md §1.5) is a materially bigger unknown than anything text-editing
required. Treat this document as evidence for the prediction's
direction, not proof of the outcome.
