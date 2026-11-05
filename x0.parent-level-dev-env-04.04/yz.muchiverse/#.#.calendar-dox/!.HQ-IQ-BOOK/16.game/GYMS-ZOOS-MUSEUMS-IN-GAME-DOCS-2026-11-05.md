# Gyms, zoos and museums: documentation that lives in the game (research, design, roadmap, 2026-11-05)

Labels: **SAID** owner 2026-11-05; **READ** in a named source; **OBS** seen in the repo; **DOC** my reasoning, not run. Nothing here is built.

## 1. The idea and its source (READ)
Robin-Yann Storm's talk "Gyms, Zoos, and Museums: your documentation should be in-game" ([GodotFest 2025 recording](https://app.media.ccc.de/v/godotfest2025-gyms-zoos-and-museums-your-documentation-should-be-in-game), [INDIGO 2025 listing](https://www.dutchgamesindustry.nl/talk/robin-yann-storm-indigo-2025)). I read the abstract, search excerpts and the summary you pasted, **not the full talk**. Three kinds of in-game level:
| Kind | Purpose | Examples |
|---|---|---|
| **Gym** | a controlled playground that measures mechanics, so limits stay true as the engine changes | exact jump and climb heights, cover alignment, ability ranges |
| **Zoo** | every asset side by side to inspect scale, look and sound in the real game | models, props, NPCs and animations, VFX, audio cues (Half-Life 2 is the early example) |
| **Museum** | rooms that explain how a system works and why, with interactive examples | physics, scripting flows, rendering behaviour |
Claimed benefits: no wiki rot (a level breaks when the code breaks), designers and artists experience constraints instead of reading, and gyms/zoos are natural baselines for automated smoke and visual regression tests.

## 2. Why it fits this house unusually well (DOC, from OBS)
1. **"If it's not a file, it's a lie"**: our catalogs are already data (`catalog.pdl`, `blocks.pdl`, `parts.pdl` once built, `keybinds.pdl`, `verbs.pdl`, `tuning.pdl`, palettes, `recipes.pdl`). A zoo can be **generated from those rows**, so the display cannot drift from the truth: one authority per fact.
2. **The editor needs the same catalog.** A zoo is a browsable palette: the editor (`rpg-edit-hq`, palettes) picks from the same rows. Build once, use for documentation and for editing.
3. **Agents need gyms.** A "gym" is also the reinforcement-learning word for a training environment. A house gym with a measurable score is exactly the **locked judge** the delegation flywheel requires (`17.ai`, `16.game/VISION-CLARIFIED-2026-10-27.md` section 8): agents and bots can take it, humans can take it, the score is a harness row.
4. **Our docs already rot.** The book is large (22 chapters) and agents reread it every session. A museum room that is generated from, and tested against, the data replaces some prose.
5. **Existing pieces:** a `002.zoo` app exists at `44.xyz.01.00/002.zoo__...0000` (not inspected: it may be an animal toy, not this); palettes (`&.widgits/palettes`); the imported maps already include test-like rooms (`map083 dtest`, `map042/076 Console test spaces`, `map075 Basic`, `map081 Item Shop`, `map084 Forge`); `HARNESS` cases already drive worlds headlessly.

## 3. Design
**One book, "the Concourse"**, a level whose wings are the three kinds. Each exhibit is a **page generated from data** plus an optional hand-written caption (data row, so it is still a file).
- **Zoo wing (what exists):** one pedestal per row of a catalog (pets, parts, vehicles, blocks, items, tiles, sounds). Walk up: label, stats, 3D/2D model, "spawn one" (a verb). Auto-built by a generator op reading the catalog; adding a row adds a pedestal. Missing art shows a visible placeholder (so a hole is seen, as the talk intends).
- **Gym wing (what it can do):** measured lanes. Jump/step heights and stair rule (decision D12), walking speed per terrain, collision tests, vehicle handling lanes (see `VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md`), combat ranges, mana costs. Each lane has an **instrument** (a readout) and a **target row** (`GYM | lane | metric | expected | tolerance`). A harness case runs every lane headlessly and fails when the metric drifts: **the doc is the test**.
- **Museum wing (how it works):** rooms per system: possession, event pages, the ledger and marker-file model, the khtpm window shell, the economy, the chain, the flywheel router. Interactive: press a key and the exhibit runs the real verb (for example fire an event page and watch the ledger row appear).
- **Editor reuse:** the zoo is a **palette view**: the editor's "place" picker reads the same generated list; drag a pedestal item into a map. Palettes remain the storage; the zoo is a presentation.
- **Humans and agents:** an agent enters the Concourse as an entity with the same verbs, reads exhibit rows as text (the generated pages are files), and takes gym lanes as scored tasks. Spectators watch.
- **Rated use:** gym scores feed the same Elo/ladder layer as games (roadmap M2): a bot's gym rating is its router grade for that task family.

## 4. Design-choice test (D20, PROPOSED)
| Question | A. Hand-written docs/levels | B. **Generated from catalogs + hand captions + harness-checked gyms** (recommended) | 
|---|---|---|
| One authority per fact | duplicates the data | the catalog is the authority |
| Modular / scalable | each exhibit hand-made | the 10th catalog is a generator row |
| Long term | rots | rebuilt on every run; fails loudly |
| House shapes | prose | `.pdl`, op, event pages, harness |
| Cost / risk | none to start | a generator op and a page template; risk: ugly auto layouts: mitigate with hand-tuned layout rows and captions |
**Recommendation: B**, start with a single wing.

## 5. Projected effort (DOC, not measured)
| Step | Create | Validate |
|---|---|---|
| Zoo generator for one catalog (blocks or parts) + a pedestal page template | M | S (row count equals pedestal count; mutation: delete a row, the pedestal vanishes) |
| Gym lanes for walking/stairs/collision + `GYM` target rows + harness | M | M (numeric invariants; drift fails) |
| Vehicle gym lanes (per physics ruleset) | M each | M (analytic oracles, see the physics doc) |
| Museum rooms (5 first systems) | M | S for text and verb links; M for interactivity |
| Editor palette reuse | M | M (live window proof) |
| Agent scoring of gym lanes into router grades | M | M |
Footprint: static data at idle; the Concourse is a normal board window; no pollers.

## 6. Roadmap placement
Roadmap milestone **M12** (parallel track, can start after M1; the first zoo wing can start now). First slice: **Zoo of rpg-space `blocks.pdl` + `hold.pdl` items** (small, verifiable), then **Gym: stairs and walking** (feeds D12), then **Gym/Proving Grounds for vehicles** (M11).

## 7. Open questions
1. Is `002.zoo` the seed of this, or an unrelated toy? (Owner knows.) 2. Should the Concourse be one book or one per game? (Recommend one, with per-game wings.) 3. Which catalogs first? 4. Do museum rooms also host the lore (Soul Pen museum) for players, separate from the dev rooms?
