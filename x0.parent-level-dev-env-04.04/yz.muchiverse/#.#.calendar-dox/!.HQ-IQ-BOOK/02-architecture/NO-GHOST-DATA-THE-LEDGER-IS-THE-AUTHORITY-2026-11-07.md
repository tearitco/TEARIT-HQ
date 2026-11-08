# No ghost data: the ledger is the only authority for game state (standard, owner 2026-11-07)

💌 *Owner, 2026-11-07: "make sure we update docs to never do things that create ghost data by circumventing the ledger."* Written by Claude Sonnet 5.5. Labels: **SAID** owner · **READ** in a named file · **OBS** I ran it · **DOC** my reasoning.

## 1. The rule in one paragraph
**A fact about a game or a world lives in exactly one place: an append-only ledger row written through a verb the game already knows.** Everything else on disk is either a **projection** (derived from a ledger by a named verb, safe to delete, rewritten byte for byte), a **config** (an input the game reads and never writes), or a **preference** (a user's choice about their window that never changes an outcome). A file that carries a fact the ledger does not have is **ghost data**, and it is a bug.

## 2. What counts as ghost data (all forbidden)
| Pattern | Why it is ghost data |
|---|---|
| a state file that changes what the game does, written by anything other than a ledger-writing verb (a `state.txt`, `cash.txt`, a hero position file edited by hand or by a UI) | the ledger cannot explain it; a replay cannot reproduce it |
| a counter, cache or flag kept **only in a process's memory** that decides behaviour | it resets when the process ends, so the same ledger gives a different game (see 5) |
| a hand-edit of a live state file to "set up" a scenario | the scenario has no row saying who or why |
| a projection that someone edits and that is then **read back as input** | the edit becomes a second authority |
| a driver change (who is playing a seat) that is not a row | a replay cannot know who was in charge of a move |
| a "private" per-window copy of state that the page or ledger does not carry | two windows can disagree and nothing can settle it |

## 3. What is allowed (declare it, so it can be checked)
Each game lists every file its state folder may hold in **`state.pdl`**: `FILE | name | kind | writer | regenerate_with`.
* **LEDGER**: append-only rows. More than one ledger is fine (the game's event ledger, a decision ledger the bank digests), but each fact is in one of them.
* **PROJECTION**: derived by the verb in `regenerate_with`, which **must be a real VERB** in the game's `verbs.pdl`. Examples: the window feed `ui.txt`, the `mechanics.txt` projection, the drawn board page.
* **CONFIG**: `rules.pdl`, `board.pdl`, `page.pdl`, `tuning.pdl`, `layouts.pdl`. Inputs only. **Caveat (DOC):** replay uses the *current* config, so editing a rule changes how history is read; deskopoly's replay then **refuses** the old ledger (it compares every recorded result), which is safe but blunt. Recording a rules hash in the `new` row would fix it; **not done**.
* **PREFERENCE**: for example `sides_mode_memory.pdl` (which bars the user left open in each mode) or `last_book.txt`. Never read by the rules.
* **Scratch-only surgery** in a harness (`PUT` / `INIT` in a phase table) is allowed because it happens in a scratch house and the table's own rule says to explain it in a `NOTE` row. **It never happens in the live house.**

## 4. How a game proves it (four tests that can fail)
1. **Replay.** Replaying the ledger reproduces the state, and a hand-edited ledger is refused (deskopoly: `replay`, case `replay`).
2. **Files beside the ledger change nothing.** Write `cash.txt`, `state.txt`, `pet_rex.txt` into the state folder: the replay hash is identical (case `ghost`).
3. **Audit.** `audit` fails with `GHOST <file>` on any file the manifest does not declare, and refuses a projection whose regenerating verb does not exist.
4. **Regenerate.** Delete every projection, regenerate: identical bytes; a hand-edited projection is overwritten from the ledger; regenerating never touches the ledger.
The case `ghost` in `harness/cases/deskopoly.pdl` is the worked example (12 checks, 105 in the whole case at the time of writing).

## 5. Evidence from the house (READ and OBS; **not an audit**, only what I met)
* **OBS, mcl-hq:** the digest that builds its behaviour bank runs only when an **in-memory** counter (`static int since`, `mcl_hq.c` line 327) reaches 200, and verbs run as fresh processes. The ledger has 509 decision rows but the live bank files are 0 bytes (S0, handoff section 3). The cause is consistent with the counter resetting and is **not proven**; the fix is to key the digest to the ledger's byte cursor, which already exists.
* **READ, petslike games:** pet positions live in mutable files (`state/pet_<id>.txt`, `trainer.txt`); the mcl-hq phase table writes them with `INIT`, and the rpg-space driving skill tells agents to "stop the daemon, edit `state/trainer.txt`". Whether `events.txt` could rebuild them was **not checked**.
* **READ, `source=board` pc-hq games:** they "keep private state files" (hero, xelector, camera keys, `bar.txt`) that the page file does not carry (`16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md` section 1b).
* **Followed on purpose in deskopoly (OBS):** the driver of a seat is a `seat` ledger row, not a `seats.txt`; `ui.txt`, `mechanics.txt` and the board page are projections; the decision ledger is a second ledger, not game state; refused moves append nothing to the game ledger.
**Consequence:** the rule binds **new work now**; the existing games are a retrofit job (section 7), not an accusation.

## 6. Rules for any agent (also in AGENTS.md)
1. To change game state, **append a row through a verb**. Never write a state file to change behaviour.
2. Never keep behaviour-deciding state only in memory. Derive it from the ledger (a byte cursor, not an in-process counter).
3. Never hand-edit a live state file. Set up a scenario in a **scratch** house, or with a ledger row that says who and why.
4. Every derived file declares its source and regenerates identically. Never read a projection back as input.
5. A ledger row says **who acted** (a seat, a driver); a control change is a row.
6. Config and preferences are inputs and choices, never facts.

## 7. Retrofit plan (DOC, nothing started)
1. Give each petslike game a `state.pdl` and make `audit` generic (a shared op), so the **list of ghosts is a finding, not a guess**.
2. Fix mcl-hq's counter (S0b).
3. For each game, decide per file: becomes a ledger row, becomes a projection, or is declared PREFERENCE.
4. Record a rules hash in the `new` row so config edits cannot silently re-read old history.

## 8. References
`@.apps/deskopoly/{state.pdl,verbs.pdl,ops/deskopoly.c}` · `&.widgits/_shared-lib/harness/cases/deskopoly.pdl` (cases `replay`, `ghost`, `seats`) · `16.game/S1-EVENT-LEDGER.md` · `08-roadmap/design-docs/PROPOSAL-ONE-DOOR-DRIVING-AND-THE-MASTER-LEDGER-2026-11-07.md` (D49) · `08-roadmap/design-docs/PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS-2026-11-07.md` · AGENTS.md section "No ghost data".
