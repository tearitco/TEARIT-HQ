# AGENTS.md

House rules for any coding agent working in this repo. Read before
starting work.

## Commit discipline (REQUIRED, unprompted)

- Never end a work block with uncommitted code. Uncommitted work is
  fire-able — the nb-js-worker step-6/7 set was lost exactly this way
  (2026-09-05).
- At the end of every session, commit scoped to ONLY the files you
  changed (git add `<path>` per file, never `git add -A`). Runtime
  state files (`module_parent.pid`, `*.pdl`, logs, `cli_io_state.txt`)
  are noise — never sweep them into a code commit.
- Mid-work snapshots may use `wip: <what>` as the message.
- Each tool commits to its OWN named branch (`opencode` for this agent,
  `claude` for Sonnet, `grok`/`kilo`/`hai` for theirs). Never commit
  to another tool's branch, `main`, or the old all-agents branch
  `chtpm-delete-per-app-c`. See HQ-IQ-BOOK `01-orientation/
  BRANCH-STRATEGY.md`.
- Never merge, cherry-pick across branches, push, or force-delete
  unprompted — leave that to the user.
- Match the repo's commit-message style (full thoughts in "Explain tracked code" tone:
  `fix:`, `docs:`, `scoped feature: ...`).

## Concurrent agents in one checkout (REQUIRED)

Two agents can now be live in this same worktree. That breaks the
branch rule above: they share one `.git/index`, so whoever runs
`git commit` first sweeps the *other* agent's staged work into its
commit. Observed 2026-09-26: a staged 254-file rename was committed
out from under its author with a message asserting the opposite.
Worse, `git reset` is shared too — a peer agent's `reset: moving to
HEAD` can destroy uncommitted work outright.

Rules while sharing a checkout:

- Stage ONLY your own paths, and commit ONLY your own paths. If
  something you did not author is already in the index when you go
  to commit, unstage it (`git restore --staged <path>`) rather than
  sweeping it in.
- Never `git reset` with anything staged that you did not stage.
  If you must reset, use `git reset -- <path>` on your own files.
- Never `git stash` — it is shared and the other agent loses its
  work.
- Before committing, run `git diff --cached --name-only` and read
  the list. It must be exactly your files.
- Safer primitive that skips the check-then-add race: commit with an
  explicit pathspec (`git commit -m ... -- <your paths>`) so other
  agents' staged files can never ride along even if the index changed
  under you. Two sweeps happened before this rule (2026-10-07).
- If a commit lands that you did not author, do not amend or rewrite
  it. Add a follow-up commit that states the correction, and tell
  the user.
- Heaviest-collision file at the moment: `board-viewer/ops/bv_menu_input.c`
  is the placer/camera-fix merge point; only touch it if you are doing
  placer or camera work.
- LANE SPLIT: **RETIRED 2026-10-13.** The `opencode-fix` lane no longer
  exists, so the split that named it is gone with it. It used to read:
  that lane owned `ops/` worker/fetch/driver code, `tests/worker_*`, the
  `nb_js_worker.+x` sides and the FETCH protocol, and led edits to
  `network_browser_manager.c`, while the `opencode` lane owned
  row-projection into the window (page-state, xhtpm, css) and avoided
  worker/fetch C files. None of that division stands now.
  **The `opencode` lane owns the whole network browser surface** -
  renderer, projection, manager AND `ops/nb_js_worker.c` - including the
  worker tick loop and the push protocol, which are P0/G1 in
  `08-roadmap/design-docs/2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md`
  section 1a. Every other rule here stands unchanged (own branch, explicit
  pathspec, never sweep another agent's staged files, never reset or
  stash); what is gone is only the reason to hesitate before editing
  worker code. `network_browser_manager.c` remains the handshake file
  between manager and worker.
- Driving the network browser: relay file
  `#.desktop/entity_menu_history/<pid>.txt` + reference at
  `08-roadmap/NB-DEBUG-QUICKREF.md`.
- Do not kill processes carrying the shared renderer for unrelated
  windows via the same guardian pattern; every `khtpm_core_render.+x`
  is checked by argv + chtpm path — see `button.sh`.

The real fix is one worktree per agent (`.kilo/worktrees/<name>/`),
each on its own branch, so no index is shared at all. Until that
exists, the rules above are the whole defence.

## After a reset, branch switch or bulk script (REQUIRED)

