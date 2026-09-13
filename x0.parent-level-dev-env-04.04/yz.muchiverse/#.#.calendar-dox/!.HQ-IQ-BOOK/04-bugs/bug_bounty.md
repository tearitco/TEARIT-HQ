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

If the ORIGINAL symptom (blank dock, valid backing data, no restart
involved) recurs even with the 3s heartbeat in place, re-open this
entry again - that would mean the render is somehow blocked for
longer than 3s (the render loop itself stalled, not just one specific
detection path failing), a materially different, worse class of bug.

---

## ⚠️ OPEN 2026-09-13: taskbar HQ menu gets permanently stuck on nav 1, no key/click moves it

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
