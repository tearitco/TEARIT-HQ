# Pet trainer: town building, learning to feed itself, stores and trade, and "teach me" (design, 2026-10-09, NOT built)

Owner direction (2026-10-09): move into town building; the pets learn to feed themselves through **proximity to food**; they **trade and work for each other at stores** - **emergent gameplay based on learning**; they also **reach out to APIs / Gemma to learn real knowledge**, broadening their **curricula and concept banks** through **"teach me" tech-tree prompts**. And: train the pets the whole time through the concept bank.

Companion docs: WORLD-DESIGN.md (house, society layer), CAMERA-DESIGN.md, ROADMAP.md. House law this design obeys (do not weaken):
- **Gemma DESCRIBES, never CLASSIFIES**: one fixed line, picks only from existing nodes, never sets a number (`ai_describe.c`, A-TEARIT, NIGHT 30 attrition model).
- **Weights move only through candidate EDIT -> `concept_edit_validate` -> a review file a human reads -> promotion.** Preschool and elementary pets NEVER auto-promote (`AUTO-PROMOTION-RULE.md`). No auto-promotion is built.
- **Free models only** (no paid credits); **local Ollama calls go to the Mac** (`#.desktop/ai_backend.pdl` `gemma_lan_url`, never localhost); free API quotas are per account (OpenRouter 50/day); a 429 = exhausted until reset.
- Everything is rows + events (RPG Maker style), per-feature C only where a compiled op already is the house pattern. Harnesses are pal + cases (`_shared-lib/harness`), not new `.sh`.
- Change detection by append-only marker/ledger growth, never mtime.

## 1. The town (building buildings, exploring, a map that grows)
- **Town map becomes per-world state.** `world_map.txt` stays the shipped seed; `state/world/town_built.pdl` (tile overlay rows `TILE | x | y | char`) and `state/world/town_seen.pdl` (explored cells) are merged into `town_all.txt`, exactly like `built_rooms.pdl` -> `rooms_all.pdl` today. Readers (pet_world, pet_scene) prefer the merged file.
- **Buildings** are rows: `BUILDING | id | kind | x | y | owner | name`. Kinds to start: `house` (a pet's home, its door teleports to that pet's rooms), `farm` (crop plots), `store` (shelves + a counter + jobs), `workshop`. A `build_building` event (like `build_room`): needs coins/items, a free plot, refuses with a spoken reason (occupied, no money, plot not explored). Each building's door is a **teleport event page** (generated from the BUILDING rows, same as room doors).
- **Exploration + map growth.** Cells start unseen (fog). Walking reveals a radius; reaching the map edge **generates the next strip** from a seeded generator (grass, trees, water, ore, crop sites) and appends it to the overlay, so the map widens. The map screen shows seen cells only; the house mini map (key 5) gets a town level above it.
- **Going out**: the village door already goes to the world page; pets will leave on their own (weighted choice, below), not only the trainer.
- Build order inside this item: overlay merge + readers -> `build_building` + harness `pet_town` -> exploration/fog -> map growth.

## 2. Feeding itself through proximity (learned, not scripted)
Today feeding is a menu verb. The pet should learn **where food is** and go there.
- **Food sources are things in places**: fridge (house), crops (garden/farm), shelves (store), other pets' gifts. Each is a row with a position and a kind (`FOOD | place | x | item`).
- **Proximity event**: the RPG Maker "player touch" trigger. When a hungry pet is within a short distance of a food source, the `eat_near` event page fires (eat the item, lower hunger, +EXP, feedback `+1 eat`). A not-hungry pet ignores it.
- **Learning where food is** = a **place memory** `memory.pdl`: `PLACE | food | <room or tile> | weight`. A successful meal at a place raises that place's weight; a fruitless visit lowers it. A hungry pet's walking choice is a **weighted pick** among remembered food places (plus a little exploration), exactly like `self_care` weights today. No model call.
- Rewards flow through the existing ledger (`obs_feedback_log.txt` concepts `eat`, `hungry_walk`) so `entity_grade` grades it; `joint_tune` (bounded, ledgered) may nudge the weights. Anything that edits the shipped weights still goes through candidate EDIT + review.
- Harness `pet_feed_proximity` (pal): scratch pet, hungry, food placed at a known spot, it walks (manager ticks stepped), eats, memory weight rises; a second run goes straight there.

