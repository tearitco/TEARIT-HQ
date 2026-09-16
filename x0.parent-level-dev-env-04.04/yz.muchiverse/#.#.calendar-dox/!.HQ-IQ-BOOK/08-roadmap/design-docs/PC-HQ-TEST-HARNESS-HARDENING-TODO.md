# pc-hq test harness hardening — TODO

**2026-09-15.** Direct live instruction: "i think we should also
harden your ability to use harnesses, including updating your
documentation or making simpler harness etc, if u like to put that in
todo." Real, concrete pain points from this same session, not
speculative — each one below cost real time finding the off-by-one bug
in `pc_menu_input.c`'s `CONFIRM_START_MAP` dispatch.

## What actually went wrong testing pc-hq by hand this session

1. **`pc_menu_input.+x` requires the caller to `cd` into the project
   dir first**, or pass `PRISC_PROJECT_ROOT` explicitly — its own
   `resolve_root()` only reads the env var, `project_root` otherwise
   stays the literal string `"."`. Ran it once from the wrong cwd with
   no env var set; it silently did nothing (wrong relative paths, no
   error). This is genuinely easy to get wrong and get ZERO feedback
   that anything went wrong.
2. **`pc_menu_input.+x <keycode>` also drains a real inbox file**
   (`pieces/system/widget_cmds/inbox.txt`) regardless of the keycode
   argument — a real, generic, useful mechanism, but completely
   undocumented in any single place read this session; had to
   reverse-engineer it from `main()`'s own header comments across
   several unrelated design docs.
3. **No real "did this actually happen" signal.** `system(gen_cmd)`'s
   own return code is discarded (`(void)_rc`) house-wide in this file.
   Combined with `>/dev/null 2>&1` on the generation command itself,
   a completely broken dispatch (the actual off-by-one bug) and a
   perfectly working one look byte-identical from the outside unless
   you diff `world_01/state.txt`'s own `seed=`/`map_id=` fields before
   and after — real, but indirect, evidence.
4. **The off-by-one itself** (`strncmp(cmd, "CONFIRM_START_MAP:", 19)`
   vs. the real 18-character prefix) was ultimately found by manually
   inserting `fprintf(stderr, ...)` at three separate points, rebuilding
   three separate times, and re-running each time by hand. A real
   assert/test harness would have caught this the FIRST time this
   branch was written (2026-09-14), not a full session later.
5. **Every live test this session mutated real, shared game state**
   (`world_01/state.txt`, `animals.txt`, the actual chunk files,
   `hero_01`/`xelector_01` position) — every single test run had to be
   manually `git checkout`'d back afterward to avoid leaving the user's
   real desk in a test-polluted state. No sandboxed/disposable test
   mode exists for this engine at all.

## Real, concrete next steps (not built yet — this is the TODO)

- **A real `pc-hq-test-harness.sh`** (or a small C helper) that wraps
  the exact ritual that had to be reconstructed by hand every time
  this session: `cd` into the project dir, export
  `PRISC_PROJECT_ROOT`, write a real inbox command, invoke
  `pc_menu_input.+x 0`, and print a real before/after diff of
  `world_01/state.txt` (seed/map_id/tick) so a broken dispatch is
  visually obvious in ONE command instead of three separate manual
  `cat`s.
- **A real `--test`/sandboxed project-root mode** for the whole
  `pc_generate_chunk`/`pc_menu_input` pair - operate against a
  throwaway copy (or an in-memory/tmp dir) instead of the real,
  shared `pieces/` tree, so a test run never needs a manual
  `git checkout` cleanup pass afterward. Even a simple
  `cp -r pieces /tmp/pchq-test-$$` wrapper the harness script owns
  would remove this whole class of "did I remember to restore
  everything" risk.
- **Real, checked return codes** on the `system(gen_cmd)` calls in
  `pc_menu_input.c` (there are several, not just `CONFIRM_START_MAP`) -
  at minimum log a real failure line to a debug file instead of the
  current silent `(void)_rc` discard, so a future off-by-one-shaped bug
  fails LOUD instead of just quietly not running.
- **Document the inbox mechanism itself** in one real, canonical place
  (this doc, or a new one) — what file, what format, who drains it and
  when (confirmed this session: only on a real keypress/tick from the
  LIVE running engine, NOT automatically idle-polled the way this
  session first assumed — a real, live, running board session is
  required to prove an inbox command actually works end-to-end; a bare
  manual `pc_menu_input.+x 0` invocation only proves the dispatch logic
  itself, not the full real trigger path).
- **A real assert/smoke-test suite** for `pc_menu_input.c`'s own
  dispatch table (even a trivial one: for every `strcmp`/`strncmp`
  literal in the file, assert the literal's own real length via
  `sizeof(...)-1` matches whatever length constant is used alongside
  it, so an off-by-one like today's can never compile silently again).
  This is the single highest-leverage fix on this list - it would have
  caught today's actual bug automatically, at build time, with zero
  live testing needed at all.
