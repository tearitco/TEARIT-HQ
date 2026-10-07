# eden - the Eden conductor v0 (template files, not a live entity)

Built 2026-10-07 in alpha (spec `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md` sections 1-5 and 7). A **conductor** is an entity (glyph 🔘) whose context menu is a game's control panel: Setup, Start, Pause, Resume, Save/Load slots, Status, Add/Remove participant, Reset. Everything is RPG Maker-style event pages on the house clock; the one compiled piece is `ops/eden_op.c` (header = every verb). **Nothing here is installed anywhere: never copy it into live user data (`xyzfs/users`).** Asa and Ava are *scratch entities* spawned fresh from `conductor/participant_template/` (identity `ED<n>`, no copied PAL hash or uid); the harness works on `/tmp` scratch roots only. Headless: no window.

## Files
| Path | What |
|---|---|
| `toy.pdl` | the taskbar Toys identity (`SECTION | title | Eden`, `SECTION | launch | conductor/button.sh`); the scan lists a direct child of the house root, `@.apps`, `&.widgits` or `&.hq-apps` that holds `toy.pdl`, so it sits here, not in `conductor/` |
| `conductor/meta.pdl` | the context menu: METHOD rows Setup, Start, Pause, Resume, Save 1-3, Load 1-3, Status, Add participant, Remove participant, Reset; each is `sh -c 'exec "$1/&.widgits/digipet/ops/+x/event_page_op.+x" "$0" "$1" --trigger <t>'` |
| `conductor/event_pkg/pages/page_N/` | one page per trigger (`setup start pause resume status add remove reset save1-3 load1-3`); each `event.pal` is one `exec ops/+x/eden_op.+x . <verb>` |
| `conductor/game.pdl` | the manifest: `GAME` title, `SETUP` seed / length, `PARTICIPANT` rows (asa, ava), `SPARE` rows (what Add participant spawns next) |
| `conductor/kits.pdl` `items.pdl` `skills.pdl` `nodes.pdl` `weather.pdl` `crops.pdl` | role kit (seed x5, jug, hoe, nine skills, hp/mhp +10), items (`grants_skill=`, `tool=`, `nutrition=`), skills (`needs_item=`, `needs_place=`, `cost_ticks=`, `event=`), training nodes (`NODE | graft | requires=plant | exp_to_unlock=6`), rain chance (`rain_pct`), crop (growth days, yield, seed return) |
| `conductor/tunables.pdl` `weights.pdl` `phrases.pdl` | every rule number (hunger step, hungry/starving thresholds, plots, trade keep/gap, hut cost, exp), the agent seat's act weights, what talk says |
| `conductor/wiring.pdl` | where the helpers are, relative to the conductor (lc_clock, house, clock id, snapshot op, store, game tree, participants dir, template, control ledger) |
| `conductor/participant_template/` | the participant folder to copy: `pal.pdl` (no hash), `items.txt` (own `item_apple=2`), `actor_1_stats.txt`, `variables.txt`, `own/notes.txt` (the "own data" Reset/Remove must leave alone), `phone.txt` |
| `common_events/eden_day_tick/` | the Day Tick common event (`target.pdl` names the conductor, one page = `eden_op daytick`) |
| `ops/eden_op.c` | the compiled verbs |

