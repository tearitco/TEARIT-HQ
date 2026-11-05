# Vehicle crafting and modding: chop shops, dev-made vehicles, crafting menus (design, 2026-11-05)

Labels: **SAID** = owner 2026-11-05; **OBS** = seen in the repo this session; **DOC** = my reasoning, not run. Nothing here is built.

## 1. What the owner wants (SAID)
Vehicle crafting and modding: **boats, spaceships, mechas, drones, cars, trucks** and so on, introduced soon, in two forms: (1) **GTA-style chop shops** (in-world places where an entity strips, rebuilds and modifies vehicles) and (2) a way for a **developer to make vehicles through rpg-editor event pages**. Crafting menus that **an agent or a human can drive**. And an honest projection of **how hard these semi-creative tasks are to create and validate**.

## 2. What the house already has to build on (OBS)
| Piece | Where | Use for vehicles |
|---|---|---|
| Crafting grid rows | `@.apps/mcl-hq/recipes.pdl`: `RECIPE \| id \| grid \| output \| count` (3x3, `craft <id>`; 2x2-versus-table not checked yet); also `cdda-hq`, `rpg-pvp-hq` recipes | the recipe format and the `craft` verb |
| Ship hold | `@.apps/rpg-space/hold.pdl` (capacity 40, items and entities, `wreck_drop`) | cargo and parts storage |
| Mining, remnants | `blocks.pdl` (BLOCK/TOOL/NODE), `drops.txt` remnants with `damaged` flag | **salvage**: a wrecked vehicle leaves damaged parts; chop shops buy them |
| Collision and destruction | `RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md` (first slice built), ship damage, helm modes | vehicle hit points, crash, wreck |
| Everything is an entity; ships have inventory | same doc 4f | a vehicle is an entity with parts and a hold |
| Vehicle entities | `16.game/rpg-space-vehicle-entities/README.md`, engine pieces `veh_1..9` (loaded, **not drawn** in the scene) | art for vehicle bodies is partly there, not wired |
| Pieces and master pal list | `piece_mint`, `piece_check` | a crafted vehicle gets a uid like any piece |
| Item and shop ledgers | `item_ledger_op`, `shop_op` (only the editor coin shop is released; the rest of the lab book is quarantined) | prices, parts stock: reuse, do not restart quarantine |
| Event pages | `events/<name>/` pal pages edited by the blueprint viewer and `rpg-edit-hq` | dev-made vehicles and shop behaviour as data |
| Agents/humans one command path | pattern doc, seats | a crafting menu is verbs; both call them |

## 3. The design in one idea
**A vehicle is an assembly of part entities described by data; crafting, modding, repair and chop-shopping are verbs over the same rows.** Nothing is special-cased per vehicle kind.
- `parts.pdl`: `PART | id | slot | class | stats...` (slots: hull, engine, wheels/legs/rotors/thrusters, weapon mount, cargo, cockpit, power core, armor, utility). A *class* (`boat`, `ship`, `mecha`, `drone`, `car`, `truck`) decides which slots are legal.
- `vehicles.pdl`: `VEHICLE | id | class | slots ... | base stats` (blueprints) and, per built vehicle, a state file listing installed part ids, wear, owner, uid. Derived stats (speed, armor, cargo, power draw, mass) are **computed from parts**, never typed by hand: one authority per fact.
- `recipes`: reuse the `RECIPE` row shape, extended with a `class`/`station` column (for example `weld_bench`, `shipyard`, `chop_shop`); verbs: `craft`, `install <part> <slot>`, `remove <slot>`, `repair`, `scrap` (vehicle or part to remnants), `paint`, `tune`.
- **Rules as data:** power budget (engine output >= sum of draw), mass versus thrust/buoyancy/ground clearance, slot compatibility. A `check_vehicle` verb returns pass/fail rows with reasons; the same check is the harness's oracle.
- **Chop shop** = a location whose event page offers: `buy part`, `sell part`, `scrap vehicle` (pays by part condition), `swap`, `respray` (change identity), with prices from the shop ledger. "GTA style" extras (stolen vehicles, ownership, heat) depend on the law system (Centroid University, courts, the dungeon): **ownership is an `owner` field on the vehicle entity; stolen status is a flag; consequences come from the law slice later.** Keep the chop shop honest about that dependency.
- **Dev authoring:** a vehicle is a folder of `.pdl` plus an event page (pilot enters, controls map to the helm/drive verbs). In `rpg-edit-hq` the dev drops a vehicle entity and edits its parts list; no engine code.
- **Agents and humans:** the crafting menu is an `.xhtpm` list of verbs with nav numbers; a human clicks, an agent uses the per-pid relay or the same verbs. No privileged path.

