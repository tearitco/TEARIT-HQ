Events/DB Roadmap — Next Steps (2026-08-25)
=============================================
Direct conversation, 2026-08-25, after the H6/H7/H8 batch landed. User asked
for an honest gap assessment across events/db/plugins before picking what's
next, and asked for it documented. Verified facts below were checked live in
this session, not assumed.

CURRENT REAL STATE (verified, not assumed)
-------------------------------------------
- Only 3 of the 143 catalogued event commands are actually implemented
  anywhere: Change Gold, Show Text, Show Choices (see HAIKU_TASKS.md Task H1,
  #.ref/menu/event.commands.{1,2,3}.txt for the full catalogue,
  event.commands.remaining.txt for what's left).
- **Common events are NOT wired into the Play runtime.** `play_event.sh`
  (called by both an entity's own right-click Play row AND tonight's new
  events-hq Play button) only ever looks at that ONE entity's own
  `event_pkg/pages/page_N/`. It has zero knowledge that `common_events/`
  exists anywhere. Db-hq's Common Events feature is real and editable, but
  nothing actually RUNS a common event today. This was a direct, confirmed
  question tonight ("have u been duplicating and testing our events in
  common events yet?") — answer is no, because the wiring doesn't exist.
- **No Stop.** Play is fire-and-forget (`&`-backgrounded shell call) — the
  manager doesn't track a running event or offer a way to kill it mid-run.
- **Teleport ("Transfer Player" / "Set Event Location") is catalogued but
  not implemented.** Same status as the other 140 uncommitted commands —
  not special, just an example that came up in conversation.
- **No battle system, no tactics/targeting, no line-of-sight, no
  pathfinding** exist anywhere in the events/db/pal system. Unrelated
  standalone prototypes exist elsewhere in the tree (`203.gb-pokemon/src/
  battle.c`, `205.ttg-tactics/`, `@.apps/tactics-txt/`) but are separate
  top-level game projects with zero connection to events-hq/db-hq.
- **No plugin architecture exists.** No `10.plugins/` directory, no
  per-pal/per-project on/off toggle convention, anywhere. This would be new
  design work, not a gap in something half-built.

STEP 1 STATUS: DONE + VERIFIED LIVE (2026-08-25)
---------------------------------------------------
`play_event.sh` now additionally scans `$HOUSE_ROOT/common_events/*/event_pkg/
pages` for any page matching the fired trigger, and runs EVERY matching
common event (not just one - each is independently named, unlike page_N
variants of a single entity which still use "highest wins"). Entity-local
behavior is byte-for-byte unchanged (fully additive).

Real live test: populated `common_events/greet_player/page_1` with a real
Change Gold(+5) command via events-hq's own UI (same Add Command picker
verified earlier), then ran `play_event.sh` on `m8_redhorned`:
- m8_redhorned's own gold: 70 -> 105 (its own 2 commands, unchanged path)
- greet_player: fresh `inventory.txt` created with `qolq=5` - a REAL,
  independent side effect proving the new dispatch actually ran.

KNOWN LIMITATION (real, flagged in code comments too, not silently
glossed over): a common event's compiled `cmd_N.sh` resolves its target
directory via `cd "$(dirname "$0")/../../.."` - i.e. it acts on
`common_events/<name>/` ITSELF, not on the entity that was actually
Played. This proves the dispatch wiring works, but a common event can't
yet modify the calling player's own state (e.g. "give player gold" from
a shared event). Follow-up: give the compiler (`compile_page()` in both
`khtpm_events_hq_manager.c` and `ez_menu_input.c`) a way to emit a
target-dir override so `cmd_N.sh` can act on the CALLER's directory when
invoked as a common event, not just its own. Not done yet - a real next
task, not urgent relative to the plugin architecture scoping (step 2).

TAB-8 "player" CELL PLAY BUTTON - REAL STATUS + THE BIGGER GOAL (2026-08-25)
----------------------------------------------------------------------------
Direct question: "how come when i press play in 8.player it doesn't
(re)play yet?" Real answer, verified in `khtpm_taskbar_manager.c`: the
taskbar's own "player" cell (position 8, `which == 8` in the header
dispatch) has a real `play`/`pause`/`reset` submenu
(`livedesk_build_player_menu()`), but `play` and `pause` have ALWAYS
been inert placeholders - label set, `command` left empty - in BOTH
this port and the legacy `tp_taskbar.c` before it. Only `reset` (added
2026-08-11) does anything real. This is not a regression from tonight's
work; it was never wired, ever.

