# User-project → GitHub sync (in-app, GUI)

**Status: EXPLORATION, 2026-09-06.** Direct note from the user while
cleaning up the dev repo's `.gitignore`: *"later when we start shipping
user programs, we will have users use git to save their actual games/
projects to github thru our application. we will probably have a gui
app to do this. and at that point it should track commit stuff like
that."*

Companion to `PHONDO_INSTALL_IDEAS.md` (install/store shape) and
`USER-JOURNEY-COMPLETION-GRAPH.md` (the end-to-end journey — this is
the "save / back up / share my project" step of it).

## The idea

A shipped product gives each **user** a way to push their own
game/project to their own GitHub repo, from inside the app — a GUI
button ("Save to GitHub" / "Publish"), not a terminal. Under the hood
it is still `git` (commit + push to the user's repo), but the user
never sees git.

## The key inversion from THIS repo's rules

In the **development house repo** (this one), per-app runtime/state
files are *scratch* and are `.gitignore`d / `git rm --cached`d on
sight — `module_parent.pid`, `*_ui.txt`, `*_action.txt`, `*_buffer.txt`,
`cli_io_state.txt`, `pieces/**` session trees, etc. (see the 2026-09-06
`.gitignore` block). They are regenerated every launch and mean
nothing in history.

For a **user's own project**, the opposite is true: the saved game
state, the level/map files, the project's own `.pdl`/data — **that IS
the work**, and the in-app sync MUST track and commit exactly those.
A user who hits "Save to GitHub" and finds their last three hours of
building isn't in the commit will (rightly) never trust it again.

So the in-app sync needs its **own** include/exclude policy, authored
per project template, not inherited from the dev repo's `.gitignore`:

- **Always commit**: the project's declared data/state files (whatever
  that toy's manager treats as the saved document), assets the user
  added, project manifest.
- **Never commit**: transient render scratch (frame buffers, PID
  files, `*_action.txt` command queues), absolute-path caches,
  anything with a machine-local path or a secret. Same *categories*
  the dev repo excludes — but the boundary between "state that is the
  save" and "state that is scratch" is drawn deliberately for each
  toy, because for a game the save state is precisely what the dev
  repo would call throwaway.

## Open questions (for when this work starts)

1. One repo per user, one per project, or a monorepo of the user's
   toys? (Store/publish flow in `PHONDO_INSTALL_IDEAS.md` may force
   this.)
2. Auth: GitHub device-flow / OAuth from inside the app, or a PAT the
   user pastes once? (Security side: `SECURITY.md`.)
3. Conflict handling in a GUI with no git literacy — last-write-wins
   with a local backup, or a real merge UI? Probably the former for
   v1.
4. Where does the per-project include/exclude list live — in the toy's
   `toy.pdl`, a sibling `sync.pdl`, or generated from the manager's
   declared state paths?
5. Does "Publish to the store" (a different action) reuse this same
   sync, or is the store a separate artifact upload?
