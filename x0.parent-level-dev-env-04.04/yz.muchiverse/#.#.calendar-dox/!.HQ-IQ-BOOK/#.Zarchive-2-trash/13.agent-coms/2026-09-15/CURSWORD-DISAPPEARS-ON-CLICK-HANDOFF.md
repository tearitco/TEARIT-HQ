# Handoff: cursword disappears on click — RESOLVED 2026-09-15

**RESOLVED, same session, after quota was extended.** Real root cause:
`popup_draw_text()` (a shared helper) hardcoded `DefaultVisual`/
`DefaultColormap`, but cursword's own armed-only debug-log lines pass
its real 32-bit ARGB pixmap into it — a Visual/Drawable depth
mismatch, `RenderCreatePicture` BadMatch, and since this house installs
no custom `XSetErrorHandler()`, Xlib's own default handler printed the
error and called `exit(1)` with zero visible signal/crash trace. Fixed
generically in `popup_draw_text()` (real depth query + matching
Visual/Colormap, any future ARGB caller covered, not just cursword).
Full pitfall writeup, rules, and the still-open "install a real
XSetErrorHandler house-wide" follow-up:
`03-pitfalls/HOUSE_CODE_PITFALLS.md` #23.

Everything below this point is the REAL investigation log that led to
the fix — kept as-is (not rewritten) since the dead-ends and the
eventual `strace`-based method that actually cracked it are both
genuinely useful precedent for the next similar bug. Skip to "§4d" if
you just want the live-strace technique that worked.

---

## 1. The bug, exactly as reported

> "clicking cursword now deletes it from screen !? what happened it
> used to draw a yellow circle around it and allow it to change z
> levels/3d mode etc?"

Real, expected behavior (per the user, and confirmed this session as
real code that exists): clicking cursword (`g_is_cursword`, the real
player-controlled desktop selector entity — tile window shown in the
taskbar as `tile:cursword-CURS:🗡️`) should arm selection, draw a
yellow highlight circle, and let `c`/`v` keys change its Z-level
(`cursword_handle_camera_key()`, writes `desktop_pos.txt` + the shared
`#.desktop/desktop_active_z.txt` pointer — see this session's own DSR
z-level research, summarized in §4 below). **Currently, a click just
makes the window disappear instead.**

## 2. Real, known-good reference point

`/home/no/Desktop/github/xdb/44.xyz-house-1.00-09-04-064656/` — a full
house snapshot from 2026-09-04 where this bug did **not** happen. This
is the diff baseline. Do not edit anything under `xdb/` — read-only
reference.

Current, broken house:
`/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`

Relevant file both sides: `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`
(current: ~17-18k lines; the ButtonPress/ButtonRelease/KeyPress block
for tile/entity mode is around current line ~16642-16960, old snapshot
~12359-12650 — line numbers will have drifted further since this
doc was written, re-locate by content, not by line number alone).

## 3. What's already been RULED OUT this session (checked directly against the 09-04 diff, not guessed — don't re-check these)

- **`g_is_cursword` assignment**: changed from a hardcoded
  `strcmp(basename, "cursword")` to a data-driven
  `read_log_mode(package_dir)` reading `STATE|log_mode|1` from
  `meta.pdl` — a deliberate, documented 09-04-dated refactor. Verified
  both real `meta.pdl` files (`_.monads/_.cursword/entities/cursword/meta.pdl`
  and the live `xyzfs/.../livedesk/pals/cursword/meta.pdl`) both carry
  the row correctly, parser logic correct. **Not the bug.**
- **`build_shape_mask()` → `kh_build_shape_mask_generic()`** (inline
  loop refactored into a shared bitmap-building helper): read both
  fully, logic is equivalent. **Not the bug.**
- **FocusOut NotifyGrab-vs-NotifyNormal filter** (the historical cause
  of an identical-sounding self-close bug elsewhere in this house):
  already present and correct in current code. **Not the bug.**
- **Stale binary**: ruled out — binary rebuilt 14:00, process launched
  14:20, same session.
