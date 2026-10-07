# Per-user data branches (`jb` and every desk user) — design

Written 2026-10-06 from an owner conversation. STATUS 2026-10-06 (later): steps 1-5 of §4 EXECUTED on `claude` (24 verified data branches, untrack commit `09cd41ae4`; details in Q007's log); §4 steps 6-8 and the other branches are still open. Every "today" claim was read from git or
the filesystem on that date.

## 1. What the owner wants

1. The owner ("jb") owns all the agent branches, so there should be an **internal "user branch" for jb**, an integration line only the owner (or the
   manager on request) merges into. "We value git integration internally."
2. Per-desk runtime state (entity histories, indexes, signals, theme) should **move into the user session**: "each jb user on desk has its own branch
   data for its desk."

## 2. The problem, measured (2026-10-06)

- Per-user data is already physically separate: `xyzfs/users/<uuid>/home/livedesk/{pals,sessions}`.
- But it is **tracked in the same git history as the code**: 1,710 tracked files under `xyzfs/users` (1,067 in `pals`, 157 in `sessions`, plus
  `wallet.txt` and avatar files). About 91 of them were modified on a normal day (`history.txt`, `livedesk_index.txt`, `last_signal.txt`) because running
  entities write them.
- Consequence seen today: fast-forwarding `claude` to the opencode merge was blocked by 52 locally modified/untracked runtime files; it took a backup,
  a reset of those files, the merge, and a restore. Every code merge between branches that also committed runtime state will fight the same files.
- Other per-desk runtime files outside `xyzfs` are also tracked and modified in normal use (`#.desktop/livedesk_theme.pdl`, `#.desktop/pchq_board_view.txt`,
  `&.hq-apps/db-hq-pal/debug/frames/session_frame_history.txt`).
- `xyzfs/users/*/home/wallet.txt` files are tracked: sensitive data in history (same class of problem as the API keys; old history is not fixed by this design).

## 3. Design

### 3a. `jb`: the owner's integration branch (code)
- Agent branches (`claude`, `opencode`, `grok`, `kilo`, `hai`) keep committing only to themselves (AGENTS.md, unchanged).
- `jb` receives merges only from the owner or the manager on the owner's instruction, through the **throwaway-worktree routine** (merge in a scratch
  worktree, resolve, build, run the checks, commit the merge there, then fast-forward). The conflict list and the proof go in the merge commit message.
- `main` is fed only from `jb`: "owner has seen and accepted this".
- The owner's running desktop is checked out on `jb`.

### 3b. Per-user data branches (data)
- Each user's data folder `xyzfs/users/<uuid>/` becomes its **own git checkout** (a linked worktree of the same repository) on an **orphan branch**
  `user/<name>` (for the owner: `user/jb`). An orphan branch shares no history with the code branches, so code merges can never conflict with it.
- Code branches **stop tracking `xyzfs/users/`** (`git rm -r --cached`, plus `.gitignore` for the folder). Files stay on disk, untouched.
- The data branch is committed by a small op on a schedule or on desktop quit (append-friendly: `history.txt` and the ledgers are append-only, so diffs stay small),
  never by agents sweeping `git add -A` (AGENTS.md: runtime state is noise in code commits).
- Benefits: history, diff and rollback for one desk ("what did this entity's history look like yesterday"), clean code merges, per-user privacy
  (a data branch can be pushed to a private remote or not at all).
- Guests (`guest-*`): no data branch by default (throwaway); optional.

### 3c. Seed data
Code and tests may expect starter entities that are currently tracked under `xyzfs`. Before untracking, find every such dependency and move the
starters to a tracked **template user** (`xyzfs/_seed/`, copied into a new user's folder at creation). Untracking without this breaks fresh clones.

### 3d. Other per-desk runtime files outside `xyzfs`
`#.desktop/livedesk_theme.pdl`, `pchq_board_view.txt`, `session_frame_history.txt`: decide per file: move under the user's data folder, or stay a code-level
default with the live copy in the user folder. Listed in the quest, not decided here.

## 4. Order of work (do not reorder)
1. Finish the in-flight merges first (done 2026-10-06: `claude` at the opencode merge `354abbb86`).
2. **Backup**: full tarball of `xyzfs/users` plus a checksum list, outside the repository.
3. **Dry run**: a script lists exactly which files would leave code tracking, which would be needed by seed/tests, and which are sensitive. Reviewed by the owner.
4. Create `user/jb` as an orphan branch + worktree at `xyzfs/users/<jb-uuid>`; commit the current data there as the first snapshot (verify: file count and checksums equal the backup).
5. `git rm -r --cached xyzfs/users` on a code branch (`claude`) + `.gitignore`; confirm `git status` is clean of runtime noise and the desktop still runs.
6. Template user (`_seed`) for fresh clones; test: fresh clone + `button.sh build` + first run creates the user folder.
7. Create `jb` (code) and move the owner's checkout onto it.
8. A data-commit op (on quit/schedule) for `user/<name>`.

## 5. Risks
- **Untracking removes files from the next checkout of other branches** (a branch that still tracks them will delete them on checkout, like the key-file incident of 2026-10-06).
  Mitigation: do it on all active branches in one session, with the backup and `git checkout` tests in a scratch worktree first.
- Another agent working in the same checkout during the move. Mitigation: announce, do it with no agent live, per AGENTS.md shared-index rules.
- Fresh-clone experience (seed data) and Windows (`opencode-win32` consumes the same paths): check before step 5.

## 6. Open questions for the owner
1. Is `jb` the name for both the code integration branch and the data branch prefix (`user/jb`), or different names?
2. Should data branches ever be pushed (private remote), or stay local only?
3. Which per-desk files outside `xyzfs` (§3d) move into the user folder?
4. Is the existing `wallet.txt` / `guest-*` data worth a separate cleanup of git history?
