# !.tpmos-vs-khtpm_pal.md — how the current shared engine (khtpm/chtpm_parser_pal) compares to TPMOS

Written 2026-08-21, direct question: "do we use orchestrator same as tpmos? explain... how they are
similar and different. we may have done this before but a lot has changed. were aiming for very
similar shapes." Sources: `#.DOX/TPMOS_PATTERN_FINAL.md`, `#.haiku+/tpmos-re-dox/!.TPMOS_ONBORD_BIBLE_10.md`,
`014.wsr-pal💸️📌️+2/dox/pal-tpmos-parity-std.md` (the canonical TPMOS-side docs), cross-checked against
the REAL, current shared engine source: `@.apps/my-chara-txt/system/orchestrator.c`, and multiple
projects' own `button.sh` (piececraft-xyz, my-chara-txt, civ-txt, etc.) — not just docs on either side,
actual running code.

## Short answer

**Yes — `orchestrator.c` is directly descended from TPMOS's own orchestrator pattern, ported into
every khtpm_pal project's own `system/` dir** (`TPMOS_PATTERN_FINAL.md` says so explicitly: "orchestrator.c
— meta-launcher (from TPMOS, adapts for each project)"). Same job: fork/exec the real service
processes, track their PIDs, catch `SIGINT`/`SIGTERM`, kill everything cleanly on exit. Same core
`fork()`/`exec()`/`waitpid()` discipline (never `system()`), same "kill the whole process group + all
tracked children" shutdown shape.

**But the actual invocation order is inverted from TPMOS's own canonical description**, and the
"game loop" layer underneath it works completely differently now. Those are the two real,
architecturally significant divergences — everything else is close-to-identical or a straightforward
evolution. Details below.

## 1. Who calls whom — INVERTED from the canonical TPMOS doc

**TPMOS's own canonical shape** (`TPMOS_PATTERN_FINAL.md`):
```
User runs: orchestrator.c (the TOP-level process)
    -> orchestrator.c calls button.sh
    -> button.sh starts renderer/chtpm_parser_pal/game_manager/keyboard_input in background, then EXITS
    -> orchestrator.c stays alive, monitors children
    -> Ctrl+C -> orchestrator.c kills all children, exits
```
`orchestrator.c` is the entry point. `button.sh` is a one-shot service-starter that runs and exits
immediately — it does not stay alive.

**khtpm_pal's actual, current shape** (verified against real `button.sh` in piececraft-xyz,
my-chara-txt, civ-txt, and every other project in the house — this is completely consistent
across all of them, not a one-off):
```
User runs: button.sh (the TOP-level process, invoked directly - "./button.sh run")
    -> button.sh forks orchestrator INTO THE BACKGROUND, keeps its PID ($ORCH_PID)
    -> orchestrator (background) launches renderer, chtpm_parser_pal (with PAL_LAYOUT),
       chtpm_rgb_render, palnet_peer - the real service processes
    -> button.sh itself then runs ./system/keyboard_input IN THE FOREGROUND (blocking) -
       THIS is what keeps the terminal session alive, not orchestrator
    -> button.sh's own EXIT trap (fires on keyboard_input exiting, or Ctrl+C) kills
       $ORCH_PID, waits for it, runs its own project-specific cleanup, deletes the session dir
```
So the roles are swapped: **button.sh is the long-lived top-level process a human actually
interacts with** (it owns the foreground, blocking keyboard_input call and the EXIT trap);
**orchestrator is a background child button.sh forks and later kills**, not the other way around.
TPMOS's own doc has orchestrator spawning button.sh and outliving it; khtpm_pal has button.sh
spawning orchestrator and outliving IT.

**Also notably different**: TPMOS's doc says orchestrator's own Linux main loop "blocks on
`pthread_join(kb_t, NULL)`" — implying orchestrator itself owns keyboard_input as a thread it
spawns and waits on. The real khtpm_pal `orchestrator.c` never touches keyboard_input at all — it
doesn't launch it, doesn't track it, doesn't wait on it. It runs its own independent
`while (!should_exit) { check quit_flag.txt; reap dead children; usleep(200000); }` loop, polling a
`pieces/system/quit_flag.txt` marker file as its OWN, separate signal to shut down. keyboard_input is
button.sh's direct child, running in the foreground of the whole button.sh process, never a child of
orchestrator.

