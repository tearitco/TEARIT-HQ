# Roadmap to the Tear-it co promise and to TSOTS (2026-11-05)

Labels: **SAID** = the owner said it (2026-11-05); **READ** = in a file (named); **OBS** = observed in the repo this session; **DOC** = my reasoning, not run. Sizes are guesses in working sessions (S ≈ 1, M ≈ 3-5, L ≈ 10+), not dates: the owner's availability and the weak machine set the calendar. Companion docs: `../22.lore-ip/00-INDEX.md`, `../22.lore-ip/TSOTS-GAME-FAMILY-AND-LEVEL-MAP-2026-11-05.md`, `GRAND-ROADMAP-CHECKLIST-2026-11-04.md` (feature checklist), `design-docs/GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md` (the rating pattern this roadmap builds on).

## 1. The promise, as I understand it (SAID)
> TEAR-it co on Twitter: "coming soon: **てるてる** : single & multi player AI and human automation ecosystem : online Elo-rated PvP games with RPG Maker!  ~ **RPGツクール製の、Eloレーティング対応オンラインPvPゲーム（シングル＆マルチプレイ）！** 📌"

- **Japanese line** (my reading): "An online PvP game made with RPG Maker, with Elo-rating support (single and multiplayer)!". RPGツクール is the Japanese name of RPG Maker.
- **てるてる (teru-teru)**: the sound of the *teru-teru-bōzu*, the paper doll hung to wish for sun (照る = to shine); a natural Japanese sound-play on "**tear it**" (the brand, the Tear It Cards, torn paper; the company logo is "torn yellow pages with the name and a red pin on a wooden shield", and the 📌 is the pin) (READ: `crswrd.media-archive/tear-it co.txt`). I take it as the Japanese-audience name of the product line; **confirm** (is てるてる a product name, a tagline, or the account's nickname?).
- **Audience** (READ `14.biz/OUTLETS/TEAR-IT-CO/00-INDEX.md`): RPG Maker players and developers, English-speaking otaku, Japanese-speaking game audience; about 30 loyal X followers.
- **Four clauses to make true:** (A) **single and multi player**, (B) an **AI and human automation ecosystem**, (C) **online Elo-rated PvP games**, (D) **made with RPG Maker**. Plus the brand line (READ `tear-it co.txt`): faith-owned, open source, blockchain-secure, AI desktop productivity, creativity/education/play companion.
- **The rule I propose for the whole roadmap: every clause of a public claim must have a receipt** (a harness case that can fail, or a recorded run) before the claim is made. Until then the honest word is "coming soon".

## 2. What already exists, per clause (OBS unless marked)
| Clause | In the house today | Gap |
|---|---|---|
| A single and multi player | `@.apps/TSC_ELO` ("True Swords Clash", 387 files): modes HvH (hot seat), HvC (vs computer), CvC (bots duel) per `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`; rules file `+.TSOTS-ALPHA-OMEGA/#.RULES/True_Swords_Clash_Rules.txt` (2 players, HP 100, Mana from ordering Bible verses, 11 Miracles); `rpg-pvp-hq` (729 files) and `bet-rpg` (38 files) exist (not inspected) | both TSC_ELO and TSOTS-OG are **middle-house** apps (terminal + GL mirror, not the x11-hq window shell; `20.middlehouse/middle-house-conversion.txt` lists TSC_ELO in group B: never launched in that pass, frames unchecked) |
| B AI and human automation | the pattern's seats (`human`, `agent`, `peer`, `spectator`): agents use "the same event commands a human uses, no special privileges"; harness-as-game-brain judge is built with **no caller**; pets drive the same input path (designing); `HARNESS-AS-GAME-BRAIN.md`; free models only | no agent seat is rating-parameterized yet; "automation" for humans (scripts, the notes' "scripture" with limited loops) is a design idea only |
| C online Elo-rated PvP | `ops/tsc_elo.c` (273 lines: `resolve`, `get`, `update`, ratings file `player\|rating\|wins\|losses\|draws\|games`, new player 1000); a P2P duel over `palnet_peer` reported live-verified in `TSC_ELO/prog-report-pvp.md` (presence, challenge, accept, moves both ways, ledger convergence; **reported, not re-run by me**); `irc_two_machine` and `palnet_peer_net` harness cases exist | **`palnet_peer` is same-machine: `@.apps/TSC_ELO/ops/palnet_peer.c` lines 138 and 166 hardcode 127.0.0.1** (OBS); the duel-to-a-winner and the Elo write-back are not yet proven (`PATTERN` doc section 3); no lobby, no signed identity, remote ratings are "advisory" until signing exists |
| D made with RPG Maker | the owner's 98 RPG Maker maps are imported (`@.apps/piececraft-hq/pieces/system/maps/tsots/`); the house renders RMMV tile art from `#.NNEST_ASSETS/rmmv-www-img`; the editor `rpg-edit-hq` places RMMV tiles; the owner's older RMMV JS plugins exist (TSOTS SECRETS) | the house engine is **not** RPG Maker; whether "made with RPG Maker" is true depends on decision D13 below, and on the RPG Maker asset license |

