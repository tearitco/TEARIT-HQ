# Pal vs events: what each is, when to use which (guide for devs, agents and the owner)

Written 2026-11-01 by the `claude` engineer (Sonnet 5.5), after converting the edit-board closed-row check into a pal harness and asking whether events would be better. Facts are labeled: **READ** = from a file named here, **RAN** = observed running, **MINE** = judgment. Companion to `BLUEPRINT-EVENT-PAGES-GUIDE.md` and `PRISC-OPS-ARCHITECTURE.md`; read those for the mechanics. This note is only the decision.

## 1. The short answer

- **Events are the content. Pal is the machinery under them.** They are not rivals: an event page is *compiled to* a pal (`event.pal`), and a pal harness is a pal that execs two ops. Asking "pal or events" is really asking *who owns the rule and where does it live*.
- Use an **event page** when the rule is **part of a book's behavior** and a person (the owner, a modder, an AI player) should be able to see it, edit it and play it inside the book.
- Use a **pal (plus ops)** when the thing is **infrastructure that must run with no book open**: harness runners, daemons' main loops, checks, builds, anything that has to judge pass or fail.
- Use a **C op** for anything that is real logic. Neither pal nor an event page should contain behavior; both only *call* it (READ, BLUEPRINT-EVENT-PAGES-GUIDE section 3: "the op is where all behaviour lives").

## 2. What each one is (READ)

| | Event page | Pal |
|---|---|---|
| Lives in | the book: `.../maps/<book>/<page>/ev/<id>/` or `events/<name>/event_pkg/pages/page_N/` | next to its owner: `ops/`, `harness/<name>.pal`, a manager's loop |
| Source a human edits | `event.ir.pdl` (a list of command nodes) | the `.pal` text itself (RISC-V assembly: `exec`, `sleep`, `j`, `halt`) |
| Started by | a **trigger** in `condition.pdl`: `player-touch`, `on-touch`, `ui`, `Autorun`, `clock`... | whoever launches it: `prisc+x <name>.pal`, a button, a daemon |
| Compiled? | yes: the events-hq manager turns the IR into `event.pal` plus one `cmd_N.sh` per node | no, it is already the VM's language |
| Vocabulary | the registry `#.ref/menu/event_commands.registry.pdl` (135 commands) | any `exec` of an op |
| Edited by | events-hq, blueprintz, or headless `bp_edit_op` | a text editor |
| Result reported as | rows in the page's `master_ledger.txt` and the game's `events.txt` | nothing by itself (see 4) |
| Needs a running game | **yes** (a daemon or window plays the page) | **no** |

## 3. When to use which

| You want to... | Use | Why |
|---|---|---|
| Make something happen when a character steps on a cell, clicks a button or a clock ticks | **event page** | that is exactly a trigger; the owner edits it in the editor and sees it play |
| Express "expected result and effect" for a place in a book (the RPG Maker step) | **event page** | the expected result is part of the book and travels with it to cdda, mcl, df |
| Give an AI player or modder a verb (open door, mine, fire) | **event page** calling the game's op | `verbs.pdl` pages already do this for rpg-pvp-hq and game-dev-hq |
| Run a check with no window open (source, config, build, a scratch house) | **pal harness** | it must work when nothing is running; a page cannot |
| Decide pass or fail | **pal harness** (`harness_verdict_op`) | an event page only writes rows; something has to judge them |
| Keep a long-running loop alive (a daemon tick) | **pal + op** | one loop, append-only ledgers, cheap when idle |
| Test with a deliberately broken copy (a mutant) | **pal harness** | scratch copies and exit codes live there |
| Compute, parse, draw, simulate | **C op** | neither of the above should hold logic |

Rule of thumb (MINE): if the owner could say "the door opens when you touch it", it is an **event**. If the owner could say "make sure the door still opens after I change the core", it is a **harness**. A good change usually has both: the event is the feature, the harness proves the event still fires.

## 4. Why events cannot replace the harness (RAN)

1. **No verdict.** The mcl touch page `city/ev/20` has `event.pal` containing only `halt` and a condition `COND | trigger | player-touch`. Its ledger shows it triggered, and also shows two earlier failures (no `event.pal` for 20; a missing `default_op.txt`) that no one was told about until someone read the file. A page reports by writing a line, not by failing.
2. **Needs a living game.** A page plays only while a board or daemon is up. A closed window means no page runs, so "the check ran and passed" cannot be distinguished from "nothing ran". The harness's whole point is that it runs and judges even when nothing is up.
3. **A page that does nothing still looks fine.** The touch page's "effect" today is "this page runs" (its own comment: `# expected: step onto 8,4. effect: this page runs.`). That proves the trigger fires. It does not prove a result. A real effect needs an op behind the page and a harness row that reads what the op wrote.

## 5. Why a harness is not enough alone (MINE, from the same session)

- A harness row lives outside the book. When the book changes, the row drifts silently (the edit-board book menu lost its rows on `claude` while `DONE.md` called it closed; found only when a check was written, `editor_skip`, 2026-11-01).
- A feature that exists only as C plus harness rows cannot be edited by the owner or driven by an AI player the way an RPG Maker event can. The house goal (RPG Maker editing then play, on every book) needs the behavior to be a page.

## 6. The combined pattern (synergy, reuse, training)

1. **The feature is an event page** in the book (trigger, nodes, expected result written in the `event.ir.pdl` comment).
2. **The page calls an op** that writes the real effect into the game's state files.
3. **A case file (`cases/<name>.pdl`) proves it:** copy the book into a scratch house, play the page by its trigger, then `EXPECT_*` the ledger row and the state change. Add one mutant (break the op, or delete the page) and confirm exactly that row fails.
4. **The same page is the replay.** Because the page lives in the book, the harness, the editor and an AI player all run the same file. Nothing is written twice.
5. **Training and reuse:** an event page is data (`.pdl`), so the bank/attrition pipeline can read pages as examples of "trigger, condition, effect", and a harness row is a labeled outcome. Both are text, both append-only, both work for every book that uses the page file (rpg-edit, mcl, cdda, df).

## 7. What to avoid

- Writing behavior in a `.pal` or in a page's `cmd_N.sh`. Put it in an op.
- A new shell script as a harness (house rule; harnesses are pal plus cases).
- A harness for something one `EXPECT_` row in an existing case can say (README table is the index).
- Trusting a page's ledger line as a pass. Read the effect it was meant to cause.
- Judging a live state file while the window is closed; those files go stale (RAN 2026-11-01: `ui.txt` still showed an old menu).

## 8. Open items

- No verb yet plays a named page in a scratch copy and returns its ledger row to the case. Proposed as one verb to replace three (see `21.drive-doc-fix/HARNESS-REUSE.md` section 2). Not built.
- The mcl touch page has no effect yet; the first real event-plus-harness proof should give it one.
