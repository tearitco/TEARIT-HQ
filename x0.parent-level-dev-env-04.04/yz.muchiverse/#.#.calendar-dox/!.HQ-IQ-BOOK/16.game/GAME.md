# Game

A book is a session. A page is a desk. Entities on that page are
actors. Common events and entity pages are the rules. Playtest of the
whole page is taskbar cell 9 (play, stop, reset), and PC-HQ's Player
menu writes the same `khtpm_play_mode.state.txt`. An entity's own
play control tests that one entity and leaves the others alone.
`play_event.sh` is not the play button. See
`02-architecture/PLAYTEST-DESK-AND-PCHQ.md`.

What is proven, as of 2026-09-23:

- Actor rows exist for the four terumon and for Asa, Ava, and
  Cursword. Nothing in play writes exp yet.
- An event page can read a receipt (`focus_nav`) and branch with
  `if`. `send_window_key` appends `KEY_PRESSED: 13`, which is the
  K9 activate key. `send_input` still writes the bare code and was
  left alone.
- One live key was sent. It was a Down, which was the wrong key, and
  it only moved the tab highlight. Enter has been proven on the
  fixture, not on that window.
- The compiler keeps four fields and drops a fifth without an error.

Dustopia is this same machine holding one fact two ways: the page,
and a numeric step, with a stored weight choosing which one advances.
That design lives in `08-roadmap/design-docs/DUSTOPIA-HACK.md`. The
ladder of what is met and what is not is in the 2026-09-23 addendum
there, and in `GS-23-HQ-TECH.md` in the timeline packet.

The part that decides the next command, including a local model, is
not this chapter. It is `17.ai`. When that chapter's loop appends an
event command, the command runs here.

## Open parity gaps

Named so a later agent does not invent them in the middle of a
smaller test.

**Battle screen.** Not built. The 2026-09-02 handoff names Battle
Processing as unbuilt, and names the Enemies, Troops, States, and
Animations database tabs as the data behind it. There is no spec for
what the screen shows, when it opens, or how a troop is chosen.

**RPG Maker project load and save.** Not documented as a procedure,
and not shown to work. What exists instead: Mutaclysm user save slots
and a demo-project seed (`01-orientation/XYZOS_README.md`), and
`07-install-and-ship/USER-PROJECT-GIT-SYNC.md` for backing up this
house's own files to git. K9 mentions "local play from a created or
saved project" and gives no steps. Nothing here says how to open an
RPG Maker MV or MZ project, or how to write one back out.