## 3. Stores, jobs and trade (emergent)
- **Money**: coins live in the pet's inventory (the house `food_prices.pdl` / `coins` convention from digipet). A phone text can carry a request (kind `ask`/`task`, already in `phone_send_op`).
- **Stores** are buildings with `JOB` rows: `JOB | store | task | pay | needs`. Tasks are small and event-shaped (stock shelves, carry an item, harvest a plot). A pet takes a job by a **weighted choice** (its tendencies x need for coins), does it (walks, uses an action), and the completion event **pays** it. The owner of the store pays out of the store's till (ledger row both sides).
- **Buying**: a hungry pet with coins and no known free food buys at a store (price from `food_prices.pdl`); no coins -> looks for a job. That chain is not scripted anywhere: it falls out of needs + weights + rewards.
- **Pet-to-pet trade**: `TRADE` event moves items/coins between two pets who have each other's number; both sides logged; LIKE (borough) scores update (from WORLD-DESIGN.md). Texts negotiate ("i have an apple").
- **Tendencies**: `tendencies.pdl` per pet (w_work, w_trade, w_explore, w_build, w_study); outcomes are rewarded/punished in the ledger; weights move by the existing bounded tuner. Different pets drift different ways -> emergent roles (farmer, shopkeeper, explorer).
- Harnesses: `pet_jobs` (job taken, done, paid, ledger rows on both sides), `pet_trade` (item and coin conservation: totals before = after).

## 4. "Teach me": real knowledge from Gemma / free APIs, into the curriculum and the bank
Goal: a pet that finishes a lesson can **ask to learn more** and its curriculum and concept bank **grow**, safely.
- **Tech tree** `techtree.pdl`: `NODE | id | title | needs=<ids> | domain | ask="<fixed prompt>"`, e.g. counting -> coins -> trading; plants -> farming -> crops; reading -> signs -> maps. A node unlocks when its `needs` are passed in the pet's curriculum (`curriculum.pdl` subjects graded by `entity_grade`).
- **The ask** (`teach_me` event, once per pet per day, MP cost like a skill): the pet picks an unlocked node (weighted), and an op calls the provider ladder with the node's **fixed prompt**:
  1. Gemma on the Mac (`gemma_lan_url` from `ai_backend.pdl`) first;
  2. then free providers the owner has keys for (Groq / OpenRouter `:free`), each with a **per-provider daily budget** counter in state; 429 -> skip for the day;
  3. nothing paid, nothing on localhost.
  The prompt asks for **one line in a fixed format** (`TOPIC: <node id> | FACT: <one sentence> | CHECK: <one question with a one-word answer>`). Anything that does not parse is rejected and logged, never partly applied.
- **Where the answer goes**: a **lesson card** appended to `state/lessons/pending.txt` with source, model, date and `status=candidate`. It is NOT in the pet's bank yet. A `new_concept_node` / spoke candidate is generated from the card and run through `concept_edit_validate` (the type exists in the docs, unbuilt - building it is part of this item). The review file is what the owner (or an approver) reads; `PENDING_REVIEW` is the only state a preschool/elementary pet's card can reach on its own.
- **Why this is safe**: models can be wrong. Cards are tagged `unverified`; a fact is promoted only after (a) the owner approves, or (b) two different providers return agreeing cards AND the pet passes the card's CHECK question in an exam (`entity_grade` evidence). Disagreement is shown, not resolved by us.
- **What a promoted card does**: becomes a spoke/master node, a curriculum subject, an exam question (the CHECK) and vocabulary (LEX words the pet can use in chat and TTS). That is how the curriculum and concept banks broaden.
- Harness `pet_teachme` (pal): a **fake provider** (fixture script on PATH/URL) returns good lines, a malformed line, and a 429; assert: good card lands as candidate only, malformed rejected+logged, 429 skips and counts, budget stops at the limit, nothing reaches the live bank without promotion.
- Real Gemma calls are tested only through the Mac and only by an explicit run; the harness never needs the network.

## 5. Training the pets the whole time
- `pet_train` (pal, 48 checks) already teaches all six pets every taught verb through the real feedback ledger. **Every feature in this document adds a lesson set**: e.g. "go to the store", "buy an apple", "work at the farm", "ask my friend", then rerun it. The grade (`entity_grade`) is the evidence, and the day tick runs it.
- A daily **school** event: the pet studies one tech-tree node it unlocked (MP, EXP), takes the CHECK as an exam, gets graded. Real knowledge, real grading.

## 6. Build order (each step has a pal harness before it counts as done)
1. Town overlay + `build_building` + doors as teleport events (`pet_town`).
2. Place memory + proximity eating (`pet_feed_proximity`).
3. Stores: BUILDING kind `store`, JOB rows, take/do/pay, buying (`pet_jobs`).
4. Pet-to-pet trade + texts (`pet_trade`).
5. Exploration fog + map growth (`pet_explore`), town level on the map screen.
6. Tech tree + provider ladder + lesson cards + validator type (`pet_teachme`), school event.
7. Tune pacing (needs per game hour) so the economy is playable at the normal clock speed.

## Open questions for the owner
- Who approves lesson cards: you only, or also an agent reviewer (house has Claude-review tiers)?
- Which free providers may be used besides the Mac Gemma (Groq needs you to create a free key)?
- Should a pet be allowed to build in the town without you (cost-limited), or do you approve each building at first?
- Daily "teach me" limit per pet (suggested 1) and the per-provider daily budgets.

---

# Owner decisions (2026-10-09, answers to the four questions) - these override the defaults above