**If the goal is closer parity with TPMOS's own documented shape**, this is the one gap actually
worth discussing: either (a) accept the current inversion as khtpm_pal's own deliberate evolution
(it has real advantages — button.sh keeps the familiar "run and Ctrl+C to quit" terminal UX without
needing orchestrator to manage a pty/foreground thread itself), or (b) flip it back to match TPMOS
exactly (orchestrator becomes the true entry point, spawns button.sh, owns keyboard_input via a
tracked child/thread and `pthread_join`/`waitpid`s on it). This is a real design decision, not
something to change unilaterally — flagging it here since it's the most load-bearing "shape"
difference and the user's stated goal is close parity.

## 2. The game-loop layer underneath — a real, deliberate divergence, not just an implementation detail

**TPMOS's own doc is explicit and emphatic that PAL scripts are wrong for its pattern**:
> "No PAL Script — The old approach of running prisc+x with a PAL script is WRONG. Manager IS the
> game loop - it reads input and calls ops directly. Don't use modules or PAL scripts for this
> architecture."

TPMOS's actual game-loop layer is `game_manager.c` — a **pthread-based, single persistent background
daemon** that polls `pieces/apps/player_app/history.txt` for new keypresses, converts each to an
action, calls the relevant `ops/game_turn_input` binary directly, then calls
`ops/game_compose_frame` to re-render, then pulses `frame_changed.txt`. One long-running process,
no PAL interpreter, no per-screen module spawning.

**khtpm_pal's actual, current game-loop layer does the opposite of that "WRONG" warning** — it
IS built around `prisc+x` running real `.pal` module scripts (`chtpm_parser_pal.c`'s own
`launch_module()`/`run_module_synchronous()`, `orchestrator.c` explicitly passing a `PAL_LAYOUT`
env var and a PAL script path to `chtpm_parser_pal` on launch). Every real project in the house
(my-chara-txt's `main_module.pal`/`farm_module.pal`, piececraft-xyz's `new_game_module.pal`, etc.)
uses this PAL-module shape as the norm, not the TPMOS-doc's flat `game_manager.c` shape. This is
almost certainly WHY the shared engine binary is literally named `chtpm_parser_pal` (not
`chtpm_parser`, which is what TPMOS's own docs call their renderer) — the `_pal` suffix marks it as
a deliberate superset/evolution of TPMOS's original "pure renderer" design, adding real PAL-script
interpretation and per-screen module spawning that TPMOS's own doc explicitly rejected.

**Practical effect of this divergence**: TPMOS's `game_manager.c` is ONE persistent process for the
whole app's lifetime, no relaunching, no per-screen process boundary. khtpm_pal's module system
spawns a NEW `prisc+x` process per PAL module/screen, coordinated by `chtpm_parser_pal.c`'s own
relaunch logic — which is also the source of the real, unresolved "REAL, UNRESOLVED shared-engine
bug" documented in `sim-smell-fix.md` (a module-relaunch crash when `project_root` and
`session_root` diverge for a project with multiple distinct `<module>` declarations). TPMOS's own
flat, single-process `game_manager.c` shape would never hit that class of bug at all, since it never
spawns/relaunches a process per screen in the first place. Worth keeping in mind if khtpm_pal ever
moves toward closer TPMOS parity here — it would sidestep a real, currently-open bug class, at the
cost of losing the PAL-module system's own flexibility (data-driven `.pal` scripts per screen instead
of a monolithic `game_manager.c`).

## 3. What's genuinely the same / close-to-identical (real parity, already achieved)

- **fork/exec/waitpid discipline, never `system()`** ("The Fuzzpet Pattern") — confirmed in the real
  `orchestrator.c`: every child launch is a real `fork()` + `execv()`/`execl()`, `run_final_kill_sweep()`
  is the one place a shell script gets invoked, and even that goes through `fork()`+`execl("/bin/bash", ...)`,
  never a bare `system()` call.
- **Signal handling + graceful shutdown** — `orchestrator.c` registers `SIGINT`/`SIGTERM` handlers,
  calls `_exit()` from the handler after cleanup, matching TPMOS bible's own mandate ("Modules MUST
  register SIGINT/SIGTERM handlers and call `_exit()` to prevent zombies").
