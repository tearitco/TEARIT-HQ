# Handoff to Grok, 2026-11-10: back to df / Minecraft-style enhancements, through EVENTS, with META-HARNESSES that create mechanics and visuals for reuse; and the Centroid clock tower (test and live)

From claude (Sonnet 5.5) at the owner's request, 2026-11-10. This is a SECOND handoff: it continues `16.game/GROK-HANDOFF-FINAL-2026-11-05-ROADMAP-LORE-AND-QA.md` (read that one's sections 0 to 2 first; this file does not repeat them). Labels everywhere: **READ** (I opened the file today), **SAID** (the owner told an agent), **DOC** (my reasoning), **NOT READ** (I did not open it; check before you rely on it).

## 0. The rules, short (AGENTS.md at the repo root is the authority; read it)

- Branch: your branch is `grok`. As of 2026-11-10 `main`, `grok` and `claude` pointed at the same commit (`9e1da9b8b`; `claude` has moved on since). Commit only to `grok`, with `&.widgits/_shared-lib/ops/+x/commit_scoped.+x <repo> --branch grok --message-file <f> -- <your paths>` (explicit pathspec; it refuses `-A`, no paths and `xyzfs/users`). Never merge, push, stash, reset, checkout or force-delete; the owner decides. The owner has distrusted earlier `grok` commits, so keep commits small and evidence-first (SAID, earlier sessions).
- Never touch `xyzfs/users` (private user data). No Python and no new `.sh`: tools are C ops, pal, case `.pdl`, data. Heavy work only through `cpu_gate` (one job at a time, niced); the machine is weak and the owner runs a browser. **A CPU spike is OUR finding, never "the browser"** (2026-11-10: two parallel agents drove the load to 5.9; one heavy lane at a time).
- Scratch house under `/tmp` for every test; **never write into or drive the owner's live editor or windows**. Never report a number you did not observe; every assertion needs a negative control (a mutant must fail); open every PNG and describe it.
- A "not found" in a search is not a limit (the owner caught `DAY 28` saying no game could receive arrow keys: they arrive in `state/interact_relay.txt`; read the core's header comment before writing an absence into a doc).
- Documentation goes in the ONE doc the AGENTS.md route table points to; XO (`/home/no/Desktop/github/work/XO/`) takes docs and media only, items named `<n>.<name>`.

## 1. Where the previous handoff stands (READ)

Your first-task list from 2026-11-05 (zoo recon, a real True Swords Clash run, a CJK render proof, an Elo harness, a zoo generator) has **no report in the repo that I found**; nothing here changes it. `13.agent-coms/GROK/QUESTIONS-FOR-CLAUDE.md` has **one question I never answered** (D22: should signed receipts for verified training rows live in the pal-chain subchain, or in a plain signed ledger file first?). **My recommendation (DOC, the owner decides):** a plain signed append-only ledger file first (one authority, replayable, no chain dependency; the chain can anchor its hash later), the same shape as the game ledgers below.

## 2. What exists now that did not on 2026-11-05 (READ unless marked)

| New thing | Where | Why it matters to you |
|---|---|---|
| **Game foundation generator** `game_new_op` | `&.widgits/_shared-lib/harness/ops/game_new_op.c`, templates `harness/templates/game/{ledger,realtime,editor}/`, shared engine `&.widgits/_shared-lib/ops/game_core.c`, guide `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`, AGENTS.md route row "Make a new game" | **This is the pattern to copy for meta-harnesses** (section 4): args in, a working app with its own case out. Case `game_new` (it was 225 checks; the Chutes-page agent is repairing 5 pinned numbers: check the current verdict). |
| Apps built from it | `@.apps/chutes`, `snake`, `teario-make`, `teario-play` | working examples of a data-driven game; cases `chutes` 175, `snake` 180, `teario_make` 64, `teario_play` 65 |
| **Real-time driver** (30 Hz, in-process, about 0.9% of a core) | `game_core.c`, doc `16.game/REALTIME-SPIKES-HELD-KEYS-TICK-RATE-PICTURE-RATE-2026-11-09.md` | a df/Minecraft-style world that ticks needs this; arrow keys reach a driver via `state/interact_relay.txt` codes 1000-1003 |
| **Data Studio** `data-hq` | `&.hq-apps/data-hq/` (`data_op`, `chart_op`, `data_hq_manager`), plan `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md`, db menu row 4 | measure any run: tables and charts from a ledger by replay; cases `data_hq_p0` 148, `p1` 167, `p2` 87 |
| Class narration tool | `harness/ops/class_audio_op.c` (voices as data in `harness/voices/class_voices.pdl`) | turns a DAY/NIGHT class text into an mp3 on debil; no new Python |
| Plans | Chutes and Ladders + Snake (`16.game/CHUTES-LADDERS-AND-SNAKE-GAME-PLANS-2026-11-09.md`), Teario-Make/Play (`16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md`) | their EVENT tables list which house triggers exist and which are missing; decisions D58 to D65 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` |
| How a human plays each game | `16.game/HOW-TO-PLAY-THE-GAMES-AS-A-HUMAN-2026-11-10.md` | what is playable and the menus (Save/Load slot 1 saves the editor's world, not a game) |

## 3. YOUR MISSION (owner, 2026-11-10): back to df / Minecraft-style enhancements

The owner said (SAID, relayed): go back to building **df-style and Minecraft-style enhancements**; make sure they are built **with events**; create **meta harnesses for the creation of game mechanics and visuals so they are reused automatically** (document and concept bank, and so on).

**What already exists (READ):** `@.apps/df-hq` (verbs `dig`, `dig_area`, `undig`, `place`, `resolve`, `clock`, `day_tick`, event pages under `@.apps/df-hq/events/` such as `dig`, `dig_area`, `buy`, `control`, `day_tick`, `goto`; rules table `16.game/RULES-df-hq.md`, hook `GE_RULE_HOOK` in `ops/df_hq.c`, ledger `state/events.txt`, placement ledger `state/house.txt`; playthrough `play_df_hq`, 36 checks); `@.apps/mcl-hq` (verbs `mine`, `craft`, `place`; `play_mcl_hq`, 43 checks; `16.game/RULES-mcl-hq.md`); needs (`needs_tunables.pdl`, harness `digipet_needs`: hunger rises a step a day). **NOT READ by me today:** the df-hq and mcl-hq C sources, `GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`, `HANDOFF-2026-10-28-MCL-BOOK-TO-GROK.md`, `DF-HQ-DIG-HOWTO.md`, the concept-bank `ops/`: read them before designing.

**3a. Events first (rule).** Every new mechanic is an **event page** (`events/<name>/event_pkg/pages/<page>/` with `condition.pdl` and `event.ir.pdl`) or a verb row in the app's `verbs.pdl`, plus the app's `time.pdl`/clock rows for timed effects; world rules (death, respawn, destroyed, collision) are existing world-op pages. A mechanic that needs C is added as a generic option in a shared engine with a harness case, never as per-game code. Known limits of the event system for FAST games (READ in the Teario plan section 2): each page run is a shell launch and the clock polls at about 300 ms; fine for building and farming, not for per-tick physics.

**3b. META-HARNESSES (the new idea).** Copy the shape of `game_new_op` (READ the guide): a small C op that takes **arguments** and generates a working, tested thing from **template data**, never hand-writing per-item C. Two generators to build, in this order:
1. **`mech_new_op` (mechanics):** args `<app> <mechanic> --kind dig|mine|craft|place|plant|sleep|build-job ...` produces the verb row, the event page skeleton (`condition.pdl`, `event.ir.pdl`), the tuning/rule rows (`RULES-*.md` row), a phase-table case under `harness/play/phases/` that can fail, and the doc row; refuses an existing name, a bad kind, an unreplaced placeholder (the same refusal tests as `game_new`).
2. **`vis_new_op` (visuals):** args `<app> <asset kind: block|bed|plant|door|figure|tile> --palette <id>` produces the sheet/atlas row, the `VIEW`/skin row and a render check (a window dump with pixel probes, opened by a human or a checked PNG); it reuses the palette studio (decisions D33 to D35) and the label atlas code; **no copied third-party art** (SAID: Tear-it art is new levels only, never replace existing art).
3. Both register what they create in the **concept bank** so the next game reuses it: `&.widgits/concept-bank` has `AUTO-PROMOTION-RULE.md` (READ: **a design with a stubbed rule; no promotion-ledger scoring exists yet**, do not fake numbers), `banks.pdl`, `common_events/`, `proposals/`. DOC: a generated mechanic appends one row to a concept-bank proposal (name, kind, files, the case that proves it); promotion to a shared common event waits for real replay evidence, as the rule says.
4. Every generator documents itself: a section in ONE guide (add a row to the AGENTS.md route table, as the generator agent did), a decision-log row, a compendium entry (what a human must SEE), and a refreshed XO copy.
Design-choice test applies (AGENTS.md): before choosing, answer one authority per fact, modular, scalable, long term, house shapes, cost; add a row to the decision log (next free id: read the tail).

**Acceptance for 3b (each ends with evidence):** generate a new mechanic into a scratch house from args only, run its generated case green, mutate it and watch the case go red, open the generated visual PNG; the owner sees one command in the doc that does it.

## 4. The Centroid clock tower: centroid-test and centroid-live (owner, SAID 2026-11-10)

**Did the handoff mention it? No.** My 2026-11-05 handoff, `ONBOARD-GROK.md` and the Q&A files contain no mention of the clock tower or df-style pets (READ: 0 hits). The plan is in `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`, written the day AFTER that handoff and never pointed to from any Grok-facing doc. That is my omission; this section fixes it.

**What the owner says now (SAID 2026-11-10):**
- **centroid-test** = build the Centroid **clock tower** using **harnesses** (a scripted, scratch-house, can-fail run that places the tower from a blueprint through the game verbs).
- **centroid-live** = the **df-style pets build it** using **df-style instructions** (designations and jobs) **or intrinsic motives** (FSM / RL, the needs-driven kind), watched on the editor page `live`.

**What the 2026-11-06 doc says instead (READ):** `centroid-test` starts with the University already built; pets only meet food and sleep, plant and make beds; `centroid-live` is the same campus on page `live` with a skin toggle; **a manager that is shown the blueprint and breaks it into jobs for employees is listed as "later, not this test"**; and the doc says plainly "the tower is not in either run yet". Harness `centroid_test.pal` (5 checks) and `centroid_book.pal` (11) pass on a one-room, two-floor court (`@.apps/centroid-hq/data/court.pdl`, 156 cells), not the tower. **The two plans disagree; the owner's 2026-11-10 words win.** Your first act is to reconcile the doc (see 5.1).
**The target look (READ `CENTROID-VISUAL-BOOK-2026-11-06.md`):** the owner's picture (grey-stone campus on a moat, a clock tower, colonnades, roof gardens); the first chunk is a gate stair, a front court, **one tower**, one roof over one room with a door, and one bridge to one small island; three floating islands in one voxel volume, two skins (RPG Maker sheet, generated sheet).

## 5. Suggested first tasks, in order (small, verifiable, each ends with evidence)

1. **Reconcile the Centroid doc (docs only):** edit `CENTROID-TEST-AND-LIVE-2026-11-06.md` to the owner's version (test = harness-built tower, live = df-style or motive-driven pets build it; the old "prebuilt campus, pets eat and sleep" run becomes `centroid-needs` or stays as a later check; keep the manager/employees idea as the live run's job layer). Date the edit; add the "I was told 2026-11-10" label. Add this handoff to `16.game/00-INDEX.md` and the Centroid docs' headers so no agent misses it again.
2. **Tower blueprint as DATA:** a chunk file for the clock tower (from the visual book's description: stairs, court, one tower, a clock face as a distinct block kind, one roof and room, a door) next to `court.pdl`; loading twice gives the same columns; a harness checks the cell count and that a hole or a wrong block fails.
3. **centroid-test (harness):** a phase table `play/phases/centroid_tower.phases.pdl` in which a crew with empty bags builds the tower from the blueprint using the existing verbs (`dig`/`mine`, `craft`, `haul`, `place`); it passes only if the placed columns equal the blueprint and fails on any difference (the doc's own pass rule); a second attempt edits the phase table, never the pets by hand. Run in a scratch house under `cpu_gate`; no window.
4. **Needs and motives:** hunger and sleep on for these pets only (the doc's rules 2 to 5: plant, bed, the day), as FSM states first; the RL variant (a learned policy) is trained **off this machine** (debil or the Mac) from replay data with `data-hq` tables; no training on the weak machine.
5. **centroid-live (later, after 3 passes):** the same crew and blueprint on page `live`: **df-style instructions** = designations and a job list (manager breaks the blueprint into dig/haul/craft/place jobs) **or** intrinsic-motive pets choose jobs by need; a visible toggle between the two modes; the window goes through `cpu_gate`; use `relay_drive` gentle mode in a scratch house, never the owner's live window.
6. **Then 3b:** build `mech_new_op` from what steps 2 to 4 repeated by hand (a verb row, an event page, a phase case, a doc row), and `vis_new_op` from the block/sheet work of step 2. Build generators from two real examples, not from a guess.

Do **not** start vehicles, glyph learning, stairs-and-courts expansions or a new game before step 3 passes and the owner picks an order.

## 6. Traps I hit this week (so you do not)

- Pinned hashes (ledger hash, file counts) are legitimate when a template changes, but moving one needs a stated cause per number and a negative test that a corrupted ledger is still refused; never paste a new hash to turn a case green.
- The case results file APPENDS across runs: trust the `VERDICT` line (`harness/results/<case>.txt.verdict.txt`), not a grep of the whole file. Cases run with the scratch VM: `gcc -O2 -w -o /tmp/prisc system/prisc+x.c -lm` (via `cpu_gate`), then `cd harness && /tmp/prisc <name>.pal`.
- A shared file edited by two agents at once: commit your one hunk at once after `git diff -- <file>` shows only it; `git commit -- path` takes the WHOLE file.
- The 3D engine exits by design after about 270 s of no input; a frozen picture is not a crash.
- Windows have no X names: find them by size; never `pkill -f` a pattern (it can match your own shell).
- Two parallel heavy agents spiked the load to 5.9: one heavy lane at a time.

## 7. Open decisions for the owner

1. The Centroid doc reconciliation (section 5.1): confirm "test = harness-built tower; live = df-style/motive pets build it".
2. Which comes first after the tower passes: the generators (3b) or the live run (5.5)?
3. FSM only for motives at first, or also an RL policy trained off-machine?
4. D22 receipts: plain signed ledger first (my recommendation) or the pal-chain subchain?
5. Whether `grok` should work in its own git worktree (the repo's own AGENTS.md says that is the real fix for shared checkouts).

## 8. External references

Repo (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/`): `@.apps/df-hq/`, `@.apps/mcl-hq/`, `@.apps/centroid/`, `@.apps/centroid-hq/data/court.pdl`, `&.widgits/concept-bank/`, `&.widgits/_shared-lib/harness/ops/game_new_op.c`, `&.widgits/_shared-lib/harness/play/phases/`, `&.widgits/_shared-lib/ops/+x/commit_scoped.+x`, `cpu_gate.+x`. BOOK (`x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/`): `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`, `16.game/CENTROID-VISUAL-BOOK-2026-11-06.md`, `16.game/CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md`, `16.game/RULES-df-hq.md`, `16.game/RULES-mcl-hq.md`, `16.game/GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`, `16.game/HANDOFF-2026-10-28-MCL-BOOK-TO-GROK.md`, `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`, `16.game/HOW-TO-PLAY-THE-GAMES-AS-A-HUMAN-2026-11-10.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`, `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md`, `13.agent-coms/GROK/ONBOARD-GROK.md`.
