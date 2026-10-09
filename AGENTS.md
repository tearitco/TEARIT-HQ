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
- Lane split vs the `opencode-fix` work (2026-10-05): that lane owns
  `ops/` worker/fetch/driver code, `tests/worker_*`, `nb_js_worker.+x`
  sides, and the FETCH protocol, and leads edits to
  `network_browser_manager.c` while mid-flight. The `opencode` lane
  owns row-projection/layout into the window (page-state, xhtpm, css)
  and should avoid touching worker/fetch C files. The handshake file is
  `network_browser_manager.c` - rebase your row-projection batches over
  whatever that lane landed last.
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