# rpg-pvp-hq revival: a TOY on the toys menu, a BOOK on the desk (plan, 2026-10-29)

Status: **plan for the owner. Nothing in this document has been built or changed.** Labels: **SAID** = the owner's words · **READ** = read from files today, not run · **RAN** = I ran it and saw the result · **MINE** = my proposal, needs the owner · **OPEN** = a decision, do not guess.

This page is about the *shape* (how pvp becomes a toy and a book). The *game* (city, guns, duel, AI, building, steps P0 to P9) stays in `RPG-PVP-HQ-DESIGN.md`; read that first or after. Nothing there is replaced.

---

## 1. What the owner said (SAID, 2026-10-29)
- "We're gonna revive pvp." It "is supposed to be under toys and is supposed to be a book."
- Background that still holds (SAID 2026-10-28): games are BOOKS on the desk, each with its own db layer, and a pet is only an appearance of a piece that lives in the Master Pal List. House style: no shared headers, prefer ops / events / pals.

## 2. Where pvp stands today (READ, plus one RAN)

| fact | value |
| --- | --- |
| what it is | `@.apps/rpg-pvp-hq`, a **rename-copy of cdda-hq made 2026-10-20**, one 1,673-line C file (`ops/rpg_pvp_hq.c`) plus a 321-line city generator (`ops/rpg_pvp_city.c`) with its own viewer window |
| history | 12 commits; real work 10-20 and 10-21 (match session, seats, a Network tab, Downtown city, INT cameras); later commits are sweeps and a 10-27 ledger tidy |
| last played | its `state/` files date from 10-20 and 10-21; `state/events.txt` is 753,462 bytes |
| the other eight copies of cdda-hq | became **skins** of the shared `petslike_core.c` on 10-21 (`mcl_hq.c` is 500 lines, `cdda_hq.c` 473, the core 1,765). **pvp was left behind as a fork.** |
| unique to pvp | `downtown.pdl/.txt` (the city), the city generator and its viewer, `net/` (an outbox file) and the **SESSION / SEAT rows** (`human / agent / peer / closed`, peer seats are an outbox line only, **no real network**), and the verbs `equip fire pickup reload` that cdda-hq lacks |
| missing vs a converted game (mcl-hq) | `board/`, a `-pchq` folder (the 3D board), a `db/` layer and `cast.pdl`, `life.pdl`, `toolbar.pdl`, a `-board` toy sibling, a harness case |
| pieces | pets `joss` and `rex` have uids only in `state/uid_<slug>.txt`; `piece_check` (RAN today): `not_durable rpg-pvp-hq files=2`, `no_master rpg-pvp-hq joss`. The 10 converted games read `db/cast.pdl` first since commit 74376a813; pvp does not (own copy of `ent_wallet`). |
| toy entry | it has a 4-line `toy.pdl` (title, launch). The taskbar manager lists any `@.apps/<name>/` folder that has a `toy.pdl` (`khtpm_taskbar_manager.c` ~4963 to 4984). Whether pvp **shows** in the live toys menu was **not checked**. |

## 3. What "toy" and "book" mean in this house (READ)

**Toy.** A folder under `@.apps/` with a `toy.pdl`. The taskbar scans for its presence. A full game toy also carries the per-game constants the core reads (`sibling`, `hero_name`, `party_row`, ...), and most games have a second toy, `<game>-board`, for the 3D board window. pvp has the first part only in a thin form and no board toy.

**Book.** The BOOK:PAGE standard (`18.pc-hq/PAGE-FILE.md`; plan in `GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md`). The recipe that put nine games on the owner's desk (RAN 2026-10-28): `game_page_export` writes the game's page from its live state, `book_check --events` must say 0 FAIL, blueprintz must scan the event pages with flags=0, then the book is installed on the desk after a backup of the sessions tree, the page follows the game (`page_follow`) and Synch reaches the board. `game_page_export` reads the **core's** state files and the game's `<name>-pchq/` folder. **pvp has neither**, so today it cannot become a book without converting first.

## 4. The main decision: make pvp a skin of the core, not a fork (MINE, OPEN)

**Why.** Everything built since 10-21 landed in the core, and pvp has none of it: the per-game db layer, `cast.pdl` identity, `page_follow`, the life layer, the toolbar, the 3D board, the book export. Each future fix would need a second copy in pvp. That is exactly the "one that didn't get converted" problem. A fork also cannot be a book for the reason in section 3.

**Options.**
| | what | cost | verdict |
| --- | --- | --- | --- |
| A | **Convert pvp to a skin** (like the other eight) and carry its unique parts as data, event pages and small ops | one conversion slice, then everything else comes along | **recommended** |
| B | Keep the fork and add the book pieces by hand | a second copy of every shared feature, forever | not recommended |
| C | Leave it and start a new "pvp" as a fresh skin of the core | loses the city, seats and verbs unless copied anyway | same work as A without the history |

**What carries over, and where it goes (MINE).**
| pvp part | new home | note |
| --- | --- | --- |
| window, INT, shop, clock, pets walking | the **core** (nothing to carry) | already identical to the other skins |
| Downtown city + generator | **room/page data** (`rooms.pdl`, `downtown.pdl`) and the generator stays a **separate op** with its viewer | not core code |
| SESSION / SEAT rows (match, human / agent / peer / closed) | a **small op called by events** (`rpg_pvp_match`), not core code | built 2026-10-21 to the house session pattern (`RPG-PVP-HQ-SESSIONS.md`); no other game has it (checked: 0 mentions in the core and in mcl-hq / cdda-hq); stays outbox-only, real cross-machine play is paper only |
| `equip fire pickup reload` | **event pages** through the core's existing `fire <verb>` mechanism, with the old C kept as fallback until the pages are proven | same pattern as death / respawn in rpg-space |
| `tuning.pdl`, `recipes.pdl`, `time.pdl`, `voices.pdl`, `catalog.pdl` | stay as files in the folder | read as today |
| `state/events.txt` (753 KB) | copy aside before any change | the core's append drops silently over 4 MiB; rotation is an owner decision already open |

