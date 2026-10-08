# Own users per machine, local models and schools, and farm animation mirrored in pc-hq (plan, 2026-10-08)

Status: **PLAN, nothing built.** Owner (2026-10-08): debil and the Mac have local llama models that should be used to train; schools, sharing and comparing strength, scores and curriculums; different schools and event paths per machine, unique to its own user data; move the Mac and debil off user jb onto their own users "mac" and "debil"; mid term, animation for farming (on move) as visual gameplay, mirrored in pc-hq.

## 1. What is true today (read over ssh on 2026-10-08, read-only)

| | this desk | debil | Mac |
|---|---|---|---|
| logged-in user (`current_login.txt`) | jb | **jb** | **jb** |
| user uuid | 0a9558a7-7c74-… | **the same uuid** | **the same uuid** |
| user folders under `xyzfs/users` | 24 | 24 | 24 |
| local Ollama models | (not used; weak CPU) | `llama3:latest`, `llama2:latest` | `llama3-groq-tool-use` (latest and 8b), `qwen2.5-coder` 7b/3b/1.5b/0.5b, `codeqwen:7b-code`, `gemma3` 1b/270m, `llama2`, `llama2-uncensored`, `nomic-embed-text` |

**Consequences.** All three machines are the *same user* with copied data, so entity identities, phone numbers and any wallet are duplicated across machines. That is harmless while they never talk, and a problem the moment they trade, share quests or compare scores: two "jb"s with one uid cannot be told apart. The groq tool-use model is on the Mac only; debil has plain `llama3`, no tool-use variant and no coder models.

## 2. Separate users: "debil" and "mac" (procedure, to be proven on a scratch house first)

Goal: each machine's desk runs as its own user with a fresh uuid, fresh entity identities and its own wallet, while the copied jb data stays on disk, untouched, as an archive.
1. **Create the user through the house's own signup path**, not by hand-editing: the login/signup app writes the user folder and `current_login.txt` (`current_user_id`, `current_user_uuid`, relative `current_xyzfs`); `seed-user.sh` then copies the starter files, never overwriting and never seeding identity. Entities for the new user are created fresh, so their `entity_uid`s, hashes and phone numbers differ from jb's.
2. **Prove it in a scratch house first.** The scratch houses already on debil and the Mac (`/tmp/pnh`) have no users: sign up "debil" / "mac" there, check the folder, the login file and a freshly spawned entity's uid differ from jb's, then repeat on the live desk.
3. **Live switch is a data change on someone's desk:** back up with a tarball and checksum list first (house rule), do it with the desk windows closed, relaunch after. jb's folder is not deleted or edited. `session.pdl` (the stale second record) is rewritten to match.
4. **Peers then show real names.** The Friends pane's profile uses `current_user_id`, so the Online list reads "debil", "mac" and "jb" instead of three jbs.
5. **Unverified:** the exact signup op to call without the GUI, and whether anything else caches the old uuid (the registry, `ai_instances_registry.txt`, running windows). Both are checked in step 2.

## 3. Local models for training (use what each machine has)

- **Update to a standing rule, needs the owner's confirmation:** until now all Ollama/Gemma calls went to the Mac because this desk is weak. The new direction is that *each* machine trains with its *own* local models, and results are shared. Calls from this desk still go to a stronger machine; debil and the Mac may use their own local Ollama. Free models only.
- **Who does what, by what is installed:** the Mac, with `llama3-groq-tool-use` and the coder models, is the tool-use/code worker and the judge; debil, with `llama3`/`llama2`, is a language/curriculum worker. A model that debil lacks (a tool-use variant, a small coder) can be pulled there by the owner; pulling a model is a download I will not start without asking.
- **What "train" means at first:** run schools (below) with each machine's local model as the student and a deterministic exam as the judge, record graded rows, and share the rows (see `SHARED-TRAINING-ACROSS-MACHINES-PLAN-2026-10-08.md`). Weight training comes later.

## 4. Schools, strength, scores, curriculums

What exists: `ENTITY-SCHOOL-YEARS-DESIGN.md` (partly built: grades, curriculum files, `entity_grade`, a Grade Tick, MP and levels); entity grades are earned by exams, levels and MP by skills.
- **A school per machine/user.** The curriculum, the event path and the exams live in that user's data, so "mac" and "debil" each run their own school with their own entities and their own scores. Different event paths per machine means different `event_pkg` pages per user, not a shared one.
- **Comparing strength.** Exams are deterministic checks over the curriculum (the same style as the CSV lints), so any machine can score any student. Each exam result is a row `EXAM | user | entity | course | score | model | ts`. Shared through the peers, the union gives a leaderboard (grade, level, average score per course) that every machine computes identically. Show it in the Friends pane (Online tab: a strength column) and later in a school window.
- **Sharing curriculums.** A curriculum is a small content pack (course list, exam definitions, expected answers) with a manifest, shared as a pack, never with the student's private history.
- **Fairness checks:** rows carry the model and the machine; a student cannot grade itself; the examiner is a deterministic op or a different model.

## 5. Farm animation, visual gameplay, mirrored in pc-hq (mid term)

What exists: Eden's history already records the farming (GROW, RIPE, ITEM, ACT rows per day); entity move works (`move_entity_init/tick`, a live window moves); pc-hq has a board with piece placement and a projector; neither shows Eden.
- **One source of truth, two views.** The Eden state (history tail + entity states) is the truth. A **desk view** (the planned Eden world viewer) and a **pc-hq view** (the same entities as board pieces on a farm map) are both projections of it; neither writes game state. That is what "mirrored" means: change the farm once, both show it.
- **Animation on move.** Eden ACT rows that move something (walk to the plot, carry grain) emit a move marker (`MOVE | entity | from | to | day`); the views interpolate it with the existing move tick; GROW stages change a tile's sprite (seed, sprout, ripe); RIPE plus harvest plays a short sprite change. Keep animation in the viewer, never in the game logic, so a paused game freezes the picture.
- **Build order:** (1) world viewer with a static farm and tiles from the history tail; (2) ACT rows emit move markers, the viewer animates them; (3) pc-hq projection of the same state onto a farm map (a pc-hq level for the farm, pieces keyed by entity); (4) crop growth sprites; (5) the same for the second machine's farm, so a visitor can watch a friend's farm through the peers (view-only).
- **Harness:** a fixture history produces an exact sequence of frame states (positions per tick); the pc-hq projection of the same fixture matches the desk view's positions. Mutant: let the viewer write game state and the read-only check fails.

## 6. Order of work

1. Prove "mac"/"debil" user creation on the scratch houses (section 2), then plan the live switch with the owner. 2. Ledger replication (shared training plan, step 2). 3. A school/exam harness on one machine, then an exam-row share between two. 4. The Eden world viewer (also the Phase A item of the Grok handoff), then move markers, then the pc-hq mirror.

## 7. Owner decisions

Confirm the rule change (each machine may use its own local Ollama); whether to pull a tool-use model onto debil; when it is safe to switch the live desks to their own users (windows closed, backup taken); which courses and exams the first school runs.
