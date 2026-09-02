# 🚚 S1_HOUSE_PATH_MIGRATION.md — moving the house off emoji paths

**Status: PLANNING ONLY, 2026-09-01. No files moved yet, no code
changed for this yet.** Direct instruction: draft the plan first,
execute later once agreed.

## 1. Why

Real, concrete problem reported live: tool UI shows **"Path hidden
(unsupported character)"** for files under this house's own current
location whenever an edit touches them — genuinely inconvenient, not
cosmetic. The real cause is almost certainly the emoji sequences
themselves: several path segments use multi-codepoint emoji (ZWJ
joiners, variation selectors — e.g. `❤️‍🔥️`, `🤖️🪤️🏠️`) that some tools'
path-handling/display code doesn't round-trip cleanly. Two separate
real problems stack on top of each other today:

1. **The house's own real root folder name** —
   `44.xyz❤️‍🔥️00.17` — is itself emoji-laden.
2. **Every ancestor directory above the git repo root is ALSO emoji-
   laden** — `🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/` — so even if the house's own
   internal folder names were clean, the full path would still trip
   the same class of tool bug.

Direct instruction: fix both, in two separate phases, moving the
whole tree to a clean-ancestor location FIRST (since that's the
precondition for the second phase actually solving anything), and
renaming the house's own internal emoji folder(s) SECOND, as a
follow-up once the move itself is proven working.

## 2. Scope decision

- **New location**: `/home/no/Desktop/github/work/NNEST-12.00/`
  — a clean, ASCII-only parent path.
- **Version bump**: `NNEST-11.17` → **`NNEST-12.00`**, direct
  instruction ("since this is a big jump") — this migration itself is
  the reason for the jump, not a coincidence with unrelated version
  history.
- **What moves, unchanged, in this first pass**: everything from
  `x0.parent-level-dev-env-04.04/` down, **including** the still-
  emoji `44.xyz❤️‍🔥️00.17` folder itself — renaming that is explicitly
  Phase 2, not part of this move. The git repo root
  (`NNEST-11.17/.git/` and everything alongside it) moves as one
  unit — the whole directory, not just the `x0.../` subtree, since
  that's where `.git/` actually lives.
- **What does NOT move**: `#.NNEST_ASSETS/` and any other real sibling
  of `NNEST-11.17` under `NNEST_CLEAN_PARENT/` are out of scope for
  this pass unless something inside the moved tree turns out to
  depend on their current absolute location (see §4.3).

## 3. Real, pre-flight safety steps (before touching anything)

1. **Full stop of every live process** this house currently runs —
   not just the taskbar. Real sequence, in order:
   - `sh run_khtpm_strip.sh stop` (kills the strip mode + manager via
     its own real, tested kill sweep).
   - Explicit `pkill` sweep for anything the stop script's own pattern
     might miss — **direct, concrete reason to be paranoid about this
     exact step**: this same session found two ancient, orphaned
     copies of the already-retired `khtpm_strip_parser.+x` still
     running, invisible to the current kill sweep because it only
     matches the CURRENT binary name (see `HOUSE_FAQ.md`'s own
     "PROCESS / REGISTRY HYGIENE" entry, 2026-09-01). Confirm zero
     `khtpm_core_render`/`khtpm_taskbar_manager_main`/any entity
     process remains alive (`ps aux | grep -E 'khtpm_core_render|khtpm_taskbar_manager|tp_desktop'`)
     before copying — an actively-written state/PID file mid-copy is
     a real, avoidable source of a confusing false failure after the
     move.
2. **A full, timestamped backup archive of the CURRENT location**,
   made BEFORE the copy, same discipline as every other risky pass
   this session (`zip`/`7z` of `x0.parent-level-dev-env-04.04/` at
   minimum, ideally the whole `NNEST-11.17/` including `.git/`).
3. **Note the exact list of currently-open entities/windows** (or just
   accept a fresh respawn from the taskbar's own registry after the
   move — either is fine, but decide before, not during).

## 4. Real risk inventory — what could actually break on a path move

### 4.1 Hardcoded absolute paths in C source / shell scripts
The house's own already-stated standard (confirmed real, e.g.
`resolve_livedesk_paths()`/`self_exe_path()` via `/proc/self/exe`,
and `RMMV-ASSET-SOURCE-LOCATION.pdl`'s own pointer-key indirection
per `HOUSE_FAQ.md`'s "ASSETS" section) is that **nothing should ever
hardcode an absolute path** — house_root is always resolved
dynamically at runtime, either by climbing from the binary's own real
`argv[0]`/`/proc/self/exe` location or by being passed explicitly as
`argv[1]`. This migration is the real, concrete test of whether that
standard was actually followed everywhere, not just where it's been
checked before.

**Real, required audit before/during Phase 1's live-test**: grep the
ENTIRE moved copy for the literal old absolute path (or distinctive
fragments of it — the emoji sequences themselves are good, unusual
grep targets, e.g. `🤖️🪤️🏠️`, `NNEST-11.17`, `NNEST_CLEAN_PARENT`) across
every real file type: `.c`, `.h`, `.sh`, `.pdl`, `.chtpm`, `.css`,
`.py`, `.md` (docs referencing the old path are lower priority but
still worth a pass), and any compiled `+x/` binary (`strings +x/*.+x
| grep ...` — a real, cheap way to catch a path baked in at compile
time that source-grepping alone might miss, e.g. via a stale copied
build artifact).

**Where a hit is expected to be fine (already dynamic) vs. a real bug
to fix**:
- Fine: any `argv[1]`-derived `house_root`, any `/proc/self/exe`-based
  self-location, any `.pdl` pointer-key like `img_root` that itself
  just HOLDS a real absolute path as DATA (that's what it's for —
  the fix there, if the path moved, is updating the one PDL value,
  not a code change).