Run `git ls-files --deleted | grep -v /pieces/sessions/ | wc -l`. More than a few
dozen means the working tree was wiped (2026-10-05: 31,571 files). **Stop**: do not
`git add -A`, stash, clean or reset. Follow
`#.#.calendar-dox/!.HQ-IQ-BOOK/03-pitfalls/INCIDENT-2026-10-05-WORKING-TREE-WIPE.md`.
Compiled programs are in no branch: after a wipe, clone or branch switch run
`sh '$.crypts/button.sh' build`. `sh rezip-house.sh` makes a full local copy
(compiled programs included) and refuses to run on a wiped tree.

## User desk data is NOT in code branches (REQUIRED, since 2026-10-06)

`xyzfs/users/` (every user's entities, histories, phones, wallets) is no longer tracked by `claude`; it lives on local orphan data branches
`user/jb` (the owner), `user/<uuid8>`, `user/guest-<8>` (design: `08-roadmap/design-docs/USER-DATA-BRANCHES-DESIGN.md`, quest Q007). Other branches
(`opencode`, `kilo`, `grok`, `main`, ...) may still track those files until they are converted. The files are live user data, so:

- **Never `git checkout`, `git switch`, `git reset --hard`, `git clean` or fast-forward a live checkout from a branch that tracks `xyzfs/users` onto one that
  does not.** git deletes the tracked files from disk (reproduced on a test tree: all 1,725 files; the same mechanism deleted the owner's API key files).
  Merge in a scratch worktree (`git worktree add /tmp/x <branch>`), resolve and build there, and move the live tree only with a verified backup.
- **A merge that brings `xyzfs/users` files into `claude`: keep them deleted** (`git rm --cached -r xyzfs/users`, resolve modify/delete as deleted). Do not re-add them.
- **Never `git add` anything under `xyzfs/users`** (it is gitignored; `-f` would put private data and wallets into code history).
- **Never push `user/*` branches.** They hold `wallet.txt` and private chat history; they are local only.
- Data is committed by `sh '$.crypts/button.sh' save-data` (also runs automatically after `quit`/`reset`); plumbing only, it never touches your working tree.
- Before any bulk operation that could touch `xyzfs/users`, take a tarball + `sha256sum` list outside the repo and verify the file count.

## Our programs run alongside normal user activity (REQUIRED, since 2026-10-27)

The owner keeps a browser and other work open while the house runs, on a weak CPU. A feature that needs the whole machine is not
done. Launch heavy things niced (`nice -n 15`) and one at a time, keep idle cost near zero, say what a feature costs (idle and
active, measured with a browser running) in its commit, and treat a `cpu_guard` kill (`sh '&.hq-apps/proc-mon/guard.sh' status`)
as a finding in our own footprint, never as "the browser". Details: HQ-IQ-BOOK `03-pitfalls/hq-cpu-safety.md` section 9.

## Heavy work asks first (REQUIRED, since 2026-11-03)

The CPU guard reacts after the machine is hot (2026-11-03 00:54: it ended 31 house processes while a compile, the 3D engine and a screenshot loop ran next to a browser).
Start every compile, engine start/relaunch, nudge/screenshot loop and TTS/ffmpeg batch as `cpu_gate <house> run <label> -- <cmd>` (`&.widgits/_shared-lib/ops/+x/cpu_gate.+x`):
it checks load, temperature and the house share, allows ONE heavy job at a time, and runs it niced. Never two relaunches back to back; never compile while an engine runs; stop the
engine daemon when you are not testing. New code is checked by `cpu_lint` (`CPU-LINT-BASELINE-2026-11-03.txt`: a file may only get fewer hits). A guard kill is a finding in our own
footprint, never "the browser". Details: HQ-IQ-BOOK `03-pitfalls/hq-cpu-safety.md` section 10.

## CPU work: read the pitfalls first (REQUIRED, since 2026-11-03)

Before any CPU optimization, load test or "the cpu is high" investigation read HQ-IQ-BOOK `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` (22 pitfalls, each from a real failure that night: wrong sampler, wrong sensor, an unread producer, an orphaned clock, test loops that got the house killed, a `%` tab frozen by a dead writer). A number shown to the user needs a live writer; a producer is optimized only after finding who reads it.

## Never report a number you did not observe (REQUIRED)

Three wrong conclusions in one session traced to this, all from my own
instrumentation rather than the code under test:

- **`grep -c` on a file that does not exist prints nothing**, and an empty
  count reads as "0". A lazily-written artifact read as a clean result
  produced an entire invented theory. **Confirm the artifact exists before
  trusting its count.**
- **A surprising constant may be historical.** Counters that were never reset
  made a page that never overflowed report drops it inherited from an earlier
  one. Check whether a number is per-frame or cumulative before building on it.
- **Verify a passing assertion can actually fail.** Negative-test it once.

Corollary: read artifacts by identity (our own pid), never "newest on disk" —
with two clones running that is another window's file.

## Runtime files must have an enforced cap (REQUIRED, since 2026-11-02)

Anything the house writes while it runs (frames, logs, ledgers, relays, snapshots) needs a cap enforced on disk, an independent second guard, and a row in
HQ-IQ-BOOK `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`. A trim that skips an oversize input is a bug: 1.68M frame files / 7.6 GB piled up for three weeks
because the renderer's trim returned early on a big `index.txt`, in a gitignored folder nobody looked at. A commit that adds a writer states its files or
bytes per hour. `cpu_guard`'s `hygiene_sweep()` is the second guard; add new folders to it.

## Every process the house starts is registered in a file (REQUIRED, since 2026-11-02)

"If it's not a file, it's a lie" applies to processes. A process the house starts must be recorded before it does any work, by the shell funnel (`&.widgits/_shared-lib/kh_spawn.sh`) or by running
the op `&.widgits/_shared-lib/ops/+x/proc_reg_op.+x <house> register ...` (plain fork/exec, no header; registry `#.desktop/proc_registry.pdl`), and a long-lived one must exit when
its owner is gone (default lifetime `owner`; `keep` only by an explicit `keep_running` setting). Do not write a new `setsid nohup X &` or `fork`+`setsid()` outside the funnel;
the harness case `spawn_lint` fails on it. The 150 existing unregistered sites are listed in
HQ-IQ-BOOK `08-roadmap/design-docs/PROC-REGISTRY-BASELINE-2026-11-02.txt` and are being converted (plan: `PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md`); the
list may only shrink. The CPU guard ends house processes when no LyfeDesk window exists for 120 s (`hq-cpu-safety.md` 8b).

## Product name: LyfeDesk (2026-10-15)

The product is **LyfeDesk** (written "LyfeDesk"), formerly "livedesk", renamed to avoid a future trademark conflict. Anything a user
sees (window titles, labels, banners, launchers, icon, docs prose) says LyfeDesk. Internal identifiers still say `livedesk` on purpose
until the deep rename (directories, file names, `livedesk:` command prefix, constants, WM_CLASS, user-data paths): do not rename those
piecemeal, and do not write new user-facing text with the old name. The plan and checklist are in `04-bugs/bug_bounty.md`.

## Reports to the owner (REQUIRED, since 2026-11-04)

A report for the owner is personal and emoji-friendly, states goals/calls/decisions, and has an "external references" section (repo paths + outside URLs) so other AI agents can
pick up context. Save it as `.md` and `.html` in `/home/no/Desktop/github/work/XO/<month>/<nov-N>/` as `0.<name>` (docs only; see the XO rule below). Example:
`XO/31.Nov/nov-4/0.personal-report-2026-11-04.md`.

## XO is the owner's folder, not for code (REQUIRED, since 2026-10-25)

`/home/no/Desktop/github/work/XO/` is where the owner exfiltrates learnings (docs, mp3, mp4, PNG presentations) to other AI tools. Every owner item there is
named `<pre>.<name>` (`0.LLMUD_BIZ`, `$.Brainstorm`, `29.marathon-sprint-drive`). Do not put code, scripts, binaries, game data (`.pdl`), logs, scratch trees or
"safekeeping" copies there, and do not create unprefixed items in it. Scratch work goes in `/tmp` or the session scratchpad; a frozen reference binary is rebuilt
from its git tag. 2026-10-25: 333 agent items (15,685 files) were moved out to `XO-quarantine/` (next to `XO/`, with `MANIFEST.tsv`; nothing deleted).

## Source of truth

Full house conventions live in the HQ-IQ-BOOK:
`x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/`
- 03-pitfalls/OPERATIONAL-LANDMINES.md — operational rules (#10 is the
  commit rule).
- 02-architecture/CENTROID_GOLD_STD.md — renderer/manager standards.

## Verification

Never report "fixed/done" without a fresh build + fresh run + real
evidence (diff, screenshot, state file). A clean compile is not
evidence.