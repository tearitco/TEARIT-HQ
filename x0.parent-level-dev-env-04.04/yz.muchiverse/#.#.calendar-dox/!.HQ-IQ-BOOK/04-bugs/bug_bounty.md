# 🎯 bug_bounty.md — hard-to-pin / recurring bugs, tracked until closed

Different from `BUG-LOG.md` (append real fixed/found entries) and
`03-pitfalls/` (lessons already extracted). This file is for a bug
that's **real, reported more than once, and not yet fully explained**
— so the next person/agent who hits it again doesn't start from zero.
One entry per bug. Update in place as evidence accumulates; don't
re-open a NEW entry for the same symptom.

---

## ⚠️ REOPENED 2026-09-13 (4th occurrence): entities drop off the bottom taskbar after a while, but stay on-screen

**Reported:** 2026-09-11/12, direct live report: "why after a while
entities are dropping from the bottom toolbard (but staying on
screen)... a separate agent caused a crash [since then unable to
verify live]."

**What's confirmed:**
- ✅ A REAL, separate, related leak was found and fixed while
  investigating (2072cc72): `livedesk_hq_windows_<pid>.txt` registry
  files (one per HQ **app** window — chat-hai/db-hq/network-browser,
  gated on `g_default_has_sidebar_panel`) never got cleaned up on a
  crash/SIGKILL — only a clean exit's `atexit()` did. Confirmed 64
  stale files for long-dead PIDs sitting in `#.desktop/`. Now
  self-heals: `ktb_merge_hq_windows()` (the taskbar's own reader,
  `khtpm_taskbar_manager.c`) deletes a registry file the moment its
  own liveness+identity check (`ktb_pid_is_hq_renderer`) proves it's
  stale, instead of skipping past it forever.
- ❌ That fix does **NOT** explain the entity-TILE symptom directly —
  desktop entity/pal tiles (`m8_redhorned`, `self`, `m1_ninjadragon`,
  `asa`, `ava`, `book-stack`, etc.) run through `tp_main()` (entity/
  tile mode), a completely different code path from the
  `g_default_has_sidebar_panel`-gated HQ-window registry above. Not
  yet traced.

**Real next steps, not yet done:**
1. Find the ENTITY/tile equivalent of `ktb_merge_hq_windows()` — how
   does the strip build its list of pal tiles to show at the bottom?
   Likely a separate registry/ledger (see
   [[proc-ledger-consolidation]] memory — `livedesk_proc_list.txt` may
   be the real source of truth here, not `livedesk_hq_windows_*`).
2. Check whether that list is liveness-checked the SAME way (PID alive
   + correct comm), or via a weaker/staler check that could silently
   drop a still-alive entity under some real condition (a reparse
   race, a stale mtime check — see [[prefer-marker-files-not-mtime]],
   or a cap like `KTB_MAX_HQ_WINS` if an entity-tile equivalent cap
   exists and something is filling it with dead entries first).
3. Reproduce for real: leave several entity tiles open for an extended
   period (the report says "after a while" — this may be a slow leak
   or a periodic-tick bug, not an instant one) and watch the relevant
   ledger/registry file(s) directly for the exact moment an entry
   disappears, rather than guessing from code alone.
4. The report notes a **separate agent's own crash** happened around
   the same time — a real, plausible confound. Rule out (or confirm)
   whether that crash's own cleanup (or lack of it) is what triggered
   this specific instance, vs. a structural bug that would recur
   regardless of any one crash.