## D1. Lessons come from a FORMULA that evolves with testing
- A **lesson formula** (`lessonformula.pdl`) is data, not a fixed script: slots (topic node, fact style, how many examples, hint style, CHECK question style, difficulty step, length) and **tunable weights** for each choice. The "teach me" call fills the formula to build the prompt, and the formula builds the lesson card from the answer.
- **It evolves by testing.** Every card produces evidence: did the pet pass the CHECK, how many tries, did two providers agree, did later exams on that topic improve. Those outcomes score the formula choices (Laplace score `(reward+1)/(reward+punish+2)` as everywhere in the bank). The existing bounded tuner (`joint_tune`) nudges the weights with a ledger row per change and a rollback; the six pets act as **a small A/B population** (different pets can run different formula variants; the better variant spreads).
- Approval therefore moves from "a human reads every card" to "**tests decide**" for cards; the safety rails stay: weights only move through bounded, ledgered edits (candidate EDIT -> `concept_edit_validate`), a card is `unverified` until it passes its CHECK + provider agreement, the owner can read the ledger and roll the formula back, and a new formula **shape** (a new slot type) still queues for a human. The house rule "preschool/elementary never auto-promote" is kept for the **concept bank nodes**; the formula (how lessons are made) is allowed to evolve on its own because it is measured by exams.
- Harness `pet_lessonformula` (pal): fake provider, deterministic exam results; assert the weights of the formula choices that led to passes rise (bounded), failures lower them, every change has a ledger row, a rollback restores the previous values.

## D2. Providers and money
- **Keys exist** for Groq and Poolside (and the OpenRouter free key). Use the house's existing provider ladder pattern from `^.hai-horn/ops/horn_chat_backend.c` (OpenAI-compatible `/chat/completions`; key from the env var `GROQ_API_KEY` / `HORN_POOLSIDE_KEY` / `HORN_API_KEY` or the key file next to it). Never print or log a key; if a call needs a key that is not found, say so and ask the owner. Order: Mac Gemma, then Groq, Poolside, OpenRouter `:free`, each with a per-day budget and 429 handling. Free only.
- **Pets build on their own when they have the money and the materials.** Materials are bought at stores or **gathered**: cut trees (wood), farm crops, mine ore; gathering is a job/skill with a cooldown, the tree regrows on the clock. A `build_building` needs `cost = materials + a land fee` and refuses with a reason otherwise.

## D3. Land on the voxel planet: scarcity, price, renting
- Land is **parcels** on the voxel planet grid: `PARCEL | id | x | y | z | owner | price | rent`. A building must stand on a parcel the pet owns or rents.
- **Price rises as free space runs out**: `price = base * (1 + k * used_fraction^2)` (k and base are tunables in `land.pdl`; the used fraction is parcels owned / parcels in the explored area). New land appears only as the map is explored/generated (design section 1), so exploring is how land is found.
- **Renting**: an owner can list a parcel `for rent` (a `RENT` event); another pet pays rent per day (a clock event moves coins, ledger rows both sides). Non-payment -> a warning text on the phone, then the lease ends. Rent and price are also weighted choices the pets learn (what rent does a pet accept, what does it ask).
- Harness `pet_land`: price curve rises with usage; buy refused without coins; rent paid on the day tick; coins conserved.

## D4. The phone is the hobby of the mind; other RPG Maker skills train too
- **Study by phone, all day**: on the pet clock a pet regularly picks a hobby action: text a contact about a topic it is studying, or ask the model through "teach me" (budgeted). Chats are real texts through the phone server (already built); the pet's replies come from its taught words and, when it has an unlocked topic, from the lesson cards it holds. Time spent chatting gives **Intellect** EXP (an RPG Maker style stat).
- **Skills**: Power, Magic, Defense (and Intellect) are skills in `skillbook.pdl` like care actions: cost MP, give EXP, level up (`entity_grade use`). Each can be trained several ways, **including chatting**: a message tagged with a topic (training talk, spell talk, defense talk) can award that skill's EXP, graded through the same feedback ledger (`concept=power|magic|defense|intellect`).
- The pet schedules these by its **tendencies** (weights), not a script, so a pet that likes talking becomes a scholar and one that likes working becomes a builder.
- Training harness lesson sets get a row per skill (talk about power/magic/defense with a friend, ask a question).

## Updated build order
1. Town overlay + parcels/land price + `build_building` with materials and coins (`pet_town`, `pet_land`).
2. Gathering (trees, farm) + materials in the inventory.
3. Place memory + proximity eating (`pet_feed_proximity`).
4. Stores, jobs, buying; rent (`pet_jobs`, rent in `pet_land`).
5. Pet-to-pet trade + texts (`pet_trade`).
6. Skills Power/Magic/Defense/Intellect + chat-training (`pet_skills`).
7. Lesson formula + provider ladder + lesson cards + phone study hobby (`pet_lessonformula`, `pet_teachme`).
8. Exploration fog + map growth (`pet_explore`), pacing.
