# progress/ — dated working log for MarS StreetRace Windows port

Written for other agents picking this up cold. Newest file wins.

## Files

| Date | File | Covers |
|------|------|--------|
| 2026-09-27 | [2026-09-27-win32-port.md](2026-09-27-win32-port.md) | Full port: compat layer, build, launcher, orchestrator, 7 bugs |

## How to read this

Each dated file is a snapshot at the end of that session: what works, what
does not, what was verified and how, and what is deliberately not done. Later
files supersede earlier ones — do not act on a "TODO" from an older file
without checking whether a newer one closed it.

## Ground rules for this project

**The port edits zero `.c` files.** Every Windows adaptation lives in
`win32-compat/`, force-included with `gcc -include` or found via `-I
win32-compat`. This is deliberate: the same 31 sources must build on Linux
with no shim involved, and `git diff -- '*.c'` staying empty is the proof
that the Linux build is untouched.

This is a *porting* constraint, not a ban on touching the game. Genuine
cross-platform game bugs have been fixed in the `.c` files as such — see
the "Game bugs fixed" section in the dated log. The rule is narrower than
"never edit `.c`": never edit a `.c` file *to make Windows work*. If a
change is needed purely because Windows lacks a POSIX facility, it belongs
in `win32-compat/`.

**Generated state is not source.** `corporations/generated/`, `data/`,
`players/`, `multiverse/`, `price_history.txt` and `gl_cli_out.txt` are all
rewritten by simply running the game. Do not commit them, and do not
`git add -A`. After a play-test, restore them — and scope the restore to
those paths, because restoring the whole project directory reverts
uncommitted work. That mistake was made once already in this project and
cost an afternoon of re-applied edits.

## Current state (2026-09-27)

Builds 31/31 and plays. Two copies of the port exist and only one is
authoritative — see the "Open items" section of the dated log before
editing anything.
