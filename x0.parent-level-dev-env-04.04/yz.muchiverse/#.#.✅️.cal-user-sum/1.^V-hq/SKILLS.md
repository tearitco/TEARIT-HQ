# SKILLS.md — house wisdom for any agent, not any one task (2026-08-29)

This is not a task doc and not a replacement for `!.HOUSE_STDS.md`
(`44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md`, the technical architecture
reference) or `HQ-WINDOW-MAP-AND-AGENT-INPUT.md` (the input-relay
reference). Those tell you *what the house is*. This tells you *how to
operate in it well* — the operational judgment, habits, and hard-won
lessons that don't fit neatly into an architecture doc. Read this once,
cold, before touching anything; re-read the "landmines" section before
any session that involves live windows or shared files.

---

## 1. The one belief everything else follows from

**Real, file-based state only — never in-memory — for compliance and
audit.** This is not a style preference, it's the house's actual
constitution. Every mode's state lives in a real file on disk
(`.pdl`, `.chtpm`, `*_state.txt`, `action.txt`) that a human or another
process could open and read at any moment and see the truth. When
you're tempted to hold something "just in the running process" because
it's simpler — don't. If it needs to survive a crash, be auditable, or
be visible to a second process, it goes in a file, and the file is the
source of truth, not a cache of it.

This belief explains almost every other convention below: why input is
file-relayed instead of synthetic X11 events, why state is republished
on every real change instead of computed on demand, why PDL
(`SECTION | KEY | VALUE`) is everywhere, and why "just keep it in a
global for now" is the wrong instinct here even when it would work.

---

## 2. Orientation: how the rendering side of this house actually works

- Most visible windows (db-hq, events-hq, chat-hai, palettes,
  bookmarks, stats-hq, taskbar-settings, entity-menu popups) are all
  **one binary**, `khtpm_entity_menu_render.+x`, built from
  `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c`. Which
  mode a given process runs is decided by a `g_is_<mode>` flag set
  from argv/the `.chtpm` it was launched against — it is genuinely one
  merged binary serving many roles, not many binaries that happen to
  share a name.
- The paint layer is shared: `khtpm_render_core.c` (Elem tree,
  `hit_test`, `find_by_id`/`find_by_tag`, `css_layout_pass`),
  `khtpm_draw_core.c` (`draw_elem`, `render_tree`, sprite/font
  caches), `khtpm_css_parser.c` (real CSS-like stylesheets, including
  compound class selectors). **Before writing per-mode drawing code,
  check whether the shared layer already does it.** A large fraction
  of this session's real work was finding and killing duplicated
  per-mode paint code that should have called the shared functions
  from the start.
- Dynamic UI content (a list of commands, a sidebar of sessions, a
  palette of blocks) is injected into the Elem tree via
  `reusable_slot(pool[], max, index, tag)` — a fixed static array
  reused every rebuild, never a fresh `elem_new()` per frame. This is
  the house's answer to "how do I add rows without leaking memory or
  exhausting a bump allocator." Use it, don't invent a fresh pattern.
- Keyboard/mouse nav is a real, numbered system: every element that
  carries a real action gets a `nav_index` (visible as a `[ ]N.`
  badge), digits jump focus directly, Enter activates. The house
  standard is **onClick-driven auto-numbering** — if an Elem has a
  real `onclick`, it should be nav-reachable; if you add a new
  interactive row/button anywhere, make sure whatever `assign_nav_
  indices()` function handles that mode actually walks to it. This is
  the single most common class of bug found and fixed this session:
  new UI added, nav-numbering pass never extended to reach it.

---

## 3. Landmines — real, this-session-caught, will bite you again if you don't know them

1. **Build scripts sometimes `cp` a canonical shared file over the
   local copy you're looking at, on every build.** Editing
   `ops/khtpm_draw_core.c` felt right and compiled clean — the build
   script silently overwrote it from `&.widgits/_shared-lib/
   khtpm_draw_core.c` on the very next build, discarding the edit with
   zero error. **Before editing any file that looks like it might be a
   generated/copied artifact, grep the relevant `build_*.sh` for a `cp`
   line targeting it.** If one exists, edit the source it copies
   *from*, not the copy.

2. **Shared per-mode files can silently broadcast to every window of
   that mode at once.** The input-relay history files used to be keyed
   by mode name only (`db_hq_history.txt`) — if a real user's window
   and a test window were both "db-hq," they read the identical file,
   and test keystrokes landed in the real window too. This was found
   live, mid-session, when a user's real nav state got corrupted by an
   agent's own test input. Fixed 2026-08-29: these are now per-PID
   (`db_hq_history/<pid>.txt`), mirroring `nav_tab`'s own older
   per-PID convention. **The general lesson survives past this one
   fix**: before assuming a file is safely scoped to "the window I'm
   testing," check whether its path is keyed by mode, by PID, or by
   something else — and if by mode, ask whether more than one window
   of that mode could plausibly be open.