- **Tile/entity mode dispatch itself**: confirmed cursword really does
  run through `tp_main()` (tile/entity mode, `argc==2` dispatch), not
  the shared default-mode `redraw()`/click path. This session's OWN
  earlier default-mode-only edits (chrome/row-packing generalization
  in the flat-page layout path, the new joystick-focus marker write)
  live entirely inside default-mode's `redraw()` — tile mode never
  calls that function, so those edits are very unlikely to be
  involved. Worth a final confirmation but not the prime suspect.
- **The ButtonPress/ButtonRelease/KeyPress block itself**: a full
  targeted diff (old ~12359-12650 vs current ~16642-16960) turned up
  only two real changes, both benign: `XUngrabKeyboard` calls wrapped
  into a new `kh_ungrab_kbd()` helper (same real effect), and the 1-4
  ↔ 5-8 camera-key remap being abandoned (09-09-dated, unrelated
  design change, not a regression). **Not the bug, as far as this
  block goes** — but see §5, this only covers the block itself, not
  window-creation-time setup.

## 4. Real, confirmed background (from earlier this session's own research, relevant context)

- Z-level switching for desktop entities (including cursword) is a
  simple scalar mechanism, not a per-level file/grid format:
  `cursword_handle_camera_key()` (current file, `c`/`v` keys) does
  `g_entity_z += (v?1:-1)`, rewrites the clicked entity's own
  `desktop_pos.txt` (`x=/y=/z=`), and writes the new z into the one
  shared `#.desktop/desktop_active_z.txt` pointer file, then calls
  `bump_camera_changed(house_root)`. Every OTHER entity's own window
  polls `desktop_active_z.txt` each tick and shows/hides itself
  (map/unmap) depending on whether its own saved z matches — so
  Z-switching is a visibility filter over already-loaded windows, not
  a "load a new map" operation. (Full research: this session's DSR
  z-level investigation — see chat history if this doc's summary isn't
  enough; not re-copied in full here to keep this doc focused.)
- `khtpm-tp_main-globals-footgun` (house auto-memory, already known
  before this session): tile/entity mode (`tp_main()`) uses a LOCAL
  `Display` connection and never sets the file-scope `dpy`/`cmap`/
  `screen` globals the shared default-mode code (`alloc_pixel()`,
  `redraw()`, etc.) expects. Calling a default-mode-assuming function
  from inside tile/entity mode crashes or no-ops instead of working -
  use `tp_hex_pixel()`-style local-Display-safe calls instead. This
  was the user's own live hypothesis ("i think it could be cause we
  have cursword on a different render track or something?") - **not
  yet confirmed or ruled out**, the investigating fork ran out of time
  before checking this specific angle thoroughly. Real, live lead -
  check next.

## 4b. REAL, CONFIRMED, BIG negative result (2026-09-15, later same session)

**`tp_main()` itself is now RULED OUT ENTIRELY as the bug's location.**
Did a full wholesale swap: replaced the CURRENT `tp_main()` function
body byte-for-byte with the 09-04 snapshot's own `tp_main()` (only 2
purely mechanical fixes needed to compile — `draw_glyph_rgb()`'s
signature gained two params, `Visual*`/`Colormap`, since 09-04; passed
the window's own real `win_vis`/`win_cmap` locals, zero logic change).
Rebuilt clean, relaunched cursword with the fully-swapped binary,
**user tested with a real physical left-click — still disappeared,
identically.**

This is conclusive: the entire tile/entity-mode function that owns
cursword's window, click handling, arm/disarm, shape mask, and camera
code is now, for this test, IDENTICAL to the known-working 09-04
version — and the bug still reproduces. **The regression is NOT
inside `tp_main()`.** It must be in something `tp_main()` calls that
lives OUTSIDE that function - either elsewhere in this same file
(a shared helper used by both default-mode and tile-mode), or in
`khtpm_draw_core.c`/`khtpm_render_core.c`/`khtpm_css_parser.c` (the
shared, text-included core files both modes pull in), or genuinely
environmental (compositor/XWayland behavior difference between real
hardware input and everything tested so far).