## 4. Design-choice test (house rule; log as D19 in the decision log if accepted)
| Question | A. Per-vehicle-kind code (ship code, car code, ...) | B. **One part/slot/recipe data model + verbs** (recommended) |
|---|---|---|
| One authority per fact | stats typed in many places | derived from parts, one place |
| Modular | code per kind | `.pdl` rows + one shared check |
| Scalable (10th vehicle kind) | new code | new rows |
| Long term (where the roadmap points: events/pal/pdl, entities, ledgers) | against | with |
| House shapes | C ops | `.pdl`, op, event pages, harness |
| Cost / risk | cheap to start, expensive later | more upfront design; risk of over-generalizing: mitigate by building **one** class (ship) end to end first, then a second to prove the model |
**Recommendation: B**, ship first (it has helm, hold, collision, remnants), then a wheeled class (car/truck) to prove the model before mechas and drones.

## 5. Projected difficulty: creating and validating semi-creative tasks (DOC, not measured)
Units: S ≈ 1 working session, M ≈ 3-5, L ≈ 10+. "Create" = make the thing; "Validate" = prove it works without a human staring at it. Creative content is cheap to make and the expensive part is checking it, so each row says what the cheap check is.

| Task | Create | Validate | Why / what the check is |
|---|---|---|---|
| Part and vehicle data schema (`parts.pdl`, `vehicles.pdl`) + loader | S | S | harness parses rows; bad row counted (the `RECIPE` bad-row pattern) |
| `check_vehicle` rules (power, mass, slots) | M | S | pure function over rows: table-driven harness cases, red-on-old/green-on-new, mutation proof |
| Craft/install/remove/scrap verbs (ship class) | M | M | state-file diff before/after; remnants appear on scrap; round trip install-remove restores hold |
| Crafting menu window (human) | M | M | needs real window proof: relay click path, dump PNG (house rule: visible before/after evidence) |
| Agent drives the same menu | S | S | the same harness case calls verbs; assert identical state |
| Chop shop location (buy/sell/scrap/swap) | M | M | shop ledger rows + coin balance asserts; prices from data |
| Dev authoring in `rpg-edit-hq` (drop vehicle, edit parts) | M | L | editor UI is the costly validation (live window, no automation shortcut) |
| Vehicle art: wire `veh_1..9` into the scene | M | M | engine-side; "drawn at all" is a snapshot check; looks-right is human-only |
| Drive/handling per class (boat water, car ground, mecha legs, drone air) | L each | L each | feel is subjective: validate with numeric invariants (top speed from parts, turning), then a human playtest; ground/water physics are new work (only space flight exists) |
| **Generating new vehicle designs with an agent** (the semi-creative part) | S per design | M per design | cheap to propose, so **gate every proposal with `check_vehicle` + a smoke drive**; reject by rule, not taste; humans pick favorites. An LLM proposing parts lists is a hint-picker among legal slots (house AI rule: models pick inside a fixed shape) |
| Balance (prices, stats across 50+ parts) | M | L | bot-vs-bot Elo or a simulated race ladder (the house's balance method), then human review |
| Stolen/ownership/heat, law | L | L | depends on the courts and officers slice; do after |
| Multiplayer rated vehicle combat | L | L | needs the rated session layer (roadmap M2/M3) and arena (M9) |

**Honest summary:** the *data model and rules* are small and highly checkable; the *window and editor* are medium and need live evidence; *handling and balance* are where the time goes because "good" is partly human. Agent-made content scales well **only because validation is mechanical**: if a check cannot fail on a bad design it does not count (negative-test it once, per AGENTS.md).

## 6. Suggested slice order
1. Schema + loader + `check_vehicle` + harness (ship class, 6 parts). 2. `craft/install/remove/scrap` verbs on the rpg-space hold. 3. Crafting menu window; relay-driven proof. 4. One chop shop location, priced from the shop ledger. 5. Second class (wheeled) to test the model. 6. Dev-authoring in the editor. 7. Handling, balance, then law/ownership. Footprint: data files only at idle; menu costs a window; nothing polls.

## 7. Open questions for the owner
1. Does a crafted vehicle become a **piece** (a uid in the user's pals) or an item/entity owned by a piece?
2. Are there **tech tiers** (the common skill/tech tree you described for entities and items gate which parts craft)?
3. Is vehicle theft/chop-shopping meant for players against NPCs, players against players, or both (needs the law system)?
4. Which class first: ship (recommended: the engine already flies one), or a car/truck for the University board?
5. Do vehicles enter the rated PvP arena (parts affect Elo fairness: needs a "stock class" ruleset)?

## 8. Owner answers 2026-11-05
- Question 1: a crafted vehicle is **an entity owned by a piece** (not a piece itself): it has an `owner` field and its own hold; a chop shop or theft is a change of owner; scrapping leaves remnants. Questions 2-5 stay open.
- Physics is **fun only for now** (see `VEHICLE-PHYSICS-AND-PROVING-GROUNDS-2026-11-05.md`): the part stats are simple numbers; the material and engine columns stay in the data for a later realistic ruleset.