THE REAL GOAL (direct instruction, 2026-08-25): pressing Play here
should play ALL common events, each firing however IT is configured to
fire - "just like rpg maker." And a common event's trigger is properly
supposed to be fired THROUGH one of the player's own event pages (a
page doing something like Set Variable / Conditional Branch), not by
the page and the common event happening to share the same bare trigger
NAME (which is what tonight's common-events-into-Play wiring actually
does - a real, working, but simplified stand-in for the real design).

Direct, honest answer to "do we have enough infrastructure for this
yet?": **No - Control Switches, Control Variables, Conditional Branch,
and an explicit "Call Common Event" command are all UNBUILT** (all
catalogued in `event.commands.remaining.txt`, none implemented). BUT -
verified by reading `101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x.c` directly
- **the underlying VM already has everything needed for real branching**:
`OP_BEQ` (branch-if-equal), `OP_J`/`OP_JALR` (jump), `OP_LW`/`OP_SW`
(load/store), `OP_READ_STATE`. This is NOT a "build a new engine" gap -
`event.pal` today only ever compiles to a flat `exec cmd_N.sh` sequence
because the COMPILER (`compile_page()` in `khtpm_events_hq_manager.c`/
`ez_menu_input.c`) never emits anything else, not because the VM can't
run anything else.

What's actually needed, in real dependency order:
1. **Control Switches + Control Variables** (`event.commands.remaining.
   txt` already scoped this: one KV file per pal, `switches.txt`/
   `variables.txt` - "cheap soon," pure file I/O, same shape as
   `inventory.txt`). New ops (`mr_control_switch.+x`/
   `mr_control_variable.+x`) + event-ez/events-hq picker entries, same
   pattern as the 3 existing commands.
2. **A real "Call Common Event" command** - a page can explicitly
   invoke one named common event, compiling to something that execs the
   target's `event.pal` directly (reusing tonight's `MUCHI_CALLER_PKG`
   mechanism so any player-visible UI it opens - e.g. Show Choices -
   correctly targets the calling player, not the common event's own
   directory). This is the REAL RPG-Maker-accurate replacement for (or
   complement to) today's implicit trigger-name auto-match in
   `play_event.sh` - RPG Maker common events are either Autorun/
   Parallel (run unprompted, gated by their OWN switch condition) or
   explicitly Called from a page's command list - never "runs because
   some unrelated page happens to share its trigger name," which is
   what this house currently does as a real, working, but simplified
   stand-in.
3. **Conditional Branch** - the actual compiler work: translate a
   NODE (`if switch X is ON` / `if variable Y == Z`) into real
   `OP_READ_STATE` + `OP_BEQ`/`OP_J` opcodes instead of `event.pal`'s
   current flat `exec`-only shape. This is the piece that most needs
   real design attention (label/jump-target addressing scheme within
   a compiled `event.pal`), not just a mechanical KV-file op like
   #1/#2.
4. **Re-scope the player-cell Play button itself**: once #1-3 exist,
   change its semantics from "run every common event whose trigger
   name matches whatever fired" to the RPG-Maker-real version: run
   every common event flagged Autorun/Parallel (checking its own
   switch condition each time), and leave on-demand ones to be reached
   only via an explicit Call Common Event node placed in a real page -
   this likely means the CURRENT trigger-name-matching behavior in
   `play_event.sh` gets replaced, not just extended, once this lands.

RESOLVED (2026-08-25) - the "how should the shared event loop be
architected" question, via a real, working precedent: direct pointer to
`101.lpns+map+4` (a DIFFERENT house checkout,
`NNEST-11.14+DEEP/.../44.xyz❤️‍🔥️00.10/101.lpns+map+4`), described as
"the source of truth for how shared event loops should take place."
Verified directly, not taken on faith:
- `system/game_manager.c` runs ONE dedicated polling thread in ONE
  process: `while (running) { poll_relay(); usleep(POLL_INTERVAL); }`.
- That SAME process is the SOLE writer to `data/master_ledger.txt` - a
  single, shared, append-only event log every player's/NPC's action
  (word/move/end_turn) funnels into. Per-player ledgers
  (`players/<name>/ledger.txt`) are filtered VIEWS derived from the one
  shared ledger, never independently maintained state.
- Nothing else in that system polls the relay or writes the ledger
  independently - confirmed via `ledger-4-agent-trace.md`'s own process
  tree (`orchestrator -> renderer / keyboard_input / chtpm_parser_pal ->
  game_manager` - exactly one polling+dispatch node, everything else is
  a pure reader/renderer).

