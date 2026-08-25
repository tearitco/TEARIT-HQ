# PLAN.md — 102.agy-txt: self-contained text editor (slop-ed-dev style)

## 0. Thesis

`102.editor-📄️00.00` + `&.widgits/file-menu` is the WIDGET-TOOLCHAIN
text editor — two separate processes/GL windows, connected over a
relay-file cmd-bus, real-key-injection-proven this session (PITFALL 58
found and fixed, `test-harn-ed-app` green).

`102.agy-txt` is its missing self-contained sibling — ONE manager
process, CHTPM `<module>`-owned layout switching, no second widget
process. This is the exact same pairing as `300.rpg-xyz` (widget) vs
`300.rtp-xyz` (self-contained) — see `1.ngn/todo-j30.txt` for that
pairing's own plan, which this document mirrors on purpose. Building
this pair for TEXT EDITING first is cheaper and faster to prove than
doing it first for the much bigger RPG surface, and a working proof
here directly de-risks `todo-j30.txt`'s own Track B (rtp-xyz) before
any of that larger work starts.

Reuse target, per direct instruction: as many real ops from
`@.apps/text-editor-xyz` (the widget-pair app — its own `102.editor-
📄️00.00` + `&.widgits/file-menu`) as actually fit, rather than
reimplementing from scratch or porting the TPMOS reference's own ops
verbatim.

## 1. The real reference, precisely (not from memory — read in full
   this session)

`1.TPMOS_c_+rmmp.0103.0001/projects/agy-text-editor` is a REAL, working
self-contained editor with this exact shape:

- **One manager binary** (`agy-text-editor_manager.c`) — after a real,
  documented 2026-07-11 refactor, it is a thin orchestration/
  presentation layer only. Business logic lives in 4 separate,
  reusable, CLI-testable ops it `fork()+execv()`s into:
  - `file_copy.+x <src> <dst>` — atomic copy (tmp+rename)
  - `dir_browse.+x <dir> <search_query> <output_path>` — real live
    directory listing, dirs-first, case-insensitive substring filter
  - `text_edit_key.+x <document> <cursor> <key_code>` — one keystroke,
    2D `(cursor_x, cursor_y)` model, separate `insert_text()` path for
    multi-byte UTF-8 (emoji)
  - `text_editor_view.+x <document> <cursor> <w> <h> <output_path>` —
    scrolled viewport, camera-centers on cursor, plain text output
    (manager applies its own box-drawing markup on top)
- **4 layouts, all pointing at the same manager binary**: `editor.chtpm`
  (typing), `file_menu.chtpm` (hub: NEW/SAVE/SAVE AS/LOAD/back),
  `file_browser.chtpm` (real dual-mode load/save browser, live
  autocomplete), `load_slot.chtpm` (**dead/vestigial** — hardcoded
  "[Slots deprecated]" — do NOT port this one, it's scaffolding with no
  real backing data).
- **Two DIFFERENT layout-switch mechanisms, and this distinction
  matters for the port**:
  1. `href="X.chtpm"` — the chtpm_parser's generic click handler clears
     `current_module_path`/`current_module_pid`, forcing a fresh
     `fork()+execv()` of the manager. Non-disk-persisted in-memory
     state (e.g. `browser_mode`) is LOST across this transition. Used
     for editor↔menu↔load_slot and the browser's own Cancel.
  2. The manager's own `transition_to_layout()` — appends the target
     path to `layout_changed.txt`; the parser's poll loop updates
     `current_layout` WITHOUT clearing `current_module_path`, so
     `launch_module()`'s own guard short-circuits and the SAME process
     keeps running. Used only for menu→browser (Save As / Load File).
  So it "feels" like one continuous session because state is disk-
  persisted, not because it literally always is one process. **Decide
  explicitly which of these two mechanisms to use for which transition
  in the port — don't assume "self-contained" means zero respawns.**