**The swap was rolled back** (`git checkout` on `khtpm_core_render.c`,
confirmed clean, rebuilt) — do not re-attempt a `tp_main()`-only swap,
it's a dead end, confirmed twice now (once via exhaustive line-by-line
diff finding no meaningful difference, once via wholesale swap-and-
retest finding no behavior change either).

## 4c. Real, ruled-out-by-elimination scope for next investigation

Given §4b, the search space for next steps should shift AWAY from
`tp_main()`'s own ~2500 lines and toward:
- Shared helper functions `tp_main()` calls that are defined OUTSIDE
  it in the same file (anything above line ~14925 or below ~17383 in
  the current file - `draw_glyph_rgb()` was one such function that
  changed signature since 09-04; there may be others whose BEHAVIOR
  changed, not just signature, that are worth a real diff).
- The shared, text-included core files (`khtpm_draw_core.c`,
  `khtpm_render_core.c`, `khtpm_css_parser.c`) — these are `#include`d
  into `khtpm_core_render.c` and used by BOTH default-mode and tile-
  mode, so a change there could affect cursword without touching
  `tp_main()` at all. This session's OWN earlier work touched
  `khtpm_draw_core.c` (the theme-accent-color / default-text-color
  changes) - already checked those are additive/default-only and
  shouldn't affect cursword's own explicit-color draws, but a fresh,
  careful look wouldn't hurt given everything else has failed.
