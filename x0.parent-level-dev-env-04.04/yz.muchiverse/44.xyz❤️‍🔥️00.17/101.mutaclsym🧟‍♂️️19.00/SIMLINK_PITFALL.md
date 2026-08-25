# SIMLINK_PITFALL.md — symlink-elimination gotchas (mutaclysm-neo, 2026-08-20)

**Status: DONE.** Camera mode switching (and rendering generally) confirmed working
after this pass, both via direct testing and by the user live in the GL window.
Keep this doc as the checklist for the NEXT time any project's session-directory
symlinks get removed for Windows compatibility.

## The core problem, in one sentence

Removing session-directory symlinks means `PRISC_PROJECT_ROOT` (persistent project
root) and the process's own CWD (disposable per-launch session dir) are no longer
the SAME path — every file access must now deliberately pick the right one, and
every single site that got this wrong failed *silently* (fopen returns NULL, code
falls back to a hardcoded default, nothing crashes, nothing errors — it just quietly
shows stale/wrong data).

## The two buckets every file access falls into

- **Persistent / shared data** (written by one process, read by another, must survive
  across relaunches): hero state, world data, registry/fonts/emoji assets, config.
  → resolve via `PRISC_PROJECT_ROOT` (project_root).
- **Session-local / ephemeral display state** (recreated fresh every launch, only
  meaningful to processes launched by THIS run): `current_frame.txt`,
  `frame_history.txt`, `rgb_frame.raw`, `rgb_frame.receipt.txt`,
  `rgb_frame_changed.txt`, `renderer_pulse.txt`.
  → resolve via **CWD** (session_root, via `getcwd()`), NOT project_root.

Getting a file's bucket wrong doesn't crash — it just means two processes end up
reading/writing two *different* real files that happen to share a relative name,
and nobody notices until a human watches the live window and sees stale output.

## The real bugs, in the order they bite

1. **`button.sh` bare relative launch paths** (`./system/renderer`, `./system/gl_mirror`,
   `./system/chtpm_rgb_render`, `./system/keyboard_input`) — silently resolve wrong
   the moment CWD stops having the symlinked `system/` dir sitting in it. Fix: always
   launch via `"$SCRIPT_DIR/system/..."` absolute paths.

