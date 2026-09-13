# 🎯 bug_bounty.md — hard-to-pin / recurring bugs, tracked until closed

Different from `BUG-LOG.md` (append real fixed/found entries) and
`03-pitfalls/` (lessons already extracted). This file is for a bug
that's **real, reported more than once, and not yet fully explained**
— so the next person/agent who hits it again doesn't start from zero.
One entry per bug. Update in place as evidence accumulates; don't
re-open a NEW entry for the same symptom.

---

## 🕵️ OPEN: entities drop off the bottom taskbar after a while, but stay on-screen

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

**Do NOT close this entry** until an entity tile has been watched
disappearing live, with the exact file/mechanism identified — the fix
above was a real, confirmed, worthwhile leak closed along the way, but
closing this bounty on that alone would be declaring victory on the
wrong bug.