- **Kill discipline is actually MORE elaborate than the TPMOS bible describes**, not less: khtpm_pal's
  `orchestrator.c` does a real 3-layer cascading kill (`kill(0, SIGTERM)` on the whole process group,
  then a tracked-PID sweep from its own `pieces/os/proc_list.txt` with SIGTERM then SIGKILL, then a
  final `kill_all.sh` shell sweep as a last resort) — the TPMOS bible only mandates `setpgid(0,0)` +
  a signal handler, a lighter version of the same idea.
- **Marker-file "pulse" discipline** — `frame_changed.txt`-style marker files, "One Writer Rule"
  (only the renderer/parser writes `current_frame.txt`, daemons trigger re-renders via a marker, never
  by writing the frame directly) — matches throughout the current codebase, e.g. board-viewer's own
  `bv_screen_changed.txt`, piececraft-xyz's `pc_screen_changed.txt`.
- **No shared `.h` files, no `#include`-shared source** — every project keeps its own local `.c` copy
  of the shared engine files (`chtpm_parser_pal.c`, `chtpm_rgb_render.c`, `prisc+x.c`), rebuilt from
  `014.wsr-pal💸️📌️+2`'s own canonical copies by hand when the shared engine changes, never symlinked
  or `#include`d across project boundaries. Matches TPMOS's own "Header Convention: No shared .h
  files. Every .c is a self-contained island" mandate exactly (this is also the house-standard
  memory note: "TPMOS is THE architecture standard... real ops = fork/exec binaries, not #include-shared
  source").
- **Bracket-cursor menu navigation, digit-jump, piece.pdl-as-single-source-of-truth for menus** —
  matches the PAL-TPMOS parity doc's own standards #2 and #4 throughout every current project's own
  `.pal`/`.chtpm` layouts.
- **State-file-as-single-source-of-truth ("if it's not in a file, it's a lie")** — matches; every
  project's own `pieces/system/config.txt`/`state.txt` pattern, no in-memory-only state that doesn't
  survive a process restart.

## 4. What's a real, unresolved gap (worth a decision, not a bug report)

- **`project_root` resolution mechanism differs from TPMOS's own mandate.** TPMOS bible: "Path
  Mandate: Resolve `project_root` from `pieces/locations/location_kvp` ONLY. Hardcoded paths are
  lies." khtpm_pal's real `orchestrator.c` instead resolves it from the `PRISC_PROJECT_ROOT`
  environment variable (falling back to `getcwd()`) — a completely different mechanism, no
  `pieces/locations/location_kvp` file involved anywhere in the current codebase. This whole
  session's own symlink-elimination work (see `sim-smell-fix.md`) was entirely about managing the
  fallout of `PRISC_PROJECT_ROOT` vs. the session/CWD split — a TPMOS-style `location_kvp`-file
  resolution mechanism might have sidestepped that whole bug class differently (or introduced its
  own). Not something to silently reconcile — a real design question if closer TPMOS parity is the
  goal.
- **The orchestrator/button.sh inversion and the game_manager/PAL-module divergence above** (sections
  1 and 2) are the two biggest structural gaps. Both are large enough that "fixing" them toward
  TPMOS's canonical shape would be a real architectural change, not a quick patch — flagging them
  clearly rather than guessing which direction the user wants to go.

## Summary table

| Aspect | TPMOS (canonical) | khtpm_pal (current, real) | Same? |
|---|---|---|---|
| Top-level process | `orchestrator.c` | `button.sh` | **No — inverted** |
| Who spawns whom | orchestrator spawns button.sh | button.sh spawns orchestrator | **No — inverted** |
| keyboard_input owner | orchestrator (thread, `pthread_join`) | button.sh (direct foreground child) | **No** |
| Game loop | `game_manager.c`, one persistent pthread daemon, no PAL scripts | `prisc+x` + PAL modules, per-screen process spawn/relaunch | **No — real divergence** |
| Child process discipline | fork/exec/waitpid, never `system()` | fork/exec/waitpid, never `system()` | **Yes** |
| Signal handling | SIGINT/SIGTERM -> cleanup -> `_exit()` | Same, plus a more elaborate 3-layer cascading kill | **Yes (khtpm_pal is a superset)** |
| Marker-file pulse / one-writer rule | Yes | Yes | **Yes** |
| No shared `.h`, no `#include`-shared source | Yes | Yes | **Yes** |
| Menu nav (bracket cursor, digit-jump, piece.pdl-driven) | Yes | Yes | **Yes** |
| `project_root` resolution | `pieces/locations/location_kvp` file | `PRISC_PROJECT_ROOT` env var | **No** |

