# sim-smell-fix.md — systematic checklist for the next symlink-elimination pass

Written 2026-08-20, after mutaclysm-neo, board-viewer, piececraft-xyz, and my-chara-txt all needed
real, distinct fixes beyond the mechanical "remove `ln -s`, switch `PRISC_PROJECT_ROOT`" step. This
doc is the **shape/smell checklist** — a repeatable procedure to find this whole bug class by
pattern, not by re-discovering it project-by-project through live trial and error (which is what
happened today, at real cost). Companion to `SIMLINK_PITFALL.md` (the incident narrative) and
`101.mutaclsym🧟‍♂️️19.00/todo-muta-0g+-.txt`.

## 🚀 HANDOFF FOR A FRESH AGENT — read this whole section before touching anything

**You have zero memory of the conversation that produced this document.** Everything you need is
written down here or in the files this section points to. Do not assume any prior context — verify
everything with the actual commands given below before acting on it, because file state can have
moved on since this was written.

### 0. What this task actually is, in one paragraph

The house used to isolate every project's "live session" (one run of `button.sh`) by symlinking
static code (`system/`, `ops/`, `pal/`, etc.) and sometimes real persistent data (`config.txt`,
`data/`) from the project's real root into a disposable per-launch session directory
(`pieces/sessions/<timestamp>/`). Real symlinks don't survive a Windows checkout/copy, so the house
is being swept clean of them, project by project. The fix is NOT to just delete the symlinks — it's
to replace the *mechanism* they served (giving the session dir access to the project's files) with
something that works without a symlink. There are two different mechanisms in use across the house
(full comparison in "Two strategies, compared" below); the recommended one for anything not yet
migrated is the **copy-based strategy**. Your job, picked up from wherever the table in "STEP 2 of 2"
below leaves off: for each listed project, finish converting it to the copy-based strategy, verify it
still works (build + test harness if one exists + a manual run), and get the human's signoff before
calling it done.

### 1. Two strategies, compared — pick the copy-based one unless you have a specific reason not to

Before the migration, three things were always the exact same value for any running session:
`PRISC_PROJECT_ROOT` (an env var), the disposable session directory, and the process's own working
directory after any internal `chdir()`. Because they were always identical, it never mattered which
one a piece of code used to build a file path. The whole bug class this doc exists to catch is code
that silently assumed two of those three are still the same value, once a real migration makes them
different for the first time. See "The one-sentence root cause, always" below for the fuller version.

**Strategy A — `PRISC_PROJECT_ROOT = $SCRIPT_DIR` (switch project_root to the REAL, non-session root).**
This is what `101.mutaclsym🧟‍♂️️19.00` uses. It requires auditing and fixing every single place in the
project's own C code (and the shared engine, if it also assumed session_root == project_root) that
reads or writes a `project_root`-relative path, reclassifying each one into "this needs the SESSION
root" vs "this needs the REAL root" (see checklist items 1-5 below for exactly how to find these).
Mutaclysm-neo needed ~7 distinct fixes across `orchestrator.c`, `renderer.c`, `game_dispatch.c`,
`chtpm_rgb_render.c`, plus missing `mkdir -p` calls for directories that used to only exist inside the
session dir. **It also has a known, unresolved, real crash risk**: if the project has more than one
`<module>` PAL file per screen (a real `game_dispatch.c`-style multi-module architecture, not just one
persistent module for the whole app), switching `PRISC_PROJECT_ROOT` away from the session dir can hit
a bug in the shared engine's own `chtpm_parser_pal.c` `launch_module()`/`run_module_synchronous()`
relaunch logic — confirmed live, reproducibly, in my-chara-txt (see "REAL, UNRESOLVED shared-engine
bug" section below). This is the HARDER, RISKIER path. Only still relevant for consistency-with-
mutaclysm reasons; do not use it for a NEW migration unless you have a specific, understood reason.

**Strategy B — copy-based (keep `PRISC_PROJECT_ROOT = $SESSION_DIR` unchanged) — THE RECOMMENDED FIX.**
This is what `my-chara-txt`, `@.apps/piececraft-xyz`, and `&.widgits/board-viewer` use, and it's the
one every remaining project in the "STEP 2" table below should be converted to. The insight: you don't
have to eliminate the SESSION/PROJECT_ROOT split to eliminate symlinks — you only have to eliminate
the *symlink*, which you can do by copying instead. Since `PRISC_PROJECT_ROOT` never changes value,
every existing piece of C code that reads/writes a `project_root`-relative path keeps working exactly
as before — **zero downstream C-code changes required**, which is why this is dramatically lower-risk
than Strategy A. Full mechanical detail in the "⭐ THE SOLUTION" section immediately below this one.

**Why mutaclysm-neo isn't just being retrofitted to Strategy B for consistency**: it works today, was
fixed BEFORE Strategy B was even discovered, and (being single-module) was never at risk of the
relaunch bug Strategy B exists to sidestep. Retrofitting it means reverting `PRISC_PROJECT_ROOT` back
to `$SESSION_DIR` AND re-auditing/reverting every one of those ~7 fixes, which is real risk for zero
functional benefit. Leave it alone unless the human specifically asks for that retrofit.

### 2. The tools you have

- **`check-symlinks.sh`** (house root, `$HOUSE_DIR/check-symlinks.sh`) — run with no args from the
  house root to audit the whole house; pass a path to audit anything else (a backup folder, say). Two
  checks: (1) real symlinks that exist on disk right now (excludes `pieces/sessions/*` by default,
  since those are ephemeral and regenerate every launch — pass `--include-sessions` to check those
  too, useful for a frozen backup folder where "session" dirs won't regenerate); (2) `button.sh` files
  that still contain literal `ln -s`/`ln -sf`/`ln -sfn` calls — this catches projects that look clean
  on-disk-right-now but still CREATE real symlinks every time they're actually run. **Always run check
  #2, not just check #1** — the original "zero symlinks house-wide" claim earlier in this doc's own
  history was wrong for exactly this reason (see the CORRECTION note further down).
- **`$.crypts/symlink-swap.sh`** — does ONLY the safe, mechanical half of Strategy B: rewrites every
  `ln -s`/`ln -sf`/`ln -sfn` call in a `button.sh` to the equivalent `cp -r` call (works for both files
  and dirs), and backs up the original as `button.sh.pre-symlink-swap` next to it before overwriting.
  Usage: `./symlink-swap.sh [--dry-run] <button.sh> [<button.sh>...]` or
  `./symlink-swap.sh [--dry-run] --list-file <path-to-a-list.txt>` (one button.sh path per line).
  ALWAYS run with `--dry-run` first and read the diff before running for real. This tool does **not**
  decide what's persistent state (see step 4 below) — that's a judgment call you make per project.
- **A project's own `test-harn-same/` or `test-harn-*/` directory**, if present — see "How to test"
  below. Not every project has one.

### 3. Current status — what's done, what's a false alarm, what's left

Re-verify this table against the live filesystem before trusting it (things may have moved since this
was written) — `./check-symlinks.sh` from the house root is the fastest way.

**Confirmed FALSE POSITIVES, need NO symlink work at all** (their `ln -s` grep hits were inside
comments, not real calls — verified by hand):
- `102.agy-txt/button.sh`
- `102.editor-📄️00.00/button.sh` — has its own real test harness though (`test-harn-same/`,
  `demo_interact_canvas.sh`); running it surfaced an UNRELATED pre-existing bug (`FAIL: CLEAR did not
  empty buffer` in the editor's own CLEAR FILE command) — that's a real bug but has NOTHING to do with
  symlinks; don't fix it as part of this task unless the human separately asks.

**Confirmed DONE, both Strategy B steps complete, awaiting only final long-term confidence**:
`my-chara-txt`, `@.apps/piececraft-xyz`, `&.widgits/board-viewer`. All three: `ln -s` fully replaced
with `cp -r`, `PRISC_PROJECT_ROOT` left unchanged, `persist_session_state()` added where needed, live
board-viewer channel bug fixed in piececraft-xyz's `ops/pc_menu_input.c` (see that project's own
button.sh/sim-smell-fix.md write-up above for the exact pattern — reuse it, don't rediscover it, if
you find the same `widget_cmds/inbox.txt` shape in another project).

**STEP 2 COMPLETE, awaiting human signoff (2026-08-20)**: `0.user-pal👤️/00.login-signup` —
`persist_session_state()` added (copies back `users/`, `current_login.txt`, `xyzfs/session.pdl`;
NOT whole xyzfs — see HOUSE_ROOT discovery below), plus one NEW button.sh export:
`export HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"`. Harness went FAIL → **real 17/17 PASS**
(`demo_login_signup.sh`, via `harness-runner.sh`); `demo_account_switch.sh` also PASS. Awaiting
the human's manual live test before deleting its `.pre-symlink-swap` backup.

**STEP 1 (mechanical `ln -s` → `cp -r` swap) DONE house-wide; STEP 2 (persistent-state wiring)
FIXES APPLIED 2026-08-21 to all of the list below — batch testing is the remaining work**
(per-project status in the classification table further down + completed-sym-list.md). Original
verification note: each one had 100% real, non-comment `ln -s` calls before conversion (checked
via `grep -E '^\s*ln -s' | grep -v '^\s*#'` against each one's own `.pre-symlink-swap` backup):

```
0.user-pal👤️/01.avatar-creation👤️/button.sh
@.apps/aomorai-editor/button.sh
@.apps/civ-txt/button.sh
@.apps/my-biotech/button.sh
@.apps/my-lawyer/button.sh
@.apps/myne-qrypto/qtc/button.sh
@.apps/tactics-txt/button.sh
@.apps/text-editor-xyz/button.sh
@.apps/TSC_ELO/button.sh
@.apps/TSC_ELO/widgets/setup/button.sh
@.apps/yahoo-app/button.sh
+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00/button.sh
&.widgits/context-menu/button.sh
&.widgits/event-editor/button.sh
&.widgits/event-ez/button.sh
&.widgits/file-menu/button.sh
&.widgits/tile-picker/button.sh
&.widgits/yahoo-broker/button.sh
```

Full per-project classification (what's persistent, what likely needs the board-viewer live-channel
fix too) is in the table under "STEP 2 of 2 NOT STARTED" further down this doc — **treat that table as
an unverified first-pass guess from filenames, not ground truth**; re-derive it from the project's own
C code per step 4 below before trusting it.

**Deliberately NOT touched, needs a human decision first**: `&.widgits/event-ez.backup-20260805-163329/`
— this is a dated backup directory sitting next to the live `event-ez/` project, not itself a live
project. Confirm with the human whether it's still needed at all before doing anything to it (including
just leaving its symlinks as-is — don't assume either way).

**Backups — UPDATE 2026-08-21: archived, not sitting loose in-tree anymore.** `symlink-swap.sh`
used to leave a `<name>.pre-symlink-swap` copy of the original `button.sh` next to every file it
touches. Now that Step 2 is done house-wide (see "STEP 2 of 2 DONE" below) and every project has at
least been agent-tested, all 19 of those `.pre-symlink-swap` files — plus the older whole-project
zip backups (`my-chara-txt_PRE-SIMLINK-FIX_20260820.zip`, `piececraft-xyz_PRE-SIMLINK-FIX_20260820.zip`,
`board-viewer_PRE-SIMLINK-FIX_20260820.zip`, `my-chara-txt=pre2.7z`), a stale `_BACKUP_101.mutaclsym-old+18.01/`
dir, and one unrelated stray (`041.pal-chain⛓️/data/blockchain.txt.backup-...`) — were swept into a
single archive: **`trash/symlink-migration-backups-20260821.zip`** (house root), then deleted from
their original locations. If you need one of these for the pre/post regression-comparison technique
in "How to test" below, `unzip -l trash/symlink-migration-backups-20260821.zip` to find it, then
`unzip trash/symlink-migration-backups-20260821.zip "<path>" -d /tmp/restore` to pull just that one
file back out — no need to unpack the whole archive. **Still true regardless of where the backup
lives**: don't consider a project's migration fully closed out, and don't delete anything from the
trash archive itself, until the human has actually signed off on it per the per-project recipes in
`completed-sym-list.md`.

### 4. Step-by-step procedure for ONE project from the list above

Do not batch multiple projects' Step 2 work together — one at a time, get signoff, then move to the
next. This mirrors the human's own stated preference throughout this whole effort ("do one at a time
... try one at a time and i will test... each time").

1. **Confirm Step 1 is really done**: `grep -c "ln -s" <project>/button.sh` should be 0. If not, run
   `symlink-swap.sh --dry-run` on it first, read the diff, then run for real.
2. **Check for a test harness**: `find <project> -maxdepth 1 -iname "test-harn*"`. If one exists, look
   at its own `button.sh` for a `compile`/`build` action and its `scenarios/` dir for what it actually
   runs (usually a `demo_*.sh`). Not every harness has a populated `scenarios/` dir — some are stubs
   (e.g. `my-lawyer`'s own harness references a scenario file that doesn't exist; skip it, don't invent
   one).
3. **Re-derive the persistent-file list for real**, don't trust the guess-table further down: read the
   project's own `button.sh` for its `cp -r "$SCRIPT_DIR/...` lines (post-swap) and, for each one that
   isn't obviously static code (`system/`, `ops/`, `pal/`, `pieces/chtpm/`, `pieces/registry/`,
   `default_op.txt`, `projects/<name>/pieces/`), grep the project's own `ops/*.c` for where that file
   gets WRITTEN (not just read) during a session — `grep -n '"<filename>"' ops/*.c` and look for
   `fopen(..., "w")`/`fopen(..., "a")` nearby. If something writes it mid-session and it's meant to
   survive the session ending, it's persistent state.
4. **Add `persist_session_state()`** to the EXIT trap, following the EXACT working pattern in
   `my-chara-txt/button.sh` or `@.apps/piececraft-xyz/button.sh` (search either file for
   `persist_session_state` to see the real, tested shape): a shell function that `cp`s each confirmed-
   persistent file/dir from `$SESSION_DIR` back to `$SCRIPT_DIR`, called from the trap BEFORE
   `rm -rf "$SESSION_DIR"` runs.
5. **Check for the board-viewer live-channel bug**: `grep -n "widget_cmds\|board_widget_bridge" <project>/ops/*.c`.
   If the project's own menu-input op resolves the inbox path via bare `project_root` instead of a
   `resolve_real_root()`-style helper (search `pc_menu_input.c` for `resolve_real_root` to see the
   working fix), it needs the identical fix piececraft-xyz already got: drain the inbox via the REAL,
   non-session root, not the session-local copy, because board-viewer writes commands directly to the
   real root and a session-local copy will never see them.
6. **Rebuild**: `bash scripts/build.sh` (or whatever the project's own build script is called — check
   `button.sh compile`/`c`/`build` action if unsure) from the project's own root. Must say "build ok"
   or equivalent with no new errors (pre-existing unrelated warnings are fine, ignore them).
7. **Run the test harness if one exists** (see "How to test" below for the full protocol, including
   the pre-existing-bug-vs-regression distinction).
8. **Hand off to the human for a manual live test.** Do not declare a project "done" on your own say-
   so — the human tests, then gives explicit signoff, per this whole effort's established protocol.
9. **Only after signoff**: the project's own `.pre-symlink-swap` backup may be deleted (ask first if
   unsure).
10. **Update this doc's own status tables** (STEP 2 list above, and the classification table further
    down) to move the project from "not started" to "done," and note anything you discovered along the
    way (a new persistent file, a new live-channel bug shape, anything future-you or another agent
    should not have to re-discover).

### 5. How to test — automated harness AND manual, plus the pre-existing-bug trap

**Automated (if a `test-harn*/` dir exists)**:
```
cd <project>/test-harn-same   # or whatever it's actually named
bash button.sh compile        # builds the harness's own small C ops (tk_inject_key etc.)
bash button.sh demo           # or whatever action its own help text says runs the real scenario
```
Read the tail of the output for `PASS`/`FAIL` lines and a final `=== FAILED (N checks) ===` or success
message. Most harnesses also drop proof frames/logs under the project's own `proof/` directory —
useful for post-mortem if something fails and you need to see what the screen actually looked like at
each step.

**CRITICAL — distinguish a real regression from a pre-existing, unrelated bug before reporting
anything as broken.** Two confirmed examples from this exact effort:
- `@.apps/TSC_ELO`'s own `pvp_duel.sh` harness fails at its `BOOT` state (the setup widget never
  finishes launching) — confirmed, by testing, to fail IDENTICALLY whether `button.sh` is the
  post-swap (`cp -r`) version or the original `.pre-symlink-swap` (`ln -s`) version. Pre-existing bug,
  unrelated to this migration. Do not spend migration-task time chasing it; note it and move on.
