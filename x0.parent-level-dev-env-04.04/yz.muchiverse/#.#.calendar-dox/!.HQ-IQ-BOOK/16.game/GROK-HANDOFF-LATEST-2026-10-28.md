# 🤖➡️🤖 GROK HANDOFF — the LATEST and most focused (2026-10-28) 🎯
### From Claude (Sonnet 5.5) · you work on branch `grok` · your specialty here: **MCL (Minecraft-like) 🧱**

> 🏷️ **SAID** = the owner's words · **RAN** = Claude ran it · **MINE** = Claude's proposal · **OPEN** = ask the owner, don't guess.
> 📚 This file **supersedes the db part** of `HANDOFF-2026-10-28-MCL-BOOK-TO-GROK.md` (the MCL slices there still stand; slice 3 is rewritten below). Also read once: `GROK-HANDOFF-2026-10-27-VISUALS-AND-ORGANIC-TRACK.md`, `RULES-mcl-hq.md`, `GAME-MECHANICS-SPRINT.md`.

---

## 🧭 0. In one minute

- 🏠 **mcl-hq is a BOOK on the owner's desk** (`xyzfs/users/<uuid>/home/livedesk/sessions/mcl-hq/`), 1 desk page, **37 event pages** (blueprintz flags=0), `book_check --events` **16 pass / 0 fail** (RAN 2026-10-28).
- 🗄️ **Each game now has its OWN db layer over a shared trunk** (new today). Edit a game's names **without touching the other 10 games**.
- 💎 **Pieces (pets) have ONE identity.** Do **not** mint, rename or delete pets. mcl-hq's pet `rex` has a fixed uid. (Section 4.)
- 🚫 Do not touch `petslike_core.c` (11 games share it), the chain, `xyzfs/users`, or the renderer.
- 🧪 Everything you do ends with a **pal harness + a mutation test + a before/after picture or state file** — see section 6.

---

## 🧱 1. Your 3 slices (MCL) — in order, stop and report after each

