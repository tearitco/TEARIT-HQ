# Relay-injection window/PID targeting — design (not started)

**Status: DESIGN ONLY, written 2026-09-18 specifically so this survives
a token-limit disconnect before implementation starts.** Triggered by
a direct question: when an agent drives relay injection instead of a
human clicking, how does it get the write to land on the RIGHT window,
and should that targeting be a first-class, spec'd part of testing —
not something re-derived ad hoc every time.

## 1. The real current mechanism (verified by direct code read, not assumed)

- Every `khtpm_core_render.c` process polls **its own** relay file,
  keyed by its own PID:
  `history_path()` (line ~9171): `snprintf(out, outsz, "%s/%d.txt",
  dir, (int)getpid())` → `#.desktop/entity_menu_history/<pid>.txt`.
- **There is no active "which window" selection step.** An agent must
  already know the target PID before it writes anything. Today that
  PID is obtained ad hoc: `ps aux | grep khtpm_core_render`, or reading
  a `module_parent.pid` file next to the window's package dir (the
  convention named in the `khtpm-house-standards` skill).
- **Relay dispatch does NOT require or check real X11 focus at all**
  (confirmed: `poll_agent_history()` is called unconditionally, no
  check against focus state; a real design comment at
  `khtpm_core_render.c` ~9623-9630 says this is deliberate — "consume
  this process's own history mailbox even when another window has X
  focus," specifically so a human and an agent can share one display
  without fighting over focus).
- **Multiple windows of the same app never collide** — this was a real
  bug (relay files were mode-keyed, not PID-keyed, so one test's input
  broadcast to every open window of that type) fixed 2026-08-29, now
  strictly per-PID (`OPERATIONAL-LANDMINES.md` documents the fix).
- The taskbar DOES track a real focused/unfocused flag per window
  (`#.desktop/livedesk_hq_windows_<pid>.txt`, a `focused=0/1` line,
  read by `ktb_merge_hq_windows()`), but **that flag is pure UI
  bookkeeping — it does not gate relay consumption.** A relay-driven
  agent can drive a window that the taskbar considers unfocused or
  backgrounded; a real human could not do the equivalent without
  clicking to raise/focus it first.

## 2. The real conceptual split this design has to name explicitly

Two genuinely different things get conflated when people say "focus":

1. **Dispatch target** — which process's mailbox receives an injected
   event. Fully determined by which `<pid>.txt` file gets written to.
   Has nothing to do with X11 state. Already unambiguous today (per-PID
   files), just not formally *documented as a target-resolution step*.
2. **Visual/input focus** — the X11-level "this window is raised and
   would receive real keyboard/mouse events if a human were driving."
   Relay injection **bypasses this entirely by design** — which is a
   real, useful property (agent and human can coexist on one display),
   but it also means **a relay-only test can pass even though the
   equivalent real human action would have required an extra click to
   raise/focus the window first.** This is the same risk this house
   already named in the `relay-testing-may-mask-real-focus-bugs` house
   rule (real precedent: `entity_menu_history` relay tests passed 3x
   for a `cli_io` focus bug real hardware still showed).

**The fix isn't to make relay injection require focus** (that would
defeat the whole point of the mechanism) — it's to make the CHOICE of
whether a given test also needs a real focus-raise step **explicit and
logged**, instead of silently absent.

## 3. Proposed design (not built yet)

### 3a. Formal target-resolution step, required in every relay test/script

Every relay-driven test (human-written or agent-written) should record,
not just perform silently:
- **How** the target PID was obtained (which registry/grep/file), so
  a later human or agent can re-derive it without guessing.
- **A liveness check** before writing (`kill -0 <pid>` or a
  `/proc/<pid>` existence check) — writing to a stale PID's relay file
  is a silent no-op today; a liveness check turns that into a loud,
  immediate failure instead of a confusing "nothing happened."
