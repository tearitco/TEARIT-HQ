# Operational landmines

*Condensed from `SKILLS.md` §3-4, 2026-09-02. Read before any session
involving live windows or shared files.*

1. **Build scripts sometimes `cp` a canonical shared file over the
   local copy you're editing, on every build**, discarding your edit
   with zero error. Before editing a file that looks generated/copied,
   grep the relevant `build_*.sh` for a `cp` line targeting it — if one
   exists, edit the source it copies *from*.
2. **Shared per-mode files can silently broadcast to every window of
   that mode at once.** Input-relay history files used to be keyed by
   mode name only; a test window and a real user's window of the same
   mode would read the identical file, and test keystrokes would land
   in the real window. Fixed 2026-08-29 (now per-PID:
   `<mode>_history/<pid>.txt`). Lesson survives the fix: before
   assuming a file is safely scoped to the window you're testing,
   check whether its path is keyed by mode, PID, or something else.
3. **Desktop tiles are real, live entities, not fixtures** (`asa`,
   `book-stack`, `m1_ninjadragon`, `cursword`, etc. are things the user
   may have open right now). Use a dedicated sandbox entity for
   testing, not a real-looking named one, unless the task is explicitly
   about that entity.
4. **Never use `xdotool click` at absolute screen coordinates.** It
   clicks whatever is actually at that pixel on the user's real,
   shared desktop. Use `xdotool windowfocus <hex_id>` (never
   `windowactivate`/`click`) on a specific test window, then drive it
   entirely through its own file-relay history file.
5. **`dump_frame_png_op.+x <hex_window_id> <output_path>` is the only
   reliable way to see real pixels in this sandbox** — external
   screenshot tools return solid black here. Convert decimal window
   IDs to hex with `printf '0x%x\n' <decimal>` first.
6. **A hypothesis in a code comment is not a verified fact.** A
   comment claiming a guard "confirmed... exists identically" in two
   functions was simply wrong — nobody had read the function. Re-derive
   from actual source before trusting either a doc or your own
   assumption when they seem to contradict a live bug.
7. **A width computed from label text alone is wrong if a nav badge
   shares the same box.** Add real headroom for badge+gap (`+34`, not
   a bare `+20`) whenever an Elem's `w` is computed from
   `measure_text_px(label)` and it will also carry a `nav_index`.
8. **"Currently focused" is not "the thing the user just interacted
   with."** Reading `g_focus_nav` inside a button's own click handler
   assumes it still points at a previously-focused row — but
   navigating TO that button already moved focus onto it. Prefer
   designs where an action lives inside the same interaction that
   already has the right context, not a separate control referencing
   global focus state.
9. **Kill child processes, not just the window process.** A rendered
   window often forks a real backend manager
   (`khtpm_events_hq_manager.+x`, etc.) as a child. After any test
   session, `ps aux | grep` for manager names too, and confirm zero
   strays.

## Verification discipline (non-negotiable)

- Never say something is fixed without a fresh build + a fresh live/
  headless run + real evidence (a screenshot actually looked at, a
  real file diff, a direct action-file round-trip). A clean compile is
  not evidence of correctness.
- When a fix doesn't visibly work, confirm the first fix actually
  executed at all before guessing a second fix on top of it — a cheap
  temporary stderr trace beats three rounds of blind re-testing.
- Real A/B regression testing for shared-code refactors: `git stash`
  the change, rebuild "old," screenshot, restore, rebuild "new,"
  screenshot again, diff pixel-for-pixel.
- If a real mistake touches the user's live environment (a stray
  click, a broadcast keystroke, an ambiguous process kill), say so
  plainly and immediately and fix the root cause — never quietly patch
  around the symptom.