- **Real save/load pipeline** (`file-op-dox.txt`, current/shipped
  behavior — the doc's own OLDER `save-as-pipeline-study+.txt` describes
  a prior, now-superseded era, kept only as design history): one
  `file_copy.+x` call per explicit Save/Save-As/Load action, NOT per
  keystroke. Keystrokes themselves DO already fork into
  `text_edit_key.+x` individually (contrary to what the older doc
  claims about "no external ops" — that was only ever true of the
  since-replaced legacy code). No polling/marker-wait needed for the
  copy itself — `run_op()`'s `fork()+waitpid()` is synchronous.

## 1.5. CORRECTION found while starting implementation — no manager.c
   (still true), but href DOES respawn the module (tested, not assumed)

The TPMOS reference's own "thin C manager daemon that forks into ops"
shape does NOT need porting - THIS house's own convention already has
a direct equivalent: `editor.chtpm`'s own real `pal/main_loop_chtpm.pal`
IS the manager, written in PAL bytecode instead of C. No custom daemon
binary needed - `system/prisc+x` (vendored via the same copy-from-
wsr-pal pattern every project uses) interprets this script.

**CORRECTION, found by actually building Phase T2's own stub and
testing it live (not by reading a code comment and trusting it):**
my first draft of this section claimed plain `href` navigation between
layouts keeps the SAME persistent module process alive, citing
`launch_module()`'s own guard (`strcmp(launch_str, current_module_path)
== 0 && current_module_pid > 0`). That claim was WRONG. Built the real
T2 stub (three real `.chtpm` layouts, identical `<module>` line each,
a real op that writes the composing process's own PID into the frame),
launched it for real, and injected three real `[TS] KEY_PRESSED: 13`
keystrokes via `pieces/keyboard/history.txt` to walk editor -> file_menu
-> file_browser -> confirmed via `ps aux`:

```
PID before nav 1: 1821693
PID after  nav 1: 1822388   (different)
PID before nav 2: 1822388
PID after  nav 2: 1823813   (different)
```

Reproducible, not a fluke. Root cause, found by reading
`chtpm_parser_pal.c` directly after the test contradicted the theory:
`cleanup_module()` (which unconditionally kills+reaps the current
module and clears `current_module_path`) has its own comment noting it
was fixed to "actually get called on every screen transition" - i.e.
the fix that comment describes was about ensuring cleanup ALWAYS runs
on an href transition (so a stale module never leaks), not about
avoiding a respawn. `launch_module()`'s own guard can only ever
short-circuit for something OTHER than an ordinary href click - there
is a separate `pieces/display/layout_changed.txt` pulse-file mechanism
in this same file (real, present, not investigated further this
session - a candidate for genuine same-process switching, worth reading
before Phase T3 if staying in-process ever becomes load-bearing) but
plain `href`, the mechanism this plan actually uses, is NOT it.

**This does not break the plan - it changes what Phase T2 actually
proves.** The reference project's own real, shipped design ALSO
respawns its manager on href (§1's own citation: "any purely in-memory
static... is lost/reset on these transitions") and works fine anyway,
because it - like every project in this house - keeps real state on
disk (`document.txt`, `cursor.txt`), never in a process-resident
variable that would need to survive a respawn. Phase T2's real proof
target is corrected to: **state persists correctly across a respawn**,
not **the same PID persists** - the second claim was never actually
necessary for a working editor, only assumed to be. `editor_buffer.txt`/
`editor_state.txt` (already the real, harness-proven state files from
`@.apps/text-editor-xyz`) are exactly this kind of disk state already -
nothing about T3/T4's own reuse plan (§2) changes because of this
finding.

## 2. What's reused from `@.apps/text-editor-xyz`, concretely

Real interfaces, confirmed by reading both sides this session — this is
the actual reuse map, not aspirational:

| Reference (TPMOS) op | `text-editor-xyz` equivalent | Reuse plan |
|---|---|---|
| `dir_browse.+x <dir> <query> <out>` | `fm_scan_dir.+x <dir> [filter]` (file-menu's own op — dirs-first, case-insensitive, already does almost the identical job) | **Reuse directly**, adapt only if the output-file-vs-stdout shape differs; this is the single closest 1:1 match between the two codebases |
| `text_edit_key.+x` (2D x,y cursor, separate `document.txt`+`cursor.txt`) | `editor_menu_input.+x <keycode>` (own model: single flat `editor_buffer.txt` + `cursor_pos` — a LINEAR position, not 2D x,y — confirmed via that op's own header comment) | **Do NOT force-fit** — the two cursor models are genuinely different (2D grid vs linear offset). Keep `editor_menu_input.c`'s own linear model (it's the one already harness-proven this session) rather than porting the reference's 2D one — this is a real, deliberate divergence, not an oversight |
| `text_editor_view.+x` (plain-text scrolled viewport) | `editor_compose_frame.c` (already renders the buffer for the widget-pair app) | **Reuse/adapt** — same job, adapt call signature to agy-txt's own layout needs |
| `file_copy.+x` (atomic tmp+rename copy) | `editor_widget_cmds.+x` (NEW/SAVE/SAVE_AS/LOAD/PING via an inbox file, already the PITFALL-58-fixed, harness-proven real save/load path this session built) | **Reuse the underlying save/load LOGIC**, not the inbox-file transport (agy-txt has no second widget process to relay from — the manager calls the same underlying operation directly, in-process or via a trimmed-down version of this op minus the cross-process inbox layer) |
| N/A (file-menu's own real menu nav) | `fm_menu_input.c` | Reference for the FILE MENU / FILE BROWSER layouts' own key-dispatch shape — agy-txt's manager needs equivalent logic, adapted to being one process instead of a separate widget |

## 3. Directory layout (house-native, not a literal TPMOS copy)

```
102.agy-txt/
  button.sh                    <- compile/run, house convention
  project.pdl                  <- project_id=agy-txt, entry_layout=...
  scripts/build.sh
  system/                      <- vendored prisc+x/chtpm_parser_pal/
                                   renderer/gl_mirror/chtpm_rgb_render
                                   (copy-from-wsr-pal pattern, matching
                                   editor's own scripts/build.sh)
  pal/main_loop_chtpm.pal      <- IS the manager (§1.5) - no daemon
                                   binary, prisc+x interprets this
  ops/
    +x/
    agy_dir_browse.c           <- adapted from fm_scan_dir.c (§2)
    agy_edit_key.c              <- editor_menu_input.c's own logic,
                                   trimmed of the cross-process inbox
                                   parts, kept as the SAME linear-
                                   cursor model
    agy_compose_view.c         <- editor_compose_frame.c's own logic,
                                   adapted for this project's layouts
    agy_file_copy.c            <- small, new - atomic copy, same shape
                                   as the reference's own file_copy.c
                                   (this one IS worth porting near-
                                   verbatim, it's tiny and generic)
  pieces/
    chtpm/layouts/
      editor.chtpm
      file_menu.chtpm
      file_browser.chtpm
                                <- NO load_slot.chtpm - §1 says it's
                                   dead weight in the reference, don't
                                   carry the dead weight into the port
    system/
      editor_buffer.txt
      editor_state.txt
    display/ ...
    keyboard/ ...
```

## 4. Phased build plan

Numbered to slot directly into `1.ngn/todo-j30.txt`'s own Track B
(rtp-xyz) as its de-risking precedent — phase names deliberately
parallel that document's own B1/B2/B3.

**Phase T0 (this document + directory scaffold)** — DONE as of this
write-up: reference read in full, reuse map written down, directory
shape decided.

**Phase T1 (= todo-j30.txt's own B1 "read the reference architecture
first")** — DONE, see §1.

**Phase T2 (= B2 "manager + module skeleton, prove layout-switching
FIRST") — DONE, 2026-07-30**: no daemon to write, per §1.5 - built the
ONE `pal/main_loop_chtpm.pal` and three layouts (`editor.chtpm`/
`file_menu.chtpm`/`file_browser.chtpm`), each declaring the identical
`<module>system/prisc+x pal/main_loop_chtpm.pal</module>` line, with a
real stub op (`agy_compose_stub.c`) that writes the composing process's
own PID into the frame. Launched for real, injected three real
keystrokes via `pieces/keyboard/history.txt`, tracked the module PID
across all three transitions via `ps aux`. Result, and the real reason
this phase existed: **`href` respawns the module every time** (three
different PIDs across three navigations, reproducible) - NOT what §1.5
originally assumed. Root cause and why this doesn't block the plan are
both in §1.5's own correction. Concretely proven: layout switching
itself (`current_layout.txt` updates correctly, the right `.chtpm` file
renders each time) works correctly via plain `href` - that half of the
phase's own goal holds up fine, it's specifically the "same PID"
assumption that was wrong and has been corrected.

**Phase T3 (real editing) — DONE, 2026-07-30**: `agy_edit_key.c`/
`agy_compose_view.c`/`agy_widget_cmds.c` copied near-verbatim from
`editor_menu_input.c`/`editor_compose_frame.c`/`editor_widget_cmds.c`
(all three already fully generic - zero project-specific paths beyond
the `project_id` string and a `piece.pdl` path, both fixed). Wired into
`editor.chtpm`'s own real `EDIT TEXT (INTERACT)` button + a real
`pal/main_loop_chtpm.pal` matching editor's own compose/read-relay/
dispatch/recompose shape. VERIFIED LIVE: launched for real, engaged
INTERACT via a real Enter keystroke (`active_gui_is_typing.txt` flipped
to 1), typed " it works" via real per-character `interact_relay.txt`
injection (0.05s throttle, matching this house's own PAL-loop-rate
constraint) - `editor_buffer.txt` shows the real typed content
(`hi agy-txt\n it works`) and the rendered frame shows the cursor
marker `[X]` tracking correctly. Real editing works end to end, not
just composes.

**Phase T4 (real save/load) — DONE, 2026-07-30**: shape changed from the
original plan once actually built - no `agy_file_copy.c`/
`agy_dir_browse.c` needed yet (directory browsing/autocomplete stayed
explicitly out of scope per §5; `agy_widget_cmds.c`'s own already-
reused `do_save_to()`/`do_load_from()` use plain `fopen()`, no separate
copy op needed for a same-machine save). `file_menu.chtpm` got real
NEW/SAVE buttons; `file_browser_save.chtpm`/`file_browser_load.chtpm`
(two layouts instead of one + a mode flag - a chtpm button can only
carry ONE action, href OR onClick, so there's no single button that
could set a mode AND navigate in one click) got a real PASTE+per-char
path field wired into `agy_edit_key.c`'s own extended dispatch, writing
`SAVE_AS:<path>`/`LOAD:<path>` directly into the same local
`widget_cmds/inbox.txt` `agy_widget_cmds.c` already drains - reused
unchanged, per §2's own reuse note.

**FOUND AND FIXED A REAL BUG IN THE ALREADY-SHIPPED 102.editor-📄️00.00
PROJECT WHILE PROVING THIS**: `button.sh`'s own `mkdir -p` list
included `docs` right before the `ln -sfn ... docs` line that's
supposed to symlink it to the durable house-level docs/ - pre-creating
it as a real directory makes `ln -sfn` nest the symlink one level too
deep instead of replacing it, so every relative `docs/...` SAVE_AS
path silently landed in an ephemeral, session-scoped directory that
gets deleted on exit. This bug was ALREADY PRESENT in editor's own
button.sh (byte-identical shape), never caught because `test-harn-
ed-app`'s own `demo_save.sh` always used an absolute save path, which
never exercises this code path. Full account: PITFALL 62 in
`!.xyzos-pitfalls+1.txt`. Fixed both button.sh files, verified via
`stat -L` inode comparison + a real save+load round trip through the
real running app.

VERIFIED LIVE, real key injection throughout: navigated FILE MENU ->
SAVE AS, engaged INTERACT on the path field, typed `docs/t4-fixed.txt`
via real per-character `interact_relay.txt` injection, confirmed SAVE
- real file appeared at the durable `102.agy-txt/docs/t4-fixed.txt`
with the real buffer content. Then triggered a real NEW (buffer went
empty), navigated to LOAD, typed the same path, confirmed LOAD - buffer
correctly reverted to the saved content. Both directions proven
through the real UI, not a shortcut.

**Phase T5 (level-2 harness, from the start, not bolted on later) - DONE
(2026-07-30)**: built `test-harn-agy-txt/` (`button.sh demo` ->
`scenarios/demo_save_load.sh` + `common.sh`), following
`test-harn-ed-app`'s own scenario shape as the direct template - real
key injection via `pieces/keyboard/history.txt` for nav (agy-txt is a
real CHTPM buttoned layout, same channel editor's own harness uses),
asserting the real rendered frame AND the real file on disk.

Full loop VERIFIED, 8/8 PASS: launch -> real INTERACT engage -> type a
unique marker -> exit INTERACT -> nav FILE MENU -> SAVE AS -> type a
path -> real SAVE -> assert the real file on disk at the durable path
contains the marker (PITFALL 62 regression covered) -> real NEW clears
the buffer -> nav LOAD -> type the same path -> real LOAD -> assert the
buffer is restored from disk, not just "looks unchanged".

Two real findings surfaced while building this harness (not assumed):
1. Digit-key `jump_to()`, not counted arrow presses - `chtpm_parser_pal
   .c`'s own ARROW_UP/DOWN nav WRAPS AROUND cyclically for buttoned
   layouts, doesn't clamp, so "press UP N times" is non-deterministic
   depending on where focus already was. `isdigit(key)` -> `do_jump()`
   is the real, deterministic channel - see the script's own header.
2. Multi-char typing must go through PASTE, not per-character
   `interact_relay.txt` injection. `test-harn-ed-app`'s own
   `demo_save.sh` hit this exact symptom first (marker truncated even
   with the 50ms/char throttle - a second bottleneck at the relay hop
   beyond the throttle) and fixed it with `ed_paste()`, calling the
   real op binary's own PASTE mode directly. `agy_edit_key.c` had
   already inherited that same PASTE branch verbatim from being copied
   off `editor_menu_input.c` (§2's reuse paying off directly) - added
   `ag_paste()` to `common.sh` as the same fix, used for the marker and
   both path fields. Passed on the first run once swapped in.

**Phase T6 (comparison) - DONE (2026-07-30)**: full side-by-side
comparison written up at `#.haiku+/30.jul-30-handoff/1.ngn/
ed-app-vs-agy-txt.md` - real LOC/reuse numbers, the two bugs each
harness found and handed the other for free (§2 there), and a new,
not-previously-known finding that changes `todo-j30.txt` §5's own
"level-2 from the start" guidance: synthetic key injection (this
project's own harness included) cannot see a real dual-input-capture
bug that only shows up with an actual GL window + real human typing -
found via live human testing THE SAME DAY this harness went green,
root-caused and fixed in the shared `chtpm_parser_pal.c`/`gl_mirror.c`
system layer (not agy-txt-specific), confirmed fixed live. See that
doc's own §3 for the full account and the concrete recommendation for
both rpg-xyz's and rtp-xyz's own future harnesses.

## 5. What's explicitly OUT of scope for the first real slice

- `load_slot.chtpm` and any save-SLOT concept (the reference's own
  version is dead weight; this house's own SAVE_AS/LOAD-by-path model,
  already proven in `test-harn-ed-app`, is the one being ported, not
  slots).
- The reference's 2D `(cursor_x, cursor_y)` model (§2 - staying with
  the linear model already proven in this house).
- Autocomplete in the file browser (real and nice-to-have in the
  reference, not load-bearing for proving the architecture - add after
  T5's harness is green, not before).
