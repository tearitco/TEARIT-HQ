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
1. ~~Year length~~ **ANSWERED (owner 2026-10-07, section 7): a year is a unit of training PASSED, not elapsed time; age = years passed.**
2. **Who is principal:** owner only for every `DECISION`, or a teacher entity per school later?
3. ~~Do all entities attend?~~ **ANSWERED (section 7): an entity attends when it is placed in a school's inventory.**
4. **Grade ladder:** use the four named tiers only, or numbered years inside them as above?
5. **Report card home:** a tab in the Concept Bank window, its own `school-hq` window, or both?
6. Should a report card ever be **printed to the entity's phone** (a message), or stay a file/window only?

## 7. Owner corrections (2026-10-07): they supersede sections 3a, 3e and 4 where they conflict

Owner: "learning is done as a 'play' of a particular 'page'; or can happen independently if it's just 1 entity or for some other reason. An entity can learn from a 'phone' if the lesson is placed on the 'server computer' etc., graded from the computer, no big difference but handwaving non-physical learning entities / training / gameplay styles. Years will be stored within the entity; if it passes 2 years of training, that's its age. Entities should be placed in a 'schools' inventory, where they may undergo accelerated ageing and trainings."

**7a. Three lesson modes, one ledger.** A lesson/exam row gets a `mode` field: `mode=page|solo|phone`. They differ in how the lesson reaches the entity, not in what is recorded.
- **page**: the lesson is a **play of a page** (a map/event page, the house's `event_pkg/pages/page_N/` unit): the entity plays the page; the exam is a deterministic check on the state or events that play produced (a harness case file over the page's result).
- **solo**: a single entity learns on its own, no page (its own learner/ledger and lesson files). Used when only one entity is involved, or for any other reason.
- **phone**: for non-physical entities and "handwaved" training: the lesson is placed on the **server computer** (`^.hai-server`), delivered to the entity's phone as a `task` message (the entity reads its phone by size growth, as the phone design already says); the server **grades** it (the design's `quest.score` already runs a quest's deterministic `verify.sh` and records the verdict; a class exam is the same thing) and writes the `result` message back plus the `EXAM` row. Quests and lessons are the same shape; reuse `verify.sh`/harness, do not invent a second grader.

**7b. Age is stored in the entity and means years of training passed.** Not elapsed or wall-clock time. Reading: each training **year passed** adds 1; after passing 2 years the entity is age 2 (confirm this reading). The count is derived from the entity's own `YEAR_END` rows with a pass and mirrored as an `AGE` line in `zz.school/enrollment.txt` (rebuildable from the ledger, never typed). **This removes the dependency on a per-entity clock** for age and for year length (open question 1 is closed). A clock is only needed for the school's pacing (7d).

**7c. A school is a place: an inventory.** A school is an entity/item with an `inventory/`; entities **placed in it are enrolled** (placing = enrolling; taking out = leaving). It holds the shared catalog (classes, calendar, `school.pdl`): the hidden layer the earlier design pointed at. Each student keeps its own private record (`zz.school`) with a pointer to the school it is in. This is the "schools" inventory the marketplace concept doc also assumes (a school owns a curriculum). Note from the drag/drop design: **a drop is a MOVE** (the dropped folder is moved, not copied), so putting an entity in a school moves its folder into the school's inventory; entities there are not desk windows (they are "non-physical" while enrolled). That has consequences for running entities that must be decided before building: how an enrolled entity is started or shown, and what happens to its desk position and open processes when it is moved.

**7d. Accelerated ageing and training.** A school applies a **time/training scale** (`school.pdl`: `time_scale`, lessons per game tick) so a training year passes quickly inside it; the age gained is still counted from passed years, not from the scale. The school's own pacing is a small clock owned by the school (one clock, for all its students), not one per entity.

**7e. Retroactive step changed.** `school_ensure_op` (section 4) now only **creates each existing entity's `zz.school` record (age 0, no enrollment) and reports**; it never moves an entity into a school. Placing an entity in a school is the owner's action (a drop), so no live entity folder is relocated by a script. Terumon still get their missing `learning_limits.pdl` as a reviewed step.

**7f. What this changes in the build order.** Add before step 1: define `school.pdl` and the school-as-inventory convention (with one hand-made test school and one scratch test entity in beta); add the three `mode` values to the ledger rows and one harness case per mode; the phone mode reuses `server.route` and `quest.score`. Report-card and year-end steps are unchanged except that `YEAR_END` + a human `DECISION=advance` is what increments age.

**7g. New open questions.** (1) Confirm: passing one training year adds exactly 1 to age? (2) Is a "year" in a school a fixed list of classes that must all pass, or a pass mark across them? (3) What does an entity do while it is inside a school inventory (does it keep running, is it shown)? (4) Does the entity's age cap or unlock anything (tier, abilities) or is it only a label for now?

## 8. Owner answers (2026-10-07, later): report-card ownership, "in school", age, care
- **Report card is owned by both the entity and the teacher.** One card, derived from the entity's private ledger; a copy is written into the entity's `zz.school/report_cards/` and a **sealed snapshot** into the teacher's/school's records at term end. The entity's ledger stays the single authoritative source (the card is derived), so the two copies cannot disagree; the teacher's copy is read-only to the entity.
- **"In school" = physically in the school's inventory.** An entity learning by **phone** is **not** in school (no school clock, no cafeteria); its lessons and grades still reach its ledger with `mode=phone`.
- **Age is only a label for now** (answers 7g.4): it unlocks nothing; keep it as the derived `AGE` line.
- **New requirement: needs and care** (hunger, hygiene, sleep, health, weight, optional death), accelerated by the school clock: see `ENTITY-NEEDS-AND-CARE-DESIGN.md`. The school's cafeteria and the day clock come from there.
- Still open (7g): does one passed year add exactly 1 to age; all classes vs pass mark; what an enrolled entity is shown as.