- A genuinely environmental cause (compositor, XWayland version,
  something about how a REAL physical click differs from every
  synthetic test attempted so far) - real physical clicks could not be
  reproduced by this investigation at all (xdotool synthetic clicks on
  a freshly-launched cursword window, both pre- and post-swap, worked
  fine and armed correctly - only the user's own real click fails).
  This asymmetry (synthetic always works, real never does, even across
  a full tp_main() swap) is itself a real, significant clue - worth
  investigating what's DIFFERENT about the user's real input path
  (their actual mouse/compositor setup) rather than assuming it's pure
  application code at all.

## 4d. REAL, DEFINITIVE evidence (2026-09-15, live strace on the actual bug)

**Also ruled out**: `livedesk_ensure_cursword()`'s dedup/respawn check in
`khtpm_taskbar_manager.c` (reverted `ktb_pid_is_this_pal()` back to the
simpler Sept-11 `ktb_pid_alive()` check, rebuilt, restarted the whole
taskbar via `run_khtpm_strip.sh new` - user re-tested with a real
click, still disappeared identically). Kept the revert anyway (it's a
real, separate, correct fix for the PID-reuse-guard's own false-
negative risk against cursword's dedicated always-open loop), but it
is NOT the cause of this bug.

**Then got a real, live, definitive signal** by launching cursword
under `strace -f -tt -e trace=signal,exit_group,kill` and having the
user click it for real:

```
360631 15:12:22.401869 exit_group(1)    = ?
360631 15:12:22.404480 +++ exited with 1 +++
```

**No signal was delivered to the process at all** - no SIGTERM,
SIGKILL, SIGSEGV, nothing in the trace. The process calls `exit_group(1)`
**on itself** - a real, in-code exit with status 1, not a crash and not
an external kill. This is the single most important fact found this
session: whoever picks this up next should stop looking for "what
kills cursword" and start looking for "what internal code path returns/
exits with status 1 in response to a click."

**Ruled out as the source of that exit(1)** (checked directly):
- The two `return 1;` guards inside `tp_main()` (current lines ~14928,
  ~15083) - both are real, but BOTH are startup-only (`XOpenDisplay`
  failure and an early arg-parse guard), unreachable once the window
  is already live and responding to clicks for 30+ seconds before the
  click that kills it.
- `tp_main()`'s own normal end-of-function return - confirmed
  unconditionally `return 0;` (current line ~17382), so a normal
  `running = 0` loop-exit can never itself produce exit code 1.
- No plain `exit(1)` call exists ANYWHERE in `khtpm_core_render.c`
  (grepped the whole file) - only four `_exit(1)` calls, three of
  which are inside `fork()`-child-only exec-failure guards (would be a
  DIFFERENT, forked pid in the strace output, not pid 360631 itself),
  the fourth is one of the two startup guards already ruled out above.
- No custom `SIGSEGV`/crash signal handler exists that could be
  silently converting a real crash into a clean `exit(1)` (checked -
  only `SIGTERM`/`SIGINT` have handlers registered, nothing else).
- `ulimit -c unlimited` is set in the spawn command
  (`livedesk_ensure_cursword()`) but no core dump was ever produced -
  consistent with this being a real, clean, intentional exit call, not
  an actual segfault/crash.

**Real, direct instruction from the user, not yet acted on**: "why are
u looking at renderer if it was killed by the entities own code? why
dont u compare the 2 entities?" - i.e. stop assuming the shared
`khtpm_core_render.c` binary itself is where the `exit(1)` call lives;
compare cursword's own PACKAGE DATA (its `pals/cursword/` directory)
against a normal, working entity's own package directory, since the
renderer binary is shared/identical for both but their DATA differs.

**Real comparison already done, partial**: `pals/cursword/` has real
extra content no normal entity has: `bookmarks.chtpm/.css/.pdl/
_state.txt`, `chat/`, `harnesses/`, `presentations/`, `user-notes/`,
`switches.txt`, `variables.txt`, `gold.txt`, `inventory.txt`,
`audit/`, `anim/`. `meta.pdl`'s own `METHOD` rows (`Dir`/`Chat`/
`Bookmarks`/`Play`/`Close`/`Cancel`) are real, live-executed shell
commands - but these are the RIGHT-CLICK context-menu rows specifically
(confirmed: `STATE | grab_pointer | 1` / `STATE | grab_keyboard | 1`
are documented in-code, current line ~10985-10988, as governing the
right-click POPUP MENU's own pointer/keyboard grab - NOT left-click
arm behavior), and right-click is confirmed working fine by the user,
so meta.pdl parsing itself is provably not broken. **Not yet checked**:
whether any of `harnesses/`, `switches.txt`, `variables.txt`, or
anything under `event_pkg/`/`pieces/` gets read specifically during
the LEFT-CLICK arm sequence (not the right-click menu path) and could
plausibly trigger a real, deliberate exit(1) if malformed/missing/
changed since Sept-11 - `harnesses/` in particular is cursword-unique
(no other entity has one) and hasn't been inspected at all yet.

**Quick peek at `harnesses/` (2026-09-15, later same session, session
ended here on quota)**: contents are `events_hq_*_test_harness.sh`,
`presentation_harness.py`, `run_desktop_trigger_harness.sh`,
`run_db_hq_tab_switch_demo.sh`, etc. - these read as standalone dev-
testing tools (events-hq test scripts, presentation demos) meant to be
run manually by a human/agent, not anything that looks wired into a
click-response code path. Not a confirmed dead end (not traced/grepped
for actual call sites FROM `khtpm_core_render.c` - just read the
directory listing), but a weak lead. Still-unchecked, stronger
candidates: `event_pkg/` (compare cursword's own vs. a normal entity's
- both have one, but CONTENT may differ), `switches.txt`/
`variables.txt` (small, real state files unique in role to cursword,
worth a quick `read_kv`-style grep for who reads them), and the
`bookmarks.*` cluster (cursword has real Bookmarks integration no
other entity does - `meta.pdl`'s own `METHOD | Bookmarks | sh ...
bm_menu.sh` row - though this is again right-click-menu-reachable, not
obviously left-click-reachable).

**Recommended next concrete step, cheap and high-value**: add one real
`fprintf(stderr, "CURSWORD EXIT PATH X\n")` (or `append_history()`
call, same real pattern this file already uses everywhere) immediately
before EVERY real exit/return point that could conceivably run during
the live event loop (not just the two known startup guards - audit
every `return` inside `tp_main()`'s own `while` loop, and every
function it calls from the click-handling branches, for one that
returns a value bubbling up to a `return 1`-shaped exit, OR trace
harder: `strace -f -tt -e trace=all` (not just signal/exit_group) on
the next live click to see the FULL syscall sequence in the seconds
before `exit_group(1)` - the current trace only proves WHAT exited,
not WHY; a full trace of the preceding ~50 syscalls (file opens,
reads, anything reading `harnesses/`, `switches.txt`, `event_pkg/`, or
any unexpected path) would likely show the real trigger directly.

## 5. Where to look next (real, concrete, unconfirmed leads — in priority order)

1. **Window-creation-time attributes in `tp_main()`'s own setup path**,
   not the click-dispatch block. Compare `override_redirect`,
   `XSetWMHints`, and the X event mask (`XSelectInput`) cursword's own
   window gets at creation time, old snapshot vs current — a diverged
   event mask or WM hint could plausibly cause "click unmaps/closes"
   behavior that never touches the click-handling code at all. This
   was flagged but not actually diffed before time ran out.
2. **The tp_main()-globals-footgun angle directly** (see §4) - trace
   whether the CURRENT click-handling code for cursword calls anything
   that assumes default-mode's `dpy`/`cmap`/`screen` globals are set,
   when it's actually running under tile/entity mode's own local
   `Display`. A no-op or wrong-Display call here could plausibly
   manifest as "the window just vanishes" if e.g. an unmap gets issued
   against the wrong display connection, or a redraw silently fails
   and something else's cleanup path fires instead.
3. **Live repro, not static diffing.** The investigating fork tried
   appending a `MOUSE_EVENT` line to cursword's real relay history file
   and it was NOT consumed after 1s — meaning tile mode likely does
   NOT poll `#.desktop/entity_menu_history/<pid>.txt` the same way
   default mode's `poll_agent_history()` does. Find tile mode's own
   REAL input path (probably direct `XNextEvent`/`ButtonPress` handling
   in `tp_main()`'s own event loop, not the relay-file mechanism) and
   use THAT to get a reliable live repro/instrumentation signal, rather
   than assuming the default-mode relay convention applies here too.
4. Once you have a real live repro signal, add temporary `fprintf(stderr, ...)`
   tracing at the actual click-handler entry point and at whatever
   XUnmapWindow/window-destroy call might be firing, run it live, and
   read the real trace - don't keep static-diffing blind past this
   point once repro is possible.

## 6. What NOT to do

- Don't re-diff the ButtonPress/ButtonRelease/KeyPress block itself
  further — already covered in §3, only benign changes found.
- Don't re-check `g_is_cursword`/`build_shape_mask`/FocusOut filter —
  already confirmed correct in §3.
- Don't assume the relay-file (`entity_menu_history/<pid>.txt`)
  mechanism works for tile/entity mode the way it does for default
  mode — it apparently doesn't (§5.3), confirm the real mechanism
  before building a test harness around the wrong one.

## 7. What we mean to do after this is fixed

Once the actual click regression is found and fixed:
1. Rebuild `khtpm_core_render.c` via `build_khtpm_strip.sh`
   (`_.monads/_.livedesk-taskbar/ops/`), confirm clean.
2. Live-verify: click cursword for real, confirm the yellow highlight
   circle draws and `c`/`v` still changes Z-level (check
   `#.desktop/desktop_active_z.txt` actually updates and sibling
   entity windows show/hide correctly across the change).
3. Add a dated entry to `04-bugs/BUG-LOG.md` under a NEW dated section
   (append-only convention - do not rewrite existing entries) with the
   real root cause and fix, cross-referencing this handoff doc.
4. This session already has several OTHER real, unrelated fixes
   pending commit in the same working tree (board-viewer proc-registry
   gap fix, a duplicate taskbar Player-menu row removed, PALCRAFT/pc-hq
   work) — check `git status`/`git diff` before committing anything,
   scope your own commit to just the cursword fix, don't bundle it with
   unrelated pending changes you didn't make.