Rule from the current Grok handoff still applies: **do not put pvp-only code into `petslike_core.c`** (11 games share it).

## 5. Order, each step ends with something visible (MINE)

| step | what | proof that can fail |
| --- | --- | --- |
| R0 | **Backup and rehearse.** Tarball + sha256 of `@.apps/rpg-pvp-hq` outside the repo and outside `/tmp`; do the conversion in a **scratch copy** first (house rule: rehearse in staging, additive to live) | file count = tarball; live folder hash unchanged after rehearsal |
| R1 | **Skin.** New `rpg_pvp_hq.c` in the `mcl_hq.c` shape (`PET_APP`, `PET_OP`, `PET_SKIN`, one include of the core), `toy.pdl` constants, seed `board/`, `-pchq`, `db/`, `life.pdl`, `toolbar.pdl` from the mcl-hq pattern; `build_rpg_pvp_hq.sh` (see `PETSLIKE-FRAMEWORK-PLAN.md`, `PETSLIKE-NEW-ENTRIES.md`) | the **per-game verb-script diff gate** used for the first eight: the old fork and the new skin run the same verb script and the shared verbs leave the same state files; a harness case (pal + cases) with a mutation test |
| R2 | **Toy.** Full `toy.pdl`, a `rpg-pvp-hq-board` sibling, confirm both appear in the toys menu | a picture of the live toys menu (one process, niced, owner OK first) |
| R3 | **Book.** `game_page_export`, `book_check --events` 0 FAIL, blueprintz flags=0, then install on the desk after a backup of `sessions/` | `book_check` output, `ls` of the desk books before and after, files byte-identical to backup |
| R4 | **Pieces.** Adopt `joss` and `rex` into the Master Pal List with `piece_mint --adopt-uid` (ids unchanged), commit `db/cast.pdl` | `piece_check` errors not raised, warning `not_durable` for pvp gone; pals backup first |
| R5 | **The game.** Resume `RPG-PVP-HQ-DESIGN.md` at P1 (city on the 2D board), then guns P2, duel P3, AI P4, 3D P5 | each step's own proof in that doc |

R1 comes first because R2 to R4 depend on it; R4 can run earlier if the owner wants the ids made durable before anything else.

## 6. Risks (MINE)
- **A conversion can silently lose behaviour.** That is what the diff gate is for; seats and the Downtown door pair are the parts most likely to differ.
- **Owner data.** The desk install (R3) and the pals (R4) touch real user data: backup, rehearse in scratch, verify by hash, restore test changes after proof.
- **Board launches froze the machine once** (2026-10-28, cause unknown). No board or window is opened without the owner's OK; niced, one process.
- **`rex` is already five different pieces** across the games (same name, different ids). Which one pvp's `rex` is, is an owner call (section 7).
- **Two agents, one checkout.** Explicit pathspec commits only; this work touches `@.apps/rpg-pvp-hq/` and does not need `petslike_core.c`.
- **The two empty ids** (cdda-hq `harold`, tcg-rpgce `pal_1`) and `blue` / `nico` are not part of this and stay open.

## 7. Decisions for the owner (OPEN)
1. **A, B or C** from section 4? (I recommend A.)
2. Is pvp's **`rex` a new piece, or the same as another game's `rex`?** Default if you say nothing: a separate piece, ids unchanged. `joss` appears only in pvp.
3. **Who builds R1:** me, or hand it to Grok (its current slices are MCL)? The conversion touches the shared pattern, so I lean to me.
4. **Name under toys:** keep `rpg-pvp-hq`, or a friendlier title? (Presumably the `title` row of `toy.pdl` is what the menu shows; I did not check.)
5. The four questions already open in `RPG-PVP-HQ-DESIGN.md` section 7 (2D first or straight to 3D; does a dead pet respawn; where the Doom assets live; whether building waits until P8).
6. May a window be opened for the R2 and R3 pictures (which one, when)?

## 8. Not verified
- That pvp **currently appears** in the live toys menu. I only read that `toy.pdl` presence is the rule.
- That the old fork **runs today**. Nothing was launched; the last activity in its `state/` is 2026-10-21.
- What `rpg_pvp_hq.c` does beyond the parts named in section 2; I read its header, its seat/session lines, its includes and its verb list, not all 1,673 lines.
- The line numbers are from today's read and move with every edit.

## 9. Related documents (READ)
- `RPG-PVP-HQ-DESIGN.md` - the game: city, guns, duel, AI, building, steps P0 to P9, art question, 007-goldeye.
- `RPG-PVP-HQ-SESSIONS.md` - match session, the Network tab, local and networked seats (built 2026-10-21; cross-machine play still blocked by the palnet limit it describes).
- `RPG-PVP-HQ-EVENTS.md` - what "event" means here and what pvp uses (2026-10-20: only the copied chrome events existed, no shooter event page yet).
- `GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md`, `HANDOFF-2026-10-28-MCL-BOOK-TO-GROK.md` - the book recipe and a finished example.
- `PETSLIKE-FRAMEWORK-PLAN.md`, `PETSLIKE-NEW-ENTRIES.md` - how a game becomes a skin of the core.
- `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` - the piece rules behind R4.
