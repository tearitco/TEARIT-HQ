# 13.agent-coms — durable learnings from archived binder notes

Condensed from the `2026-09-08/` binder (GROK.md/OPENCODE.md/SONNET.md,
moved whole to `#.Zarchive-2-trash/13.agent-coms/2026-09-08/`) and
`2026-09-15/CURSWORD-DISAPPEARS-ON-CLICK-HANDOFF.md` (also archived —
resolved same day). `2026-09-15/GROK.md` is **not** archived: it's the
live PALCRAFT task handoff and still the real onboarding doc for that
in-progress work (see `08-roadmap/design-docs/PALCRAFT-DESIGN.md`).

These were the per-agent "binder" notes from before **Co-lab-h-ai**
(the real, human-approved multi-agent chat room, now the primary
channel — see `README.md`). Kept here only for the standing rules they
established, which Co-lab-h-ai's human-approval-gate doesn't replace.

## Multi-agent git protocol (the actual recurring failure mode)

Two real incidents, both root-caused to the same thing: **one shared
checkout, one shared `HEAD`**, so any agent's `git checkout`/merge/FF
moved the *other* agent's working tree and branch ref out from under
them mid-edit, silently.

- **Fix that actually holds**: per-agent `git worktree`s (one repo,
  separate checkouts per branch) — `git checkout <other-branch>`
  becomes physically refused ("already checked out at ..."), not just
  discouraged by a rule. Written up in
  `01-orientation/GIT-WORKFLOW-FOR-BEGINNERS.md`.
- **Standing rule**: each agent commits and pushes ONLY to its own
  branch (`git push origin <own-branch>`); never `checkout`/`merge`/
  `fast-forward`/`cherry-pick`/`update-ref` another agent's branch or
  `main`. Merges to `main` are the user's call.
- **"Stash, then fast-forward" is not a universal remedy.** When a
  branch has genuinely diverged (not just gained commits — merge-base
  is behind both tips), a fast-forward is topologically impossible
  regardless of a clean tree; the real fix is a real merge commit
  (`git merge origin/<branch>`, never force/reset/rebase). Also: don't
  assume "nothing in the dirty set is mine" without diffing file paths
  — two agents' changes can genuinely overlap on the same files, and a
  blind `stash apply` will refuse to restore cleanly if a rename
  orphaned part of the stashed tree.
- **Untracked shadow copies actively cause this class of bug**: an
  untracked root-level `SONNET.md`/`OPENCODE.md` sitting alongside the
  tracked dated-dir versions meant both agents were silently editing
  the wrong copy and "seeing nothing" from each other. If a file is
  meant to be the shared channel, make sure only one tracked copy of
  it exists.

## NB-JS / network-browser rung ladder (historical context only)

The 2026-09-08 SONNET.md binder tracked a long real sequence of NB-JS
engine work (CLI/REPL modes, CommonJS, ESM, cookie jar, history/
location, localStorage, document-order script execution, real
`http://` fetch, CSS layout awareness) landing rung by rung through
2026-09-10. All of it is superseded by whatever's current in
`08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md` and the network app's
own current status docs — this binder is not the place to check
current NB-JS state, only a historical log of how it got built.

## cursword disappearing on click — RESOLVED 2026-09-15

Real root cause: `popup_draw_text()` (a shared helper) hardcoded
`DefaultVisual`/`DefaultColormap`, but cursword's own 32-bit ARGB
pixmap needs a matching Visual/Colormap — the mismatch caused
`RenderCreatePicture` to BadMatch, and since this house installs no
custom `XSetErrorHandler()`, Xlib's default handler printed the error
and silently called `exit(1)` with no visible crash trace at all.
Fixed generically in `popup_draw_text()` (real depth query + matching
Visual/Colormap, covers any future ARGB caller, not just cursword).
Permanent pitfall entry + the still-open "install a real house-wide
`XSetErrorHandler()`" follow-up: `03-pitfalls/HOUSE_CODE_PITFALLS.md`
#23.

**Durable investigation technique** (the thing that actually cracked
it after diff-reading dead-ended): a live `strace` on the actual
running, about-to-die process, not another read-the-diff pass. Worth
reaching for directly, earlier, the next time a window silently
vanishes with zero error output — this house has no crash handler
installed, so "the process just disappeared" is a real, recurring
symptom shape for this exact class of Xlib error, not a fluke.