## Rules (all numbers in the .pdl files)
- **Grants** (append-only `grants.txt`): `GRANT|eden|<entity>|item/skill/param|<id>|<amount>|applied=<0/1>|day=<n>`; a triple already granted is skipped (Setup twice grants once); `applied=0` means the entity already had the skill (Reset does not take it away); items revoke up to what the entity still holds; params are additive deltas so they reverse. Registered commands `change_items` / `change_skill` / `change_hp` write `items.txt` `item_<id>`, `actor_skills.txt` `skill_1:<id>`, `actor_1_stats.txt`; `eden_op` writes the same files directly (a page cannot loop over a kit).
- **Acts** are derived: a skill is available if stored OR implied by a held item (`items.pdl grants_skill=`), the skill's `needs_item` is held, and its rule precondition holds (plant: seed and a free plot; water: dry growing plot and no rain today; collect: ripe plot; eat: hunger >= `eat_min_hunger` and food; trade: a mutual swap exists; talk: a partner; build: hoe and grain/seed for the hut; study: an unlockable node; rest: always). `acts <who>` lists them, `menu <who>` writes `acts_menu.pdl` METHOD rows.
- **Day Tick** (`eden_day_tick`): day+1, seeded weather (`rain_pct`), then per participant: hunger rises (HP loss at `starving_at`, floor 1), growing plots grow when watered (rain or the water act), then the **agent seat**: critical-need gate (hungry and food: eat), else one seeded weighted pick (`weights.pdl`) among the available acts. Dice = integer hash of (seed, day, participant ordinal, salt): no clock, no rand().
- **Ledgers**: `eden_history.txt` (DAY WEATHER NEED GROW RIPE ACT ITEM UNLOCK JOIN RETIRE SETUP RESET), `participants.txt` (JOIN / RETIRE tombstone), `grants.txt`, and the control ledger (SAVE/LOAD/START/PAUSE/RESET, outside the game tree so a Load does not rewind it). No wall time inside the tree, so runs compare byte for byte (`eden_op digest`).
- **Save/Load**: `game_snapshot_op` over the game tree (conductor + participants) with the clock state, `reminders.pdl` and `schedule_ledger.txt` as `--extra`; Load = `restore --apply --prune --extra-dir` (participants added after the save are pruned). A slot cannot be overwritten (the op refuses an existing id).

## Install note (by hand, in a scratch house first)
1. `gcc -std=gnu11 -Wall -O2 -o ops/+x/eden_op.+x ops/eden_op.c` (the `+x` programs are git-ignored); the pages find the op at `<conductor>/ops/+x/eden_op.+x`, so copy it there (a pal `exec` cannot name the house root). Also needed: digipet `event_page_op.+x`, `lc_clock.+x`, `game_snapshot_op.+x`.
2. layout: `<house>/game/conductor` (copy of `conductor/`), `<house>/common_events/eden_day_tick` (copy), `lc.+x` and `gso` beside `game/` as `wiring.pdl` says (edit `wiring.pdl` for any other layout).
3. run the row Setup (installs the clock `g1` and the schedule row `common:eden_day_tick`, `repeat=every:1day`), then advance: `lc_clock <house> cmd g1 advance 20d`, `lc_clock <house> step 0` (or `daemon`); `LC_CLOCK_EVENT_RUNNER=<house>/&.widgits/digipet/ops/+x/event_page_op.+x EVENT_PAGE_TRIGGER=parallel EVENT_PAGE_PRISC=<prisc>`.
4. Toys: nothing to do; `toy.pdl` is picked up by the scan (not verified in a GUI). A desk button would need a desk row for the conductor folder (not done).

## Gaps found (what the house cannot express yet) and how v0 sidesteps them
- **toy.pdl format**: the taskbar's `read_key_value` skips every line that starts with `META`, so the spec's `META|title` rows are never read; the real toys use `SECTION | title | X`. Used `SECTION`. The scan also only looks at direct children of the roots, so `toy.pdl` is in `eden/`, and `launch` is `conductor/button.sh` (run as `sh button.sh run`).
- **no variable compare / arithmetic / random / row loop in pages**: every such rule is a verb of `eden_op` (same answer as ring-board's tiny ops, but one op with many verbs because the rules read each other's state).
- **a page cannot learn the house root or take arguments**: paths live in `conductor/wiring.pdl` (relative, so two scratch runs have identical bytes); Save/Load slots and Add/Remove are separate triggers (`save1`...) and a SPARE list.
- **exec exit status is hidden and `event_page_op` exits 0 whenever a page ran**: a refused verb (Start before Setup, Save onto an existing slot) is only visible in `op_errors.txt`.
- **one target per house for a common event**: `common:eden_day_tick` resolves to `<house>/common_events/eden_day_tick` with one `target.pdl`, so two Eden games on one house cannot share the event; the harness uses a second scratch house for the peer game.
- **game_snapshot_op**: refuses to overwrite a slot id; a prune leaves empty directories (the digest ignores them).
- the registry's `change_items` etc. are PAL templates bound to `{STATE_DIR}`; not executed here (a block `eden_verb` was appended to the registry for the op).
- `needs_place` is a label (no map in v0: every place is present), `cost_ticks` is informational (one act per day), `event=` names the act, which is a verb, not a separate page.

Harness: `&.widgits/_shared-lib/harness/eden_conductor.pal` (`cases/eden_conductor.pdl`).