3. **Desktop tiles are real, live entities, not fixtures.** Names like
   `asa`, `book-stack`, `m1_ninjadragon`, `cursword` are actual
   pals/entities the user has running on their real desktop. Editing
   "asa's" event data to test a Scratch-block feature is editing a
   real thing the user might open at any moment. **Use a dedicated
   sandbox entity for testing** (`/tmp/evhq_sandbox/event_pkg` existed
   for exactly this reason this session) rather than a real-looking
   named entity, unless the task is explicitly about that entity.

4. **Never use `xdotool click` at absolute screen coordinates for
   testing.** It clicks whatever is actually at that pixel on the
   user's real, shared desktop — this session accidentally opened four
   real entity popups this way. The correct, safe pattern is:
   `xdotool windowfocus <hex_id>` (never `windowactivate`/`click`) to
   give a *specific* test window real X focus, then drive it entirely
   through its own file-relay history file. This never touches
   anything outside the one window you meant to touch.

5. **The only reliable way to see real pixels in this sandbox is
   `dump_frame_png_op.+x <hex_window_id> <output_path>`.** External
   screenshot tools (`scrot`, etc.) return solid black here — this
   isn't a fallback choice, it's the only one that works, because it
   does a real `XGetImage` from inside an X11 client. Convert decimal
   window IDs to hex with `printf '0x%x\n' <decimal>` before passing
   them — a literal `0x` prefix on a value that's already hex-with-
   prefix, or a bare decimal where hex is expected, throws a real
   `BadWindow` X error.

6. **A hypothesis written in a code comment is not a verified fact.**
   This session inherited a comment claiming `draw_elem()` had a
   `w<=0||h<=0` guard "confirmed... exists identically in both the old
   and new draw function." It didn't exist at all — nobody had actually
   read the function, they'd reasoned about what it *should* do. The
   real bug (a whole class of "hidden" elements bleeding through as
   ghosts) was found only by opening the function and reading it
   line by line. **When a comment or a doc asserts something the
   current bug seems to contradict, re-derive it from the actual
   source before trusting either the doc or your own assumption.**

7. **A UI element's width computed from its label text alone will be
   wrong if a nav badge is drawn inside the same box.** Found and
   fixed twice this session (once for view-tabs, once for footer
   buttons) — the exact same bug, because the second instance wasn't
   recognized as "the same bug" until the button backgrounds visibly
   stopped covering their own text. If you're computing an Elem's `w`
   from `measure_text_px(label)`, and that Elem will also carry a real
   `nav_index`, add real headroom for the badge+gap (this codebase's
   own established constant for that is `+34`, not a bare `+20`).

8. **"Currently focused" is not the same as "the thing the user just
   interacted with."** A first attempt at a delete feature read
   `g_focus_nav` inside a *different* button's own click handler,
   assuming it would still point at a previously-focused row — but
   navigating TO that delete button had already moved focus onto the
   button itself. Any design that says "act on whatever currently has
   focus" from within a control that itself has to be focused and
   activated first is structurally broken. Prefer designs where the
   action lives inside the same interaction that already has the
   right context (e.g., a Delete option inside the edit dialog a row's
   own click already opens), not a separate control referencing global
   focus state.

9. **Kill child processes, not just the window process.** A rendered
   window frequently forks a real backend manager (`khtpm_events_hq_
   manager.+x`, `dbhq_pdl_publish_manager.+x`, etc.) as a child. Killing
   only the renderer during test cleanup orphans these — this session
   accumulated over a dozen stray manager processes across one evening
   of testing, several of them racing on the same `action.txt` and
   silently swallowing real actions. **After any test session, `ps aux
   | grep` for the *manager* names too, not just the renderer, and
   confirm zero strays before moving on.**

---

## 4. Verification discipline (non-negotiable, not a suggestion)

- **Never say something is fixed without a fresh build + a fresh
  live/headless run + real evidence** (a `dump_frame_png_op` screenshot
  read back and actually looked at, or a real file diff, or a direct
  action-file round-trip test). A clean compile is not evidence of
  correctness. This house has been burned before by confident claims
  that turned out to be untested assumptions — see landmine #6.
- When a fix doesn't visibly work, **don't guess a second fix on top of
  the first without first confirming the first one actually executed
  at all.** This session lost real time re-testing a broken delete flow
  three times before finally adding a one-line stderr trace that
  immediately showed which Elem was actually being activated. A cheap,
  temporary debug print that gets removed before commit is faster than
  three rounds of blind re-testing.
