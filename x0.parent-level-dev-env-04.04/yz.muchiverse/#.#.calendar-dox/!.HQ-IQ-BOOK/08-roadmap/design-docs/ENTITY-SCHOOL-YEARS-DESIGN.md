# Entity school years and report cards: design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner brief (2026-10-07): "make them go through school each 'year' and have report cards etc."

Read first (all exist): `NIGHT_20_THE_SCHOOL.txt` (school model), `NIGHT_21` (return path), `NIGHT_23` (terumon classroom), `NIGHT_30/31`, `A-TEARIT-IS-ALL-YOU-NEED.md` section 5 (the four tiers), `terumon-dev/TERUMON-SPEC.md` (`learning_limits.pdl`, `schools:`), `ENTITY-WORD-BANK-DESIGN.md`, `HARNESS-BEHAVIOR-BANK-DESIGN.md`.

## 1. What the house already says, and what is missing (checked, not assumed)

- **School model (NIGHT 20):** each entity has its own learner instance; it attends *classes* (curricula authored by a teacher: the owner now, Gemma later); each class is tested pass/fail and repeated until it passes. Discrete, gated, per-skill lessons, not one big fine-tune.
- **Four tiers (A-TEARIT 5):** preschool (Gemma may only propose `spoke_weight_delta` on existing spokes, small bounded deltas) -> elementary/HS (new spokes on existing masters, simple FSM tuning) -> associate/bachelor (`new_concept_node` proposals, Claude review) -> master/PhD (full `goap_action_describe`). Tier is raised on a **measured acceptance rate**, "never granted by default". `learning_limits.pdl` already has `max_tier` and `schools: enrolled / pass_required_before_next`.
- **Missing, and NIGHT 20/23 say so themselves:** no curriculum format exists (not one class, lesson or pass/fail check written as a real file); no per-entity clock (NIGHT 30: "there is no clock for one character"); no report card anywhere. And on disk: the four live terumon (`terumon_001_ember` .. `004_solvent`) have **no** `learning_limits.pdl` or `schools/` (the spec's seed layout was never applied; only `^.hai-horn/learning_limits.pdl` exists, `max_tier: elementary_hs`).
- **Current behavior to keep in mind:** the halo validator promotes edits straight into the bank at tier >= 2 without checking the ledger (HARNESS-BEHAVIOR-BANK-DESIGN 4e). School years give that tier a measured basis; they do not fix it by themselves.

## 2. The idea in one paragraph

Every entity with a learner (terumon, robots, tomom itself, later any entity with a word bank) lives on a **school calendar**. A **year** has **terms**; in each term it attends **classes**; each class ends with an **exam**; results are counted into a **report card** at term end and year end; at year end the report card is the **evidence** for "advance to the next year/tier or repeat". Tier-level promotion of an entity (its trust level) is a **human decision** in v1 (the report card is what the human reads), consistent with NIGHT 30's "promotion criteria are not designed yet; let real records pile up first".

## 3. Pieces

**3a. Years, terms, grades.** Coarse **tier** stays the one the validator and `learning_limits.pdl` use. Inside a tier there are **years** (grades): proposed ladder preschool (year 0), elementary/HS (years 1..12 or fewer), associate (2), bachelor (2), master (2), PhD (n). Years are labels for pacing and report cards; only tier boundaries change what an entity may *propose*. Year length is a **school calendar** setting (a `school.pdl`): measured in **game-time** (the owner's saved "in-game" clock) not wall-clock; this needs the per-entity/shared clock NIGHT 30 says does not exist yet (dependency, open).

**3b. Classes (the missing curriculum format).** A class is a folder: `class.pdl` (id, subject, tier/year it belongs to, pass mark, enrolled-before prerequisites), `lessons/` (ordered demonstrations: the existing Watch-style rows an entity learns from), `exam.pdl` (the checks). **An exam is a harness case file** (`_shared-lib/harness/cases/*.pdl` shape): deterministic PASS/FAIL against written expectations, run by the pal harness, leaving the append-only ledger rows NIGHT 31 describes. So an exam result is a Watch record with its own objective valence, no thumb needed. Tomom's ten existing curricula (Astronomy .. Programming) are already class *subjects*; they have no exams yet.

**3c. Where the record lives (hidden-layer pointer + private numbers, as decided).** Shared: a **school** (the catalog of classes and the calendar), the hidden layer; many entities point at it. Private, per entity, in `inventory/zz.school/` next to `zz.phone` / `zz.wordbank`: `enrollment.txt` (`SCHOOL=<id>` pointer + current year/term + enrolled classes), `ledger.txt` (append-only rows below). Private numbers are live user data (AGENTS.md data rules; rehearse in beta per STAGING-ENVIRONMENTS.md).

```
ENROLL | class | year | term | ts
LESSON | class | lesson_id | ts
EXAM   | class | attempt | result=PASS|FAIL | detail | ts          (written by the harness)
GRADE  | class | term | reliability=(reward+1)/(reward+punish+2) | n | ts   (derived row, rebuildable)
TERM_END | year | term | ts
YEAR_END | year | ts
DECISION | year | action=advance|repeat|hold | by=<human> | note | ts   (human only, v1)
```
Everything but `DECISION` and teacher notes is **derived or measured**; a `GRADE` is **counted** from `EXAM`/proposal rows (NIGHT 31's two-number rule: reliability is counted; what a class is *about* is proposed through the usual validator path, never edited by a grade).

**3d. The report card** is a generated file, never hand-edited, one per term and one per year, rebuilt from the ledger by an op (same "derived mirror" rule as NIGHT 22):
- header: entity, entity_hash, school, year, term, tier, owner/teacher;
- per class: lessons attended / assigned (attendance), exam attempts, best/last result, term grade (reliability and n);
- **proposal record**: edits this entity proposed this period, how many were accepted/rejected/pending in review, **acceptance rate** (this is A-TEARIT 5's real trust metric, counted from the review file rows);
- **word bank line**: aliases scored, hand-score agreement (ENTITY-WORD-BANK-DESIGN);
- **teacher comment**: owner free text, or a Gemma-proposed line in the fixed shape (`TARGET: <class> | STRENGTH: high|medium|low | REASON: <phrase>`) that a human accepts; Gemma never grades;
- **standing**: `advance recommended / repeat recommended` is shown as a plain threshold check *for the human*, not an automatic action (thresholds are open and start as owner-set numbers in `school.pdl`).

**3e. Year-end.** `YEAR_END` writes the year's card; a human adds `DECISION`. `advance` moves the year (and, at a tier boundary, `learning_limits.pdl` `max_tier` is raised by the owner/teacher entity, one reviewed row), `repeat` re-enrolls the same classes. Graduation ceremony, school entities (a teacher or a whole school owning a curriculum and collecting tuition, as in the marketplace concept doc) are later.

**3f. Tomom as a student.** Tomom attends the same school (NIGHT 20). Its classes are its curricula; its exam can be the reproducible Ask used in `tomom-hq` (fixed seed 7) against expected words, plus per-class loss; its report card lists which subjects it passed. Honest limit: its current answers are curriculum word salad, so the first exams will mostly FAIL, which is a correct report card.

**3g. Entities and the word bank.** The **first class for every entity is "Vocabulary"**: the hand-scoring of its seeded word bank is lesson and exam in one (the owner's marks are the teacher's marks); chain-wide word scores are the standardized test (advisory until signing exists). This gives the retroactive pass a purpose: each existing entity is enrolled in year 0 with that class.

## 4. Retroactive enrollment
Same contract as `phone_ensure_op` / the word bank: `school_ensure_op <house_root> [--apply] [--report FILE] [--pals-root DIR]`, dry run by default, copy-first, idempotent, never overwrites an existing ledger or any `DECISION` row; backup + sha256 + count before touching `xyzfs/users`; rehearse in beta first. Existing entities start at year 0 / preschool, enrolled in "Vocabulary". Terumon get their missing `learning_limits.pdl` as a **reviewed** step (not silently).

## 5. Build order (each step: pal harness first, rehearse in beta, develop in alpha)
1. Shared core (text-include): ledger append, derive `GRADE`, generate a report card from a ledger; harness cases (empty, one class passing, failing then passing, acceptance rate from a review file, rebuild equals replay, `DECISION` never auto-written).
2. Class format + one real class: "Vocabulary" with an exam written as a harness case file.
3. `school_ensure_op` dry run + report on a copy of the pals tree.
4. Report-card view window (X11-HQ; read-only first; a tab in the Concept Bank window or its own `school-hq`).
5. Spawn hook enrollment; real-tree apply after backup.
6. Year-end flow with the human `DECISION` row; then calendar tied to game-time once a clock exists.
7. Tomom class exams.

## 6. Open questions for the owner
1. **Year length:** game-time days? a fixed number of play sessions? or advance only when the owner presses "next year" (simplest, recommended to start)?
2. **Who is principal:** owner only for every `DECISION`, or a teacher entity per school later?
3. **Do all entities attend** (every pal) or only learner entities (terumon, robots, tomom)?
4. **Grade ladder:** use the four named tiers only, or numbered years inside them as above?
5. **Report card home:** a tab in the Concept Bank window, its own `school-hq` window, or both?
6. Should a report card ever be **printed to the entity's phone** (a message), or stay a file/window only?
