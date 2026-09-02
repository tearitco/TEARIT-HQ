# Owner Report — Event-Scripting System (Common Events / db-hq / events-hq)
**2026-08-27, jbro885@wgu.edu**

Not a marketing document. This is the real technical status of what got
built/fixed this session, what it means for RPG-Maker-style games built
on this house, how to test it yourself, and one real infrastructure
inconsistency you should know about before relying on it further.

---

## 1. What actually got built/fixed this session

1. **Common Events inline editor (db-hq)** — sidebar + panel in one
   window, RPG-Maker-style: pick a common event on the left, edit its
   command list on the right, no separate windows.
2. **Command rows made nav-reachable/editable** in both db-hq and
   events-hq (previously some rows existed but weren't keyboard/click
   reachable).
3. **Control Switches / Control Variables commands** (Task 1) and
   **Call Common Event, including nesting** (Task 2) — real PAL-emitted
   VM instructions, not shell-wrapper stubs.
4. **Conditional Branch (Task 3)** — rewritten compiler as a real
   two-pass compile (collect IR, then resolve if/else/end jump labels).
   **Found and fixed a real bug**: the old single-pass compiler's
   `skip_depth` counter didn't distinguish the true-branch from the
   false-branch of an if/else — so BOTH branches silently compiled to
   nothing. Fixed, and independently re-verified live (real `.pal` file
   contents inspected, real switch-driven branch test, real 2-level
   nesting test — 13/13 fresh tests).
5. **Common Events manager (Task 4)** — Autorun triggers fire once on
   an OFF→ON switch transition (edge-triggered, not every tick);
   Parallel triggers fire repeatedly while the switch is ON, gated by a
   ~1-second cooldown so they don't hammer the machine. Real ledger
   file tracks state; no polling hack in the renderer.
   - **Found and fixed an undisclosed design gap**: the first pass at
     this auto-generated a switch name (`ce_<event_name>`) with no UI
     to see or change it. You confirmed the fix: switches should be
     addressable by name (in addition to a numeric slot, RPG-Maker
     style) — a real "Switch" field now exists on the trigger row,
     showing the configured name or the `(unset, using ce_<name>)`
     fallback.
   - **Found and fixed a real bug in that fix**: the trigger-type
     handler and the switch-name handler each rewrote the event's
     condition file by deleting ALL condition lines and writing back
     only their own — so setting one silently erased the other. Fixed
     so each handler only touches its own line. Verified live in both
     directions (set trigger, then switch — both survive; set switch,
     then trigger — both survive).

Everything above is pushed to `TEARIT-HQ` (confirmed correct remote
before each push). Nothing here is faked or simulated — every claim
above was checked by directly reading the generated `.pal`/condition
files and by driving the real UI through the relay-testing harness
this house uses for itself (see `_.0.aigent-testing-k9.txt` in this
same directory for that convention).

---

## 2. What this means for RPG-Maker-style games / future titles

- You now have the core of an RPG-Maker "Common Events" system running
  on our OWN small VM (`prisc+x`), not a black-box engine: switches,
  variables, conditional branches, and common-event calls (with
  nesting) all compile to real, inspectable instructions in a flat
  `event.pal` file.
- Because it's data-driven text files (not compiled-in game logic),
  any future game built on this house can be authored by non-C people
  using db-hq/events-hq the same way an RPG Maker user authors events —
  and an AI agent can read/write/test those same files directly, which
  is a real differentiator RPG Maker itself doesn't have.
- The one thing still missing versus RPG Maker proper: a numeric
  switch/variable ID space with a configurable memory limit (you
  mentioned wanting the memory limit to scale with project size). Right
  now switches are addressable by name; the numeric-ID layer is not
  built yet. Worth scoping as its own task rather than assuming it's
  covered by the name-based field already shipped.

---

## 3. How to test this yourself

1. Open **db-hq**, go to an entity (or a common event) with the
   Common Events sidebar. Confirm you can select an event on the left
   and its command list renders/edits on the right in the same window.