## 3. Decisions the roadmap needs from the owner (design-choice test run; recommendations are DOC)
**D13. What does "made with RPG Maker" mean?**
| Option | One authority | Modular / scalable | Long term | House shape | Cost / risk |
|---|---|---|---|---|---|
| 1. A real RPG Maker (MV/MZ) plugin pack + server: the game is an RPG Maker project | rules live in plugins | JS, per-title | ties the product to RPG Maker's runtime and license | JS is not C/pal/pdl (AGENTS.md language rule: owner decides) | honest to the promise; reaches the audience; new language |
| 2. House engine that imports RPG Maker maps and art ("RPG Maker-style") | the house session ledger | what exists | matches the roadmap | all house shapes | the promise wording must change ("RPG Maker-style", "uses RPG Maker maps"), and the **asset license must be checked**: RPG Maker's bundled art is, to my knowledge, licensed for use with RPG Maker only (verify the EULA) |
| 3. **Hybrid: rules, sessions and Elo live headless in the house (one authority); a thin RPG Maker plugin client speaks the same wire (`MSG\|...` session lines) as a projection** | yes: one session ledger, several clients | the 10th client costs a plugin, not a rewrite | headless sessions are already the pattern's rule | op + IPC + pdl | the plugin is new JS; only needed when the client is built |
**Recommendation: 3, built in order: house client first (it exists), RPG Maker client at M4.** Deciding reason: it keeps one authority per fact and makes clause D true without moving the rules into JS. Until M4 ships, say "RPG Maker-style" publicly, not "made with RPG Maker".
**D14. First rated game.** Recommendation: **True Swords Clash** (the owner's own order: "we do tsots game first ... classic rules, nothing changed", `+.TSOTS-ALPHA-OMEGA/#.wussup+@o.txt`). It already has Elo, a duel, a P2P proof, and a Bible-verse mana challenge that is the product's identity.
**D15. Elo constants.** Two values are on record: start 1000, K=32 (`PATTERN` doc, `tsc_elo.c`) and start 1200, K=32 for chess (`16.game/VISION-CLARIFIED-2026-10-27.md`). One authority: choose per game kind in a data file (`elo.pdl`); recommend 1000/K32 for TSC.
**D16. Bible text for the verse challenge.** Use a **public-domain** translation (for example KJV or WEB) so the shipped game has no text-license problem; modern translations (the owner's chat paste used RSV) are copyrighted. The challenge needs one verse list per translation and a stable chronological order.
**D17. Client and distribution.** The house runs on Linux X11 (macOS via XQuartz; Windows thin shims). The RPG Maker audience expects Windows or the web. Recommend: M3 proof on Linux/Mac; M4 decides web (the house browser lane) versus an RPG Maker plugin client for Windows.
**D18. Japanese.** The promise is bilingual: UI strings in EN and JP, and the renderer must draw Japanese glyphs (not tested: DOC; M4 gate).

## 4. The roadmap (milestones, each with a proof gate)
Critical path: **M0 → M1 → M2 → M3 → M4 → M5** (the promise). M6 to M9 are TSOTS and the University, which reuse the same infrastructure. M10 (vehicles), M11 (physics, proving grounds), M12 (gyms, zoos, museums) and the AI track are parallel.

### M0. Decisions, naming and legal hygiene (S, now)
Answer D13 to D18; rename **Diagonal Alley** (the owner's own `legal.txt` already flags Hogwarts and Chrono Cross as trademark risks: see `../22.lore-ip/SOUL-PEN-LORE-SUMMARY-2026-11-05.md`); check the RPG Maker asset license for art used outside RPG Maker; confirm what "cursword" and "てるてる" mean. **Gate:** decisions written in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.

### M1. True Swords Clash complete, offline (M)
Run it for real (observe, do not trust the report): launch TSC_ELO's duel, fix what breaks; move it from terminal + GL to the x11-hq window shell (the owner's standing conversion rule, `20.middlehouse`); finish the two missing proofs: **duel to a winner** and **Elo write-back**; Bible-verse data (public domain) and the mana challenge; Miracles as data rows; HvH, HvC, CvC. **Gate:** a pal harness case (mutation-tested) plays a full duel in each mode and shows both ratings changing by the standard Elo vectors; a recorded run the owner can watch. **Claim allowed:** none yet.

### M2. One session, seat and rating layer; bots (M)
Generalize `tsc_elo` into the shared rating op the pattern asks for (ratings file, append-only result ledger, recompute-from-ledger); seats `human|agent|peer|spectator` as `SETUP` rows; **agent seats whose strength is their rating** (the opponent's rating is the AI's difficulty, as TSC already does); rule bot first, then weighted, then a free-model hint-picker among enumerated legal moves, graded by Elo against the rule bot; give the harness-as-game-brain judge its first caller. **Gate:** harness: bot ladder is monotonic (a stronger bot beats a weaker one by Elo over N games); an agent uses only the human command path. **Claim allowed:** "single player vs AI with Elo", "AI opponents" (clause A single side, B partly).

### M3. Online (L, the hard one)
Cross-machine `palnet_peer` (remove the 127.0.0.1 hardcode, per `CROSS-MACHINE-NETWORKING-PLAN.md`); a **provable peer identity and signed results** (the pattern's trust gate: remote ratings stay advisory until then); the **lobby ledger and screen** (`SESSION|JOIN|START|CLOSE` rows, rating range); disconnect = forfeit = a loss (the owner's old rule); reconnection; basic anti-cheat by ledger convergence. **Gate:** a two-machine rated duel, driven by a harness over ssh (the `irc_two_machine` shape), with both ledgers equal and the rating written once. **Claim allowed:** "online Elo-rated PvP" (clause C).

### M4. Player-facing client, language, distribution (L)
Implement D13 (house client polished; the RPG Maker plugin client if option 3 is chosen); EN and JP UI with a glyph proof; install and onboarding; a lobby a stranger can find; the cost line for each running piece (the house rule: idle cost near zero on a weak machine). **Gate:** a person who is not the owner installs and plays a rated duel against a bot, then against another person, from a clean machine. **Claim allowed:** clause D, worded as the decision allows.

### M5. Public alpha = the promise, kept (S, after M4)
Replace "coming soon" with "play now" **only if** A, B, C, D each have a receipt. Truth ladder for what to post: M1 "a duel game with Elo is working locally"; M2 "play against AI opponents that are rated"; M3 "online rated duels (alpha)"; M4/M5 the full line. **Gate:** a receipts table (clause → proof) in this doc.

### M6. TSOTS: the sword into the Bible (L)
Per the owner's order (`#.wussup+@o.txt`): after the duel game, Bible levels: Adam and Eve "Dwarf Fortress style", do **good or evil** ("cursword"), gain influence over characters and **possess them** (the snake in the garden; the house already has possession: Enter, Tab, `9`, Esc-first, lock to hero), **capture** characters as **pals**, trade them; remember which verses a player did (achievements, personal lore, history: "half of product-market fit"); unlocked levels and videos. Story mode uses the imported maps (`../22.lore-ip/TSOTS-GAME-FAMILY-AND-LEVEL-MAP-2026-11-05.md`); no imported map is named as a Bible level, so Bible levels are new maps. **Gate:** one Bible level (for example the Garden) playable with possession, a capture rule and a verse record, harness-proven. Needs owner answers: Bible scope and order, capture rule, tone.

### M7. The chess-like board: TSOTS / TSOMOJIO (L)
Bible characters and merchants as pieces on a chess-like board, 1v1 and 2v2 (the "chimochio 4x4" board = `map071`, 4 players), rated, with the classic rules unchanged. Reuses M2 and M3. **Gate:** a rated 4-player game on `map071` with human and agent seats.

### M8. More games and their bots (L, later; owner request 2026-11-05)
**AOW / Booty Land** ("Armor of War", rules in `#.RULES/AOW.Booty_Rules.txt`: 4 players, about 25x25 water and islands, one action per turn, coin-flip combat, Capital and units) and **FFU / Footrace Fu = Time Fu** (rules in `#.RULES/ff-fu-IQ-a29-RULES...txt`: 2 to 4 players, 5 ninjas, race, duel and "Buffet" variants), each with **bots** (rule bot, weighted bot, hint-picker), using the same session, seat, rating and lobby layer; **B.O.T.S.** (Battle of the Sovereigns, `BOTS_Rules.txt`: 8x8, 32 pieces, blood tax) is the third rules set already written. **Gate per game:** the M1 and M2 gates (full game in each mode, monotonic bot ladder). Order: AOW first (the simplest: one action per turn, coin flips), then FFU, then B.O.T.S.

### M9. Centroid University and the Soul Pen levels (L, after M3)
The University's **arena and tournaments are this same rated infrastructure**; party battles (manual, auto, hybrid) are seat kinds; shops, schools, courts and the dungeon follow `../16.game/CENTROID-UNIVERSITY-VISION-AND-GAPS-2026-11-05.md`; stairs by decision D12; the Soul Pen cast and creatures come after the Bible levels. Rename Diagonal Alley before any public use.

### M10. Vehicle crafting and modding (L, can start in parallel after M1; owner 2026-11-05)
Boats, spaceships, mechas, drones, cars, trucks: one part/slot/recipe data model, crafting menus driven by humans or agents through the same verbs, GTA-style chop shops, and dev-made vehicles through rpg-editor event pages. Design, difficulty projection and slice order: `../16.game/VEHICLE-CRAFTING-AND-MODDING-DESIGN-2026-11-05.md`. Ship class first (helm, hold, collision, remnants exist), a wheeled class second to prove the model. Theft, ownership and heat depend on the courts slice (M9); vehicles in rated PvP need a stock-class ruleset (M2/M3/M9). **Gate:** harness: `check_vehicle` rejects an illegal build and accepts a legal one (mutation-tested); craft, install, scrap round trip on the hold; an agent and a human produce identical state through the menu.

### M11. Vehicle physics and proving grounds (L, parallel; owner 2026-11-05)
Mass from materials, engines with fuel or an energy source, buoyancy, lift, friction; one integrator, two rulesets (fun and realistic); new test maps (vacuum range, test basin, test track, airfield, mecha yard, weigh station), each lane checked against an analytic oracle. Design and effort table: `../16.game/VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md`. Order: space, boat, car, plane/drone, mecha. **Gate:** harness: simulated rocket burn matches the rocket equation within tolerance and fails when a constant is perturbed.

### M12. Gyms, zoos and museums: documentation in the game (M, parallel; first zoo wing can start now)
One "Concourse" book generated from the catalogs (zoo), measured lanes (gym), explained systems (museum); the editor palette reuses the zoo; agents take gym lanes as scored tasks. Design: `../16.game/GYMS-ZOOS-MUSEUMS-IN-GAME-DOCS-2026-11-05.md`. **Gate:** deleting a catalog row removes its pedestal; a drifted gym metric fails the harness.

### AI track A0-A7 (always on, ordered in one page)
Micro-AI, the delegation flywheel, in-house models and data, token minimization, and what (little) belongs on the chain: `../17.ai/AI-ROADMAP-MICRO-AI-AND-IN-HOUSE-MODELS-2026-11-05.md`. Every milestone above adds a judge; a judge is what lets a cheap model be tried safely.

## 5. Cross-cutting tracks (always on)
- **Proof discipline:** every milestone gate is a pal harness case plus a recorded run; no "done" without a fresh build, a fresh run and evidence (AGENTS.md).
- **Footprint:** idle cost near zero, heavy jobs through `cpu_gate`, runtime files capped (`growth_probe`); say the cost in each commit.
- **Docs and skills:** each milestone updates this doc's receipts table and the local skills (`soul-pen-lore`, `tsots-game-map`).
- **IP and legal:** scrub trademarked names; public-domain Bible text; RPG Maker art license; do not publish unpublished lore.
- **Content discipline:** the owner's own note says the lore is "solid and beautiful" and that over-extending it costs self-focus; priority is code, games, outreach, marketing, data science (READ `中.SP_00.00/👩🏻‍⚖️️.gripes?2edit?.txt`). Do not write new lore to avoid shipping.

## 6. Risks
1. **A public claim ahead of proof** (the promise is already public). Mitigation: the receipts rule and the truth ladder.
2. **"Made with RPG Maker" and the art license** (D13): resolve in M0.
3. **Online is the long pole** (identity, signing, lobbies, anti-cheat). Mitigation: M3 is its own milestone with a two-machine gate; ratings advisory until signed.
4. **Linux-only distribution** versus the RPG Maker and Japanese audience (D17).
5. **Weak CPU:** servers and bots must idle near zero; a `cpu_guard` kill is our footprint.
6. **IP:** Diagonal Alley, Hogwarts and Chrono Cross references, art and Bible translation licenses.
7. **Scope creep** (Bible levels, University, Soul Pen): the M1 to M5 slice is deliberately a small rated duel game.
8. **Tone** for a gamified Bible with "play evil" and capture mechanics: an owner decision with the audience in mind (the TSOTS outlet voice is "reverent-but-playful").

## 7. Receipts table (fill in as clauses are proven)
| Clause | Receipt (harness case / recorded run) | Date |
|---|---|---|
| A single and multi player | not yet | |
| B AI and human automation | not yet | |
| C online Elo-rated PvP | not yet | |
| D RPG Maker | not yet (decision D13 pending) | |

## 8. Open questions for the owner
1. D13 to D18 above (RPG Maker meaning, first game, Elo constants, Bible translation, client, Japanese).
2. What is "てるてる": the product name, a tagline, or an account nickname?
3. "AI and human automation ecosystem": do you mean (a) bots as rated players, (b) humans scripting their own automation (the notes' "scripture"), or both? (I assumed both.)
4. Do bots share a rating pool with humans (the pattern doc's open question 3)?
5. Is True Swords Clash the first public game, or the 4-player chess-like board?