- Real A/B regression testing when refactoring shared code: `git
  stash` your change, rebuild "old," capture a screenshot, restore,
  rebuild "new," capture again, diff pixel-for-pixel. This is how a
  refactor gets to claim "byte-identical to before" instead of "looks
  about right."
- If you make a real mistake mid-session that could have touched the
  user's live environment (a stray click, a broadcast keystroke, an
  ambiguous process kill), **say so plainly and immediately**, explain
  the actual root cause once you know it, and don't quietly patch
  around the symptom hoping it goes unnoticed. Every incident like this
  in the session log became a real, permanent fix once surfaced — never
  a hidden one.

---

## 5. Documentation and continuity

- This house survives context cuts by writing real findings into real
  docs as you go, not by remembering harder. If you're mid-investigation
  and might get cut off, write down: what's confirmed, what's still a
  hypothesis, and what the next concrete step is — see any
  `STATUS <date>` block in `EVENTS-HQ-RENDER-UNIFICATION-PLAN.md` for
  the house's own standard shape of this.
- `INDEX.md` (`1.^V-hq/INDEX.md`) is the front door — dated entries,
  newest additions near the top of the list, each pointing at the real
  doc with a one-line summary of what actually landed. Add an entry
  here for anything a future agent would need to discover, not just
  things you personally did.
- When correcting a doc that turns out to have been wrong (a "believed
  to exist" comment, a stale status), **say so explicitly in the
  correction** ("this doc's earlier claim was never verified, actual
  cause was X") rather than silently rewriting it — the house's
  convention for shared collaboration docs (see below) is append/
  correct-with-attribution, not silent rewrite.

---

## 6. Working alongside other agents (Grok, opencode, others)

- Shared handoff docs (`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`,
  `!.OPEN-2do-events-db-networking-2026-08-28.md`) are genuinely
  two-way, asynchronous collaboration surfaces, not just your own
  scratch notes. **Append dated entries; don't rewrite another agent's
  history** in them, even one you're correcting — the "docs-only
  correction pass, Grok — append, not rewrite" convention already
  named in `INDEX.md` is the house's explicit policy on this.
- Before implementing something that reads like it might already be
  someone else's in-flight work, check the relevant handoff doc's most
  recent entries first. This house runs Sonnet, Grok, and opencode
  concurrently on real overlapping scope; duplicated effort is a real,
  recurring cost here, not a hypothetical.
- A breaking change to a documented external contract (a file format,
  a relay convention, a function's real behavior other agents rely on)
  needs a loud, dated, unmissable announcement in whatever doc the
  other agents actually read from — not a quiet mention buried in a
  commit message. See the per-PID history-file fix's own handoff-doc
  entry for the shape this should take: what changed, why, and exactly
  what to do differently starting now.

---

## 7. The user's actual working style (read this to calibrate tone and pace)

- Wants real bugs found and real fixes verified live — not
  reassurance, not "should work now" without having actually run it.
- Explicitly values documentation that survives a session cutoff; asks
  for it proactively when work is getting deep.
- Says "no exceptions" when asking for something to be generalized —
  means it literally; a fix that covers 3 of 4 call sites and leaves
  one hardcoded is not done.
- Wants to be asked before destructive/irreversible steps (deleting
  legacy code, force-pushing, killing an ambiguous process) but does
  not want to be asked about things with an obvious right answer or a
  clear existing convention to follow — reads as over-cautious
  hand-holding if you ask too much, and as reckless if you don't ask
  enough. When genuinely unsure which side of that line a decision is
  on, err toward asking, especially for anything hard to reverse.
- Pushes to git at real checkpoints (after a verified fix, before a
  risky next step) rather than only at the very end — treat "let's
  push" as a normal, frequent part of the rhythm here, not a special
  occasion.
- Catches things in live screenshots himself and expects that to be
  treated as ground truth over any prior assumption, including your
  own — see landmine #6 and #7, both caught this way.
- Prefers a small number of well-reasoned options over an exhaustive
  survey when a real decision point comes up; give a recommendation,
  not a menu, unless the choice genuinely has no obvious best answer.

---

## 8. A cold-start checklist

If you're picking this house up with zero context:

1. Read `INDEX.md` top-to-bottom far enough to know what's currently
   in-flight and what's considered done.
2. Read `!.HOUSE_STDS.md` section headers to know where the technical
   reference lives for whatever you're about to touch — don't
   re-derive architecture that's already documented.
3. Read this file's §3 (landmines) again, specifically.
4. Before your first live test: identify whether the file/window/
   entity you're about to touch is shared with anything real the user
   might have open, and if there's any doubt, ask or use an isolated
   sandbox copy instead.
5. Before your first commit: confirm you edited the real source, not a
   build-generated copy (§3.1).
6. Before saying anything is fixed: get a real screenshot or a real
   file-level proof, not a clean compile.
