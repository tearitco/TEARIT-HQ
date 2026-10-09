# pet-house - a tamagotchi on the house's own systems (claude, 2026-10-09)

First consumer of the layout studio. Everything is data plus one verb script; nothing here is a new engine.

| File | What |
|---|---|
| `ops/pet_event.sh` | the verbs: `new_pet [seed] feed|give <item> sleep wash play tick self_care evolve status`. State lives in `PET_DIR` (default `state/`, not committed). Exit 0 on bad input |
| `skillbook.pdl` | skills + level curve (`entity_grade`): eat/sleep/wash/play/trick and `self_care` (unlocks at level 4) |
| `curriculum.pdl` | the pet's school (report card from FEEDBACK rows, auto graduation) |
| `evolution.pdl` | level stages and habit traits -> `pet_gen` pins (scale, ears, colour) -> new OBJ + sprite frames |
| `items.pdl` | food / toy / soap rows; add a row to add an item |
| `weights.default.pdl`, `joints.pdl` | every tunable number; only `joint_tune` moves them (bounded, ledgered). `pref_<item>` and `w_<action>` are learned by the pet (autonomy=1) |

## How it maps to the house's systems
- **RPG Maker skill curve**: `entity_grade use` = skill (needs level, costs MP, gives EXP); EXP -> LEVEL by `exp_per_level`; `rest` refills MP on the day tick. Same op the Eden/learner work uses.
- **Weighted events**: a care verb is a normal event command (`doom_event.sh`-style); the pet's own choice (`self_care`) is a WEIGHTED choice among valid actions (need x priority weight), never a model (rule from ENTITY-NEEDS-AND-CARE-DESIGN section 6).
- **Learning**: each care verb writes a FEEDBACK row (valence +1 when it met a real need, -1 when not). Preferences and priorities move +-1 through `joint_tune` with a ledger row (`proposer=pet`). Grades come from the same rows via `curriculum.pdl`.
- **Not built**: the house screen (fridge/bed/bath desk + control window), registry rows so events-hq can pick these verbs, a DB-backed memory, and an internal chat model (see roadmap docs below).

Roadmap docs: `08-roadmap/design-docs/ENTITY-NEEDS-AND-CARE-DESIGN.md` (needs, GOAP, weighted choice), `LEARNING-LOOP-BANKS-WEIGHTS-NO-REPROMPT-DESIGN.md` (weights, banks; says no model weights are trained today), `ENTITY-SCHOOL-YEARS-DESIGN.md`, `RPGMAKER-PRIMITIVES-FOR-EVERYTHING-DESIGN.md`, digipet (`&.widgits/digipet/`, the needs loop this builds on).
