# The hai / pals / books reuse and compatibility guide (2026-11-24; the standing contract for this line of work)

Owner, 2026-11-24: *"id like to have this be the ultimate guide for future design for maximum reusability and compatibility so look over arch and suggest anything b4 going forward, your responsible for future behaviors so doc."* This file is that guide. It is a CONTRACT (what any agent, me included, must do) plus an AUDIT of what I built before reading the architecture standards (what breaks the contract, in fix order). Labels: **READ** (file read today), **BUILT** (ran with a case that can fail), **DESIGN**, **OPEN** (owner decides), **NOT VERIFIED**. The hai side of the design: hai repo `book/25` to `book/27`.

## 1. The contract (every rule is one the house already set; where I deviate I say so)
Sources READ today: `STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, `NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `TRUNK-AND-BRANCH-SHARED-DATA-STANDARD-2026-11-16.md`, `EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`, `PAL-VS-EVENTS.md`, the decision log (D1 to D121), AGENTS.md.
1. **One rule = one pure module + one thin CLI op + one `COMMAND` row in `#.ref/menu/event_commands.registry.pdl` + a harness case + an event page** (STANDARD section 2). A rule that is not an op behind an event page is not done (D92).
2. **The ledger is the only authority** (NO-GHOST-DATA): rows are `EVENT|ts|actor|verb|target|at|...`; `ckpt` rows pin a state hash; replay refuses a forged row; every other file is declared PROJECTION, CONFIG or PREFERENCE in `state.pdl`.
3. **Numbers are data**: RULE rows inside the rules hash, never constants in C.
4. **Shared data is trunk + branch, never a copy** (TRUNK-AND-BRANCH, op `trunk_op` is BUILT): defaults on the trunk, a per-owner branch holds only changes, latest row per key wins, `private` rows never leave their owner.
5. **Every behaviour-deciding fact has ONE authority** and every other view is a projection (design-choice question 1).
6. **A harness case that can fail sits beside every op, proven by mutants; anything a person sees is checked in the real window and the picture opened.** Parity cases (C vs JS) need steps that force carried remainders, every boundary and every palette slot (traps hit 2026-11-23/24).
7. **Runtime files have an enforced cap and a row in RUNTIME-FILE-GROWTH-STANDARD**; a commit that adds a writer states bytes per hour.
8. **Processes are registered; heavy work asks first (`cpu_gate`)**; features state idle and active cost with a browser open.
9. **No Python, no new `.sh`**: pal + case `.pdl` + C ops. JavaScript is allowed only as the web/browser port of a rule that already has a C op and a parity case (my exception, section 2 item 1).
10. **No symlinks in code** (owner 2026-11-18); **no shared-code change for one project** (khtpm shared renderer, `petslike_core.c`): a generic change only, behind a fallback test that every other book is byte-unchanged.
11. **Private data stays private** (`xyzfs/users`, keys, wallets); a test never calls the network or a paid model.
12. **Do not move or leave the owner's live state changed**: record before, restore by hash after, say what was left (hai-pal editor launch left the saved book changed: handoff section 4).
13. **Every design choice answers the five questions and gets a decision-log row** (AGENTS.md). Today's row is pending (section 4: the log file has other agents' uncommitted hunks).

