# TEARIT-HQ roadmap: the front door

**Written 2026-10-26.** One page for three readers: customers, investors, developers. It is a map, not a promise: it says what is proven, what is being built, and what is only planned, and it points to the document that holds the evidence. If a row has no source, it does not belong here.

**Checklist view (2026-11-04):** every feature area as `[x]` / `[~]` / `[ ]` with spec links is `08-roadmap/GRAND-ROADMAP-CHECKLIST-2026-11-04.md`.

**How to read the status words** (the house rule from `16.game/GAMES-ROADMAP.md`): `proven` = it was run and the result recorded; `building` = work is under way, not finished; `designing` = a written design exists, no code; `planned` = an intention only. A row is never `proven` without a place to check it.

**Not on this page, on purpose:** the house's own prices, traction, revenue, legal structure, headcount. Those are founder facts and live as drafts in `14.biz/` (see `14.biz/00-INDEX.md`). Nothing here was invented to fill that gap. The only market numbers are third-party comparables in the investor view, each with its source and date.

Where to go for more: this page → `00-compact/` and `README.md` (book index) → the chapter named in each row.

---

## 1. What the house is

In the founder's draft wording (`14.biz/ELEVATOR-PITCH.md`, DRAFT): a plain-text, file-driven desktop and app platform you can read, script and build games on, where every window, menu and piece of state is a file, and an AI agent is a first-class user. The architecture in five lines is in `HQ-BRIEF.md` section 2: file-based state, one shared renderer with no per-app code, one input path shared by people and agents, one small manager process per app, Linux first.

## 2. Where things stand today (proven, with evidence)

