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

## 8. The users come from the install script, and the toys are bought from the store (OWNER, 2026-10-08)

**OWNER:** the debil and mac users should be created by installing from the install script, and then "buy" the toys from the store, downloaded from GitHub. Set that up soon.

This replaces the "sign up in the old copied house" idea in section 2: instead of editing the copied jb desks, each machine gets a **fresh install** with its own user, then acquires everything else through the store. It is also the "from install" opening scene of the network demo.

**What exists (checked 2026-10-08):**
- Both GitHub repos exist and are readable: `tearitco/tearit-install` (`install.sh`, the one-line bootstrap) and `tearitco/tearit-hq-payload` (the curated payload). Install is `curl … | sh -s -- <product>`; the product name picks the folder under the home directory (`$HOME/<product>`), so `tearit-mac` and `tearit-debil` would sit beside, not on top of, the existing houses. The payload ships the taskbar, login/signup, cursword and the clock; Linux only. I did not run the installer today and do not know how current the payload is: it was last built by hand from an older house.
- The store: `&.widgits/store/catalog.pdl` lists **six items** (`xyzfs-jb`, `concept-bank-seed`, `delegation-bank-seed`, `eden-game`, `quest-pilot-kit`, `hidden-layer-seed`), each with kind, version, price (`free` or coins), requirements and root. Their status is **planned or data-ready; none is installable**. There is no checkout window, no `owned.pdl`, no pack builder and no installer for packs. The design's flow is: catalog, checkout, owned, install (`XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md` sections 9 and 10). The `xyzfs-jb` entry is a private user pack and must never be offered to anyone.
- A buying path with coins exists only in part: `chain_send` and `chain_escrow` (test chains, honor-based, no signing).

**The path, in order (each step a pal harness with a mutant, scratch folders only):**
1. **Refresh and leak-check the payload.** Rebuild it with `make-payload.sh` from current `claude` (it now carries the macOS fixes). Add the leak check first: a harness that builds the payload into a scratch folder and fails on any provider key, wallet, `xyzfs/users/<uuid>` content or absolute home path. The payload repo is pushed only after that passes and only when the owner says so.
2. **Toy pack format.** A toy is a pack with a manifest: id, version, price, requires, a file list with a sha256 per file, a build step (compiled programs are in no pack, each install builds), and a deny-list check. First toys: `irc-chat-hq` plus its peer, then `forum-hq`, then Eden. Hosted as folders in a GitHub repo (or release assets) the installer downloads; the downloader verifies every sha256 before extracting, extracts into the user's house only, and regenerates identity for anything that carries one.
3. **`xyzfs_pack_op`**: pack, verify and install, dry run by default, `--apply` to write, idempotent, refusing paths outside the house and anything on the deny-list. (From the install doc build order, step 3.)
4. **"Buy."** v1: a free toy is "bought" by appending an `OWNED | user | item | version | ts` row to the user's `owned.pdl`; a priced toy first needs a payment: on a test chain, `chain_send` to the store wallet plus the transaction id as proof, checked by the buy op before it writes the row (honor tier, T0). No real value until signing exists.
5. **Store window (checkout-hq).** Lists the catalog (name, price, status, owned or not), a Buy row and an Install row, reachable by nav number so a relay can drive it. It is the thing a person watches in the demo.
6. **First real run:** install `tearit-debil` and `tearit-mac` with the install script into fresh folders, sign up users "debil" and "mac" there, buy and install `irc-chat-hq` from the store, and open it; the Friends pane then shows three differently named users. Run beside the existing houses, not over them.

**Risks and rules:** the installer downloads and runs code, so toys come only from the owner's repos and are hash-checked; macOS needs its own check in the harness (the Mac renderer now compiles, the install path is untested there); keys never enter a pack or the payload; installing never touches `xyzfs/users` of another install.

**Decisions for the owner:** which repo and layout hosts toy packs; which toys are free and which cost coins; whether the payload repo may be rebuilt and pushed once the leak check passes; the product names for the two machines.

## 9. A reusable install harness with labeled installs (OWNER, 2026-10-08)

**OWNER:** a reusable install harness so installs can be labeled per machine and version (mac-v1 … mac-v1000) for experimenting with fresh installs.

**What the installer already gives us (read `install.sh`):** argument 1 is the product name and picks the folder (`PREFIX` defaults to `$HOME/<product>`); `BINDIR` defaults to `$HOME/.local/bin`, where it writes a launcher named after the product. So a **label is just a product name** (`mac-v1`, `debil-v7`): each label is its own folder and its own launcher, installs never collide, and removing one is `uninstall.sh` for that label.

**Design, a pal harness with data cases (never a new .sh):**
- **Install ledger** `installs.pdl` (append-only, kept outside any install): `INSTALL | label | host | ts | payload_ref | result` and `REMOVE | label | ts`. The next free label is computed from it (`mac-v` + highest + 1), so numbering is never reused and the history of experiments is kept.
- **Targets by data, no hardcoded address:** a hosts file maps a short name to `user@host` for ssh (`mac`, `debil`, and `local` for this desk); the case file names the target, never an address. Needs one new case verb: `REMOTE <target> <command>` (ssh `-n -o BatchMode=yes`, with a timeout), so one case file runs on any machine. This is also the verb the three-machine chat harness is missing.
- **Case steps, per label:** pick the next label; run the installer for that label into a fresh folder (payload ref chosen by the case: `main`, or a branch to test a candidate); check the folder, launcher and build outputs exist; run the **leak check** over the installed tree (no provider key, no wallet, no `xyzfs/users/<uuid>` of another install, no absolute home path of another user); sign up the label's user through the real signup path and check a fresh uuid differs from jb's; start the desk; check the taskbar window is up (frame dump or its state file); optionally "buy" and install a toy from the store and open it; then either keep the install (a numbered experiment) or `uninstall` it. Teardown only ever touches that label's own folder and launcher, never another install or the existing houses.
- **Mutants:** an installer that writes a key into the tree must fail the leak check; two installs of the same label must be refused; an install that skips the build must fail the "window is up" step.
- **Evidence:** every run copies its transcript, the file listing with sha256 and a frame dump into `results/`; the ledger row records pass or fail, so "mac-v12 failed at build, mac-v13 passed" is answerable later.
- **Safety:** the harness never uses `PREFIX` outside `$HOME/<label>`, never deletes unlabeled folders, and does the live macOS and debil installs only after the same cases pass on a local scratch label first.

**Order:** (1) hosts file plus the `REMOTE` verb with a loopback test; (2) the ledger op (next label, append, list); (3) the local scratch install case (label `local-v1`, nothing remote), including the leak check with its mutant; (4) the same case on debil, then the Mac, as `debil-v1` and `mac-v1`; (5) toy purchase steps once the store has a pack to buy. Step 3 is also the payload leak-check harness already listed in section 8.