2. Add a **Control Switch** command, a **Conditional Branch** around
   it, and put different commands in the true vs. false branches. Flip
   the switch, re-run, confirm the correct branch actually executes
   (this is the exact bug that was silently broken before this
   session's fix — worth specifically re-checking).
3. Add a **Call Common Event** command that calls a second common
   event, and nest one more level (that common event calls a third).
   Confirm all three actually run in order.
4. Set a common event's trigger to **Autorun**, give it a Switch name
   in the new field, flip that switch ON once from another event —
   confirm the Autorun event fires exactly once (not repeatedly) until
   you flip the switch OFF and back ON again.
5. Set another common event's trigger to **Parallel** with its own
   switch — confirm it fires repeatedly while the switch stays ON,
   but not faster than roughly once a second (the cooldown).
6. While a Parallel event is armed, go back and change ONLY the
   trigger type (leave switch alone), then go back and change ONLY the
   switch name — confirm neither action erases the other's setting.
   This is the exact bug found+fixed this session.

---

## 4. Files/desks and switching — will the work be saved?

This needed a real answer, not a guess, so here's what the code
actually does (`resolve_session_root()` in
`khtpm_events_hq_manager.c`, and the taskbar's own session/desk
bookkeeping in `khtpm_taskbar_manager.c`):

- **A taskbar "file" IS a session** (`xyzfs/.../livedesk/sessions/s1`,
  `s2`, …). What you called "files" in your question and what the code
  calls "sessions" are the same thing.
- **A "desk" is a sub-concept INSIDE one file/session** — each
  session's own `session.pdl` has an `active_desk` field. So one file
  can contain several desks; switching desks does not change which
  session you're in.
- **For an entity's OWN events** (not common events): switches and
  variables are session-scoped when the entity's event package lives
  under a `sessions/<id>/` directory. That means: **switching desks
  within the same file keeps the same switches/variables** (still the
  same session). **Switching to a different file (session) gives you a
  different, independent set of switches/variables** for that entity's
  own events.
- **For Common Events specifically, this is different, and it's the
  real infrastructure nuance you asked me to surface:** common events
  live at `common_events/<name>/event_pkg/` at the HOUSE ROOT, not
  under any session directory at all. `resolve_session_root()` walks
  UP from an event package looking for a `sessions/<id>/` ancestor; for
  a common event, that walk never finds one, so it always falls back
  to using the common event's own directory as the switch/variable
  location.
  **Practical effect: a common event's switches/variables are the SAME
  regardless of which file or desk you're on.** They are not global in
  the sense of "one shared pool for the whole house" (each common
  event has its own switch file), but they are not per-file/per-desk
  either — they're pinned to the common event itself. If you build a
  game expecting "Autorun switch X" to reset or differ between save
  files/desks, it currently will not; it's shared across all of them.

## 5. The underlying infrastructure issue to track

The inconsistency above — entity events are session-scoped,
common events are always house-root-global-per-event — was already
flagged earlier in this project's history as a known gap, and this
session's work (the Switch field, the trigger/switch bug fix) sits
directly on top of it without changing it. It works correctly for
what it does today, but if a future game wants per-save-file common
event state (e.g., "has this global boss been defeated, tracked
per playthrough"), that needs a deliberate design decision — either
give common events an explicit per-session variant, or accept and
document that common-event state is intentionally shared across all
files/desks. Recommend deciding this before building a real game on
top of it, rather than discovering it mid-development.

---

## 6. Presentation video

Not yet produced (a separate long-form MARKETING outline exists as
`MARKETING-PRESENTATION-OUTLINE.md` in this directory, also not yet
produced). An owner-report video covering the above can be built with
the same relay-driven screen-capture + `make_presentation_video.py`
pipeline already used for this session's own task verification
recordings (see pointer files in
`xyzfs/.../pals/cursword/presentations/`) — say the word and I'll
record the 6 test steps above live and produce it, archived per the
usual convention.
