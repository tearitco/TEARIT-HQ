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
- One Enter was sent with the highlight on the Actors tab (pid
  369123, `focus_nav` 1). After `KEY_PRESSED: 13` the marker was
  `16. 1. Harold` and `focus_nav` was 16. The tab title stayed
  Actors and the detail stayed Harold. No second key. The earlier
  Down that walked onto Classes is a separate, older result.
- The compiler keeps four fields and drops a fifth without an error.

Dustopia is this same machine holding one fact two ways: the page,
and a numeric step, with a stored weight choosing which one advances.
That design lives in `08-roadmap/design-docs/DUSTOPIA-HACK.md`. The
ladder of what is met and what is not is in the 2026-09-23 addendum
there, and in `GS-23-HQ-TECH.md` in the timeline packet.

The part that decides the next command, including a local model, is
not this chapter. It is `17.ai`. When that chapter's loop appends an
event command, the command runs here.

## Picker

The events editor lists every `COMMAND` in
`#.ref/menu/event_commands.registry.pdl`. `evhq_projector.c` loads
that file. The cap is 128 types. The file has fewer than that.
`read_receipt`, `send_window_key`, and `advance_fact` are in it, so
they show up next to the RPG Maker commands the next time the editor
opens. House-specific commands are allowed. Networking, and anything
else a page must name, can be a command when it has to be. A window
can exist before the command does. hq-ftp is that case: a network
menu row and a placeholder window, design in
`08-roadmap/design-docs/HQ-FTP.md`, no transfer code yet.

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

**Range.** The hit is real. The skill menu is not. `apply_range` is an event command. An NxN square starts at the origin cell (range 2 from `a1` covers `b2` and not `c3`). A cell inside it subtracts 1 from that actor's `page_value` only. `prisc+x` on page 2: Glacine (actor 6) went from 20 to 19 on `a1`→`b2` range 2, and stayed 20 on `a1`→`c3` range 2. Ember's `page_value`, her `mhp`, and Ember's `numeric_value` stayed 20. Glacine was restored to 20. The placing grid fills that same square when `PLACE_RANGE` is set, or when the jump buffer is only digits. The placer binary was rebuilt. The overlay was not opened, so there is no frame of the fill. Move, Use, and Attack rows from a `.pdl` are still not built. Play mode is not required. The side-view battle screen is still the separate parity gap.

**Cli-io is every entity. Act is only an actor.** The old `CLI_IO` action opened a bare popup. Every `menu.chtpm` now opens `&.widgits/entity-cli/entity-cli.xhtpm`, a real `<cli_io>`. Enter runs the typed line (`range a1 b2 2 6` calls `apply_range`; `advance page` calls the advance script). The shared renderer supplies the `^` lock when that field is armed. Act, the window built from `skills.pdl` (Move, Use, Attack), is only on actor pals: the four terumon, Asa, Ava, and Cursword. Doors, stores, book-stack, and the other pals keep Cli-io and do not get Act. A pal reads `menu.chtpm` at startup, so it must be restarted before the new rows show. `objects.pdl` on Ember has `PAGE | act` and `GOTO:act` for the legacy menu path. The menu you see is still `menu.chtpm`.

## Two facts, one weight

`#.desktop/harnesses/two-facts/facts.txt` names actor id 5, Ember.
`page_value` is a field on that actor in `db_hq_actors.state.txt`
and in `actors.pdl` (both stay even). `numeric_value` is
`terumon_001_ember/numeric_fact.txt`. `weight.txt` is `which=page`
or `which=numeric`. `advance_fact` runs `advance.sh`. `prisc+x`:
`which=page` made the actor 21 and left the numeric file at 20.
`which=numeric` made the file 21 and left the actor at 20. `mhp`
stayed 20. `which=both` wrote nothing. All three were put back to
20. This is Ember's HP-shaped pair. It is not a battle and not a
range grid.
