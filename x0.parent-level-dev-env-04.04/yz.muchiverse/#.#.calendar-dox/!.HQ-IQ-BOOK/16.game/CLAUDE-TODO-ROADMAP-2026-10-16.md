# Claude's to-do and roadmap (2026-10-16)

What claude (Sonnet 5.5, branch `claude`) actually intends to do, in order, with how each item is proven. Nothing here is built unless it says so. No dates: the owner said slow is fine, and I have no measured speed to promise one with. Status words as in `GAMES-ROADMAP.md`: `planned`, `building`, `proven`, `blocked`.

Lanes (owner, `GAMES-DECISIONS-2026-10-13.md` section 2): **grok** owns df-hq and cdda rules (crafting, vehicles, map generation, NPCs, enemies, stats, inventory, menus). **claude** owns the infrastructure under them, the integration notes and proofs, and the gta design with the owner. I do not write df or cdda rules.

Standing rules for every item: fresh build, fresh run, observed evidence, a negative control, say what was not tested; commit only my own paths; push only when the owner says.

## 0. Open now (small, finish first)

| # | Item | Proof | Status |
|---|---|---|---|
| N1 | Startup loading bar: see it on the real desktop at the next start (off-screen drawing, whole-width bar, learned compile time). The shake itself was not observable by sampling | Owner looks at one real start; if still jittery, fix further | built, pushed `982c48a20`, not seen on the real desktop |
| N2 | Grok's three small fixes (bright-skin dark writing, `PCHQ_PKG` fallback) taken into `claude` | Owner sees dark writing on a bright skin in a real board | committed and pushed `df00659c7`, not seen on screen |
| N3 | Board idle cost (grok's `back-to-sonnet-oc16.md`): gate the projector build, `df_hq` `pet_load`, and the 3D daemon file opens on a **marker or size**, not mtime; renderer loop last and only after measuring `hq_idle_tick` vs the 5 s redraw. Never remove the 30 ms floor; never gate input polling on the canvas file | Before/after `/proc` jiffies per process (3 s window) with a control (gate disabled) | planned; awaiting owner's go |

## 1. Games spine (mine)

Order follows `GAME-MECHANICS-SPRINT.md` "Order to build"; spine ids from `GAMES-ROADMAP.md`.

| # | Item | Proof | Status |
|---|---|---|---|
| S1 | **Event line and resolver** shared by all three games: `EVENT | time | actor | verb | target | at | result`, first verbs `give_item`, `take`, `walk`, `dig`, `build`, `craft`. Appends to a marker ledger (size growth); a resolver op reads the tail from a cursor, idempotent. Same shape as the blueprintz/play_event path, reuse it before inventing | One scripted event list resolves to the expected state; replaying it gives identical state; a malformed row is rejected, not skipped silently | planned; first thing to build |
| S2 | **M1 z-cell**: already proven on paper + fixture; wire a reader into the resolver so `at` is world,z,x,y | A 3-floor fixture loads; per-z counts match; bad row fails | paper proven; no game reads it |
| S3 | **M3 decision ledger**: one marker-ledger line per actor decision; daily review file per game | One live move gives exactly one line; the review file lists it | planned |
| S4 | **M2 concept-bank rows** for scale, gravity, drag, thrust | `concept_edit_validate` accepts a clone, rejects a malformed one; physics vectors unchanged | planned (later; not needed for the three sprint games) |
| S5 | **Idle-cost rule for game daemons**: any new game daemon follows the `house_wait` floor and marker-gated work from the start (lesson of N3) | CPU read of each new daemon idle, recorded | rule only |

## 2. df-hq (grok writes rules; I provide the spine and the proof)

| # | Item | Proof | Status |
|---|---|---|---|
| D1 | Designation becomes a job row; one dwarf walks to the stand tile and digs (sprint step 2). **Check with grok before starting**: the roadmap lists D1 as grok's | Map file shows the dug cell | planned |
| D2 | Haul to a stockpile; craft job at a workshop waiting on inputs; eat/drink need rows | Item moves; the order completes only when inputs exist | planned |
| D3 | One hostile wave (`spawn_actor` with a hostile weight) | One wave resolves; ledger lines recorded | planned |
| D4 | Scripted df playthrough (D5 in the roadmap) | Script runs start to visible result; owner watches | planned |

## 3. cdda-hq

| # | Item | Proof | Status |
|---|---|---|---|
| C1 | One house, a basement stair (needs S2), a recipe list, one wound, one monster turn (sprint step 3). Craft uses the SAME resolver as df workshops | Walk onto the stair: state z changes; craft: inputs removed, output appended | planned |
| C2 | Vehicle as a part-cell grid, `install_part`, seat engage | Seat engaged in state; parts move together | planned, after C1 |
| C3 | Scripted cdda playthrough | As D4 | planned |

## 4. mcl-hq (Minecraft-style block world)

**Not in the decisions doc or `GAMES-ROADMAP.md`; it appears only in grok's sprint page. Needs an owner decision before I touch it:** who builds it, and is it in scope with df and cdda? Until answered it is `blocked`. If approved, it reuses S1 (`dig`, `build`, `take`, `craft`):

| # | Item | Proof | Status |
|---|---|---|---|
| M-1 | Owner decision: lane, scope, art (Mineclonia textures stay the art; no Luanti server) | Written into `GAMES-DECISIONS` | blocked on owner |
| M-2 | Break/place a block; sand/gravel gravity; chest as item rows | Block removed and drop appears; sand falls | planned after M-1 |
| M-3 | 2x2 and 3x3 shape-match craft; furnace with fuel and time | Plank, table, pick crafted from a log | planned |
| M-4 | Hunger/health; one mob rule on dark cells | Health ticks at zero hunger; mob spawns only on dark cells | planned |

## 5. gta-hq (design with the owner; grok's code stays as is)

G0 design page first (island size menu, detail-level op, word-bank to image pipeline). No code until the owner answers its open questions. `planned`.

## 6. Other tracks that are still mine (not games)

| # | Item | Status |
|---|---|---|
| A1 | AI backlog survey (oldest/most central first, per memory: read ROBOT-CHAT-BLUEPRINT.md and the latest 2do.md before chat/Concept-Bank work). First bridge step: `auto` runs a blueprintz event under pause, steps it, judges the trace. Waiting on the owner: has heard no audio, judged no picture, microphone never opened (owner-only) | planned |
| B1 | Blueprintz leftovers: read `bp_sources.pdl` from the loaded book; window smoke test; scroll region for the flags panel; parallel events loop; m8 `objects.pdl` rows still point at NNEST-11.17; 346 doom events have no event folder; real-window check of doom touch after pc-hq relaunch; Player-menu row needs a taskbar rebuild by the owner | planned |
| L1 | LyfeDesk rename layer 2 (design docs) and layer 3 (deep rename of dirs, files, `livedesk:` prefix, `KTB_LIVEDESK_*`, WM_CLASS, user-data paths). Needs a scratch worktree, backup, and every agent paused. Trademark search is the owner's job | planned; bug bounty entry `04-bugs/bug_bounty.md` |
| H1 | Machine health: disk was 99% full and the machine hard-crashed twice on 2026-10-15; cause not investigated. Keep scratch out of `/tmp` | planned; look before the next heavy build |
| H2 | Foreign uncommitted work in this checkout (another agent's; not mine, never commit): `_shared-lib/.gitignore`, `tsots/map*/ev/`, two doom `map.txt`, `rezip-house.sh`. Two grok commits (`6a8762cfd`, `90a407fad`) are on grok's branch, not mine to push | watch only |

## 7. Order I will work in

1. N3 first three gates (projector, `df_hq`, 3D daemon) once the owner says go; measure before/after.
2. S1 resolver + a scripted event list (also the base every game needs).
3. Ask the owner the mcl-hq question (M-1) and the "who starts D1" question to grok.
4. S2/S3, then C1 (cdda house + craft) and D1/D2 with grok, one proof each.
5. A1 and B1 in between, whenever a game step is waiting on the owner or grok.
6. L1 only when the owner schedules a quiet window.
