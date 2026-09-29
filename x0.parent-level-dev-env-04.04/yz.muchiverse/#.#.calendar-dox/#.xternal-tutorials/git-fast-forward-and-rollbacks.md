# Git: fast-forward merges & rolling back `main`

*Written 2026-09-10, in answer to "does doing ff allow main rollbacks?"*
*Context: our workflow commits only to `claude`, then FF-merges `main`
to it and pushes. This note explains what that FF does and does not do.*

## TL;DR

A fast-forward does **not** change what rollbacks are possible. It's
just a pointer move. Rollback options are the same before and after.
What actually keeps `main` easy to undo is that our history stays
**linear** (FF-only, no merge commits).

## What a fast-forward actually is

A branch in git is just a **movable label pointing at one commit**.

Before our merge:

```
99acee4c   <- main
        \
         A - B - C - D - E - F(f77278a5)   <- claude
```

`claude` is `main` plus 6 commits **in a straight line** — nothing
exists on `main` that isn't already on `claude`.

"Fast-forward" = since the line is unbroken, git just **slides the
`main` label forward** to `f77278a5`:

```
         A - B - C - D - E - F(f77278a5)   <- main, claude
```

- No new commit.
- No merge commit.
- Nothing rewritten.

(A **non**-fast-forward merge happens when the two branches have
diverged — then git makes a brand-new "merge commit" with two parents
to tie the lines back together. We avoid that on purpose.)

## Rolling back

Because a branch is just a pointer, you can move it anywhere, anytime.

### Option 1 — move the label back (rewrites history)

```
git checkout main
git reset --hard 99acee4c      # main points at the old commit again
```

- The 6 commits are **not deleted** — still reachable via
  `git reflog`, and `claude` still points at them. Nothing lost locally.
- To publish this you must `git push --force` (or the safer
  `--force-with-lease`), because the remote sees you rewinding a shared
  branch. **This is the dangerous part** — anyone who already pulled
  `main` now has history that disagrees with the remote.
- Fine on a private branch. Avoid on a shared `main`.

### Option 2 — revert (does NOT rewrite history) ✅ preferred for shared `main`

```
git revert f77278a5            # makes a NEW commit that undoes F
git revert E..F               # undo a range (new commits, newest-first)
```

- `main` keeps moving **forward**; the undo is itself a normal commit.
- No force-push. Every clone stays consistent.
- This is the standard way to undo something already pushed to `main`.

### Option 3 — just work forward

Since `claude` is where we commit, a "rollback" is often simplest as a
new fix commit on `claude`, then FF `main` again.

## Why linear history matters here

Both Option 1 ("reset to the commit before X") and Option 2
(`git revert X`) stay simple **only when history is a single straight
line**. Merge commits make "which commit do I go back to?" and "what
does reverting this actually undo?" ambiguous. FF-only merges are what
keep our `main` trivially reasoned about.

## Handy commands

```
git log --oneline --graph -20        # see the shape of history
git log --oneline origin/main..main  # commits local main has, remote doesn't
git reflog                           # every position a branch label has held (your undo history)
git reset --hard HEAD@{1}            # "undo my last branch move"
```