2. **`system/chtpm_rgb_render.c` (this project's own diverged local copy)** — wrote
   `rgb_frame.raw`/`rgb_frame.receipt.txt`/`rgb_frame_changed.txt`/read
   `current_frame.txt` all via **project_root** instead of **session_root**. This is
   THE bug that produces "camera state updates correctly but the GL window never
   visibly changes" — `x11_mirror.c` watches the session-dir copy of these files,
   which this daemon was silently writing somewhere else. **Do not assume this file
   rebuilds from a shared canonical source** — check `build.sh`'s own compile line;
   if it compiles `system/chtpm_rgb_render.c` (this project's own copy) rather than
   pulling from `&.widgits/_shared-lib/`, any project-specific divergence (this one
   has real local emoji-generation code) needs this same session/project split
   applied by hand.

3. **`system/orchestrator.c`** — `launch("./system/chtpm_parser_pal", ...)` and
   sibling `stat()`/`launch()` calls used bare relative paths too. This is the one
   that actually starts the whole engine — get this wrong and the window opens but
   shows nothing, ever.

4. **`system/keyboard_input.c`** — checked but did **NOT** need a session/project
   split in this project (confirmed by direct diff against a known-working backup:
   byte-identical). Don't blindly apply every fix from a similar-looking prior
   writeup without re-verifying against the current file — an older internal
   planning doc claimed this file needed fix #4, but empirical diffing proved it
   was already fine as-is. When a doc and direct evidence disagree, trust the
   evidence.

5. **`system/renderer.c`** — same project/session split needed for
   `current_frame.txt`, `frame_history.txt`, `renderer_pulse.txt` (session-local);
   `pieces/display/state.txt` stays on project_root (nothing else writes it, so it
   doesn't actually matter, but no reason to move it).

6. **`ops/game_dispatch.c`** — needed the same split for `interact_relay.txt`
   (written by `chtpm_parser_pal` via project_root — must be READ via project_root
   too) and for the `ops/+x/*.+x` binaries it `run_op()`s (must be launched via
   absolute project_root paths once CWD stops guaranteeing a symlinked `ops/`).

7. **`pieces/system/bv_state.txt` seeding in `button.sh`** — `chtpm_parser_pal.c`
   (shared, unmodified) does `chdir(project_root_path)` before launching the
   persistent PAL module. Once `PRISC_PROJECT_ROOT` became `$SCRIPT_DIR` instead of
   the session dir, every child process spawned from that module (`game_dispatch`,
   `muta_render_3d`) started running with **CWD = `$SCRIPT_DIR`**, not the session
   dir — so their own RELATIVE `pieces/system/bv_state.txt` read/writes silently
   moved to a completely different file than the one `button.sh` was seeding.
   `muta_render_3d.c` early-returns with no-op if `bv_state.txt` has no
   `focused_project_root` line — meaning camera_mode/cam_yaw updated correctly in
   game state, but the 3D view never re-rendered with them. Fix: seed `bv_state.txt`
   at the path the processes ACTUALLY use (`$SCRIPT_DIR`), not just the session dir.

## The meta-lesson (why this took so many rounds)

- **A whole class of "it looks like it works" false positives exists here**:
  hero state files updating correctly (confirmed via direct `grep`) does NOT prove
  the render pipeline picked it up — always verify with an actual pixel-level dump
  (`dump_rgb_png.+x`, which reads `rgb_frame.raw` directly, no window-capture race),
  not just a state-file diff or a checksum. A checksum changing is necessary but
  not sufficient proof either if you haven't confirmed BOTH the writer and the
  reader of that raw file are agreeing on the same real path.
- **Don't skip a documented fix because it "should" already be handled elsewhere.**
  This session skipped `chtpm_rgb_render.c`'s fix on the assumption build.sh always
  compiles from a shared, already-fixed source — that assumption was wrong for this
  specific file (a genuinely diverged local copy), and skipping it broke rendering
  entirely for a full round-trip before being caught.
- **Test after EVERY individual fix, not just at the end.** Bundling multiple file
  changes together and testing once at the end makes it much harder to know which
  specific change broke something when the test fails.
- **When a process's CWD might have silently changed (chdir in a shared library
  function you don't control), verify by directly reading `/proc/<pid>/cwd`** rather
  than assuming it matches what you launched it with. This is what actually
  surfaced bug #7 above — `ps` showing the persistent PAL module's own CWD as
  `$SCRIPT_DIR` instead of the session dir was the concrete, undeniable evidence,
  not a guess.
- **A backup/reference copy claiming "confirmed working" from a prior session can
  itself be stale or only partially verified** — the original `!.symlink-migration-
  todo.md` in this project said the `19.00` baseline was "untouched, confirmed
  working," but direct testing this session found that same baseline's own
  orchestrator process list looked identical either way — the actual proof always
  has to be a fresh, live check against the CURRENT file state, not trust in what
  an older doc asserted.

## Quick verification checklist for the next symlink-elimination attempt

1. `grep -rn '"\./' button.sh` and every `.c` file in `system/`/`ops/` — any bare
   `./relative/path` string is a candidate bug the moment CWD stops matching the
   old symlinked shape.
2. For every file two DIFFERENT processes both touch (writer + reader), confirm
   both resolve it via the SAME base (project_root or session_root) — mismatches
   here are the most silent and hardest to spot.
3. Check `/proc/<pid>/cwd` for every long-lived process after a fresh launch —
   don't assume it matches the CWD you launched it with if anything in the call
   chain does its own `chdir()`.
4. After each individual file fix: rebuild, relaunch, and get a real
   `dump_rgb_png.+x` PNG proof before moving to the next fix. Don't bundle fixes.
5. Never trust "confirmed working" from an older doc without a fresh, live
   re-check against the CURRENT file state.