## ❓ USER QUESTIONS

**1. Is `prisc+x`/PAL-module usage actually wrong, or just wrong relative to TPMOS's own opinion? Is
it causing problems?**

Not wrong in any absolute engineering sense — TPMOS's own doc's "WRONG" is scoped to ITS OWN specific
pattern's requirements ("Manager IS the game loop... Don't use modules or PAL scripts **for this
architecture**"), not a universal judgment against PAL modules as a concept. `prisc+x`/PAL modules
are a real, working, deliberately different design choice that every project in the house currently
runs on successfully, and they buy something TPMOS's flat `game_manager.c` shape can't: data-driven,
per-screen `.pal` scripts that don't need a recompile to add/change a screen, vs. TPMOS's single
monolithic C daemon that owns the whole app's logic in one process.

**It DOES cause one specific, confirmed, real problem** — not a flaw in the PAL-module idea itself,
but a genuine bug in its current implementation: the shared engine's own `chtpm_parser_pal.c`
`launch_module()`/`run_module_synchronous()` relaunch logic can silently fail to spawn a NEW module
process when a `<button href>` navigates to a `.chtpm` layout declaring a DIFFERENT `<module>` than
the one currently running — confirmed live, reproducibly, in my-chara-txt this session (see
`sim-smell-fix.md`'s own "REAL, UNRESOLVED shared-engine bug" section for the full writeup and
root-cause hypothesis). This only manifests for a project with a genuine multi-module architecture
(more than one distinct `<module>` PAL script across different screens) AND only once `project_root`
and `session_root` actually diverge as two different real paths (which is exactly what this whole
session's symlink-elimination effort was migrating every project toward) - a single-module project,
or one still on the old symlink/session==project_root setup, would never hit it.

**So: the PAL-module approach itself is sound and not going anywhere on its own merits.**

## How to fix that bug

**UPDATE 2026-08-21: this bug is ALREADY FIXED — in my-chara-txt's own local copy of
`chtpm_parser_pal.c` specifically — just never ported anywhere else. `sim-smell-fix.md`'s own
"REAL, UNRESOLVED shared-engine bug" section is now stale; corrected there too.** Confirmed by
actually re-reproducing the original crash scenario live, in an isolated scratch copy (not the real
project): copied my-chara-txt to a scratch dir, switched `PRISC_PROJECT_ROOT` to `$SCRIPT_DIR` (the
exact divergence that used to trigger the crash), launched a session, and drove it through
`main -> Farm -> main -> Mine` via real key injection. Every transition rendered the correct new
screen, a genuinely new `prisc+x` process spawned each time (confirmed via `ps`, distinct PIDs), and
`debug.txt` showed clean, sequential `launch_module()` calls each followed by a real
`forked module pid=<N>` line — the exact sequence that used to silently produce no new process at
all. No crash, no stuck screen, on any hop.

**Root cause** (now confirmed, not just hypothesized): the ORIGINAL/canonical `chtpm_parser_pal.c`
(still the shape of `014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c` today) uses a SINGLE
`project_root_path` global for every file path in the whole parser — genuinely session-local,
ephemeral files (`current_frame.txt`, `current_layout.txt`, `active_gui_index.txt`,
`pending_command.txt`, the `frame_changed.txt`/`layout_changed.txt` marker files, keyboard
`history.txt`) get resolved against the exact same root as genuinely persistent/shared files
(`pal/`, `system/`, `pieces/chtpm/`, `projects/<id>/pieces/`). That's harmless as long as
`project_root_path` and the session dir are the same value (true for every project still on the old
symlink setup, or the copy-based strategy) — but once a project switches `PRISC_PROJECT_ROOT` to the
real, non-session root (Strategy A), every one of those session-local files silently starts reading/
writing against the WRONG root. Two concrete, confirmed failure modes from this single root cause:
1. `current_layout.txt` (which a project's own `compose_frame`/`menu_input` ops read to know which
   screen is REALLY active) gets written only to the session dir by the parser, but those ops — once
   `chdir()`'d to `project_root_path` by `launch_module()`'s own fork - had no way to find it, so
   they silently kept rendering whatever screen they last knew about (the specific "screen header
   changes but content stays stuck on the old screen" symptom).
2. Enough of this same class of mismatch elsewhere in the relaunch path (module-liveness bookkeeping
   depending on other session-local marker files getting confused about which root to check) could
   make a real, freshly-forked module process appear to just never happen at all - the "debug.txt
   logs launch_module() but no new process appears" original symptom.

**The actual fix, already written and live-verified in my-chara-txt's own
`system/chtpm_parser_pal.c`** (search that file for `session_root_path` and
`build_session_path_malloc` to see the real, exact diff):
1. Split the single `project_root_path` into TWO globals in `resolve_root()`: `session_root_path`
   (always literal `getcwd()` — the true session dir) and `project_root_path` (from the
   `PRISC_PROJECT_ROOT` env var, falling back to `session_root_path` if unset — matches
   `orchestrator.c`'s own identical split, see section 1 above).
2. Add a second path-builder, `build_session_path_malloc(rel)` (same shape as the existing
   `build_path_malloc(rel)`, just anchored to `session_root_path` instead), and re-point every
   genuinely session-local file's own `build_path_malloc()` call to
   `build_session_path_malloc()` instead — `current_frame.txt`, `current_layout.txt`,
   `active_gui_index.txt`, `active_gui_is_typing.txt`, `pending_command.txt`, `frame_changed.txt`,
   `layout_changed.txt`, `pieces/keyboard/history.txt`. Leave every genuinely persistent/shared
   path (`pal/...`, `system/...`, `pieces/chtpm/...`, `projects/<id>/pieces/...`) on the original
   `build_path_malloc()` — those SHOULD resolve against the real, persistent root.
3. One additional, narrower fix layered on top of the split: `parse_chtm()` now writes
   `current_layout.txt` TWICE - once to the session dir (the authoritative copy, via
   `build_session_path_malloc`) and, ONLY when `project_root_path` and `session_root_path` actually
   differ, an additive second copy to the project root (via `build_path_malloc`) — specifically so
   ops that got `chdir()`'d away from the session dir by `launch_module()` can still find it. This is
   the same "write it to both roots" pattern already used elsewhere in the house for exactly this
   shape of problem (mutaclysm's own `bv_state.txt`, per this file's own header comment).

**UPDATE 2026-08-21: action 1 below is DONE.** Ported the exact fix (the `session_root_path`/
`project_root_path` split in `resolve_root()`, `build_session_path_malloc()`, all 11 session-local
call sites repointed, the `current_layout.txt` dual-write block in `parse_chtm()`) into
`014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c` — the canonical source every new project's own local
copy gets built from. Verified: compiles clean (`gcc -Wall -Wextra`, zero new warnings), the
project's own `scripts/build.sh` completes with no errors, and a live smoke test (launch, real
render) confirms zero regression for the common case (a project that never sets
`PRISC_PROJECT_ROOT` — `session_root_path` and `project_root_path` end up identical, byte-for-byte
the old behavior).

**Also found and fixed while in this file, same pass**: `clear_saved_active_index()` — a real,
documented bug fix (stale `active_gui_index.txt` causing wrong focus after a layout transition,
per its own header comment) — was disabled at all 3 of its real call sites with a
`// TEMP-DISABLED-FOR-TEST` marker and never re-enabled, making the function dead code
(`-Wunused-function`) and leaving the actual bug it fixes live. Re-enabled all 3 call sites.
Rebuilt and smoke-tested clean.

**Action 2 below is NOT done yet** — still real, standalone follow-up work if/when it's needed:
2. **Port it into any OTHER existing project's own local `chtpm_parser_pal.c` copy** that (a) has a
   genuine multi-`<module>`-per-screen architecture AND (b) either already uses, or might ever want
   to use, the `PRISC_PROJECT_ROOT=$SCRIPT_DIR` strategy (mutaclysm-neo is the current example, but
   it's single-module so was never at risk in the first place - the risk is for the NEXT
   multi-module project that tries Strategy A). Mechanical, low-risk process: diff the target file
   against `014.wsr-pal💸️📌️+2/system/chtpm_parser_pal.c`'s own now-fixed copy, find every
   `build_session_path_malloc` call site and the `current_layout.txt` dual-write block, and port
   each one across (they're small, self-contained, additive changes — not a wholesale file
   replacement, since each project's own copy has likely also diverged in other, unrelated ways by
   now).
