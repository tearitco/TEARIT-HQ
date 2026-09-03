# What is TEARIT-HQ

*Condensed from `SKILLS.md` §1-2 and `!.HOUSE_STDS.md` §A, 2026-09-02.*

TEARIT-HQ ("the house") is a file-backed desktop OS project: a
"livedesk" taskbar spawns entities (apps/games/tools, each its own
process) onto a virtual desktop, and every one of them communicates
through plain files, never shared memory or synthetic input events.

## The one belief everything follows from

**Real, file-based state only — never in-memory — for compliance and
audit.** Every mode's state lives in a real file on disk (`.pdl`,
`.chtpm`, `*_state.txt`, `action.txt`) a human or another process can
open and read at any moment and see the truth. This explains almost
every other house convention: input is file-relayed instead of
synthetic X11 events, state is republished on every real change
instead of computed on demand, PDL (`SECTION | KEY | VALUE`) is
everywhere, and "just keep it in a global for now" is the wrong
instinct even when it would technically work.

## The basic project shape (PIECE/MODULE/OS)

Most projects in this style share:
- `system/` — shared engine binaries (`prisc+x` the PAL VM,
  `chtpm_parser_pal` the menu/layout engine, `keyboard_input`,
  `renderer`), usually **copied** (never symlinked — see below) from a
  canonical source project into each consumer.
- `ops/` — project-specific compiled binaries that do real work: read/
  write state, render content, dispatch commands.
- `pal/*.pal` — tiny PAL scripts gluing ops together in a loop (read a
  key, dispatch it, compose a frame, sleep, repeat).
- `pieces/chtpm/layouts/*.chtpm` — layout files defining screens:
  buttons, text, substitution variables.
- `pieces/system/`, `pieces/display/`, `pieces/apps/player_app/` —
  where state, frame buffers, and trigger/marker files live.
- `default_op.txt` — registers every op name a `.pal` script can call.
- `button.sh` — the launcher (`run`/`build`/`kill`/`check` verbs).

**HOUSE RULE: never use symlinks anywhere in this tree.** They don't
survive Windows checkouts/zip transfers. Sharing is done by compiling
from canonical source or copying files outright. If you're about to
write `ln -s`, copy instead.

## Session isolation

A "session" is `pieces/sessions/<timestamp>-<pid>/` — a fresh,
isolated working directory per running instance. The model is
**copy-in / persist-out**: `button.sh` copies read-only inputs into
the session dir at launch; every writable state file/dir needs a
matching entry in `persist_session_state()`, which copies it back on
exit. A writable file missing from BOTH lists silently writes into the
ephemeral session dir and vanishes on cleanup — this bug class has
recurred across multiple projects; always audit `button.sh`'s copy-in
+ persist lists against everything your ops actually write. Binary
rebuilds do not apply live to a running session — restart it. Mid-
session, code must read the SESSION copy of state, not the project
root.

## The digit-dispatch convention (a real, easy-to-relitigate mistake)

Numbered screen rows (`[>] 1. Label`) come from `.pdl`
`METHOD | Label | COMMAND` rows via `chtpm_parser_pal.c`'s
`${piece_methods}` generator, whose internal `method_idx` starts at
**2**, not 0/1. So `resolved_item = (key - '0') - 1` is the CORRECT
compensation for that +2 offset in the dominant case — do not "fix"
this away in a file whose numbered items are auto-generated via
`${piece_methods}`. The `-1` is only wrong for code paths reading a
genuinely raw keyboard digit that never passed through `KEY:N`
regeneration (rare — a hand-rolled digit accumulator reading
`interact_relay.txt` directly). When unsure, check
`chtpm_parser_pal.c`'s `method_idx` initial value directly.

## Focus glyphs

`[>]` = focused (navigable), `[^]` = active/engaged (typing, or
`onClick="INTERACT"` currently owns input), `[ ]` = neither. Drawn
entirely inside the shared `chtpm_parser_pal.c` engine — don't
reimplement it per-project.

## Marker discipline

Three marker files, purposes never to be conflated:
1. `pieces/display/frame_changed.txt` — the render trigger. Only a
   compose op or menu-input key-tail should grow it.
2. `pieces/apps/player_app/state_changed.txt` — **never grow from any
   op.** The parser's main loop reacts to any growth by re-parsing and
   restoring focus from `active_gui_index.txt`. Growing it on every
   idle compose silently clobbers user navigation within ~1.5s.
3. `pieces/display/<project>_screen_changed.txt` — your project's own
   "something changed, re-compose" marker.

Also: KV read helpers must strip trailing `\r\n`, or a `strcmp`
against a derived value can fail forever and silently swallow
dispatched keys (real bug, fixed in yahoo-app; see
`@.apps/yahoo-app/ops/yahoo_menu_input.c` for the reference fix).

## Runtime-tunable values belong in PDL, not C constants

Positions, colors, sizes, labels, toggles — anything a user might want
to tweak without recompiling — goes in a `.pdl` file
(`SECTION | KEY | VALUE`), with safe fallback defaults if the PDL is
missing/incomplete. Extend the nearest existing PDL rather than
inventing a new config file.

## The two rendering families (see 02-architecture for full depth)

- **chtpm_parser_pal family** — the original ASCII/text-grid engine,
  PAL-VM driven, no box model. Still running in production for several
  apps (IRC chat, forum, chain, mutaclysm, piececraft, and most
  `@.apps/`).
- **khtpm family** (`khtpm_core_render.c`) — the newer Elem/CSS
  engine with a real box model, used by db-hq, events-hq, chat-hai,
  palettes, bookmarks, stats-hq, taskbar-settings, and (as of
  2026-09-01) the taskbar strip itself. This is the GOLD STANDARD
  target for all new UI — see `02-architecture/CENTROID_GOLD_STD.md`.

Neither family is "correct" and the other "wrong" — chtpm_parser_pal
apps are not deprecated and are not owed a retrofit; the house's
posture is "migrate opportunistically" whenever a real feature/bug
already has a reason to touch an app's display layer.