- `102.editor-📄️00.00`'s own harness fails on `CLEAR did not empty buffer` — this project needed ZERO
  symlink work (false positive, see status table above), so there's no "before/after" comparison to
  even make; it's just a pre-existing bug, full stop.

**The exact technique to tell the difference**, whenever a harness fails on a project that DID get the
`ln -s` → `cp -r` swap: temporarily restore `<project>/button.sh` (and any other swapped `button.sh`
under the same project, e.g. a `widgets/setup/button.sh`) from their own `.pre-symlink-swap` backups,
rerun the exact same harness command, and compare. Identical failure on both = pre-existing, not your
problem to fix as part of this task (note it, move on). Only fails on the post-swap version = a real
regression you introduced — go find it (most likely: a file that should have gone in
`persist_session_state()` didn't, or the board-viewer live-channel fix wasn't applied where it was
needed). **Remember to restore the post-swap version afterward** — don't leave the project reverted to
symlinks after doing this comparison.

**Manual test** (required regardless of whether a harness exists, and required either way before
human signoff): `bash button.sh run` (or `widget`/`app` per the project's own help text), interact with
it briefly, `bash button.sh kill` (or just let the session's own EXIT trap run), then verify the
persistent files you identified in step 4 above actually survived at `$SCRIPT_DIR` (not just inside
the now-deleted session dir) — e.g. `cat <project>/pieces/system/config.txt` and confirm it reflects
whatever you just did, not the stale pre-session value. This is the single most important check: it's
exactly the failure mode (real save data silently vanishing every session) that this whole migration
exists to avoid reintroducing.

### 6. Future work flagged by the human — not started, don't build blind, just be aware

Two house-wide tools are planned, for AFTER the per-project migration above is finished:
1. **A house-wide compile/distribution script**, living in `$.crypts/` (same directory as
   `symlink-swap.sh` and `check-symlinks.sh`) — same idea as TPMOS's own compile step: build once,
   copy/distribute the resulting binaries to every project in a single run, instead of each project's
   own `button.sh`/`scripts/build.sh` doing an independent build every time.
2. **A house-wide test-harness runner** ("harness 2" per the human's own phrasing) — runs every
   project's own `test-harn*/` scenario(s) in one pass and drops a single consolidated report showing
   which projects have "a basic level of fx" (working) and which don't, instead of an agent manually
   running each one project-by-project like this doc's own "How to test" section describes.

Neither is designed or built yet as of this writing. If the human asks for either, don't rediscover
the intent from scratch — this paragraph IS the design brief; ask the human for any missing specifics
rather than guessing the shape.

## ⭐ THE SOLUTION, confirmed working 2026-08-20 (my-chara-txt) — try this FIRST

**Do not switch `PRISC_PROJECT_ROOT` away from the session dir at all.** The `PRISC_PROJECT_ROOT
= $SCRIPT_DIR` approach (what mutaclysm-neo needed, because that's how its own `game_dispatch.c`
was already architected) is the HARDER, riskier path — it requires auditing and fixing every single
file-bucket collision in the checklist below, one at a time, and for any project with a real
multi-module-per-screen architecture (a separate `<module>` per `<button href>`, not just one
persistent module for the whole app) it can hit a genuine crash in the shared engine's own
`launch_module()`/`run_module_synchronous()` relaunch logic when project_root and session_root
actually diverge (root-caused live in my-chara-txt, not yet fixed upstream — see the "REAL,
UNRESOLVED shared-engine bug" section below).

**The easier, safer, now-proven fix**: keep `PRISC_PROJECT_ROOT="$SESSION_DIR"` exactly as before
(zero downstream C-code changes needed — every existing project_root/session_root assumption stays
correct, because they're still the same value) — and replace every `ln -s`/`ln -sf` in `button.sh`
with a plain `cp -r`/`cp -p` into the session dir instead. This alone eliminates 100% of the real
symlinks (Windows' actual complaint) with **zero risk to the shared engine or any project-specific
op**, since nothing about path resolution semantics changes at all.

The one real nuance: split what's being copied into two buckets.
- **Static/read-only** (`system/`, `ops/`, `pal/`, `pieces/chtpm/`, `pieces/registry/`,
  `default_op.txt`, `projects/<name>/pieces/`) — copy in at session start, never touch again. Safe
  to duplicate freely, nothing writes back to these.
- **Real persistent state** (`data/` — ledgers, save files; small state files like `config.txt`/
  `plots.txt`) — these get WRITTEN during a real session and must survive after the session dir is
  deleted. Copy in at start same as above, but ALSO copy back out to `$SCRIPT_DIR` in the `EXIT`
  trap, BEFORE `rm -rf "$SESSION_DIR"` runs. See my-chara-txt's own `persist_session_state()`
  function (`button.sh`) for the exact working pattern — it's a 3-line `cp` fan-out, trivial to
  port to any other project's own set of persistent files.

**Trade-off, acknowledged, acceptable for single-player/non-concurrent projects**: unlike a real
symlink's continuous live write-through, two sessions of the SAME project running truly
concurrently would each mutate their own private copy and clobber each other on exit instead of
merging. Fine for a single-player game (`NO_NET=1`); would need real thought (e.g. a file lock +
merge, or just accepting last-writer-wins) for anything designed for genuine concurrent sessions.

**Order of operations for the next project**: try this copy-based strategy FIRST, always. Only fall
back to the harder `PRISC_PROJECT_ROOT`-switch approach (full checklist below) if the project
genuinely has no real persistent-state files worth worrying about AND you specifically want the
session/project-root split for some other reason - not as the default.

## The one-sentence root cause, always

Before the migration: `PRISC_PROJECT_ROOT` == the disposable session dir == the process's own CWD
(after any internal `chdir()`) — three things that were always the SAME value, so it never mattered
which one any given file access used. After the migration: those three things are three
**different, real paths** for the first time ever. Every bug below is some code silently assuming
two of them are still the same value when they no longer are.

## The checklist — run ALL of these on any new project before declaring it "done"

### 1. `grep -rn '"\./' button.sh system/*.c ops/*.c` — bare relative paths
Any bare `./system/foo` or `"pieces/x/y.txt"` string is a candidate. Classify each hit as needing
either `project_root`-relative or session (`CWD`)-relative resolution using bucket #2 below — never
assume, check what the OTHER end (the paired reader or writer) actually uses.

### 2. For every file two DIFFERENT processes touch, name which bucket it's in
- **Persistent/shared** (survives across relaunches, or written by one process and read by a
  DIFFERENT persistent process): `hero_01/state.txt`-style entity data, registry/fonts/assets,
  config. → `PRISC_PROJECT_ROOT`.
- **Session-local/ephemeral display state** (recreated fresh every launch, meaningful only to this
  run): `current_frame.txt`, `rgb_frame.raw`+receipt, `frame_history.txt`, `renderer_pulse.txt`.
  → CWD (`session_root`, via `getcwd()`).
Write this mapping down per-project before touching code - a bug is almost always one file that got
classified into the wrong bucket, or classified differently by its writer vs. its reader.

### 3. Check every directory a project_root-relative `fopen(path, "w")` write depends on
**This was the single most expensive, most repeated bug today** (hit in mutaclysm — masked by luck
— AND in my-chara-txt — not masked, immediately broke navigation). `button.sh`'s own `mkdir -p`
calls almost always only ever created directories under `$SESSION_DIR`, never under `$SCRIPT_DIR`
itself, because in the old convention that never mattered (project_root WAS the session dir). Once
project_root becomes `$SCRIPT_DIR` for real, any op that `fopen("w")`s into a directory that was
only ever created session-side will silently fail (`fopen` returns NULL, most of this codebase's
own `if (!f) return 1;` guards swallow this with zero visible error).
**Procedure**: `grep -n 'fopen.*"w"' ops/*.c`, extract every directory each write path implies,
`grep` `button.sh`'s own `mkdir -p` lines, and confirm EVERY one of those directories is created at
`$SCRIPT_DIR` level, not just `$SESSION_DIR` level. Directories hit today: `pieces/apps/player_app/`
(twice, two different projects), `pieces/display/` (once). Assume there are more not yet hit only
because no test happened to exercise that code path yet — audit ALL of them proactively, don't wait
for each one to break something visible.

### 4. Find every `chdir()` in the call chain, then verify EVERY spawned child agrees
`launch_module()` and `run_module_synchronous()` in the shared `chtpm_parser_pal.c` BOTH
`chdir(project_root_path)` before exec'ing the module — meaning the PERSISTENT module process, and
therefore every op it spawns via the PAL VM, permanently runs with **CWD = project_root**, not the
session dir, for the ENTIRE life of that session. This is easy to miss because it's not the
project's own code — it's inherited from the shared engine.
**Procedure**: `readlink -f /proc/<pid>/cwd` on the real, live PAL module process (`prisc+x`) after
a fresh launch - don't assume from reading source alone. If it's project_root (it almost certainly
is, for any project using the shared `chtpm_parser_pal.c`), then ANY op spawned from that module has
NO independent way to discover the true session dir - not from CWD, not from any env var. A
`session_root = getcwd()` fix INSIDE such an op is a no-op bug fix that looks plausible but changes
nothing (this cost real time today - see mychara_menu_input.c's own first, wrong fix attempt).
**The real fix pattern**, when an op spawned from the chdir'd module genuinely needs a session-only
file: have the SHARED parser (which still knows the true session dir at its own top-level, pre-chdir
`resolve_root()`) additionally write that file to project_root too, matching the pattern already
used for `bv_state.txt`/`current_layout.txt` today - an additive, harmless second write, not a
per-op workaround.

### 5. Any file written by BOTH the shared engine AND a project's own op — check both writers agree
`current_layout.txt`, `pieces/apps/player_app/state.txt` (`active_target_id`), and similar
engine-owned files are written by the shared `chtpm_parser_pal.c` in one code path and READ (or
sometimes also written) by a project's own custom ops in another. When only ONE side's `mkdir`/root
convention gets fixed, the two sides silently diverge. Grep the SAME filename across both the
project's own `ops/*.c` AND the shared `_shared-lib/system/*.c` before assuming a single-sided fix
is complete.

### 6. Test with the REAL capture path, and watch the REAL process list — not assumptions
- A raw file-append test can pass even when the real X11-capture-to-relay chain has a genuine, separate
  bug — prefer the real XTest injector (`tp_test_send_key.+x`) once basic plumbing is proven, per
  `_.0.aigent-testing-k9.txt` Rule 11.
- **Never trust a `pgrep -f "<literal-string>"` absence as proof a process is dead** if that process
  might have been launched via a bare relative argv0 (`./system/foo`) — the literal string you're
  grepping for (a project name, a directory name) won't appear in the command line at all. Use
  `ps aux` broadly, or match by PID/cwd (`readlink -f /proc/<pid>/cwd`), not by string content.
- **Isolate test sessions completely between rounds** - kill everything, confirm zero stray
  processes (`pgrep` scoped and re-verified, matching the k9 doc's own Rule 3/8), THEN relaunch
  fresh. Manually invoking ops directly (`PRISC_PROJECT_ROOT=... ./ops/+x/foo.+x`) WHILE a real
  session is also live, testing the same files, produces contaminated, contradictory results that
  waste far more time than the isolation discipline costs upfront. This is the single biggest
  process mistake made during today's my-chara-txt investigation - overlapping manual tests against
  a live session muddied several real findings and had to be partially re-done.

### 7. A "confirmed working" backup/reference is not proof-of-behavior until re-tested fresh
An old build's own working behavior may be masking the exact same latent bug class (bucket
collisions that never mattered because two paths were coincidentally equal) - don't assume the
backup is bug-free just because it's the reference; re-verify the SPECIFIC symptom against it fresh,
isolated, same as bucket #6, rather than trusting memory of what it "should" do.

## Quick pre-flight command block

Run this against any new project BEFORE starting the actual migration, to scope the real work
upfront instead of discovering it live:

```sh
PROJ=/path/to/project
echo "--- bare relative paths ---"
grep -rn '"\./' "$PROJ"/button.sh "$PROJ"/system/*.c "$PROJ"/ops/*.c 2>/dev/null
echo "--- PRISC_PROJECT_ROOT convention ---"
grep -n "PRISC_PROJECT_ROOT=" "$PROJ"/button.sh
echo "--- session symlinks ---"
grep -n "ln -s" "$PROJ"/button.sh
echo "--- fopen('w') write targets (audit directories against button.sh's own mkdir -p) ---"
grep -n 'fopen.*"w"' "$PROJ"/ops/*.c
echo "--- button.sh's own mkdir -p calls (compare against the above) ---"
grep -n "mkdir -p" "$PROJ"/button.sh
echo "--- local (diverged) copies of shared-lib files, vs compiled-from-shared ---"
grep -n "gcc.*ops/chtpm_rgb_render.c\|gcc.*chtpm_parser_pal.c\|_SS=" "$PROJ"/scripts/build.sh 2>/dev/null
```

## ACTUALLY FIXED, 2026-08-21 (was "REAL, UNRESOLVED" below — corrected, see `!.tpmos-vs-khtpm_pal.md`)

The section below was written when this bug's root cause wasn't understood yet. It's since been
found AND fixed — in my-chara-txt's own local `system/chtpm_parser_pal.c` specifically, confirmed
by live re-reproduction in an isolated scratch copy (switched `PRISC_PROJECT_ROOT` to `$SCRIPT_DIR`,
drove `main -> Farm -> main -> Mine` via real key injection - every transition now renders correctly,
a genuinely new process spawns each time, no stuck screen, no missing process). Full root-cause
writeup and the exact fix shape (the `session_root_path`/`project_root_path` split +
`build_session_path_malloc()` + the `current_layout.txt` dual-write) is in
`!.tpmos-vs-khtpm_pal.md`'s own "How to fix that bug" section - not duplicated here to avoid drift
between two copies of the same explanation.

**Not yet done**: porting this fix to the canonical `014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c`
source, or to any other project's own local copy. Until that happens, any OTHER project attempting
the `PRISC_PROJECT_ROOT=$SCRIPT_DIR` strategy with a genuine multi-`<module>`-per-screen architecture
should still expect to hit this bug — the fix exists and is proven, it just isn't everywhere yet.

## (Historical) REAL, UNRESOLVED shared-engine bug — module relaunch on `<button href>` + different `<module>`

Found and confirmed live in my-chara-txt (2026-08-20), root cause not yet fixed upstream. When a
project's own `<button href="...">` navigates to a NEW `.chtpm` layout that declares a DIFFERENT
`<module>` path than the one currently running (e.g. `main.chtpm` → `pal/main_module.pal` vs.
`farm.chtpm` → `pal/farm_module.pal`), the shared `chtpm_parser_pal.c`'s own module-liveness/
relaunch logic can fail to actually fork the new module - `debug.txt` logs a `launch_module()` call,
but no new process appears, and the OLD module process (already exited/killed as part of the
transition) is simply gone. Nothing crashes loudly; the symptom is "the screen header changes but
the content/menu never updates again," because nothing is left running to ever recompose it.

**Only reproduces when `PRISC_PROJECT_ROOT` genuinely diverges from the session dir** - this is why
it only ever surfaced in my-chara-txt (the one project of the four with a real per-screen module
architecture) once its own button.sh switched `PRISC_PROJECT_ROOT` to `$SCRIPT_DIR`. Any OTHER
project attempting the harder `PRISC_PROJECT_ROOT`-switch strategy that ALSO uses a different
`<module>` per screen should expect to hit this same bug - check for multiple distinct `<module>`
lines across a project's own `pieces/chtpm/layouts/*.chtpm` files before choosing that strategy.
**Sidestepped, not fixed**, by the copy-based strategy above (`PRISC_PROJECT_ROOT` never diverges,
so this code path's own pre-existing behavior - whatever it actually is - keeps working exactly as
it always did). If a future project genuinely NEEDS the `PRISC_PROJECT_ROOT`-switch strategy AND
has a multi-module-per-screen architecture, this bug needs a real fix in the shared
`chtpm_parser_pal.c` first - don't attempt that combination blind.

## Status as of this writing

Applied clean, confirmed working:
- `101.mutaclsym🧟‍♂️️19.00` — full `PRISC_PROJECT_ROOT`-switch pass, camera/rendering confirmed
  (single-module project, never at risk of the relaunch bug above).
- `@.apps/my-chara-txt` — copy-based strategy (the ⭐ solution above), confirmed working live by
  direct user test ("that actually worked"). `button.sh`'s `ln -s`/`ln -sf` all replaced with
  `cp -r`/`cp -p`; `PRISC_PROJECT_ROOT` left as `$SESSION_DIR`; `persist_session_state()` added to
  the EXIT trap for `data/`, `config.txt`, `plots.txt`. Orchestrator.c/renderer.c's own Category B
  path fixes (Step 1 of the staged rollout) were applied first and kept - harmless either way, but
  not what actually fixed the crash; the button.sh copy-strategy (Step 2) is what did.

- `@.apps/piececraft-xyz` — copy-based strategy, applied 2026-08-20. `button.sh`'s `ln -s`/`ln -sf`
  all replaced with `cp -r`/`cp -p`; `PRISC_PROJECT_ROOT` left as `$SESSION_DIR`;
  `persist_session_state()` added to the EXIT trap for `data/`, `config.txt`, `board.txt`,
  `entities.txt`. One extra real bug found and fixed beyond the mechanical swap: `widget_cmds/
  inbox.txt` and `board_widget_bridge.txt` are a **live bidirectional channel** with board-viewer —
  board-viewer writes/reads them via its own `resolve_real_root()`/`focused_project_root`
  resolution (the REAL, non-session root), so a plain session-local copy would silently never see
  board-viewer's writes at all. Fixed by making `ops/pc_menu_input.c`'s own inbox drain resolve via
  `resolve_real_root()` too, matching where board-viewer actually writes — then the `widget_cmds`
  directory symlink and the `board_widget_bridge.txt` symlink were both deleted outright (dead
  weight once both sides read/write the real root directly, no session copy needed at all). Build
  clean, awaiting user test/signoff.
- `&.widgits/board-viewer` — copy-based strategy, applied 2026-08-20. Turned out to need almost no
  thought: every one of its own ops (`bv_compose_frame.c`, `bv_menu_input.c`) already reads/writes
  ALL real state via `focused_project_root` (resolved through `resolve_host_root()`), never through
  its own `project_root`/session dir — board-viewer's own session-local files (`bv_state.txt`, the
  ledger, `pieces/display/*`) are genuinely ephemeral by design. So the whole block of `ln -sfn` (
  `system`, `ops`, `pal`, `pieces/chtpm`, `pieces/registry`, `default_op.txt`,
  `projects/board-viewer/pieces/board_viewer`) converted straight to `cp -r`/`cp -p` with **no**
  `persist_session_state()` needed at all — nothing here is real persistent state. Build clean,
  awaiting user test/signoff.

Every project in the house is now confirmed to have zero real `ln -s`/`ln -sf` left in its own
`button.sh`. mutaclysm-neo is the one outlier still on the harder `PRISC_PROJECT_ROOT`-switch
strategy (see "one-sentence root cause" above for why: it was fixed FIRST, before the copy-based
strategy was even discovered, and a single-module project was never at risk of the relaunch bug
that strategy exists to sidestep) — retrofitting it to the copy-based strategy for consistency is
possible but not yet done; not urgent since it works and isn't multi-module-fragile.

**CORRECTION, 2026-08-20 (later same day)**: the "zero symlinks house-wide" claim below was WRONG —
it only checked what's on disk *right now*, but most projects' `ln -s` calls live inside `button.sh`
and only materialize once a session actually launches (into `pieces/sessions/`, which that sweep
excluded). A project can look symlink-free on disk while its own `button.sh` still creates real
symlinks on every single run. Re-audited properly by grepping every `button.sh` in the house for
literal `ln -s` calls (not the resulting on-disk state) — **22 of ~68 button.sh files in the house
still use live `ln -s` and have NOT been migrated to either strategy**:

```
0.user-pal👤️/00.login-signup/button.sh            (10)
0.user-pal👤️/01.avatar-creation👤️/button.sh        (10)
102.agy-txt/button.sh                              (1)
102.editor-📄️00.00/button.sh                       (2)
@.apps/aomorai-editor/button.sh                    (14)
@.apps/civ-txt/button.sh                           (15)
@.apps/my-biotech/button.sh                        (10)
@.apps/my-lawyer/button.sh                         (10)
@.apps/myne-qrypto/qtc/button.sh                   (9)
@.apps/tactics-txt/button.sh                       (17)
@.apps/text-editor-xyz/button.sh                   (8)
@.apps/TSC_ELO/button.sh                           (9)
@.apps/TSC_ELO/widgets/setup/button.sh             (7)
@.apps/yahoo-app/button.sh                         (8)
+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00/button.sh        (7)
&.widgits/context-menu/button.sh                   (6)
&.widgits/event-editor/button.sh                   (6)
&.widgits/event-ez.backup-20260805-163329/button.sh (6, likely a dead backup dir - verify before touching)
&.widgits/event-ez/button.sh                       (6)
&.widgits/file-menu/button.sh                      (7)
&.widgits/tile-picker/button.sh                    (7)
&.widgits/yahoo-broker/button.sh                   (9)
```

(count in parens = number of `ln -s`/`ln -sf` lines in that file.) Everything NOT in this list either
already uses the copy-based strategy (my-chara-txt, piececraft-xyz, board-viewer) or the
`PRISC_PROJECT_ROOT=$SCRIPT_DIR` strategy (mutaclysm-neo, mutaclysm+18.0G, pal-forum). **The actual
fix for every project in the list above is the copy-based strategy (⭐ solution at the top of this
doc)** — same protocol each time: backup zip → baseline test → convert `ln -s`/`ln -sf` to
`cp -r`/`cp -p` → add `persist_session_state()` for any real persistent files → rebuild → user tests
→ signoff → delete backup.

## STEP 1 of 2 DONE, house-wide: mechanical `ln -s` → `cp -r` swap

Use `check-symlinks.sh` (house root) to re-verify current state at any time:
`./check-symlinks.sh` (audits the house) or `./check-symlinks.sh /path/to/other/folder` (audits
anything else, e.g. a backup - add `--include-sessions` to also see frozen session dirs in a backup).

`$.crypts/symlink-swap.sh` was written and run against all 21 files in the list above (except
`102.agy-txt` and `102.editor-📄️00.00`, whose `ln -s` count turned out to be comments mentioning
`ln -sfn`, not real calls - zero actual migration needed there). It does ONLY the safe, mechanical
part: rewrites `ln -s`/`ln -sf`/`ln -sfn` → `cp -r` in a button.sh and backs up the original as
`button.sh.pre-symlink-swap` next to it. Usage: `./symlink-swap.sh [--dry-run] <button.sh>...` or
`./symlink-swap.sh [--dry-run] --list-file <path-list.txt>`.

**Result**: 20 of 21 real targets fully converted, 0 `ln -s` calls remaining in each. `PRISC_PROJECT_ROOT`
was NOT touched in any of them (still `$SESSION_DIR`, per the ⭐ solution) - so this step alone is safe
and shouldn't have broken anything behaviorally, but **none of these 20 have been rebuilt or
user-tested yet**. `&.widgits/event-ez.backup-20260805-163329/button.sh` was deliberately left
unswapped (it's a dated backup dir, not a live project - verify with the user before touching it).

## STEP 2 of 2 DONE, house-wide: `persist_session_state()` + live-channel fixes, per project

This is the judgment-requiring half `symlink-swap.sh` deliberately does NOT automate: deciding which
of the newly-copied files are REAL PERSISTENT STATE (must be copied back to `$SCRIPT_DIR` in the EXIT
trap before `rm -rf "$SESSION_DIR"`, via a `persist_session_state()` function - see my-chara-txt's or
piececraft-xyz's own button.sh for the exact working pattern) vs. static/read-only code that's fine to
just re-copy every launch and never touch again.

**Classification already done by grepping each button.sh's own `cp -r "$SCRIPT_DIR/...` lines for
anything that looks like session/save/config data** (this needs re-verification per project - it's a
first-pass guess from filenames, not a confirmed audit of the C code):

| Project | Needs `persist_session_state()` for | Likely also needs the board-viewer live-channel C fix |
|---|---|---|
| `0.user-pal👤️/00.login-signup` | ✅ DONE 2026-08-20: `users`, `current_login.txt`, `xyzfs/session.pdl` + `HOUSE_ROOT` export (see discovery notes below). Harness FAIL → real PASS. Signed off 2026-08-21, backup deleted. | — |
| `0.user-pal👤️/01.avatar-creation👤️` | ✅ FIXED+TESTED 2026-08-21: `pieces/world_01` only (`avatar_window_pids.txt` deliberately NOT persisted — truncated every launch by design). No HOUSE_ROOT export needed: identity ops take `USERPAL_LOGIN_ROOT`. Both harness scenarios fixed (stale `$LOGIN/$XYZ` paths → house-level; open-window message string) → both OVERALL PASS. Live injection test PASS incl. unclean-kill persistence. Full report: completed-sym-list.md #2. | — |
| `@.apps/aomorai-editor` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt`, `board.txt`, `entities.txt`, `widget_cmds/`, `board_widget_bridge.txt` (audit found NO `data/` copy-in — table guess wrong) | verify inbox flow during batch test |
| `@.apps/civ-txt` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt`, `board.txt`, `terrain_legend.txt`, `entities.txt`, `widget_cmds/`, `board_widget_bridge.txt` (no `data/`) | verify inbox flow during batch test |
| `@.apps/tactics-txt` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt`, `units.txt`, `board.txt`, `terrain_legend.txt`, `entities.txt`, `widget_cmds/`, `board_widget_bridge.txt` (no `data/`) | verify inbox flow during batch test |
| `@.apps/my-biotech` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt` (no `data/`) | — |
| `@.apps/my-lawyer` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt` (no `data/`) | — |
| `@.apps/myne-qrypto/qtc` | ✅ FIXED-UNTESTED 2026-08-21: `wallets/`, `data/` | — |
| `@.apps/TSC_ELO` | ✅ FIXED-UNTESTED 2026-08-21: `config.txt` (no `data/`). Ledger ops already write REAL house-level xyzfs via seeded `house_root.txt` — verified copy-safe | — |
| `@.apps/TSC_ELO/widgets/setup` | ✅ FIXED-UNTESTED 2026-08-21: `projects/setup/pieces/setup/` (not "none obvious" — it does copy this in) | — |
| `@.apps/yahoo-app` | ✅ FIXED-UNTESTED 2026-08-21: `data/` via `$BANK_SESSION` (var-name trap handled) | — |
| `&.widgits/yahoo-broker` | ✅ FIXED-UNTESTED 2026-08-21: `projects/yahoo-broker/pieces/{yahoo_broker,broker_widget}/`, `data/` | — |
| `+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00` | ✅ FIXED-UNTESTED 2026-08-21: `location.txt` | — |
| `@.apps/text-editor-xyz` | ✅ FIXED-UNTESTED 2026-08-21: `docs/` via `$EDITOR_SESSION` (the user documents — handled) | — |
| `&.widgits/context-menu`, `event-editor`, `event-ez` | ✅ VERIFIED STATELESS 2026-08-21 — no persist needed (no data copy-in, empty real-root pieces/system, ops write per-session scratch or take explicit dest args) | — |
| `&.widgits/file-menu` | ✅ FIXED-UNTESTED 2026-08-21: `projects/file-menu/pieces/file-menu/`; exit-trap ledger_append already house-safe | — |
| `&.widgits/tile-picker` | ✅ FIXED-UNTESTED 2026-08-21: `picker_items.txt` (confirmed real) | — |

**Step 2 fix phase COMPLETE 2026-08-21; agent-side batch testing ALSO COMPLETE 2026-08-21** —
all 19 accounted for (2 tested+signed-off/tested, 14 patched+tested, 3 verified stateless).
Testing summary: 14/14 persist functions mechanically validated (after mkdir -p hardening);
live mutation tests PASS on all three structural variants (civ-txt, qtc, text-editor-xyz);
boot/clean-shutdown smoke PASS on all others except yahoo-broker (host-launched widget,
mechanical-only); harness suites: civ-txt/my-lawyer/tactics/biotech×2/qtc×2 PASS, TSC_ELO
pvp_duel FAIL = documented pre-existing. Two harness scenarios fixed during testing (qtc wallet:
stale mid-run real-root assertion → session-path + new post-exit persistence check; biotech +
qtc: `setsid` on session launch so cleanup sweep can't kill the scenario). Full per-project
reports in `completed-sym-list.md`. Remaining work = human signoff per project.

**"the board-viewer live-channel C fix" referenced above** means the exact same bug/fix as
piececraft-xyz's `widget_cmds/inbox.txt` writeup further down in this doc: if the project's own
`ops/*_menu_input.c` drains `widget_cmds/inbox.txt` via bare `project_root` (= session dir, under this
strategy) instead of `resolve_real_root()`, it will silently never see commands board-viewer writes to
the REAL root. Grep each candidate project's own `ops/*.c` for `widget_cmds/inbox.txt` and
`board_widget_bridge.txt` to find the exact spot, same as `pc_menu_input.c`'s fix.

**NEW DISCOVERY, 2026-08-20 (login-signup) — check ALL remaining projects for this smell**:
some ops resolve their REAL root by `realpath()`-ing THROUGH what used to be a session symlink
(login-signup's `resolve_install_root()` probes `project_root/users` with `realpath()`; under
symlinks that pierced into the real project root; under plain copies it resolves to `$SESSION_DIR`
itself). Worse, its `resolve_house_root()` walks TWO grandparents up from that — under copies that
landed on `pieces/`, so new signup xyzfs trees were silently created at `<project>/pieces/xyzfs/`
(garbage). The general fix, no C changes needed: **if an op's root-resolution has an env-var
escape hatch, pin it from button.sh** (`resolve_house_root()` checks `HOUSE_ROOT` first →
`export HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"`). Grep each remaining project's ops for
`realpath(` + `getenv(".*ROOT")` during pre-flight; if a realpath walk has NO env escape hatch,
that project needs a small C fix (add one), not a button.sh hack.
Related stale-assertion trap: login-signup's own harness asserted user trees at PROJECT-level
`xyzfs/users/<uuid>` while the C code (and all 14 real user trees on disk) put them at HOUSE-level
`<house>/xyzfs/users/<uuid>` — i.e. part of its FAIL was a fake fail against a layout that hasn't
existed for a while. Scenario now asserts house-level mid-session + adds a post-exit
persist-survival block (the doc's principle 2a/2b applied verbatim). Also: `xyzfs/session.pdl`
(login/logout state) IS written into the session copy and needed persisting; the per-user TREES
did not (written straight to house via HOUSE_ROOT).

**Recommended order of operations for whoever (human or agent) picks this up**, per project:
1. Re-verify the classification table row above against the project's own `ops/*.c` (don't trust the
   guess - confirm which files are actually written mid-session vs. only ever read).
2. Add `persist_session_state()` to the EXIT trap for the confirmed-persistent files (copy-back before
   `rm -rf "$SESSION_DIR"`), following my-chara-txt/piececraft-xyz's own button.sh as the template.
3. If board-viewer integration exists, grep for `widget_cmds`/`board_widget_bridge` in the project's
   own ops/*.c and apply the `resolve_real_root()` fix if it's still reading via bare `project_root`.
4. Rebuild (`bash scripts/build.sh` or equivalent).
5. Hand off to the user to test live - do NOT declare a project done without their signoff, per the
   house's own established protocol ("try one at a time and i will test... each time").
6. Only after signoff: the project's own `button.sh.pre-symlink-swap` backup can be deleted.

**Also flagged by the user, 2026-08-20**: once this migration list above is worked through, a
house-wide build/distribution tool (same idea as TPMOS's own compile step) is planned — compile once,
copy/distribute the resulting binaries to every project in a single run, instead of each project's
own `button.sh`/`scripts/build.sh` doing it independently. Not designed or built yet; noted here so
the next agent doesn't rediscover the intent from scratch.

**Full house-wide sweep, 2026-08-20 (earlier, on-disk-only — see correction above)**: `find "$HOUSE"
-type l | grep -v '/pieces/sessions/'` returns nothing — confirmed zero real symlinks left anywhere
in the house outside of disposable live session directories (which are ephemeral by design, recreated
fresh every launch, not a Windows-portability concern on their own). Two stragglers found and fixed
during this sweep, beyond the button.sh/session-dir work above:
- `101.mutaclsym🧟‍♂️️+18.0G/pieces/hero/hero.pdl` — a static content symlink (`-> ../world_01/
  map_start/hero/piece.pdl`), unrelated to session isolation. Replaced with a plain file copy. Note:
  this is now a point-in-time copy, not a live link — if `piece.pdl` is edited later, `hero.pdl` will
  NOT automatically follow; re-copy manually if that ever matters.
- `@.apps/my-chara-txt/system/{chtpm_parser_pal.c,chtpm_rgb_render.c,prisc+x.c}` — cross-project
  symlinks into `&.widgits/_shared-lib/`'s own shared-engine source, the one place my-chara-txt
  hadn't followed the house convention every OTHER project (piececraft-xyz, board-viewer,
  mutaclysm-19.00) already uses: a plain local copy of the shared engine source, rebuilt from wsr-pal
  by hand when the shared engine changes, not auto-tracked via symlink. Replaced with plain copies;
  rebuilt clean (`scripts/build.sh` → "build ok"). Same trade-off as above: my-chara-txt will no
  longer auto-pick-up future `_shared-lib` changes — re-copy by hand if/when the shared engine is
  updated and my-chara-txt needs the update too.

## 📦 MANAGER HANDOFF — 2026-08-21 session complete (Step 2 + batch test + compile sweep)

Written by the opencode agent that finished the migration, as a handoff back to the manager agent.
Read this section instead of re-deriving state from git-style diffs; everything below is verified,
not planned.

**State of the world:**
- **Step 1 + Step 2 both DONE house-wide.** All 19 projects accounted for: login-signup
  signed off (backup deleted), avatar-creation agent-tested, 14 patched+agent-tested, 3 verified
  stateless (`context-menu`, `event-editor`, `event-ez`). Per-project detail: table above +
  `completed-sym-list.md` (which also has human-test recipes for signoff).
- **House-wide compile sweep: ALL 44 build scripts PASS** (baseline established this session;
  re-run `/tmp/opencode/house_compile_sweep.sh`-style find+loop if you want to re-verify — it's a
  plain `find . -name "build*.sh"` + run-each loop). Five PRE-EXISTING breakages fixed en route
  (none caused by the migration): undefined `$_SS` vars in `002.zoo.../02.z00-INK`,
  `01.muchi-pals-🥚️-13.01`, `101.mutaclsym🧟‍♂️️+18.0G` build.sh (pointed at their own local copies,
  per those scripts' own self-containment headers), wsr-pal missing `-I/usr/include/freetype2`
  + `-lfreetype`, tile-picker missing vendored `stb_image.h` (dangling include left behind by the
  2026-08-14 livedesk-taskbar consolidation).
- **NEW STANDING HOUSE RULE** recorded in `!.HOUSE_STDS.md` §A.2 + pitfall 7b: **NEVER use
  symlinks anywhere in this tree — they break on Windows.** Copy or compile-from-canonical-source
  instead. §A.2 was rewritten for the copy-in/persist-out model; pitfall 10 corrected: live binary
  rebuilds NO LONGER apply to running sessions (sessions run their own copied `ops/`) — restart
  after rebuild.
- Harness scenarios touched this session (5 total): avatar×2 (stale project-level paths →
  house-level), qtc wallet (mid-run real-root assertion → session-path + NEW post-exit persistence
  check), qtc + biotech research (sessions launched with `setsid` so cleanup sweeps can't kill the
  scenario). TSC_ELO pvp_duel FAIL = documented pre-existing, untouched.

**Behavioral changes the manager must internalize for its own work** (each of these has already
bitten a real agent or scenario once):
1. Mid-session, real-root state does NOT change. Anything reading "current" state mid-run must read
   `$SESSION_DIR/...`; the real root only updates at clean exit via `persist_session_state()`
   (single files overwrite wholesale, dirs merge via `cp -r src/. dst/`, every line mkdir-guarded).
2. Rebuilds don't reach running sessions — restart the session after `scripts/build.sh`.
3. Fresh checkouts are safe: persist blocks auto-create real-root dest dirs.
4. Identity/user data lives at HOUSE level `<house>/xyzfs/users/<uuid>/`, not project level.
5. Volatile files deliberately NOT persisted: `avatar_window_pids.txt`, quit_flag, history, relays,
   gui_state. Don't "fix" their absence from persist lists.
6. Launching sessions inside tests/scripts: use `setsid`, or the button.sh cleanup death-sweep will
   group-kill your script.
7. Pre-existing quirk (unchanged): external `kill -TERM` on `button.sh run` defers the EXIT trap
   while foreground `keyboard_input` runs; Ctrl+C in-UI or killing keyboard_input fires cleanup.

**Pending (human work, not agent work):**
- Per-project human signoff using the recipes in `completed-sym-list.md` → only then delete each
  project's `button.sh.pre-symlink-swap` backup.
- The house-wide build/distribution TOOL flagged below (2026-08-20) is now HALF-built
  (2026-08-21): `$.crypts/compile-runner.sh` (sibling of `harness-runner.sh`, same usage shape:
  bare / `<substring>` filter / `--list`) runs every project's OWN build script house-wide and
  drops one consolidated REPORT.md under `$.crypts/build-reports/<ts>/`. It is deliberately NOT a
  compile-once-distribute-everywhere tool — each project stays self-contained per standing user
  instruction; what it buys is the single command + single report. Verified live: 44/44 PASS on
  first run (baseline report kept at `$.crypts/build-reports/20260821-040431/`).

## `$.crypts/harness-runner.sh` — house-wide test-harness sweep, usage + what it found

Built 2026-08-20 in direct response to "future work item 2" mentioned earlier in this doc (a
house-wide test-harness runner that drops one consolidated report). Finds every `test-harn*/`
directory in the house (not just the migration list above — this covers EVERY project with a
harness, including ones nobody is currently touching), compiles each harness's own small C ops if
it has a `compile`/`build` action, auto-detects and runs its real scenario action, and writes a
`REPORT.md` + one `.log` per harness under a timestamped `$.crypts/harness-reports/<timestamp>/`
directory.

**Usage**:
```
$.crypts/harness-runner.sh                 # run every harness found in the house
$.crypts/harness-runner.sh <substring>      # only run harnesses whose path contains <substring>
$.crypts/harness-runner.sh --list           # list what would run, without running anything
HARNESS_TIMEOUT=300 $.crypts/harness-runner.sh   # override the default 240s per-harness timeout
```
Runs correctly under both `bash` and plain `sh`/dash (verified live) — no bashisms (arrays,
`[[ ]]`, `<<<`, process substitution, or a bare `set -o pipefail` that dash treats as fatal even
under `|| true`) anywhere in it, matching `check-symlinks.sh` and `symlink-swap.sh`.

**Scenario auto-detection**: scans each harness's own `button.sh` case statement for the label
whose body invokes a `scenarios/*.sh` script, and runs THAT action — not hardcoded to "demo",
since different harnesses use different verbs (`pvp`, `demo-load`, etc.). If a harness has multiple
scenario-invoking labels (e.g. `text-editor-xyz`'s separate `demo-load`/`demo-save`/`demo` actions),
it picks whichever label appears FIRST in the file — check the harness's own button.sh by hand if
you want a specific/more-comprehensive action run instead.

**Classification priority** (`classify_harness()` in the script): a scenario's own printed
`=== OVERALL: PASS ===` / `=== OVERALL: FAIL ===` verdict (a convention most of the house's
harnesses already use) is trusted FIRST, before the outer process's exit code. This matters because
a scenario can fully pass and then hang or get killed during its own TRAILING cleanup (its EXIT trap
calling `button.sh kill`) — the outer `timeout` then kills it with a nonzero exit code even though
the real test already succeeded. Trusting exit code alone is a confirmed false-negative (caught live
testing my-chara-txt: exit 143 from a slow cleanup phase, but `=== OVERALL: PASS ===` with all 13
checks already printed). Only falls back to exit-code/FAIL-line heuristics when no OVERALL verdict
is present at all.

**Three real bugs found and fixed while building/using this tool, 2026-08-20**:

1. **`harness-runner.sh` itself was `cd`-ing into the wrong directory.** It ran each harness with
   CWD = the harness dir (`<project>/test-harn-same/`), but house convention for scenario scripts is
   inconsistent: some `cd "$PROJECT_DIR"` themselves at the top (my-chara-txt's own
   `demo_end_turn.sh`), others use bare relative paths (`data/blockchain.txt`, `button.sh`,
   `pieces/sessions/*/`) that silently assume the CALLER's CWD is already the project root
   (`041.pal-chain`'s `demo_2wallet_ux.sh`, confirmed live: broke with `cannot open
   /pieces/keyboard/history.txt` — a bare, project-prefix-less absolute path). Fixed by always
   running with CWD = the PROJECT root (one level up from the harness dir), which satisfies both
   conventions. **041.pal-chain was never part of the symlink migration** (already on the
   `PRISC_PROJECT_ROOT=$SCRIPT_DIR` strategy, confirmed) — its harness failure has a separate,
   pre-existing, unrelated root cause (its own compiled binaries aren't present/current in this
   checkout) that this fix did not resolve and this task doesn't need to resolve.

2. **A real "mid-session vs. post-session assertion" bug, found in FOUR scenario scripts**
   (`my-chara-txt/test-harn-same/scenarios/demo_end_turn.sh`,
   `civ-txt/test-harn-same/scenarios/demo_setup_and_turn.sh`,
   `my-biotech/test-harn-same/scenarios/demo_research_and_end_turn.sh`,
   `tactics-txt/test-harn-same/scenarios/demo_setup_and_battle.sh`). Each one set its own
   `CONFIG`/`LEDGER` (and my-biotech's `CORPUS`) variables to `$PROJECT_DIR/pieces/system/config.txt`
   and `$PROJECT_DIR/data/master_ledger.txt` — the REAL, non-session root. Under the copy-based
   strategy this doc recommends, that's WRONG for a mid-session assertion: real persistent state only
   gets copied back to `$PROJECT_DIR` by `persist_session_state()` in the EXIT trap, i.e. only once
   the session ENDS. Every mid-session check against those paths either read stale data or failed to
   open the file at all — a pure false-negative, not a real bug in the game/app itself (confirmed:
   my-chara-txt's FINAL config.txt at `$PROJECT_DIR`, read AFTER the session ended, was completely
   correct - day 3, health 90 - even on a run where every mid-session check against that same path
   had failed). **Fixed by pointing `CONFIG`/`LEDGER`/`CORPUS` at `$SESS/pieces/system/config.txt` /
   `$SESS/data/master_ledger.txt` instead** (the session's own live copy) in all four scripts.
   Verified: my-chara-txt went from false-FAIL to a real 13/13 PASS after the fix.
   **This is a general principle for anyone writing or fixing a scenario for a copy-based-strategy
   project**: read live game state from `$SESS/...` while a session is running; only read
   `$PROJECT_DIR/...` for (a) SETUP, before the session launches, or (b) a check that's deliberately
   verifying the persist-on-exit behavior itself, done AFTER `button.sh kill`/the session has fully
   exited.

3. **The classifier bug described above** (exit code trusted over the scenario's own verdict) —
   fixed as part of building this tool, not a pre-existing project bug.

**Three MORE real bugs found and honestly fixed (not benchmarked around), 2026-08-20, direct
instruction: "does that harness include tb and the other projects... did u fix the fake fails since
its not failing? ... yes but none of them actually fail so i dont wanna have it using that
benchmark. is there a way to honestly get them to all say pass?"**:

4. **A baseline-frame readiness race, in THREE scenarios** (`civ-txt/demo_setup_and_turn.sh`,
   `tactics-txt/demo_setup_and_battle.sh`, and intermittently `my-chara-txt/demo_end_turn.sh` on a
   fast rerun). Each one's readiness loop waited for the screen's TITLE banner (e.g. "C I V - T X T")
   to appear before locking in `$SESS` and making its first real content assertion - but the title
   banner renders in an EARLIER frame than the actual field content that first assertion checks
   (e.g. "Victory condition: (not set)"), so the assertion could race a frame that hadn't caught up
   yet. **Fixed by waiting for the actual content string the first assertion checks, not just the
   title**, in all three scenarios.
5. **A genuinely stale test assertion in `tactics-txt/demo_setup_and_battle.sh`**: the scenario
   documented and asserted around a real, reproducible "phantom END_TURN fires on screen entry" bug
   (its own header comment named the suspected root cause: an `interact_relay.txt` read-cursor not
   resetting across a `chtpm_parser_pal.c` module handoff). Manually replayed the exact sequence with
   generous settle time - **the phantom quirk no longer reproduces at all** (confirmed: `active_side`
   stayed `1`, not `2`, after screen entry). Something unrelated to this session's own work fixed the
   underlying engine bug at some point. Asserting a bug that no longer exists is now simply WRONG, not
   a real check - rewrote the assertion sequence to match the correct, current, quirk-free
   `active_side`/`turn` progression per `tactics_menu_input.c`'s own `END_TURN` handler.
6. **Contention between harnesses in a full house-wide sweep**: running all ~18 harnesses back-to-
   back left enough real leaked state (board-viewer widget processes, general system load from a
   prior harness) that LATER harnesses' own 30s session-launch readiness checks could time out even
   though the exact same harness passed cleanly seconds earlier in isolation - a real infra flake in
   the RUNNER, not a project regression. **Fixed by running the house's own `EMERGENCY_KILL.sh`
   between every harness** (see harness-runner.sh's own inline comment) so each one starts from a
   genuinely clean slate.

**`my-lawyer`'s harness was a true stub** (an empty `scenarios/` dir, its own `button.sh` pointing at
a scenario file that was never written) - not something to fake-pass or leave broken. Built
`$.crypts/ping-project.sh`: a generic, reusable, HONEST smoke test (launch a real session, wait for a
real STABLE non-empty frame - two identical polls 1s apart, not a one-shot/transient/mid-write read -
confirm the process is still alive a beat later, clean kill) for any project that doesn't have (or
doesn't yet have) a real gameplay scenario. It's a floor, not a substitute for real coverage - see its
own header comment for exactly how to wire it into another project's own harness. Wired into
my-lawyer's `test-harn-same/scenarios/demo_ping.sh` as the first real user; `button.sh`'s own `demo`
action now points at it. Confirmed honestly passing end-to-end.

**Final verified state after all six fixes above, full house-wide sweep, 2026-08-20**: 7 of 18
harnesses PASS for real - `civ-txt`, `my-biotech`, `my-chara-txt`, `my-lawyer`, `myne-qrypto/qtc`,
`tactics-txt`, `*.START_BUTTON`. The remaining 11 `FAIL`s are ALL confirmed pre-existing and/or
already-known-pending, NOT fixed as part of this (and should not be benchmarked around either -
each is a real, distinct thing worth someone's attention on its own, not this task's scope):
- `0.user-pal👤️/00.login-signup` and `@.apps/text-editor-xyz` - two of the 20 projects in the "STEP 2
  NOT STARTED" table above; their harness fails are the EXPECTED symptom of `persist_session_state()`
  not being wired up yet for them specifically (login-signup's `users`/`current_login.txt`,
  text-editor-xyz's `docs`) - not a new or surprise bug, just unstarted migration work.
- `102.editor-📄️00.00` - real CLEAR-button bug (own project code, not session/symlink related).
- `@.apps/TSC_ELO` - real setup-widget-boot bug (confirmed identical failure on both pre- and
  post-swap button.sh, see the pre-existing-bug-vs-regression technique above).
- `041.pal-chain⛓️`, `041.pal-forum👥️`, `044.pal-chat-irc👥️+2`, `045.muchi-pal-agent🤖️+1++`,
  `0.a-z-pets-plan`, `101.mutaclsym🧟‍♂️️+18.0G`, `102.agy-txt` - each a separate, real, pre-existing
  bug in a project this symlink-migration task never touched. One-line cause per project (from the
  `20260820-231117` report's own `.log` files - re-verify against a fresh `harness-runner.sh` run if
  time has passed, these are a snapshot, not guaranteed still-accurate):
  - `041.pal-chain⛓️` - project's own compiled binaries missing/stale in this checkout
    (`./system/keyboard_input: No such file or directory` at launch).
  - `041.pal-forum👥️` - its own session dir never appears at all
    (`ls: cannot access 'pieces/sessions/*/'`) - launch itself is failing, cause not chased further.
  - `044.pal-chat-irc👥️+2` - session dir DOES appear but `current_frame.txt` inside it is never
    found by the harness's own path construction - cause not chased further (may be a harness-side
    path bug rather than a project bug - not investigated).
  - `045.muchi-pal-agent🤖️+1++` - session launch times out within 30s, harness's own message
    suggests "compile-on-launch may still be running" - i.e. possibly just needs a longer timeout,
    not necessarily a real bug - not chased further.
  - `0.a-z-pets-plan` - references a file path belonging to a DIFFERENT project
    (`01.muchi-pals-🥚️-13.01/pieces/world_01/map_lobby/egg_2`, `.../test_pet_turtle/state.txt`) that
    doesn't exist in this checkout - a stale cross-project reference, not investigated further.
  - `101.mutaclsym🧟‍♂️️+18.0G` - real gameplay bug: hero position doesn't change after a real 'd'
    movement keypress while INTERACT-engaged (`before='pos_x=9 pos_y=12' after='pos_x=9 pos_y=12'`).
  - `102.agy-txt` - fails, cause not individually diagnosed (lowest priority - this project was
    already confirmed to need ZERO symlink work, a false positive from the very first grep sweep,
    so its harness failure was never chased down).
  None of the above were investigated beyond this one-line read of their own `.log` - a real
  root-cause pass on any of them is separate, standalone work, not part of this symlink-migration
  task. Use `harness-runner.sh`'s own per-harness `.log` file under the latest
  `$.crypts/harness-reports/<timestamp>/` (or re-run it) to see full output for any of these.

## ❓ OPEN QUESTIONS from the next agent (2026-08-20, appended before starting — for manager/human to answer)

Verified against the live filesystem before writing these: both tools exist; `./check-symlinks.sh`
reports zero real symlinks on disk AND zero button.sh files creating symlinks house-wide; all
`.pre-symlink-swap` backups for the list below are present; `PRISC_PROJECT_ROOT` is still
session-dir-based in every spot-checked file (yahoo-app uses `$BANK_SESSION`, as the table flags).

1. **Working set is 19, not 20?** The "STEP 2 NOT STARTED" list says "these 20" but contains 19
   entries. The correction-list arithmetic also implies 19 live targets (22 listed − 2 false
   positives − 1 deliberately-untouched backup dir). Live audit confirms exactly those 19 remain.
   Confirm: working set = the 19 projects in the STEP 2 list, nothing missing?
   *(ANSWERED inline by human: no whole-project zips needed for Step 2 — `.pre-symlink-swap`
   button.sh backups suffice; and the `event-ez.backup-20260805-163329` dir was deleted
   intentionally by the human, so that item is moot.)*

5. **Doc hygiene**: several paragraphs in this doc got text-mangled somewhere along the way (e.g.
   the "Why mutaclysm-neo isn't just being retrofitted..." paragraph, the "one-sentence root cause"
   paragraph, parts of the board-viewer status bullet) — meaning is recoverable from context. OK to
   rewrite those for clarity while updating the status tables, preserving meaning?
