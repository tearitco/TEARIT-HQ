# Git, in plain language — how work is saved here, and why nothing is lost

Written 2026-09-06 for the house owner ("im new 2 git"), after several
scary-looking branch mix-ups between the `claude` and `opencode`
agents. **Short version: no code was lost, and here's why + how to make
the mix-ups stop.**

---

## 1. What git actually is (30 seconds)

Git is a stack of **snapshots**. Every time an agent runs `git commit`,
git writes a permanent, ID'd snapshot of the changed files
(`584cbc0b`, `97f1fe2f`, …). Those snapshots **never get deleted** by
normal operations — even if no "branch" name points at one anymore,
git keeps it for ~90 days (`git reflog` lists them, and you can always
get one back by its ID). So "losing work" in git basically only
happens if you *never commit it* and then overwrite the file. Every
agent here commits constantly, so the snapshots are safe.

- A **branch** (`main`, `claude`, `opencode`) is just a *sticky note*
  pointing at one snapshot. Moving a branch doesn't destroy anything;
  it just moves the note.
- **`origin`** = the copy on GitHub. `git push` uploads your snapshots
  there; that's your off-machine backup.
- **`HEAD`** = "which branch am I looking at right now" — one per
  checked-out folder.

## 2. What went wrong (the "competition" you asked about)

**It is not GitHub, and it is not a bug.** It's that **both agents run
in the *same folder*** — `/home/no/Desktop/github/work/NNEST-12.00/`.
That folder has exactly one `HEAD`. So:

1. Claude is on branch `claude`, editing files.
2. Opencode, in the same folder, runs `git checkout opencode` to do its
   own work.
3. That **swaps the files on disk** and moves `HEAD` — *for both
   agents*. Claude's next `git commit` now lands on `opencode` by
   accident, and Claude's working files just changed under it.
4. Then a "sync" step (`merge --ff-only`) drags a third branch along
   too.

Every time this happened it *looked* like work vanished, but it was
always still there under a commit ID — recovered with
`git reflog` + `git merge --ff-only <that-id>`. **Net loss so far:
zero. Net wasted time: a couple of hours.**

## 3. The fix: one folder per agent (`git worktree`)

`git worktree` lets one repo have **several folders, each on its own
branch, each with its own `HEAD`**. They share the same history and the
same GitHub remote — they just can't step on each other's checkout.

Run this **once, when every agent is idle** (no session mid-edit):

```sh
cd /home/no/Desktop/github/work/NNEST-12.00

# give each agent its own folder, locked to its own branch:
git worktree add ../NNEST-12.00-claude   claude
git worktree add ../NNEST-12.00-opencode opencode

# (add more later the same way, e.g. ../NNEST-12.00-grok grok)
```

Then:
- **Claude sessions run in** `/home/no/Desktop/github/work/NNEST-12.00-claude/`
- **Opencode sessions run in** `/home/no/Desktop/github/work/NNEST-12.00-opencode/`
- The **original** `NNEST-12.00/` folder stays on `main` as a
  read-only reference (don't edit code there).

You (or whoever launches the agents) point each agent at its own
folder. After this, if an agent tries `git checkout opencode` from the
`claude` folder, **git refuses it** ("already checked out at …") — the
collision is now impossible, not just "please don't".

`git worktree list` shows all folders and which branch each is on.
`git worktree remove ../NNEST-12.00-grok` cleans one up.

## 4. Until worktrees are set up — the discipline rule

Every agent, every session:

1. **Know your branch.** `git branch --show-current` should say your
   own name (`claude` / `opencode` / …). If it doesn't, `git checkout
   <yourname>` and tell the user something's off.
2. **Commit only to your own branch.** Scope to the files you changed
   (`git add <path>` per file — never `git add -A`).
3. **Push only your own branch:** `git push origin <yourname>`.
4. **Never** `checkout` / `merge` / `cherry-pick` / `reset` / move
   another agent's branch or `main`. Those are the user's, done at
   session boundaries.
5. Leave notes for other agents in `13.agent-coms/` (`SONNET.md` for
   Claude, `OPENCODE.md` for opencode). For a real back-and-forth, use
   the `co-lab-hai` HQ app.

## 5. How YOU (the owner) publish work to `main`

`main` is the frozen "reviewed" branch. When you want an agent's branch
folded into it:

```sh
cd /home/no/Desktop/github/work/NNEST-12.00        # the main folder
git checkout main
git merge --ff-only claude    # or: git merge --no-ff claude  (keeps a merge marker)
git push origin main
```

`--ff-only` succeeds only if `main` hasn't diverged (the clean case).
If it refuses, the branch and `main` both have new commits — ask an
agent to sort it, or `git merge claude` (makes a merge commit) and
resolve any conflicts.

To see what a branch would bring in before merging:
`git log --oneline main..claude`

## 6. "Recover a commit that seems gone" cheat sheet

```sh
git reflog                     # every HEAD position, newest first, with IDs
git log --oneline <id>         # confirm it's the right snapshot
git branch --contains <id>     # is any branch already on it?
git checkout claude
git merge --ff-only <id>       # if claude is an ancestor of <id>, this restores it
# or, to point claude AT it exactly (only if you're sure):
git reset --hard <id>
```

Nothing here deletes data; the worst case is another `reflog` entry to
walk back.