| # | 🎯 Slice | ✅ Proof (must be real) |
|---|---|---|
| 1 | 🚶 **`walk`** in mcl-hq (`RULES-mcl-hq.md` says `todo:walk`): make it call `do_step` | harness case in `&.widgits/_shared-lib/harness` (cases pdl + `.pal`, **never a new `.sh`**): one `walk` changes `state/trainer.txt` by **exactly one cell**; a wall blocks it; the EVENT row is written. **Stop, report.** |
| 2 | ⏳ **Gravity** for sand/gravel as **rows in the rules page, not C** | harness: remove the block under a sand cell → it falls **one cell per tick**, a ledger row per move. **Mutation-test it.** |
| 3 | ✏️ **Events + names through blueprintz / the db** (the owner's priority) | on a **scratch copy first**: open one mcl-hq event page in blueprintz, rename an entity through the db, save → `bp_scan` flags=0, `book_check --events` still 16/0, the new name shows in the window. Before/after PNG (`dump_frame_png_op.+x`) + the state file. |

### 🔎 Slice 3 — the question I answered for you (RAN/READ)
- 🗺️ **Map / room names** live in the **game file** `@.apps/mcl-hq/rooms.pdl` (`ROOM | city | Overworld | …`, 6 ROOM rows) — **not** in the db.
- 🧑 **Entity / actor names** live in the **db**: `ACTOR | name | …` in `actors.pdl`. The cast of mcl-hq (hero + `DsrRpgParty` pets Rex Gus Pip Orin Maren Fenn) is read from the db — **`DsrRpgParty` in `system.pdl` is shared by 5 games** (cdda-hq df-hq dsr-rpg gta-hq mcl-hq).
- ⚠️ **If you edit the TRUNK (`&.widgits/db-hq/data/actors.pdl` / `system.pdl`) you change 5–11 games at once. Never do that.** Branch the file into mcl-hq's layer first.
- 🌿 **How to branch:** `@.apps/mcl-hq/ops/+x/mcl_hq.+x db status` · `db edit <file>` (copy-on-write into `@.apps/mcl-hq/db/`, stamped with the trunk's hash) · `db trim` (drops layer files identical to the trunk). Full description: `PER-BOOK-DB-2026-10-28.md` sections 1–7.
- 😬 **Today the layer is PER FILE**: a branched `actors.pdl` is a full 777-line copy (all 30 actors). A **per-record** layer is planned (Claude's job). Until then: branch only when you really must, on a scratch copy, and say so in the commit.
- 🔔 ✅ **FIXED 2026-10-29:** `db status` no longer calls `cast.pdl` an override (it prints `0 file(s) overridden` for a layer that holds only the cast). Harness `pet_cast` case C7. The ten game binaries were rebuilt; a game process already running keeps the old code until it is restarted.
- 🆔 ✅ **2026-10-29: the core now reads `db/cast.pdl` FIRST** for a pet's uid (`cast_uid` in `petslike_core.c`); `state/uid_<slug>.txt` is only the fallback for a pet with no cast row. Still true: **do not touch `petslike_core.c`** (11 games share it), mint nothing, ask the owner for new pets.

---

## 🗄️ 2. The db window follows the open book (RAN, but ⚠️ never opened live)

- 🔄 `&.widgits/_shared-lib/ops/+x/db_publish.+x <house> [--book <id>] [--dry]` publishes the **open book's** db to the db window's mirror files; `db-hq-pal/button.sh` runs it before the window opens.
- 👀 `--dry` writes nothing: use it to see what a book would show.
- 🚫 **Do not launch the db window or any game board** unless the owner says go (the machine froze at 15:21 on a board launch; cause unknown). If you do: `nice -n 15`, one process, machine cool.

---

## 💎 3. Pieces — read this before you touch any pet

- 📖 Full design: `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md`. Short version:
  - 🧬 **Actor** = db template (cheap) · 💎 **Piece** = one owned instance with a **uid** · 🎬 **Appearance** = that piece inside one book.
  - 🖥️ Pieces live in the **Master Pal List** (`xyzfs/users/<uuid>/home/livedesk/pals/`, **79 pals**, user data). Boards only *visit*.
- 📇 mcl-hq's cast row (committed, `@.apps/mcl-hq/db/cast.pdl`): `CAST | 692437f1cd08f7afa973c4e913058bb5 | rex | pet | 0 | mcl-hq`. That uid is `mcl-hq_rex` in the list.
- 🔍 **`piece_check <house>`** is read-only: run it **before and after** your work. Today: errors **2**, warnings **23** (the 2 errors are empty ids in *other* games). **If your change raises the error count, stop.**
- 🪙 `piece_mint` exists, **but you do not run it** (it writes the owner's real `pals/`). Need a new pet or an id change → **ask the owner**.
- #️⃣ `hash_op wallet <uid>` = the wallet id of a piece (`e` + 24 hex of SHA-256). Use it; don't write your own hash.
- 🧩 **House style (SAID today): no shared headers.** Anything several programs need is a **small op they call**, an **event page**, or a **pal** — not a header and not a copy.

---

## 🧰 4. Rules (from `AGENTS.md`, the ones that bit before)

- 🌿 Branch **`grok`** only. `git add <path>` per file; commit with `git commit -m … -- <your paths>`; read `git diff --cached --name-only` first. **Never** `git add -A`, merge, push, stash, reset, force-delete unprompted.
- 🛡️ **Owner data** (`xyzfs/users`, `sessions/`, `pals/`): back up (tarball + sha256 **outside** the repo and outside `/tmp`), rehearse on a scratch copy, verify by hash, restore test changes after proof.
- 🧊 `nice -n 15`, one heavy process at a time, say what a feature costs (idle + active, browser open) in the commit.
- 💸 Free models only; **never loop to a 429.**
- 🔢 **Never report a number you did not observe.** Confirm an artifact exists before trusting its count.
- 🧪 Negative-test every check: `harness_case_op` **silently ignores unknown verbs**.

---

## 🪤 5. Traps that cost Claude real time today

1. 🎭 A mutant that **never applied** looks like a pass → assert the replace happened exactly once.
2. 🧱 A mutant that fails **only the compile gate** proves nothing → keep mutants compiling.
3. 🕳️ A missing **boundary test** (one more than the max) lets a mutant survive.
4. ✍️ `WRITE` in a case **overwrites**; there is no `APPEND` verb (an unknown verb is ignored silently!) → write a side file and `cat >>` it in a `sh -c` step.
5. 🔤 In a case file a literal `|` must be `\p`; a `RUN` has **no shell**.
6. 📦 `cmd | head` can kill a build or hide its error → build, then read.
7. 🧷 Quote paths with `&` and `@`.
8. 🛑 A scripted `rm -rf` loop is blocked by the safety check — don't route around it; use a tested verb.

---

## 🧪 6. How to check your work (copy-paste)

```sh
cd "…/yz.muchiverse/44.xyz.01.00"
# book + events
"&.widgits/_shared-lib/ops/+x/book_check.+x" <pchq_root> --house "$PWD" --page <page.pdl> --events
# harness (scratch house, builds ops with -Werror from the repo source)
cd "&.widgits/_shared-lib/harness" && /home/no/scratch-gab/bin/prisc <name>.pal   # then read results/<name>.txt.verdict.txt
# identity audit (read-only)
"&.widgits/_shared-lib/ops/+x/piece_check.+x" "$PWD"
# what would the db window show for a book (writes nothing)
"&.widgits/_shared-lib/ops/+x/db_publish.+x" "$PWD" --dry --book mcl-hq
# pictures
"&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x" <win> out.png
```
- 🗂️ Case format and every verb: `harness/README.md`. Existing examples to copy: `cases/piece_mint.pdl` (79 expectations), `cases/db_publish.pdl`, `cases/page_follow.pdl`.

---

## ❓ 7. Ask the owner (do not guess)

1. 🗺️ Should the other 5 mcl-hq rooms become **desk pages** (needs a page-per-room export)?
2. 🔓 May a live window be opened for a picture (which one, when)?
3. 🧬 Is the **per-file** db layer acceptable for slice 3, or wait for the per-record layer?
4. 🆕 Any **new pets/entities** for MCL → they must be minted by Claude/owner, not by you.

---

## 🧭 8. After the 3 slices (MINE, only if the owner agrees)

- 🧱 MCL modes: chunk/block placement as rows, crafting via `recipes.pdl`, day/night via `time.pdl` — all as **data/event pages**, C only as a fallback.
- 🌆 Longer horizon (owner 2026-10-28): civilizations of hireable/capturable AI NPCs (cdda/df/mc/gta/board modes) — report in XO `oct-28.5`; **5 open decisions** there.

🌙 *Small steps, real evidence, nothing guessed.* 🚀
