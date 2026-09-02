# ⚔️ Battle plugins for the event system — real-time, Pokémon-style menu, and TTG-style grid tactics

> Companion to `ngn-vs-rm.txt` (§1 named "no event system" as the
> biggest gap vs RPG Maker MZ; §4's gap-closing list named an event
> system as priority #1). This file answers the direct follow-up: once
> that event system exists, how should BATTLE specifically plug into it,
> given we want real-time (mutaclsym's own existing style), Pokémon-
> style turn-based menu battles, AND TTG-style grid tactics, all
> available as options an event can trigger?

## 🎯 0. The reframe that makes this tractable

**You do not need to design a battle system. You already have two real
ones, independently built, and this house's own file-mediated-relay law
already tells you how to wire a third option to them without touching
either.**

- `205.ttg-tactics/` — grid-based tactics, ALREADY REAL, not just
  designed: `src/ttg_core.c`/`ttg_input.c`/`ttg_compose.c`/`ttg_loop.c`
  exist, `ops/+x/` has compiled binaries, `data/master_ledger.txt` +
  `data/armies/` hold real state, and `scripts/harness_01_move.sh`/
  `harness_02_illegal_move.sh`/`harness_03_attack_regicide.sh` are real,
  already-passing harness scripts (matching at least PR1-PR2 of its own
  17-PR rollout plan in `DESIGN.md`). Action economy is `moved`/`acted`
  flags per unit per turn (not free AP), 4-directional BFS movement,
  Chebyshev-distance attack range, deterministic `dmg = max(1, atk -
  def)` combat (no RNG in Phase 1), regicide win condition. Read
  `DESIGN.md` §4.7 in full before touching this — it is a genuinely
  complete ruleset, not a sketch.
- `203.gb-pokemon/src/battle.c` — turn-based MENU battle, ALREADY REAL:
  `battle_start_wild()`, `do_player_attack()`/`do_enemy_attack()`,
  `try_run()`, `battle_tick()`/`battle_input()` — a complete
  intro→menu→fight/run→win/lose state machine with a real Gen-1-style
  damage formula (`ARCHITECTURE.md` §"Battle formula (MVP)": `base =
  ((2L/5+2) * power * Atk / Def) / 50 + 2`, STAB, type multiplier,
  85-100% variance).
- `101.mutaclsym🧟‍♂️️+18.01/ops/tick_monsters.c` — real-time bump combat,
  ALREADY REAL and already the default: a monster steps adjacent to the
  hero and `hero_take_damage()` fires inline, every tick, no menu, no
  mode switch (verified this session, see `ngn-vs-rm.txt` §2).

Three real systems. Zero of them were designed as pluggable modules of
a shared "battle" abstraction, and **they don't need to become one**.
What they need is a **shared request/result file contract**, so an
event's new `BATTLE_START` command doesn't care which of the three it's
invoking — matching this house's own established pattern (topic 1's own
`#.desktop/` tray handoff between event-editor and mutaclsym is the
exact same shape: separate process, shared file, no direct call).

## 🧩 1. Why "separate process + file handoff," not "one shared combat
module linked into every game"

TTG-tactics and gb-pokemon are each their own standalone package (own
`button.sh`, own vendored `system/`, own session state) — this was a
deliberate choice in both (`ARCHITECTURE.md`'s own "House stack is
heavy for agent iteration" for gb-pokemon; `DESIGN.md`'s own KD1 "muta/
CHTPM runtime, not freeglut product" for TTG, which nonetheless is
STILL its own separate `205.ttg-tactics/` package with its own
`button.sh`, not literally inlined into mutaclsym's own binary). Forcing
either one's real, working, harness-proven code to be relinked into
mutaclsym's own process would mean re-doing the proof work
(`test-harn-ed-app`'s own PITFALL 58 lesson applies directly: a
"successful" merge that isn't re-proven via real key injection in the
MERGED context is not proven at all).

The cheaper, precedent-consistent answer: **keep each battle flavor as
its own process, and make the hand-off a file, exactly like every other
cross-process interaction in this house.** Concretely:

```
Overworld event fires BATTLE_START <flavor> <encounter_id>
  -> overworld writes a battle_request.txt (or a #.desktop/-style tray
     entry - see §3 for which) naming: flavor, encounter data (species/
     level for pokemon-style; army/board for ttg-style; nothing extra
     needed for real-time, see §4), AND the return coordinates/map/
     switch-to-set-on-win
  -> overworld either (a) spawns the target battle app's button.sh
     run-widget (matching event-editor's own README.txt: "not a second
     human terminal" - the SAME real mechanism), or (b) if the battle
     app is already running headless/idle (viable for a frequently-
     re-triggered flavor), signals it via the request file alone
  -> battle app runs its OWN real, already-proven battle loop, totally
     unaware the overworld exists
  -> on win/lose/flee, battle app writes battle_result.txt (or updates
     the same tray entry) with: outcome, any state deltas (hp changes
     to carry back for a party system, xp/rewards), and exits or goes
     idle
  -> overworld's own event interpreter (once built - see
     1-rtp-rpg-xyz-scaffold.md §5c) polls for battle_result.txt,
     consumes it, and branches - EXACTLY matching RPG Maker MZ's own
     real "Battle Processing" event command, which has three real
     branches (Win / Escape / Lose) an event author connects to
     different follow-up commands (set switch, give item, game over,
     etc.) - this is not an invented mechanic, it's matching a real,
     well-known reference behavior with a file instead of an in-process
     callback.
```

## 📜 2. The shared contract — the ONLY thing that needs to be common

Do not try to unify unit stat schemas, damage formulas, or move lists
across all three flavors — TTG's `moved`/`acted` flag economy and
gb-pokemon's level/power/Atk/Def formula are genuinely different games
with genuinely different rules, and that's fine, matching each
reference's own real design. The ONLY shared surface is the
request/result envelope:

**`battle_request.pdl`** (or `.txt`, matching whichever this house's
convention lands on for the event system itself - see
1-rtp-rpg-xyz-scaffold.md's own event-system build):
```
SECTION|KEY|VALUE
META|battle_id|<unique, e.g. timestamp+counter>
META|flavor|realtime|pokemon_menu|ttg_grid
META|return_map|<map id to resume on>
META|return_x|<hero x on resume>
META|return_y|<hero y on resume>
META|win_switch|<switch name to set 1 on win, may be empty>
META|lose_action|<warp_to_town|game_over|none>
# flavor-specific payload below, each flavor only reads its own section
POKEMON|species_id|<n>
POKEMON|level|<n>
TTG|army_p0|<path to army pdl or inline unit list>
TTG|army_p1|<path to army pdl or inline unit list>
TTG|board|12x12
```

**`battle_result.pdl`**:
```
SECTION|KEY|VALUE
META|battle_id|<matches the request>
META|outcome|win|lose|flee
META|hero_hp_delta|<n, may be 0>
REWARD|item|<id>|<qty>          # zero or more rows
REWARD|xp|<n>
```

This is intentionally minimal and intentionally NOT trying to be a
universal combat abstraction - it's a **handoff ticket**, the same
scope as `ee_open_request_write.c`'s own open-request file (name a
target, name a return path, done). Each battle app's own internals stay
exactly as real and as different as they already are.

## 🕹️ 3. Which transport: a dedicated tray, or reuse `#.desktop/`?

Recommend a **dedicated tray**, `#.desktop/battles/` (sibling to the
existing real `#.desktop/{inbox,events,tiles}/`), not reusing `events/`
or `inbox/` directly - a battle handoff is a different kind of object
than an event-editor package or an open-request, and this house's own
existing tray convention already anticipates exactly this kind of
growth (three subdirectories today, nothing stops a fourth). Concretely:

```
#.desktop/battles/
  <battle_id>.request.pdl    <- overworld writes, battle app reads+deletes/moves to .active
  <battle_id>.result.pdl     <- battle app writes on completion, overworld reads+deletes
```

Matches the real, already-proven `event_editor_open.request` +
`event_editor_open.log` pattern (§4 of `1-rtp-rpg-xyz-scaffold.md`)
almost exactly - a request file appears, the target watches for it,
consumes it, does its job, reports back. Nothing new architecturally,
just a new tray subdirectory and two new file shapes.

## 🏃 4. Real-time: the one flavor that should probably NOT go through this handoff

Worth stating directly since it's the one place this pattern doesn't
obviously apply: mutaclsym's own existing bump combat
(`tick_monsters.c`) is **ambient**, not a discrete triggered encounter -
a roaming monster threatens the hero continuously, every tick, as a
property of the overworld itself, not as something an event author
places on one tile and triggers once. Forcing every ambient monster
step-toward-hero check through a request/result file handoff to a
separate process would add real latency (process spawn, file poll) for
something that currently happens inline, every ~16.7ms tick, with zero
overhead. **Recommendation: leave ambient real-time combat exactly as
it is, do not route it through `BATTLE_START`.**

Where real-time COULD legitimately use this same plugin pattern: a
scripted, discrete "boss arena" fight - an event author places a boss
NPC, triggers `BATTLE_START realtime <boss_id>` on interact, and the
handoff spawns a SCOPED instance of mutaclsym's own combat-tick logic
running against a small, purpose-built arena map, reporting back win/
lose exactly like the other two flavors. This is architecturally
identical to the pokemon/ttg flavors (a separate process, a scoped
encounter, a result ticket) and worth building LATER, once
`pokemon_menu`/`ttg_grid` flavors are proven - not a reason to delay
those two, and not a reason to force ambient combat through the same
path today.

## 🔨 5. Concrete build order

1. **Do not build anything battle-specific until the event system
   itself exists** (`1-rtp-rpg-xyz-scaffold.md` §5c - the
   `rtp_event_editor.c`/event command-list capability). `BATTLE_START`
   is just one more event command type in that system, not a
   prerequisite for it.
2. **Define the two `.pdl` schemas in §2 as a real, small spec doc** -
   cheap, unblocks both battle-app-side and event-side work happening
   in parallel once someone picks it up.
3. **Wire `pokemon_menu` first**, not `ttg_grid` - gb-pokemon's
   `battle.c` is a simpler state machine (intro→menu→fight/run→win/lose,
   one wild mon) than TTG's own multi-unit grid economy, so it's the
   cheaper end-to-end proof of the WHOLE handoff mechanism (spawn,
   request, result, resume) before adding TTG's own greater complexity
   on top of a still-unproven handoff.
4. **Then wire `ttg_grid`** - by this point the handoff mechanism itself
   is already proven by step 3, so this is "point the same envelope at
   a different target app," not new plumbing.
5. **Prove BOTH with a real level-2 harness** (§36.6 discipline, same as
   everywhere else in this handoff): trigger the event via real key
   injection in the overworld, assert the battle app's own window/
   frame actually appears and shows real content, inject real keys
   INTO the battle app (attack, win), assert the overworld resumes with
   the right switch set / reward applied. This is the exact same
   "prove it end to end through real running processes, not an op-level
   shortcut" standard PITFALL 58 exists to enforce - a battle handoff
   that "looks wired" but was only tested by calling ops directly is
   exactly the kind of thing that bug already taught this house not to
   trust.
6. **Real-time boss-arena flavor (§4) last**, once the pattern is
   proven twice over and there's an actual boss fight design that wants
   it.