**Ruled out, 2026-09-12** (direct question: "could that be what got rid
of the good entities? a false positive from the self-healer before?"):
NO — confirmed by code structure, not just plausibility. The registry
file the self-healer above touches is only ever WRITTEN by `redraw()`'s
own generic-HQ-window branch (gated on `g_default_has_sidebar_panel`),
which lives entirely outside `tp_main()` (entity/tile mode's own
separate function, starting well after that write code in the file,
never setting that flag). Entity tiles structurally never had an entry
in this file to begin with, so the self-healer had nothing of theirs
to false-positive delete. The self-healer is confirmed safe and
unrelated to this bounty's real symptom.

**Real root cause found, 2026-09-12 (3e334acc)**: `livedesk_registry_add()`
(an entity's own bottom-bar ledger line) is called EXACTLY ONCE, at
`tp_main()` startup - confirmed via grep, no other call site existed.
The taskbar manager's own registry reader does a real read-prune-write
cycle every tick, dropping any entry whose PID fails a single
`ktb_pid_alive()` check on that one read. Since an entity never
re-registered itself after startup, any single wrong/transient result
from that check, ever, across a long session, permanently erased the
line - while the process itself kept running and rendering, matching
"staying on screen" exactly. This is the opposite of the HQ-window
registry, which every generic HQ window rewrites on EVERY redraw tick
(already self-healing) - the entity path never got that same
treatment. **Fixed**: `tp_main()`'s loop now re-calls
`livedesk_registry_add()` every ~10s, same self-healing shape. Not yet
independently re-observed live over a long real session (the original
report couldn't be reproduced on demand) - if this resurfaces after
the fix, re-open this exact entry rather than starting a new one.

**Real regression this same fix caused, found + fixed same day (eca3c071)**:
the periodic re-call above fires on its VERY FIRST tick, not after a
real 10s wait - its `static struct timespec` timer starts at zero, so
"10s have passed" is true immediately, right alongside the real
startup registration. `livedesk_registry_add()`'s own prune loop only
ever dropped DEAD pids, so re-registering a still-alive pid appended a
second, genuine duplicate line for the same live entity.
`khtpm_taskbar_manager.c`'s `load_tabs()` then saw two live-PID lines
for one entity in a single read and SIGTERM'd the "duplicate" (logic
written for an actual zorder-respawn leftover, not this) - killing
every non-`cursword` entity within 1-2s of every spawn, reproduced via
both a manual launch and the real Player>reset path (`cursword`
structurally exempt from every close/kill sweep, hence the only
survivor). Root-caused live via temporary debug logging in
`load_tabs()` (added and fully removed same pass). **Fixed**: the
prune loop also drops any existing line for the SAME pid being
re-registered, so a re-registration replaces its own prior line
instead of piling up beside it. Verified live: all 6 killed pals
relaunched clean, survived past the 10s checkpoint that used to kill
them. Still worth independently re-observing over a real long session
before this whole entry is considered fully proven, same caveat as
above.

**A THIRD, different regression found + fixed same series, 2026-09-13
(74debf38)** - direct live report: "some entities are missing again,
after your fix... this didn't used to happen." This time nothing died
(all 6 processes confirmed alive, correctly registered, correct
`n_tabs=7` in every backing file) - the BOTTOM BAR ITSELF was visually
frozen showing a stale, out-of-order 4-tab subset, confirmed via a
real PNG dump of the live dock window (`dump_frame_png_op`), not
guessed from code alone. Root cause, found via a second round of
temporary debug logging: `write_small_file()` (writes `strip_ui.txt`,
the file driving the dock's `${n_tabs}`/`${tab.*}` vars) did
`remove(path)` THEN `rename(tmp, path)` - not atomic, opening a real
window where a concurrent reader's `fopen()` gets `ENOENT`. When that
hit `khtpm_core_render.c`'s dock-peer reparse, `parse_chtpm()`
returned NULL and the code unconditionally did
`g_dock_peer = parse_chtpm(...)`, NULLing an already-good tree with no
future retry ever repainting it - worse during rapid entity churn
(more writes = more chances to hit the gap). **Fixed**: dropped the
redundant `remove()` (every other writer in this codebase already
skips it); both dock-peer reparse sites now only adopt a non-NULL
parse result, keeping the last-good tree instead of blanking on any
transient read failure. Verified live: PNG dump of the dock window
during the same staggered 6-entity relaunch that used to freeze it now
shows all 7 tabs correctly. If entities visually vanish from the dock
again with processes/registry confirmed alive, this is the file to
re-open, not a new one - and dump the actual window pixels
(`dump_frame_png_op`) before trusting any backing file's content, since
this class of bug is specifically "data is fine, the render is stale."

**4th occurrence, 2026-09-13** - direct live report: "bottom tb is
missing entities again. no matter what this cant happen. how do we
fix this once and for all?" Confirmed the SAME "data is fine, render
is stale" class as `74debf38` above - not that fix's own specific
ENOENT race (checked directly): `strip_ui.txt` held the correct real
7-tab data the entire time, its content-hash was genuinely STABLE
across 5+ full seconds (ruling out the two-poll debounce too), and
`dump_frame_png_op` on the live dock window still showed it blank
regardless - a fourth, distinct mechanism in this same fragile
mtime/hash/debounce chain, not isolated this pass.

**Real, structural answer this time (`af273699`)**, matching the
direct "once and for all" ask: stopped trying to find and patch the
Nth specific detection gap in this chain. The dock strip now
unconditionally forces a full reparse+relayout+repaint every 3s,
completely independent of mtime/hash/debounce ever agreeing again -
gated to dock windows only (`window_is_dock()`) so every other window
keeps its existing cheaper change-only behavior. A genuinely bounded
worst case now exists: however this next fails, it self-heals within
3 seconds, not "possibly never again until a manual restart." Verified
live: a fully clean restart (every taskbar/entity process killed
first, not a partial one) held all 7 real entities correctly
rendered, with the tab order visibly reshuffling on each heartbeat
tick, across a 35s watch.

**A second, separate thing found the SAME session, worth recording so
it isn't mistaken for this bug's own root cause later**: a test
restart done WITHOUT first killing already-running entity processes
(this session's own repeated manual `run_khtpm_strip.sh new` calls
while testing unrelated code) caused a real entity die-off within
~10s - traced to colliding with `livedesk_spawn_active_desk()`'s
already-guarded (`ktb_pid_alive`-checked) respawn-on-startup logic:
old entities registered under a stale/pruned prior registry snapshot,
a fresh respawn creating a real second live PID per entity, and
`load_tabs()`'s own existing one-entity-one-PID dedup (its real,
intentional job) correctly SIGTERMing the duplicates - working as
designed, just startling to watch happen. Not a bug in the sense this
entry tracks (a normal user session never does a partial restart that
way), but real enough to name: if a future partial-restart workflow
becomes common, `livedesk_spawn_active_desk()` doing its own liveness
check via a fresher registry read (not just relying on the dedup
safety net downstream) would remove the visible flicker.

**Superseded same day (`930fd9ba`)** - direct follow-up: "we dont use
mtime or hash for render, ideally we use marker filesize change only,
or stay with last render (mtime has edgecases); do u see this
instruction in golden rules? thats the final fix." Yes -
`CENTROID_GOLD_STD.md` rule 8 says exactly this ("not on mtime, not
on a hash, not per input event"), and the 3s-timer band-aid above
never actually followed it. The REAL final fix: this exact file
already has a proven, already-wired instance of the real marker
pattern one function away - `khtpm_taskbar_manager_main.c`'s
`publish_state()` writes `strip_ui.txt`/`strip_state.txt` THEN
appends one byte to `#.desktop/strip_frame_changed.txt`
(`touch_frame_changed()`); `dock_poll_strip_state()` in this same
render file already watches that marker's SIZE growth for its own
narrower focus-sync job. `reparse_chtpm_if_changed()`'s vars-changed
gate now watches that SAME marker directly for dock windows,
replacing the hash/debounce chain AND the 3s timer entirely - no
mtime, no hash, no polling interval, growth is the only signal, per
rule 8 to the letter. Non-dock windows are unaffected (most have no
single real "the" writer process the way the strip's manager is one,
so the hash/debounce path stays correct for them). Verified live:
render tracked real entity-count changes in `strip_ui.txt` exactly at
every check across multiple restarts, no lag, no staleness, no timer.

If the ORIGINAL symptom (blank dock, valid backing data) recurs even
with the marker gate in place, re-open this entry again - check FIRST
whether `strip_frame_changed.txt` itself is actually growing on every
real `publish_state()` call (a marker that stops growing is a
materially different, worse bug than this entry's own history: the
manager's publish path itself broken, not a render-side detection
gap).

---

## ✅ CLOSED 2026-09-13: tab reordering / entities missing after restart - the real architectural cause

**Reported same day, multiple times, same underlying file:** "it just
reshuffled again... how did old codebase accomplish functionality"
and separately "some entities didn't show up on restart... was it
related to restart somehow like before? is there a guard against
that?" and again "bottom tb is missing entities again... this needs 2
stop happening. no patches. research house standards, and a real
solution."

**Real root cause, researched not guessed:** `#.desktop/livedesk_
open.txt` (the file that decides the taskbar's tab list) was written
by EVERY entity process independently, on its own unsynchronized
~10s self-heal timer - exactly the "one hot shared file, many
writers" shape `PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md` (this
house's own design doc, §1) explicitly names as the pattern the
house's one-writer rule exists to avoid. This one anti-pattern was
the real, shared cause behind THREE separately-reported symptoms:
tab reordering (each entity's periodic rewrite repositioned its own
line), entities missing after a restart (races between the manager's
one-time startup read and entities' own async writes), and a third,
related PID-reuse class (see below).

**Real, structural fix (`764944ea`, `ec77a18f`)** - not another patch
on the same timer:
1. Entities register ONCE, at their own startup; the periodic re-add
   removed from `khtpm_core_render.c`'s `tp_main()` entirely.
2. The manager is now the SOLE writer - `ktb_self_heal_active_desk_
   registry()` (`khtpm_taskbar_manager.c`, called from `ktb_reload()`
   every tick, internally gated to the same ~10s cadence) restores a
   registry line for any active-desk pal found genuinely alive via a
   real `/proc` cmdline identity scan but missing from the registry,
   and separately re-calls the already-safe `livedesk_spawn_active_
   desk()` so a pal that silently never launched gets a real retry -
   never spawns duplicates (identity-guarded).
3. A THIRD site sharing the same PID-reuse false-positive class
   fixed twice earlier the same day (`ktb_pid_is_this_pal()`, for
   `livedesk_spawn_desk`'s `already_live` check and `livedesk_ensure_
   cursword`) was found and closed: `load_tabs()`'s own dup-kill
   logic - the function that builds the tab list every single tick -
   was still using bare `ktb_pid_alive()`. Live-traced via temporary
   debug logging (added and fully removed same pass): a stale,
   PID-reused registry line alongside a genuinely fresh spawn made
   two "alive" entries look like a real zorder-respawn duplicate, and
   the correct dup-kill logic SIGTERMed one of them - a real,
   plausible explanation for repeated silent spawn failures for one
   specific entity across several restarts.

**Verified live** across multiple clean restarts (full process kill
first, not partial): registry order stable, zero reordering, entities
restored without any manual click, matching the exact "no patches,
real solution" ask.

**Real, honest caveat, not fully closed**: this fix explains and
closes every registry/respawn-skip mechanism found - but one entity
(`book-stack`) kept failing to survive across several of these same
restarts for a DIFFERENT, still-open reason (see the new bounty entry
below) - its own crash, not a registry bug. Don't mistake a future
`book-stack`-specific absence for a regression of THIS fix without
checking that entry first.

---

## ✅ CLOSED 2026-09-13: book-stack dies silently, sometime after a genuinely successful launch

**Reported:** direct live report, same pass as the registry fix
above: "bookstack is missing. it needs to be fault tolerant. (it
appeared later) but thats bugy, janky. know fix?"

**What's confirmed:**
- ✅ NOT a registry/spawn-skip bug - ruled out directly. Temporary
  debug logging (added and fully removed) proved the manager's spawn
  retry correctly, repeatedly attempts to launch book-stack every
  self-heal cycle, and on at least one clean-restart observation
  `already_live=1` was reached almost immediately (a genuinely fast,
  successful self-registration) - the registry/self-heal layer is
  doing its real job.
- ✅ A REAL, confirmed successful launch happens first: book-stack's
  own `history.txt` shows `WINDOW_OPEN` then `ENTITY_PHYMOJI_LOADED`
  (its sprite/voxel atlas load completing) on every attempt - then
  nothing. The process is gone sometime after, silently, with no
  further history entries, no stderr, no exit code ever observed
  (spawned detached via `setsid nohup ... &`, not something this
  investigation could directly `wait()` on).
- ✅ NOT reliably reproducible on demand - inconsistent across
  restarts in the same session (sometimes survives indefinitely,
  sometimes dies within seconds), and a manual foreground/synchronous
  relaunch (bypassing the manager entirely) also succeeded without
  crashing at least once - genuinely intermittent, not a deterministic
  parse/data bug (an earlier "failed to parse" finding this same
  investigation turned out to be a false lead from an incorrect
  manual repro command - argc mismatch - not a real bug in book-
  stack's own package; ruled out and corrected in-session, worth
  recording so a future reader doesn't chase it again).
- ✅ This system routes core dumps through apport
  (`/proc/sys/kernel/core_pattern`), which needs `RLIMIT_CORE` raised
  per-process to even attempt a capture - the default here was 0,
  silently discarding every crash with zero forensic trail. This is
  why nothing could be found: there was never any evidence to read.

**Real fix landed this pass (`7da1ae7b`)**: not a fix for the crash
itself (not yet root-caused) but the fix that makes root-causing it
possible - every entity spawn now does `ulimit -c unlimited` before
the real launch (all 3 spawn sites in `khtpm_taskbar_manager.c`).
Harmless when nothing crashes.

**Major update, same day, live-traced with temporary checkpoint
logging (added and fully removed)** - direct re-report after it kept
recurring: "no bookstack still". Two real findings that change the
shape of this bug:

1. **`/var/crash` stayed empty even with `RLIMIT_CORE` raised**
   (`7da1ae7b`) - `strace -p` also failed outright
   (`ptrace(PTRACE_SEIZE)`: Operation not permitted - `yama.ptrace_
   scope` blocks it in this environment). Neither forensic tool is
   usable here; core-dump capture is a dead end in this environment
   specifically, not a fix that needs more time to pay off.
2. **The real, reproducible signal**: a temporary checkpoint
   (`append_history("...ENTER_MAIN_LOOP")` immediately before the
   main event loop, plus a per-iteration counter immediately inside
   it) showed the loop-entry checkpoint fires on **every single
   launch**, but the first-iteration checkpoint (one line later,
   after nothing but a trivial `while` condition check and an
   integer increment - code that cannot itself crash) **never once
   fired**, across ~15+ consecutive observed launch/death cycles,
   each dying within about one second of reaching the loop. Trivial
   code between two checkpoints, one always logged and the other
   never reached, is a real, strong signal this is an EXTERNAL
   termination (a SIGTERM arriving in that same ~1s window) rather
   than an internal crash - book-stack's own code was never actually
   caught misbehaving.

**Real next steps, not yet done** (superseding the core-dump-focused
ones above - that path is closed off in this environment):
1. Find what's sending book-stack SIGTERM within ~1s of every
   launch. Real candidates, not yet individually ruled out: (a) the
   shared, single-line `#.desktop/.livedesk_last_launch.pid` scratch
   file `ktb_system_recorded()` uses to learn the PID it just spawned
   (header comment, `khtpm_taskbar_manager.c` ~line 94) - if a
   SECOND spawn (the manager launches several entities in a tight
   loop) overwrites this shared file before the FIRST spawn's own
   caller reads it back, a wrong PID could end up registered/reaped
   against the wrong entity; (b) `load_tabs()`'s own dup-kill SIGTERM
   (now identity-verified as of this same day's earlier fix, but not
   re-examined AFTER this specific finding); (c) any other real
   SIGTERM sender in `khtpm_taskbar_manager.c` (`kill_hq_windows.sh`,
   `livedesk_kill_stray_entities()`, the proc-registry reaper) that
   could be matching book-stack's fresh PID by an unintended pattern.
2. Add a REAL (not temporary-debug) `signal(SIGTERM, ...)` log line
   in `tp_main()`'s own handler (`handle_shutdown_signal()`) if it
   doesn't already log which signal/when - the fastest way to
   confirm (1) directly instead of narrowing by elimination.
3. `book-stack`'s own `history.txt` was unusually large (~468KB)
   compared to other entities' - still untested as a factor, lower
   priority now that (1) points away from book-stack's own code
   entirely.

**A real, honest caveat about this investigation's own methodology**:
reproducing this required repeated live restarts, which was directly
disruptive to the user's own concurrent session ("i was on tb but
dissapeared" - a live report of collateral disruption from this same
debugging). Any future continuation of this investigation should
prefer passive observation over forced restarts wherever possible.

**Real root cause found and fixed (`b5443a67`)**, direct follow-up:
"theres nothing external to house quiting booktstack it must be in
house. it must be researched and fix. also i keep seeing weirdly
that it redraws then quickly dissapears." Upgraded `tp_main()`'s
signal handler to `SA_SIGINFO` (`handle_shutdown_signal_info()`,
`khtpm_core_render.c`) - a real, permanent diagnostic, kept - so it
writes the real sender PID via async-signal-safe `write(2)` before
exiting. Caught live on the very next occurrence: the sender was
`khtpm_taskbar_manager_main.+x` itself, confirming the user's own
instinct - not anything external.

Traced to the exact site: `ktb_self_heal_active_desk_registry()`'s
registry-restore half (landed earlier the same day, `ec77a18f`) only
checked the stale `s->tabs[]` snapshot from that same tick's earlier
`load_tabs()` call before appending a "restore" line for a pal found
alive via `/proc`. If book-stack's own process self-registered (its
real, separate, one-time startup write) in the narrow window between
that snapshot and this check, self-heal had no way to see it and
appended a SECOND, genuine duplicate line naming the exact same live
PID. `load_tabs()`'s own dup-kill (real, correct logic for an actual
zorder-respawn leftover) then saw "book-stack" twice on its very next
read and SIGTERMed the second occurrence - which, since both lines
named the same PID, meant killing the only real process there was.
This is exactly "it redraws then quickly disappears": the window
opens and paints for real, then dies to a real signal about one
manager tick later.

Fix: the registry-restore write now happens as a single pass inside
the one real registry lock, re-checking the LIVE file directly (by
real cmdline identity, not just a name match) instead of trusting the
stale snapshot. Also fixed a related correctness bug found while
writing this: the original patch would have called
`livedesk_read_open()` (which itself acquires/releases the same
shared, process-wide lock fd) from inside an already-held lock,
silently dropping protection the instant it returned.

Verified live: book-stack alive and stable for 30+ seconds with zero
forced restarts after the fix, all 7 entities present. If this exact
symptom (opens, paints, dies within ~1s) recurs for ANY entity, the
`SA_SIGINFO` handler left in place should immediately name the real
sender via that entity's own `last_signal.txt` - check that first.

---

## ✅ CLOSED 2026-09-13: taskbar HQ menu gets permanently stuck on nav 1, no key/click moves it

**Reported:** 2026-09-13, direct live report: "tb has an issue now,
its stuck on 1.hq no matter what is pressed" - then, after a full
strip restart cleared it: "ok, good... but it must happen later?"
(a real ask for the recurring root cause, not just relief that a
restart cleared it once).

**What's confirmed:**
- ✅ NOT caused by any of this session's other taskbar-adjacent
  commits (`dfedf360`, `5cf91818`, `5b584ada`, `d941a7d4`) - none
  touch the HQ-menu/scope-confine code path or either dock window at
  all; `5b584ada` only touches the separate bottom "pals" row's
  startup window creation, confirmed unaffected (that window,
  `g_dock_peer_win`, was present and fine throughout).
- ✅ A real, concrete gap found by code audit and fixed (`3e87e11f`):
  the reparse scope-restore block (`khtpm_core_render.c`, runs on
  EVERY reparse - the dock's own projector rewrites its state every
  ~400ms tick, so this is constant) only ever **sets**
  `g_default_scope_confine` to 1 in its two match branches
  (target_id container, or a `<tab>` locking onto `<sidebar>`) - it
  never reset it to 0 when the current scope trigger matches neither,
  which is exactly the case for a plain header cell (`strip-cell-1`
  "HQ": bare `onclick="ACTIVATE"`, no target_id, not a `<tab>`). If
  confine was ever left at 1 from an earlier real scoped interaction,
  it would survive every later reparse regardless of what got clicked
  afterward - `kh_elem_in_scope()` has no other match for a
  target_id-less item, so nav (arrows, digit-jump) permanently locks
  to just the current trigger. Fix: explicit reset to 0 before the
  two conditional re-sets, mirroring `activate_focused()`'s own
  ACTIVATE branch (already correct - unconditional reset before
  conditionally setting).
- ❌ **Not independently reproduced live.** Multiple clean-restart +
  keyboard-only (`xdotool key --window <id>`, real X KeyPress events)
  open/arrow-nav sequences all worked correctly, both before and after
  the fix - the exact sequence that leaves `g_default_scope_confine`
  stuck at 1 for THIS window (which structurally has no `<tab>` and no
  target_id'd trigger of its own anywhere in `khtpm_strip_header.xhtpm`
  - the only two paths that ever set confine=1 at all) was not
  isolated. The fix closes a real, legitimate staleness gap, but
  whether it's the actual mechanism behind the live report is unproven.

**Real next steps, not yet done:**
1. The synthetic-event testing here (`xdotool key --window`) sends
   real X `KeyPress` events directly to the render process - but the
   real user's physical keyboard may reach this dock window through a
   DIFFERENT path: `khtpm_strip_keyboard_ascii.+x` (a separate, raw
   termios-reading binary per the house's own ASCII-relay convention).
   If that relay's key→code mapping or delivery timing diverges from
   direct X KeyPress handling, the real bug may live there instead,
   invisible to any test that only sends synthetic X events to the
   window directly. Check that binary's own code path next.
2. If it resurfaces, do NOT just restart to clear it - first read
   `#.desktop/strip_state.txt` (hq_open/hq_n_menu/hq_focus) AND, if a
   live process attach is possible, the actual runtime value of
   `g_default_scope_confine`/`g_default_active_scope_id` before doing
   anything else, to finally catch it in the stuck state instead of
   only ever seeing it cleared.
3. Re-open this exact entry if the symptom recurs post-`3e87e11f`,
   rather than starting a new one - and note whether it was triggered
   by real physical keyboard/mouse input or another synthetic test, to
   start narrowing the input-path question in (1).

**Recurred, 2026-09-13, same day** - direct live report: "nav is
stuck at 1 again." This time genuinely reproduced live (not just
suspected): a real, GENUINELY FRESH strip process (a brand-new PID
from an always-on-top respawn) already showed the stuck state on its
very first frame dump, before this investigation sent it ANY input.
Confirmed via direct testing: a digit-jump (`5`) worked fine (landed
on nav5, proving the earlier `g_default_scope_confine` fix from
`3e87e11f` was NOT the active mechanism here), but the VERY NEXT
arrow-key press snapped straight back to nav1 - a different code path
entirely (`assign_nav_and_layout()`'s own drop-zone clamp, gated on
`g_dock_drop_lo && g_default_active_scope_id[0]`, checked on every
layout pass, independent of `g_default_scope_confine`).

Root mechanism for HOW a brand-new process ends up with
`g_default_active_scope_id` already non-empty before receiving any
real click **was not conclusively found** despite a real attempt
(exhaustively grepped every assignment site - all three are
click-handler code, none can run before the process's own event loop
starts; ruled out cross-process leakage, a var-driven class seed).

**Real, structural fix anyway (`67aacbe4`)**: rather than keep
chasing the mystery initial value, a freshly-started dock window now
explicitly zeroes `g_default_active_scope_root`/`id`,
`g_default_scope_confine`, and `g_dock_drop_lo`/`hi` once, right
after its own template parses, before the first real layout pass ever
runs - a fresh process cannot possibly have a real, current nav scope
yet, so whatever these globals happened to hold becomes irrelevant.
Belt-and-suspenders on top of `3e87e11f`'s own narrower fix. Verified
live: digit-jump then arrow-step composed correctly (`5` then `Right`
→ nav6) on a fresh post-toggle process, no snap-back.

**Real, honest gap this entry leaves for a future reader**: since the
exact seeding mechanism was never caught in the act, if this recurs a
THIRD time, the new zero-at-startup guard would only mean something
is setting this state DURING the process's life (post-startup, a real
click-path bug) rather than pre-seeding it - re-open this entry and
check for that distinction specifically, not just "does the bug still
happen."