- **Whether this test intends to also validate real focus behavior.**
  If yes, it must perform an explicit, separately-logged focus-raise
  step (last-resort `xdotool windowactivate`-class call, matching the
  existing house ordering: relay first, focus-raise only when the test
  is SPECIFICALLY about focus/visibility, never as a default reflex).

### 3b. A real, concrete PID-resolution registry — extend, don't invent

`#.desktop/livedesk_hq_windows_<pid>.txt` already exists and is already
a per-window metadata registry (confirmed: has `focused=0/1`). The
natural, minimal extension — **not confirmed buildable without reading
that file's full current schema first, flag as a pre-work step, not an
assumption** — is a `purpose=<string>` or `title=<string>` field in
that same file, giving relay-driving code/tests a real
**purpose → PID** reverse lookup (e.g. "the currently-open Common
Events editor" → its live PID) instead of `ps aux` grepping or hoping
a `module_parent.pid` file is where expected. This directly serves
kilo's KILO-handoff testing work (`13.agent-coms/KILO/
claude-2-kilo-9.17.md` §3) and any future agent driving relay tests —
one documented, reusable resolution path instead of every agent
re-deriving its own.

### 3c. Fix the one stale doc found during this research

`02-architecture/INPUT-RELAY-PIPELINE.md` (lines ~22-24) still
describes a **per-mode focus gate** — this predates the 2026-08-29
per-PID fix (`OPERATIONAL-LANDMINES.md`'s own documented correction)
and is now factually wrong. Needs a dated correction note, not a
rewrite — same convention as every other stale-doc fix in this house
(append a 🔄 CORRECTION, don't silently edit history).

## 4. Non-goals for this pass (explicitly NOT being designed here)

- **Not** building a full window-manager-style focus-follows-relay
  system — the existing bypass-focus design is a real, deliberate,
  good property (human/agent coexistence) and should not be weakened.
- **Not** designing multi-agent concurrent-driving conflict resolution
  (two agents both targeting the same PID) — no evidence this has ever
  actually happened; flag as a real open question (§5) rather than
  solving a problem that hasn't occurred.
- **Not** touching `khtpm_core_render.c`'s dispatch logic itself — the
  per-PID mailbox mechanism is correct and doesn't need to change; this
  design is entirely about the **discovery/documentation layer** on
  top of it.

## 5. Open questions for the user

1. Should the `purpose=`/`title=` registry field (§3b) get built as
   part of kilo's WSR-CIV/DSR testing work (natural, since kilo will
   need exactly this), or is it a separate, smaller task worth doing
   directly rather than folding into that handoff?
2. For tests explicitly validating focus/visibility (§3a's "yes"
   branch) — is `xdotool windowactivate` an acceptable last-resort tool
   here, or should this house build its own minimal X11 focus-raise
   helper to avoid the xdotool-flakiness precedent already documented
   elsewhere in this house?
3. Is a stale-PID liveness check (§3a) worth a shared helper script/op
   (e.g. `relay_write.+x <pid> <event>` that does liveness-check +
   write + logs the resolution method in one place), or should each
   test keep doing this inline? A shared helper would make §3a's
   discipline automatic instead of relying on every test author
   remembering it.

## 6. Grounding / further reading

- `khtpm-house-standards` skill — "Driving/testing a
  khtpm_core_render.c window" section, the current ad hoc
  PID-discovery convention this design formalizes.
- `03-pitfalls/OPERATIONAL-LANDMINES.md` — the 2026-08-29 per-mode →
  per-PID relay fix this design builds on top of, not around.
- house rule `relay-testing-may-mask-real-focus-bugs` (auto-memory) —
  the exact risk §2/§3a resolve by making focus-testing an explicit,
  logged opt-in instead of a silent gap.
- `13.agent-coms/KILO/claude-2-kilo-9.17.md` §3 — the kilo handoff this
  design directly supports; kilo's relay-injection testing work is the
  first real consumer of whatever gets built from this design.