## 2. AUDIT: what I built, against the contract (fix in this order; each fix has a case)
| # | What I built | Breaks | Fix (DESIGN) |
|---|---|---|---|
| 1 | `hai_world.c` and `world.js` are TWO implementations of the rules (C + JS) with a parity case | contract 1 (one implementation) | Keep for the web, but state it: JS is the port, C the authority, parity case is the guard. Better when ready: one pure `rule_world.c` text-included by the op, and the web port built from the same source (the house vendors wasm3, D121) so there is one implementation; revisit before the third rule set. |
| 2 | World rows `SPAWN|name|...`, `MOVE|...`, `REORDER|...` with no actor, no time, no hash | contracts 2, 5 | Move to `EVENT|round|actor|verb|target|at|result` (owner chose this 2026-11-24), add `ckpt` rows, replay refuses a forged row. The actor column is what lets "the master did it as the user's hands" be a row. |
| 3 | Grid size, wall rules, name length, MAXA, caps hard-coded in C and JS | contract 3 | RULE rows read by the op and the port (and part of the rules hash). |
| 4 | No `COMMAND` registry rows, no event pages: the verbs are not visible in Blueprintz | contract 1 | One registry row and one event page per verb (SPAWN, MOVE, TELEPORT, REORDER, later FEED, PLANT), as rpg-space does. |
| 5 | `hai_pet.c` re-implements needs math the house already has (`survivor_op`) | contract 5 | `survivor_op` stays the oracle (parity case already agrees); do not grow a third copy. Decision: reuse `survivor_op` with `needs.pdl` RATE rows for the hai-pal book (quarantine release asked), keep `hai_pet` only as the web port. |
| 6 | Bodies (organic, robot, cyborg), biases, permissions as per-agent folders | contract 4 | Bodies = trunk defaults; each agent's overrides = a branch of `PUT` rows through `trunk_op`; permissions and biases are `private` branch rows. Never copy a body file per agent. |
| 7 | **Party order has two authorities**: my ledger `REORDER` and the house DB `SYSTEM|PetParty member1..6` (the order `petslike_core.c` reads for the left bar and Play) | contracts 2, 5 | One authority: the ledger row. The DB member order becomes a PROJECTION written by a named verb from the ledger (or the book reads its order through a generic hook), never edited by hand. OPEN: owner decision, default = ledger authoritative. |
| 8 | `world.ledger` grows without a cap | contract 7 | Add a cap and the second guard (rotate by `ckpt`), a row in RUNTIME-FILE-GROWTH-STANDARD, bytes per hour stated. |
| 9 | hai-pal book has no `GAME | driver`, `book_check` A1 to A10 not satisfied | contracts 1, 6 | The petslike-app route first (hybrid, book 27 sections 6 and 7), then conformance. |
| 10 | Left pals bar: the party belongs to the APP, not the open book (READ `petslike_core.c` `load_party`, line 486) | contract 10 | Hybrid agreed 2026-11-24: (a) hai-pal as its own petslike app with its own `db/system.pdl`; (b) later a generic, fallback-tested rule that the editor uses an open book's own `PetParty` if it ships one. Scratch house first; nothing in the owner's live editor. |
| 11 | Terms: "master" and "slave" are the owner's words; the house vocabulary for a party leader is "first living actor" | compatibility | Keep the owner's words in the UI and docs; in rows use the neutral `slot` (1 = master) so a rename is a label change, not a data migration. |
| 12 | The `m` move mode uses `m`, arrows move the row (owner decision) | contract 10 | Generic nav behaviour for ANY ordered list (party, TSC sort list), so build it once in the shared nav as a fallback-tested change, or as data first if the renderer already offers a hook (NOT VERIFIED). |

## 3. How I will behave from now on (my commitments)
1. Before any new feature in this line: read this guide and the five standards, answer the five design-choice questions in one paragraph, and say which contract rule it touches.
2. Build in this order: pure rule, op, registry row, case with mutants, event page, then the real window frame opened by eye. No window work before the rule has a case.
3. Never hard-code a number or a mechanic; never add a second authority; if two stores of one fact exist, I name them and ask which wins.
4. Reuse by default: search the house for an op that already does it (`survivor_op`, `trunk_op`, `ui_mode_op`, `book_check`) before writing one; record what I found.
5. Anything shared (renderer, petslike core, nav): ask, read, make it generic, prove every other book unchanged, restore live state.
6. Report what is BUILT, what is DESIGN and what I did not verify, in those words; never report a number I did not observe.
7. Keep this guide current: when I find a house rule that contradicts it, the house wins and I fix this file in the same commit.

## 4. Decisions for the owner (defaults in brackets; nothing blocks the next step)
1. Party order authority: ledger [yes] or DB member order?
2. One implementation of the world rules shared by C and the web (wasm) [later, before the third rule set] or keep C plus JS port with parity [now].
3. Reuse `survivor_op` for the hai-pal needs [yes; needs the quarantine release] or keep `hai_pet`.
4. Does the master's memory follow the slot [yes] or the pal.
5. A decision-log row for today's choices: the log file holds other agents' uncommitted edits, so I have NOT edited it; the row text is section 2 items 7 and 10 plus the hybrid party decision. Say the word and I will add it once that file is clean or commit it by index.

## 5. Compatibility map (what to call what; use these, do not invent)
| Concept | The house thing |
|---|---|
| a ledger row of what happened | `EVENT|ts|actor|verb|target|at|...` (+ `ckpt`) |
| a rule | pure `rule_<name>.c` + `ops/rule_<name>_op.c` + a `COMMAND` row + a case + an event page |
| shared data with per-owner changes | `trunk_op` (L1 file, L2 record, L3 row ledger) |
| a book | a page file + `rooms.pdl` + `game.pdl` (`book_check` A1 to A10, B1 to B4) |
| the party | DB `SYSTEM|PetParty member1..6` + `actors.pdl` + `pals.pdl`, shown by the pets bar |
| the left slot | `BAR` rows (`layouts.pdl`, `ui_mode_op`), exclusive group `left` |
| a pal's speech to a pal | `net_say.pdl`, `pet_say` on the game clock (shipped OFF) |
| user-visible needs | `survivor_op` + `needs.pdl` (RATE) + `itemdata.pdl` (USE) |

## 6. Where this lives
This file is the authority. hai repo `book/27` section 8 points here; AGENTS.md routes the task type "hai, pals, books, party order, embodied agents" to it. If you add a hai-line doc, add a row to section 5 or a line to section 2, not a parallel guide.