| Area | What works today | Evidence | Date |
|---|---|---|---|
| **Systems you can read** | Death, respawn, mining, air and collision in the rpg-space game are small event pages (30 steps) that a blueprint viewer shows and an editor changes. They replace opaque code and were compared with the old code on the same starting files. | `16.game/RPG-SPACE-BLUEPRINT-SYSTEMS.md` sections 5b to 5e; every comparison identical, and each was re-run with a deliberately broken step to prove it can fail | 2026-10-25 |
| **A game that tests itself** | A whole story of the game (open the blueprint window, die, respawn, mine, lose air, collide) replays in a scratch copy of the house: 34 checks, all pass, about 15 s. A changed expected value fails exactly one check. | `_shared-lib/harness/play_rpg_space_blueprint.pal`; method in `06-testing/GAME-PLAYTHROUGH-HARNESSES.md` | 2026-10-25 |
| **Tests as a habit** | Dozens of pal harnesses with hundreds of checks each (for example 707 for a race game, 321 for a board game, 118 for snapshot and restore). | table in `_shared-lib/harness/README.md` | 2026-10 |
| **3D board, ship over a planet** | A ship that is an entity flies over a planet that is an entity; mining leaves spinning remnants; oxygen and temperature per room; ship and land collide; asteroids and a small second ship exist as placed objects (they do not move, trade or take boarders yet; on the planet map they are seeded, minable and solid in the replay and are visible on the live board as flat coloured tiles beside the ship, 2026-10-26; true 3D drawing needs the engine's entity pieces, which still do not draw). | `16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7e to 7g, `RPG-SPACE-ENGINE-PIECES.md`, `NIGHT_54`/`NIGHT_55` | 2026-10-25 |
| **Worlds from a seed** | Planet pages are generated from a seed and a ledger; the same inputs give identical bytes. | harness cases `dust_ladder` (64 checks), `dust_seed` (79), `dust_gen` (115), `dust_zoom` (79) | 2026-10 |
| **Light on the machine** | Idle process starts of the 3D board fell from about 98 to about 19 per second (load average 14 to 1.6) after the loops were gated on file sizes; leaked loops and test daemons now end by themselves. | `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7g; harness cases `loop_exit` (11 checks) and `harness_cleanup_*` | 2026-10-25/26 |
| **Browser lane** | "JS apps actually run" (G1) and G2.1/G2.2 are closed at 100 percent and wired into the browser test run. | `08-roadmap/2026-10-14-G1-SPRINT-VERDICT.md`, `2026-10-17-G2-VERDICT-ADDENDUM.md` | 2026-10-17 |
| **AI, the part that exists** | The scoring side: a scratch playthrough, a result ledger, and per-step weights are built; the decision ledger is built in three games; an approval room for outside agents is real. | `17.ai/HARNESS-AS-GAME-BRAIN.md` section 3, `17.ai/AI.md` | 2026-10-17/18 |
| **A classic board game inside the editor** | Teropoly (internal name `deskopoly`): full classic Monopoly rules through M6 (rent, doubles and Jail, Luck and Fund cards, buildings, mortgages, bankruptcy), pets play it, a human can take a seat, played from the editor's Play button with a players bar, a footer and extruded 3D players on the board; ledger is the only authority (replay reproduces it, a hand-edited ledger is refused). | harness cases `deskopoly` 105, `dsk_classic` 79, `dsk_play` 48, `dsk_jail` 49, `dsk_cards` 42, `dsk_build` 59, `dsk_page` 42, `dsk_driver` 70, `live_dsk_seat` 23 (real window); handoff `16.game/TEROPOLY-ONBOARDING-HARNESSES-DRIVING-AND-CPU-LESSONS-2026-11-08.md` | 2026-11-08 |

## 3. Where it is going, by track

### 3.1 3D and worlds
| Next | Status | Source |
|---|---|---|
| Finish converting the remaining game behaviour (mining and building verbs, pickup, hunger tick, hp-zero detection) into pages; keep per-tick scans in one small resident process | `building` | tech report 2026-10-25, section 4 stage A (`XO/30.rpg-dust-work/oct-25/`) |
| rpg-dust gets the ticks that play its life pages, then mining; the same test script must pass on both games | `building` (data and three pages copied; no ticks yet) | `RPG-SPACE-BLUEPRINT-SYSTEMS.md` 5e |
| Ships that move, trade and fight as entities; entities in a ship have a position relative to it; draw the vehicle pieces that load but are not drawn | `designing` | collision design 4e to 4g; `RPG-SPACE-ENGINE-PIECES.md` |
| "Micro" scenes inside a ship's voxels; deep simulation (for example respiration) switched on per entity only where it earns its cost, cheap default elsewhere | `planned` | owner direction 2026-10-25, collision design 4c |
| "Semi destroyed" block animations (frames chosen by mining progress over hardness) and sprites that show more blood the more hurt a character is, with a setting for how graphic it is | `planned`, decisions made 2026-10-26: 3 stages, numbers-based sound and animation, overlay layer, graphic level per game, one hp rule for every surface that shows a character | `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7h |
| Shared z, vehicle and physics spine for the cdda, df and gta style games (z-cell format, physics concept rows, decision ledger) | `planned` (every row NOT BUILT) | `16.game/GAMES-ROADMAP.md` |

### 3.2 AI integration
| Next | Status | Source |
|---|---|---|
| An actor (a pet, the `auto` conductor, a person) proposes a phase table; the harness judges it; weights record which steps work | `designing`: the judge is built, **no caller exists today** | `HARNESS-AS-GAME-BRAIN.md` section 4 |
| Promotion: a high-weight step becomes a concept-bank entry, a training example or a pet's default plan | `designing` | same, point 3 |
| Describe, score, store loop: a model describes the world, a deterministic harness scores it, a person can reject the store | `designing` | `17.ai/AI.md` |
| Pets drive the same input path a human uses; learn with the house's own models and harnesses; may change their own harness but never the grader | `designing` (owner direction) | `18`/`17` docs; vision notes |
| Free models and a local model server only; no paid credits | standing constraint | owner rule |

### 3.3 Platform and tools
| Next | Status | Source |
|---|---|---|
| Blueprint viewer: show calls between pages made through the new commands, validate values before an edit, scale to many games | `planned` | tech report section 3 |
| Replace the interim shell test scripts with pal harnesses; one "all gates" run that includes a spawn-rate check | `planned` | tech report section 4 stage B |
| Browser to fully Chrome-replaceable (owner directive 2026-10-20) | `building` | `08-roadmap/2026-10-20-CHROME-REPLACEMENT-SIZING.md` |
| Windows and macOS: macOS runs through XQuartz today, Windows is thin shims; the main gap is the shared renderer | `planned` | `08-roadmap/OPEN-ITEMS.md` item 2, `CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md` |
| Cross-machine networking | `designing` (a plan; same-machine only today) | plan notes |
| Plugs, harnz and tests: three kinds, three windows opened from the top-bar Tools cell (plug on/off, harnz click, test run), plug header and typed rows validated, local examples on the office page | `proven` in scratch and on the live desktop through relays (cases `plugz_hq` 64, `plug_header` 26, `plug_ref` 66, `office_tools` 12; no human click yet); `building`: triggers (a plug fires a harnz and back), the multi-plug loader, the maker | `16.game/PLUGS-AND-HARNZ-SPEC-2026-11-14.md` sections 14 to 16, D76 and D77 |

### 3.4 Games (Teropoly first)
| Next | Status | Source |
|---|---|---|
| M7 auctions DONE 2026-11-08 (case dsk_auction 57; limit: bankruptcy tiles are not auctioned), M9 trading ENGINE slice DONE 2026-11-08 (case dsk_trade 113; the human trade screen is built and seen working live 2026-11-08 (4 panes, nav click); NOT built: counter/amend buttons, chat commands, pet proposals), M8 debt queue (owner requirement 2026-11-08: trading never blocks play, a per-turn offer allowance, editable terms: spec section 7.8) | `partly built` (engine only) (tiles resolve as `todo-<kind>` until then) | `16.game/DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md` section 1 |
| M10 pet personalities (the pet policy is a stopgap) and the pets/ui/tick for every phase | `building` (pets and the editor UI run through `deskopoly_driver`; personalities not built) | same |
| M12 fuzz and a narrated captioned video of the game played in the editor | `planned` | handoff section 6; XO `31.Nov/nov-8/10.youtube-deskopoly-plays-in-the-editor` (predates the glyph fix and human seat) |
| **data-hq (Data Studio)**: a generic, dynamic, AI-integratable data studio under the `db` toolbar cell (any dataset: csv, tsv, `.pdl`, ledgers; dashboard that adapts to the data; notebooks with SQL and declarative cells; history, trends, training export; Teropoly is the first provider); phases P0 to P7 | `building` (P0 dataset layer BUILT 2026-11-09, case `data_hq_p0` 148; P1 charts + `autodash` + the picture spike BUILT 2026-11-09, case `data_hq_p1` 167, decision D64, no window yet; P2 the Data Studio window BUILT 2026-11-09 (db menu row 4 `data-hq`, Toys row, sidebar of sessions, dashboard and table modes; harness case `data_hq_p2` 87 checks + the driven script `&.hq-apps/data-hq/tests/data-hq-drive.pdl` 25 steps in a nested X server; decision D66; the LIVE desktop shows the menu row only after a taskbar rebuild and relaunch by the owner); P3 to P7 plan only) | `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md`, decisions D58 and D61, `&.hq-apps/data-hq/README.md`; owner priority 2026-11-09 |
| **Chutes and Ladders + Snake** on the Teropoly stack: new apps `@.apps/chutes` (luck, a Markov chain the data studio solves exactly; board numbers FROM MEMORY) and `@.apps/snake` (real time; ledger of inputs + consequence rows + checkpoints, no row per tick); shared helpers extracted by text-include at the second consumer; Chutes C0 to C8 first, Snake S0 to S11 second; which house events each uses and why (section 17 to 19) | `building` (2026-11-10: foundations built and tested, cases `chutes` 290 and `snake` 180; Chutes C0, C1, C3, the feed and exporter and now C5, the numbered serpentine board page with figures, seen in the real 2D and 3D renderers from a scratch copy, D67; NOT built: camera follow, walking animation, Snake page, AI seats, a live check in the owner's editor) | `16.game/CHUTES-LADDERS-AND-SNAKE-GAME-PLANS-2026-11-09.md`, decision D59; owner request 2026-11-09 |
| **Teario-Make/Play** (working name; a side-view platformer level editor + playable levels): level = `.pdl`, mechanics = event pages (in-process interpreter, 30 ticks a second), inputs-only ledger with exact replay, publish only after a verified clear; one shared core with a Make shell and a Play shell (two Toys windows for the prototype, joining into one), also inside rpg-edit; recursion (nested levels, depth 2) with a sanity test; data-hq exporter; milestones M0 to M15 | `building` (2026-11-09: the shared level format, Make (EDIT-rows ledger, level export with the fixture's sha256, publish refused without a verified clear) and Play (inputs-only run ledger, `verify_clear`) built and tested, cases `teario_make` 64 and `teario_play` 57; NO physics, interpreter, UI or Toys entries yet; D63) | `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md`, decision D60; owner request 2026-11-09 |
| Owner decisions that gate polish: 3D `event_scale`, day/night from ledger rounds, neutral tile and card names, setup screen showing the play desk | `OWNER` | handoff section 6 |

### 3.5 TEARITIZATION sprints (planned 2026-11-24)

A Civ-SNES shape on events, a human plus agents, a seeded map, 2D and 3D from one desk. `civ-rpgce` is not the base. A sprint is one proof (a harness that can fail, plus a picture). Nothing in this table is `proven`. The near clip and the four-sprint and eight-sprint notes are in `XO/31.Nov/nov-24/grok-24/`. Design source: `16.game/TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md`.

| Sprint | What you can open | Still absent |
|---|---|---|
| 1 | A 16×16 seed: water, land, one mineral mark, same bytes twice, 2D and 3D stills | Digging, units, cities |
| 4 | Keys walk a 32×32 map, one city on grass, one dig of that mineral, eat/sleep and a ration count as rows | Tech, a second seat, a continent |
| 8 | Two seats on about a 64-wide chunked map, fog, one agent move with no model, dig and place survive reload | Preset personalities, combat, a stored planet |
| 16 | A few biomes, two agent presets that pick different legal moves, a ledger checkpoint, several cities, dig and place stable | A campaign, city stamps, combat |
| 32 | A region you pan across in a camera window, water as a stock, one combat rule, one real-city stamp only after its parent cell exists | A whole country, laws, molecule zoom |
| 64 | Several units with jobs, parts and a fuel count on a unit, fog gold and research stable, a series of clips rather than one trailer | Moon or Mars, imported CDDA/DF/Minecraft content |
| 128 | A short campaign a person can finish: found, research, one conflict, end the turn, agents take seats. Geology is layers as rows | A planet file, a public "finished game" line |
| 512 | The far aim of the game: seeded chunks at region and play scale, many seats, jobs, dig and place, geology one rung finer (a material, not an atom). Real cities are stamps. Laws only if ordered | Being online for strangers, a stored Earth, a date |
| 1024 | The same game, reached from outside the house. See "Done and online" below. The sprint count does not finish the game by itself | A promise that 1024 proofs have been run |

**Done and online** is not sprint 1024. The game is done when a stranger can start a match, play a full session, and leave, and a second stranger can open the same chain and get the same result. Online means all of these are true at once:

- The rules they run are the published pages and the ledger, and a harness they can re-run still fails when one expected byte is wrong.
- The board, the 2D picture, and the web page are three views of that ledger. The web page is on the public site only after you say to publish it.
- A person joins from a browser or from LyfeDesk, takes a seat, and plays with keys. An empty seat fills from the chain, as the arena already does for pets, not from a hidden roll.
- Their edits (dig, found, build) are rows. Reloading does not invent a second world.
- The clip, the how-to, and the site text match what that stranger just did. No "playable" line before that session exists.

Sprint 1024 is only the stretch where that outside door is the proof. Sprint 512 can be a finished game on your machine and still be offline. The arena page on tearit-web is the small version of this door, and it stays unpublished until you decide.

## 4. The three views

### Customers: what you can do, and what is coming
- **Today:** use the desktop and its apps, play the games that exist (the rpg-space ship and planet board, the pet games, the RPG-Maker-style tools), and read how-tos in `10-user-docs/` (feature catalog, dig, craft and block how-tos).
- **Soon:** more of the game systems you can open and change yourself (an event page is plain text), more worlds from a seed, ships you can meet and trade with.
- **Later:** pets that play and learn on their own through the same buttons you press.
- **Be aware:** there is no packaged installer experience described as finished; the install journey is a plan (`07-install-and-ship/USER-JOURNEY-COMPLETION-GRAPH.md`, written 2026-09-02).

### Investors: what is built, who it is compared with, what is not claimed

**A. Built and checkable.** Section 2 above: readable and replayable game systems, a large test-harness habit, a 3D board with worlds from a seed, a light-footprint 3D loop, and a browser lane with closed milestones. Each row names the check that proves it. The differentiator, stated narrowly: state is plain files and one input path serves people and agents, so anything can be inspected, scripted and replayed; the harness is the evidence that this is practical.

**B. Comparable products and what the market pays for them.** The house touches three markets, so it has three comparable sets. All figures are **EV / revenue (or valuation / run-rate revenue)**, collected 2026-10-26.

| Comparable set | Why it is comparable | Company | Multiple | Inputs and date |
|---|---|---|---|---|
| **Game engines and world platforms** (the 3D board, worlds from a seed, the tile and sprite tools) | build-your-own-world platforms | Unity | **8.4x** LTM | EV $18B, LTM revenue $2.2B, as of 2026-10-25 |
| | | Roblox | **5.3x** LTM | EV $37B, LTM revenue $6.9B, as of 2026-10-25 |
| **Developer infrastructure** (the replay harness, blueprint viewer, plain-file observability) | tools that make software inspectable and testable | GitLab | **7.5x** LTM | EV $8.2B, LTM revenue $1.1B, as of 2026-10-25 |
| | | JFrog | **18.4x** LTM | EV $12B, LTM revenue $630M, as of 2026-10-25 |
| | | Datadog | **21.7x** LTM | EV $93B, LTM revenue $4.3B, as of 2026-10-25 |
| **AI-native developer and agent platforms** (agents that drive the same input path as a person) | private, priced on run-rate revenue and growth | Cognition | **about 53x** run-rate | $48B valuation (Series E, announced 2026-09-08) on a run-rate near $0.9B; the earlier $26B round (2026-05) on $492M was also about 53x |
| | | Lovable | **about 22x to 33x** | $13.3B valuation (Series C, 2026-08) on ARR reported between $400M and $600M depending on date |
| | | Replit | **about 17x to 34x** | $9B valuation (Series D, 2026-03) on ARR reported between about $265M (end 2025) and about $525M (2026-04) |

Reading it: the five public companies span **5.3x to 21.7x with a median of 8.4x**; the private AI-native platforms trade at **roughly 17x to 53x** of run-rate revenue. The spread says the market pays for scale, growth and margin, and pays most for AI-native growth.

**How to use this, and how not to.**
- **These multiples value companies with revenue. The house has no revenue figure in this book, and none is claimed**, so no multiple can be applied to it and no valuation is implied. A mechanical unit example, nothing more: each $1M of annual recurring revenue would correspond to about $5M to $22M of enterprise value at the public range, and about $17M to $53M at the private AI-native range. That is what the market pays for revenue, not an estimate of this house.
- A pre-revenue platform is judged on other things: the strength of the technical claim, the team, and the size of the option. The proof in section 2 is the technical claim. The commercial proof (users, revenue, retention, growth) is **TBD by the founder** and must come from the founder, per `14.biz/00-INDEX.md`.
- **Freshness warning.** The public multiples come from a single aggregator and are higher than the same site's spring snapshots (for example Datadog 10.7x in February 2026, GitLab 4.1x in July 2026, JFrog 8.2x in April 2026), which looks like a sector-wide move but was not independently confirmed; a second source could not be loaded. The private figures come from news and tracker reports that disagree on some revenue figures (Replit, Lovable) and mix valuation dates with revenue dates. One further comparable, Cursor, is left out because the reports conflict (a funding round and a separately reported acquisition). **Refresh every figure on a market-data terminal or from the companies' filings before it goes in front of an investor.**
- Sources (retrieved 2026-10-26): [Unity](https://multiples.vc/public-comps/unity-valuation-multiples), [Roblox](https://multiples.vc/public-comps/roblox-valuation-multiples), [GitLab](https://multiples.vc/public-comps/gitlab-valuation-multiples), [JFrog](https://multiples.vc/public-comps/jfrog-valuation-multiples), [Datadog](https://multiples.vc/public-comps/datadog-valuation-multiples); Cognition: [AI Business, $48B round](https://aibusiness.com/generative-ai/ai-coding-startup-cognition-valued-at-48b), [The Decoder, $26B round](https://the-decoder.com/ai-coding-agent-devin-maker-cognition-more-than-doubles-its-valuation-to-26-billion-in-under-nine-months/); Lovable: [Dealroom, $13.3B round](https://dealroom.co/news/144597-lovable-raises-400m-at-13-3b-valuation-eyes-600m-revenue-run-rate/); Replit: [Trending Topics, $9B round](https://www.trendingtopics.eu/replit-raises-400m-tripling-its-valuation-to-9-billion-in-six-months/), [Sacra, ARR estimate](https://sacra.com/research/replit-at-253m-arr-growing-2352-yoy/).

**C. Not built, not claimed.** An AI that plays by itself (the judge exists, the caller does not); the installer and store journey; the Windows port; any revenue or user numbers.

**D. Business drafts to read next.** `14.biz/ELEVATOR-PITCH.md`, `HQ-BRIEF.md` (sections 4 to 8), `14.biz/HOLDINGS-STRUCTURE.md`. All are DRAFT, founder-reported, last edited 2026-10-05, so none mentions anything on this page. `14.biz/FUTURE-GOALS.md` is personal intake: do not forward it. Licensing: the modular architecture lets a piece be licensed without the whole house (`HQ-BRIEF.md` section 2); no terms exist.

### Developers: reading order
1. `README.md` (book index), then this page.
2. The shape: `02-architecture/CENTROID_GOLD_STD.md`, `HQ-BRIEF.md` section 2.
3. 3D board and games: `18.pc-hq/00-INDEX.md` and `LEARNINGS.md`; `16.game/` (`RPG-SPACE-BLUEPRINT-SYSTEMS.md` first, then the collision design and engine-pieces docs); `16.game/GAMES-ROADMAP.md`.
4. AI: `17.ai/00-INDEX.md` → `AI.md` → `HARNESS-AS-GAME-BRAIN.md`; `AUTO-AND-AUTO-STUDIO-DESIGN.md`.
5. Tests: `06-testing/GAME-PLAYTHROUGH-HARNESSES.md`, `_shared-lib/harness/README.md`; run `prisc play_rpg_space_blueprint.pal` from the harness folder.
6. Open items: `08-roadmap/00-INDEX.md` and `08-roadmap/OPEN-ITEMS.md` (the second is dated 2026-10-12 and older in places).
7. Narrative: the NIGHT series (`1-1.HARNECIENT.SMOL/`, latest `NIGHT_55`) retells each work day in plain language; the audio is kept outside the repo.
8. House rules before you change anything: `AGENTS.md` at the repo root.

## 5. Known gaps and risks (read these)
- The old code still sits behind each converted page as a fallback; retiring it is planned, not done.
- Mining, building, pickup, hunger and the per-tick air physics are still code, not pages.
- No actor calls the AI harness yet; "pets that learn" is a direction, not a feature.
- Several tests are still shell scripts, not the pal harness the house prefers.
- The business drafts predate the last three weeks of work.
- The 3D engine's own performance numbers are not restated here because no current document records them; do not quote a frame rate from this page.
- The live 3D launch path was not re-verified after the 2026-10-26 loop change (stub engines only).

## 6. How this page stays true
Update it with every milestone: change the status word, add the evidence link and the date, and move the row between sections. A row that cannot cite a source goes back to `planned` or is removed. Dates are absolute. The nightly class (`NIGHT_NN`) and the technical report of the day are the narrative companions; this page is the index.