This is the SAME shape already recommended above for common events,
now grounded in a real, already-built reference instead of abstract
reasoning: build ONE common-events manager (not one-per-entity), tick
on an interval (matching `game_manager.c`'s own `usleep(POLL_INTERVAL)`
shape), sole writer to a shared common-events ledger (Autorun/Parallel
firings, one line each), everything else reads that ledger rather than
re-deriving/re-checking state independently. Use this file as the
literal template to copy the manager's shape from, not just its
description.

SECOND KNOWN INCONSISTENCY (found scoping plugins, 2026-08-25): common
events currently live at `<house_root>/common_events/` — GLOBAL across
the whole house, not scoped to a session/"file" at all
(`khtpm_hq_manager.c`'s `publish_common_events()` reads `%s/common_events`
with `g_house_root`, not a session dir). This is inconsistent with
`desks/`/`entities/`, which ARE session-scoped
(`sessions/<id>/desks/`, `sessions/<id>/entities/`). Plugins are being
scoped per-session per direct instruction (see
PLUGINS-ARCHITECTURE-SCOPING.md Q1) - common events arguably should be
too, so two different "shared events" concepts don't have two different
scoping rules. Not fixed yet - flagging so it's a deliberate future
decision, not a silently-inherited inconsistency.

AGREED ORDER (direct instruction, 2026-08-25)
-----------------------------------------------
1. **Wire common events into Play, and test it.** Extend `play_event.sh`
   (or the manager that calls it) to also check `common_events/` so a real
   Player-Play action can trigger a shared/common event, not just the
   entity's own page. This is the smallest, most concrete next step and
   directly proves the reuse model (desk events + common events + player
   play, all through ONE real runtime path) before building more commands
   on top of it. Test plan: duplicate one of the 3 working commands
   (Change Gold is simplest) into a common event, trigger it via a real
   Player-Play click, confirm the same real state-file side effect occurs
   as the entity-local version.
2. **Scope the plugin architecture.** Direct clarification, 2026-08-25:
   - **"10.plugins" is a toolbar/taskbar entry**, not just a bare folder
     convention — i.e. it's a real menu cell in the house's UI (same
     tier as the other numbered taskbar cells), not something you only
     ever touch by hand-editing files.
   - **Modeled on RPG Maker's own plugin system**: a per-project list of
     plugins, each independently toggleable on/off (`file:project`
     scoping per the original ask), loaded/applied in a defined order —
     same mental model as RPG Maker MV/MZ's Plugin Manager, not a novel
     design from scratch.
   - **Written in `.pal` script**, NOT a compiled `.c` op and NOT JS
     (RPG Maker's own plugins are JS — this house's equivalent unit of
     logic is `.pal`, consistent with everything else in this system).
   - **The `.pal` script's job is to TRIGGER real ops**, not reimplement
     logic itself — same shell/manager-style separation already used
     everywhere else in this house (`.pal` as glue/orchestration, real
     compiled `.c` ops for the actual heavy lifting like LOS/pathfinding
     math). A plugin is a `.pal` entry point that calls existing or new
     ops, not a place to hand-roll algorithms in `.pal` itself.
   This is still a Sonnet-level design pass, not Haiku-ready yet: needs a
   real decision on the plugin manifest shape (what a `.pal` plugin
   declares — hooks it wants, load order, per-project enable state file),
   and how a plugin's `.pal` script hooks into the existing event/render
   loop without becoming a second parallel dispatch mechanism (same "one
   dispatcher" lesson already learned from the taskbar's own capture-only-
   writer migration). Candidate first real plugin once the architecture
   exists: line-of-sight or pathfinding (smaller, more mechanical) rather
   than a full tactics/targeting system, to prove the plugin shape works
   before committing to the biggest example first.
3. **Individual missing commands** (Teleport/Transfer Player, Erase Event,
   etc.) — mechanical, bounded, Haiku-ready one at a time, same pattern as
   H1's Show Text/Show Choices. Not urgent relative to #1/#2, but cheap to
   pick up in parallel whenever there's Haiku capacity free.
4. **Battle/tactics/targeting system** — explicitly LAST. It genuinely
   depends on the plugin architecture (or at minimum LOS+pathfinding)
   existing first; starting here early means throwaway scaffolding that
   gets redone once #2 lands.

WHY THIS ORDER (user's own reasoning, paraphrased)
-----------------------------------------------------
Common-events-into-Play gives the most "breadth of momentum" — it's small,
proves out a real architectural connection (desk events + common events +
player play unified under one runtime), and unblocks reuse for every
future event without committing to a big design decision first. Plugin
architecture comes next because everything battle/tactics/LOS/pathfinding
related depends on it existing in a real, non-throwaway form.