- Real bug: any C string literal, `#define`, or shell script literal
  containing the OLD absolute path baked in directly — this is
  exactly the class of thing "should be std for house" to avoid, per
  direct instruction, and the fix is converting that one spot to the
  same dynamic-discovery pattern every other real binary already uses
  — not inventing a new mechanism, matching whatever's closest already
  proven working nearby.

### 4.2 Git worktrees — a real, confirmed finding, not hypothetical
`git worktree list` (checked while drafting this doc) shows a real,
linked worktree:
```
/home/no/Desktop/.../NNEST-11.17                                    [main]
/home/no/Desktop/.../NNEST-11.17/.claude/worktrees/agent-a2f9c99248dc908bd  [worktree-agent-a2f9c99248dc908bd]
```
A linked worktree stores real, absolute back-references in both
directions (`<worktree>/.git` file → main repo's real `.git/`
directory; main repo's `.git/worktrees/<name>/gitdir` → the worktree's
own real location) — **a plain directory copy/move breaks this
silently** (git will report the worktree as missing/broken, not fail
loudly at copy time). Real fix, either:
- Run `git worktree repair` from the new location after the move
  (git's own real, built-in fix for exactly this), or
- Simpler, since this worktree is a disposable agent-run artifact:
  `git worktree remove` it before the move (or just don't copy
  `.claude/worktrees/` at all) and let it get recreated fresh if
  still needed after.

### 4.3 Anything outside the moved tree the house depends on
- `#.NNEST_ASSETS/` (RMMV image assets, per `HOUSE_FAQ.md`'s own
  "ASSETS" section) — resolved via `img_root` in
  `RMMV-ASSET-SOURCE-LOCATION.pdl`, NOT a hardcoded path, per that
  same doc's own account of why it was built that way. Real action
  needed: update that one PDL value to point at wherever
  `#.NNEST_ASSETS` ends up (staying in place under the OLD
  `NNEST_CLEAN_PARENT/`, or moving alongside — decide at execution
  time, not baked into this plan yet).
- `014.wsr-pal💸️📌️+2/` (the wsr-pal toolchain several build scripts
  reference via `$(cd "$(dirname "$0")/../../../014.wsr-pal💸️📌️+2" ...)`-
  style RELATIVE resolution, confirmed real earlier this session while
  reading `build_khtpm_strip.sh`) — relative-path resolution means
  this should survive the move automatically as long as its own
  relative position to the moved tree doesn't change. Worth one real
  grep pass to confirm no OTHER consumer resolves it by absolute path
  instead.
- Any cron/systemd/autostart entry, desktop shortcut, or shell
  profile alias pointing at the house's current absolute path (a real
  class of thing that wouldn't show up in a source-code grep at all).
  Real action: check `crontab -l`, `~/.config/autostart/`, and any
  shell rc file for a literal reference before considering the
  migration complete.
- This Claude Code session's own scratchpad path is itself derived
  from the current working directory — irrelevant to the house's own
  runtime behavior, but a fresh session opened from the new location
  will get a genuinely new scratchpad path; not a bug, just an
  expected, harmless side effect worth knowing about rather than
  being confused by later.

## 5. Real execution plan (once this doc is approved)

1. Pre-flight safety (§3): stop everything, confirm zero stray
   processes, take the backup.
2. `mkdir -p /home/no/Desktop/github/work/` (does not exist yet as of
   this doc).
3. Copy (not move, until Phase 1 is proven) the whole `NNEST-11.17/`
   tree to `/home/no/Desktop/github/work/NNEST-12.00/` — real,
   attribute-preserving copy (`cp -a` or `rsync -a`), so executable
   bits/symlinks survive intact. Exclude `.claude/worktrees/` per
   §4.2's own recommendation, or repair it post-copy if kept.
4. Run the §4.1 grep/`strings` audit against the NEW copy specifically
   (catches anything that silently still resolves to the OLD location
   without erroring, which a "does it launch" test alone could miss).
5. From the NEW location: rebuild everything real
   (`run_khtpm_strip.sh new` at minimum; a full top-to-bottom rebuild
   pass of every `build*.sh` this house has is the honest, thorough
   version of this step) and attempt a real launch.
6. Fix any real hardcoded-path hit found live, using whichever
   dynamic-resolution pattern is already proven working nearest to
   that spot (§4.1) — not a new pattern per fix.
7. Live-test thoroughly before declaring the new location "the real
   one": taskbar renders, a representative sample of entities
   (cursword, a pet, book-stack, the network browser) all launch and
   work, a real screenshot/relay-injection pass same as every other
   real verification this session has used.
8. Only once §7 passes cleanly: decide whether/when to retire the OLD
   location (keep as a real rollback copy for some real window of
   time, not deleted same-day) and update this repo's own real,
   working-copy path in whatever external references need it (this
   session's own future invocations, any other tool/session pointed
   at the old absolute path).

## 6. Explicitly deferred to a later phase (not this doc's scope)

- **Renaming `44.xyz❤️‍🔥️00.17` itself** (and any other emoji-laden
  folder discovered along the way) to a clean, ASCII name — direct
  instruction: do this AFTER the move is proven working, as its own
  separate, later pass, not bundled into this one.
- **Auditing/naming-convention guidance for future major project
  folders** to avoid repeating this — direct instruction: "we can
  then look at major projects that have emojis in the name after
  this to avoid" — a real, house-wide naming-standards question worth
  its own short doc once Phase 1/2 are both done and the real, full
  list of affected names is known, not guessed at now.
