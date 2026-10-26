# TEARIT-HQ roadmap: the front door

**Written 2026-10-26.** One page for three readers: customers, investors, developers. It is a map, not a promise: it says what is proven, what is being built, and what is only planned, and it points to the document that holds the evidence. If a row has no source, it does not belong here.

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
| **3D board, ship over a planet** | A ship that is an entity flies over a planet that is an entity; mining leaves spinning remnants; oxygen and temperature per room; ship and land collide; asteroids and a second crewed ship exist. | `16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7e to 7g, `RPG-SPACE-ENGINE-PIECES.md`, `NIGHT_54`/`NIGHT_55` | 2026-10-25 |
| **Worlds from a seed** | Planet pages are generated from a seed and a ledger; the same inputs give identical bytes. | harness cases `dust_ladder` (64 checks), `dust_seed` (79), `dust_gen` (115), `dust_zoom` (79) | 2026-10 |
| **Light on the machine** | Idle process starts of the 3D board fell from about 98 to about 19 per second (load average 14 to 1.6) after the loops were gated on file sizes; leaked loops and test daemons now end by themselves. | `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7g; harness cases `loop_exit` (11 checks) and `harness_cleanup_*` | 2026-10-25/26 |
| **Browser lane** | "JS apps actually run" (G1) and G2.1/G2.2 are closed at 100 percent and wired into the browser test run. | `08-roadmap/2026-10-14-G1-SPRINT-VERDICT.md`, `2026-10-17-G2-VERDICT-ADDENDUM.md` | 2026-10-17 |
| **AI, the part that exists** | The scoring side: a scratch playthrough, a result ledger, and per-step weights are built; the decision ledger is built in three games; an approval room for outside agents is real. | `17.ai/HARNESS-AS-GAME-BRAIN.md` section 3, `17.ai/AI.md` | 2026-10-17/18 |

## 3. Where it is going, by track

### 3.1 3D and worlds
| Next | Status | Source |
|---|---|---|
| Finish converting the remaining game behaviour (mining and building verbs, pickup, hunger tick, hp-zero detection) into pages; keep per-tick scans in one small resident process | `building` | tech report 2026-10-25, section 4 stage A (`XO/30.rpg-dust-work/oct-25/`) |
| rpg-dust gets the ticks that play its life pages, then mining; the same test script must pass on both games | `building` (data and three pages copied; no ticks yet) | `RPG-SPACE-BLUEPRINT-SYSTEMS.md` 5e |
| Ships that move, trade and fight as entities; entities in a ship have a position relative to it; draw the vehicle pieces that load but are not drawn | `designing` | collision design 4e to 4g; `RPG-SPACE-ENGINE-PIECES.md` |
| "Micro" scenes inside a ship's voxels; deep simulation (for example respiration) switched on per entity only where it earns its cost, cheap default elsewhere | `planned` | owner direction 2026-10-25, collision design 4c |
| "Semi destroyed" block animations (frames chosen by mining progress over hardness) and sprites that show more blood the more hurt a character is, with a setting for how graphic it is | `planned` (owner 2026-10-26; state it needs already exists) | `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` 7h |
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
