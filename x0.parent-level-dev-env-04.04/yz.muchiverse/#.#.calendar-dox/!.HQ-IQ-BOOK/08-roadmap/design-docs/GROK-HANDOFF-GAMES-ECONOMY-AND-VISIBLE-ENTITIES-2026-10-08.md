# Grok handoff: game clones, the economy, and entities you can SEE (2026-10-08)

Written by Claude (manager role) for Grok. **This replaces `GROK-HANDOFF-2026-09-02.md`.** That one was 6 KB, pointed you at other documents for the real plan, gave an order ("status graph first") without a definition of done, gave no commands, and never said how to see or drive the desktop. This one is meant to be enough to start work today without asking anyone.

Status marks used throughout: ✅ built and verified, 🟡 built, not fully verified, 📐 designed or noted only, ❌ nothing exists. Every "exists" claim below was checked against files on 2026-10-08; anything I did not check says **unverified**.

---

## 0. Read this first (the contract)

1. **Your job.** Build playable, *visible* clones of several games on this house's desktop, plus a shared multiplayer economy (stocks, auctions, bidding, chain-priced goods), and be able to **demonstrate each one on screen to the owner**. You orchestrate; you are not expected to write everything yourself (section 6, delegation).
2. **"Done" means all five of these, shown with evidence** (the house rule: a clean compile is not evidence):
   1. A fresh build from a clean worktree.
   2. A pal harness (section 5.3) with a **mutant that fails it**, then passing.
   3. A real run of the real window or entity, driven through the **relay** (section 4), not xdotool.
   4. A **frame dump or screenshot you actually looked at**, and a text state file that agrees with it.
   5. A short note stating what was *not* verified. Unverified is allowed; hiding it is not.
3. **The owner's real goal is "visually functioning entities."** A feature that exists only as files, ops and harnesses is not finished. The recurring failure of this project is building the engine and never the picture. Example: the Eden game has run 62,498 game days of farming, building and talking in a history file, and you cannot see any of it. Do not repeat that: every milestone below ends in something a person can look at and click.
4. **Everything you write will be re-verified by a fresh build and run before it is merged.** Write evidence so that check is easy.
5. **If something here conflicts with `AGENTS.md`, `AGENTS.md` wins.** If code conflicts with a doc, trust the code and fix the doc.

---

## 1. What the owner wants, and what we have actually written down

The owner's list: a Pokemon-style 2D RPG on the desk and in 3D in pc-hq; Civilization; Minecraft; CDDA (Cataclysm: DDA; the house has a mutaclysm port, directory `101.mutaclsym…`); later a 2D/3D "phymoji" GTA and a 007-style game; and a **multiplayer economy**: stock market, auctions, goods bidding, blockchain prices.

**Honest coverage as of today:**
- ✅ Written down with a plan: RPG Maker, Minecraft, CDDA, Civ, Pokemon, GTA, in one table: `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md` section 5 (what tileset, what events, what is blocking). GTA's row says "do not start without a tileset PDL."
- 🟡 Written down separately, no unifying plan: stock market (WSR, a working game; the XOD tournament drives it with agents), auctions (`AUCTION-SCREEN-DESIGN.md`, design only), blockchain (`041.pal-chain`: faucet, escrow, miner built), play economy (`PLAY-ECONOMY-POT-FAUCETS-DESIGN.md`, partial), DSR (a second stock-like economy, `DSR-*` docs).
- 🟡 **007: a standalone prototype exists, but no design document and no link to the engine.** I first said "nothing" because I searched only the book; the owner pointed me to `44.xyz.01.00/007-goldeye+01.00/` (verify-absence mistake, corrected). It is a voxel GoldenEye split-screen deathmatch in C/OpenGL (freeglut), one `src/main.c`, last log 2026-07-28: 100×100×28 island map, 4 biomes, tanks and helicopters, K/D HUD, first/third-person camera, run with `sh button.sh compile|run|kill`. It is **not** a khtpm window, entity or event game; treat it as reference for rules and map generation, and rebuild it on the house engine (steps 13 below). Needs a one-page design from the owner first.
- ❌ **No single document connects these games to one shared engine and one build order.** This document is that connection; sections 2 and 7.

The full per-doc status (what exists, what is next, who can do it) is `DOC-STATUS-AND-PATHS-FORWARD-2026-10-08.md` in this folder. Read its rows for the docs you touch.

---

## 2. The shared engine you will build on (do not rebuild any of it)

All the games reuse the same pieces. Build a game by *composing* these and adding the smallest missing piece.

| Piece | What it gives you | Where | State |
|---|---|---|---|
| **khtpm renderer** | draws every window from a `.xhtpm` + `.css`; live-reparses on file change | `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` (shared, do not fork) | ✅ |
| **Manager + projection pattern** | a compiled C "manager" owns logic and writes a text projection the renderer shows | any `&.hq-apps/<name>/` e.g. `csv-lab-hq`, `concept-bank-hq` | ✅ copy this shape |
| **Entities** | folders under the user's `pals/` with `meta.pdl` (METHOD rows = menu), `menu.chtpm`, `event_pkg`, `inventory`, `stats.pdl`, `history.txt`, phone, word bank | `xyzfs/users/<uuid>/home/livedesk/pals/` (live user data, section 8) | ✅ |
| **Events (RPG Maker style)** | command registry (934 lines), events-hq editor, common events | `#.ref/menu/event_commands.registry.pdl`, `&.widgits/events-hq` | 🟡 many commands are state files, not gameplay |
| **Game clock** | day tick, recurrence, catch-up, pause, daemon | `&.widgits/livedesk-clock` (`lc_clock`) | ✅ runs; save/restore of clock is step 5 of its doc, unbuilt |
| **Game conductor** | a "button" entity that starts/stops/saves a game; Eden is the example | `&.widgits/eden/` (`eden_op`, `event_page_op`) | ✅ works headless, ❌ not visible |
| **Save slots** | save/load a game tree plus clock | `game_slot_op`, `game_snapshot_op`, `SAVE-SLOTS-DESIGN.md` | 🟡 v1; restore model undecided |
| **pc-hq board (2D/3D)** | tiles, camera, 3D raymarch, placer | `@.apps/piececraft-hq`, `&.widgits/board-viewer` | ✅ camera works; 🟡 3D is slow (GPU daemon landed, ~17 fps live) |
| **Tilesets / palettes** | RPG Maker, piececraft/Mineclonia, CDDA/UltiCa pickers | `palettes-*`, `RMMV-ASSET-SOURCE-LOCATION.pdl` | ✅ pick; ❌ drop onto pc-hq canvas |
| **Concept / word banks, grades** | meaning, hand-scored words, entity levels | `concept-bank`, `khtpm_wordbank.c`, `entity_grade` | ✅ data; no gameplay use yet |
| **Delegation** | spec + locked harness + free workers | `&.widgits/quest-pilot/`, `^.hai-horn`, `^.grave/quests` | ✅ ops, ❌ nobody has driven it as a person |
| **Chain / market** | faucet, escrow, miner; WSR stocks; DSR; ring-board | `041.pal-chain`, `014.wsr-pal`, `&.widgits/dsr-sim`, `&.widgits/ring-board-rmmv` | 🟡 pieces, no multiplayer join |

**Missing pieces that block several games at once** (build these first, they pay off everywhere):
1. **A world viewer window** that draws any game's entities from its state files, and shows a Pause/Resume row. ❌ (this unlocks Eden, Pokemon, Civ, DSR at once).
2. **Click-voxel to event** in pc-hq. ❌ (Minecraft, CDDA).
3. **A goals board** per entity: `GOAL | entity | text | check | status`, with a deterministic check, same shape as quest rows in `^.grave/quests/INDEX.md`. ❌ (needed so entities act with purpose; ghosts and gravestones share it).
4. **Move events inside games** (the move feature works: `move_entity_init/tick`; Eden does not use it). 🟡
5. **Multiplayer transport.** `palnet_peer.c` hardcodes 127.0.0.1: today's multi-user test is same-machine only. ❌ across machines.

---

## 3. How to see and drive the desktop on screen (the part the last handoff left out)

This is *how every verification in this project is done*. Read it twice.

### 3.1 The one reliable input path: the relay

Every running `khtpm_core_render.+x` window polls its own per-process file, every tick:

```
#.desktop/entity_menu_history/<pid>.txt
```

Real keyboard/mouse input and your writes arrive through the **same code path**. Append one line per event (never truncate; it is cursor-based):

- `KEY_PRESSED: <n>`: printable ASCII 32-126 as the character code; `13` Enter, `27` Escape, `8` Backspace, `9` Tab; arrows are `200` Up, `201` Down, `202` Left, `203` Right; `204`/`205` PageUp/PageDown.
- `MOUSE_EVENT: <button> <x> <y> <is_press>`: real clicks and wheel.
- `# text`: a no-op audit comment; use it to say *why* you sent the next line.

### 3.2 A full verification loop, step by step

1. **Launch** the window with its own launcher (each app has `open_<name>.sh` or `button.sh`; e.g. `&.hq-apps/csv-lab-hq/open_csv_lab_hq.sh <house_root>`). Never launch by hand-typing the renderer.
2. **Find the pid** without `pgrep -f`/`pkill -f`: those match their own shell and kill it (a real, repeated incident). Use `ps -eo pid,args | awk '/khtpm_core_render/ && /<app>/ {print $1}'`, or read `module_parent.pid` in the app folder.
3. **Dump the frame and read the real nav numbers** before sending any digit. Nav numbers are *global across every open window*; a new window can start at 11 or 19. Never assume it starts at 1. Frame dump: `#.desktop/entity_menu_frame_<pid>.txt`.
4. **Send events** by appending to `<pid>.txt` (3.1).
5. **Check the text state first** (cheap, unambiguous): the app's published `<name>_ui.txt`, a manager's action file, or the game's ledger/history. Did a row appear? Did the day counter move?
6. **Then check pixels**: `dump_frame_png_op.+x <window-id> <out.png>` (call the binary directly; the in-window `p` key is eaten by an armed text field). Open the PNG and look at it.
7. **Only if the relay cannot express it** (a real mouse drag), use xdotool. Reaching for it first is the mistake the standard exists to prevent.
8. **Do not trust the relay alone for focus/grab fixes**: it once passed 3 times for a bug real hardware still showed. For anything about keyboard grab or focus, say "relay-verified, hardware unverified" and ask the owner for one real try.

### 3.3 Headless frames

`#.desktop/ascii_frames/` holds headless ASCII frame dumps, usable when no display is available. A frame that is all blank means the window did not paint; do not report success on that.

### 3.4 Things that will bite you (each one has already cost a day)

- A **running window does not pick up a rebuilt renderer**; relaunch it. A desktop reset (`sh '$.crypts/button.sh' reset`) kills everything, saves user data, rebuilds and relaunches, and **leaves the desktop down if the build fails**. Fix the build, reset again.
- Stale **shadow copies** of shared headers in `ops/` can break the build (`ops/khtpm_css_parser.h` once lacked a new symbol). `build_core_render.sh` now guards this; if a build says an unknown symbol, refresh the ops copy from `&.widgits/_shared-lib/`.
- In tile/entity mode the renderer uses a local `Display` and never sets the `dpy/cmap/screen` globals: use `tp_hex_pixel()`, not `alloc_pixel()`.
- `assign_nav_and_layout` and `redraw` are shared with context menus. Keep window-size mutations idempotent and gate new per-window layout.
- `content=` seeds a field's editable text; `label=` is display only. Putting real content in `label=` breaks Backspace or doubles the text.
- Do not add a new `layout_*` branch to the renderer before checking the swatch-grid and scroll-region paths (a past agent built a duplicate and got clipping, scrolling and numbering wrong).
- Change detection is **marker-file size growth**, never `st_mtime` (seconds-only).
- **Don't move the owner's windows** (hotbar positions are theirs): record before testing, restore after.
- Heavy work on this weak-CPU machine: wrap multi-minute commands in `nice -n 15 ionice -c3`.

---

## 4. How to build things here (conventions, not suggestions)

- **Never write an absolute path** into any file the program generates or tracks (scripts, `.chtpm`, indexes, registries). Resolve at run time from the file's own location (`dirname "$0"`) or `argv[0]`, or take the house root as an argument. Fixed so far: the Eden button's `ctl.sh` (commit `3d4d45973`, harness proves a copied desk follows its new location). **Still open:** the audit list in `INSTALL-STORE-ACCOUNTS-AND-USER-DATA-COMPLICATIONS-2026-10-08.md` §6. Add a harness check for any new generator you write: install into a scratch dir, move it, grep for the old path.
- **Language split.** Orchestration is `.pal` (RISC-V assembly run by `prisc+x`), real work is compiled C **ops** in `ops/`. No shell scripts as ops. Harnesses are pal too (below). Derive paths from `argv[0]`; never hardcode absolute paths (several generated files still do, see the install doc).
- **State = append-only ledgers + cursor polling.** One source of truth per fact; readers keep a cursor. This is why nothing races.
- **Sharing code, in this order:** inline if one consumer; text-include a pure `.c` in `_shared-lib` if two or more; for stateful things use an op with fork/exec/IPC. **Never a header plus link step.** Includes are a transitional measure; the end state is a shared in-memory DB.
- **New windows** copy the manager+projection shape (`csv-lab-hq` is the cleanest recent one). Zero new per-project code in the shared renderer. If you think you need it, stop and ask.
- **Comments**: keep them fresh and mirrored in docs; never bulk-strip.

### 4.1 The pal harness (every feature gets one, written *before* the feature)

Three parts, in `&.widgits/_shared-lib/harness/`: `<name>.pal` (execs the case op, then the verdict op, halts), `cases/<name>.pdl` (pipe-delimited rows: `SCRATCH`, `COPY`, `WRITE`, `RUN`, `EXPECT…`), and the two compiled ops in `ops/`. Rules that have bitten people:
- `RUN` args split on every `|`; `#` starts a comment; max 15 args per RUN; `ENV` persists.
- Build ops with `ops/build_harness_ops.sh` in a fresh worktree.
- Build prisc: `gcc -O2 -w -o /tmp/prisc_v "system/prisc+x.c" -lm`; run from the harness folder: `/tmp/prisc_v <name>.pal`.
- Delete `results/<name>.txt` and its `.verdict.txt` before a rerun.
- **Every harness needs a mutant**: a deliberate break (e.g. a `sed` edit of the code under test) that the harness must fail on. A harness that cannot fail proves nothing. Mutants must avoid `|`.
- Existing examples to copy: `csv_lab_hq` (175 checks, a window), `knowledge_hq` (191), `game_slots` (22), `eden_conductor`.

### 4.2 Commands you will actually use

```
sh '$.crypts/button.sh' build        # compile everything (compiled programs are in no branch)
sh '$.crypts/button.sh' reset        # kill, save user data, rebuild, relaunch
sh '$.crypts/button.sh' save-data    # commit desk data to the user/* branches (never touches your worktree)
sh '$.crypts/button.sh' status
cd &.widgits/_shared-lib/harness && /tmp/prisc_v <name>.pal
```

---

## 5. Delegating to free workers, and which APIs work (tested 2026-10-08)

The goal is that **Claude and you spend tokens on design and review, and free models do bulk and mechanical work behind a locked harness.** Method: write the spec and the harness yourself; a worker implements; a deterministic judge accepts or rejects; you review. Ops already built: `&.widgits/quest-pilot/ops` (`quest_check`, `ghost_run`, `ledger_to_feedback`, `record_delegation.sh`), board `^.grave/quests/INDEX.md`.

**Free providers, one tiny call each today:** Groq `openai/gpt-oss-120b`, `gpt-oss-20b`, `qwen/qwen3.8-27b` ✅; OpenRouter `nvidia/nemotron-3-super-120b-a12b:free` ✅; OpenRouter `inclusionai/ling-3.0-flash-sante:free` ❌ 404; Poolside: no key; Mac Ollama (`http://10.0.0.144:11434`, from `#.desktop/ai_backend.pdl`) ✅. **All Ollama/Gemma calls go to the Mac, never this machine's local Ollama.** Groq free tier: 1000 requests/day, 8000 tokens/minute, 200,000 tokens/day *per model*; both big models hit the cap in one session last time. Never print or commit a key; key files live in `&.widgits/open-hai/state/` and are git-ignored.

**What to delegate:** data work (translation, scoring, tagging), parsers with fixtures, small ops with a locked harness. **Do not delegate** renderer changes, anything touching user data, or design. Workers write C badly when the spec is vague and well when a harness is the spec.

**Honest state of delegation:** it has been run by Claude through scripts, judged by deterministic checks. **No person has ever driven it through the h-ai window.** Making that path real (one relay-driven session from h-ai to a worker and back, then the owner does it once) is a milestone in section 7.

---

## 6. The worked example to copy: Eden (and its gap)

Eden is a conductor entity (`eden_button`) plus a game (`&.widgits/eden`). Rows: Start game, Stop game, Next day, Status, Pause, Resume, Save 1-3, Load 1-3, slot controls, Events. It runs on the clock daemon.
- ✅ Over 62,498 game days the history holds ~131k ITEM, ~125k NEED/ACT, ~54k GROW, ~13.5k RIPE, ~14k HOUSE, ~12.5k FIND, ~11.9k TALK rows. Asa and Ava farm, build and talk.
- ❌ Nothing draws it. Entities appear as static 80×80 tiles. `status.txt` still reads `day=0 running=0`. The `Status` row writes no reply.
- **Stop game despawns (deletes) entity folders**; Start spawns fresh ones with new identities; Load restores a saved snapshot. Save a slot before Stop. Prefer Pause.
- History grows ~1 MB/hour with no cap; design for retention in `EDEN-STATE-RETENTION-DESIGN-2026-10-08.md`.

**Your first deliverable should be the Eden World Viewer** (section 7, step 1) because it unlocks every later game and is the cleanest test of the whole workflow.

---

## 7. The roadmap, in order, with exit criteria

Each step ends with the five-part "done" from section 0.2. Do not start a step until the previous exit criteria are met, except where marked parallel.

**Phase A: make the existing engine visible (weeks 1-2)**
1. **Eden World Viewer** (khtpm window, `csv-lab-hq` shape). Reads the history tail and entity states and draws Asa/Ava/crops/house as tiles with the day counter and a ticker of the last TALK/ACT rows, plus Pause/Resume rows wired to the existing `ctl.sh`. *Exit:* with the game running, relay-click Pause and the day counter stops, Resume and it moves; PNG shows it; harness against a fixture history (deterministic).
2. **Fix Status**: write `cli_reply.txt` and refresh `status.txt`. *Exit:* Status shows the real day.
3. **Human-use proof of delegation**: one relay-driven h-ai session to a free worker and back, then the owner repeats it once by hand. *Exit:* the reply appears in the chat history, evidence attached.
4. **Eden retention** (checkpoint/prune/rotate, steps 1-3 of its design; good delegation target). *Exit:* harness with mutants.

**Phase B: movement, goals, purpose (weeks 2-4)**
5. **Move events in games**: Eden ACT rows emit a move marker; the viewer animates via the existing move tick. *Exit:* entity visibly moves between tiles under clock control.
6. **Goals board** (shape in section 2). Show it in the viewer and the Ghost roster. *Exit:* an entity picks a goal, a deterministic check flips it done.
7. **First ghost entity (Tester)**, quest Q015, reading the goals board. *Exit:* it runs a check and posts a result on the quest board window.

**Phase C: one clone end to end (weeks 4-8). Pick Pokemon-on-the-desk first.**
Reason: it is the cheapest, it reuses RPG Maker events, the desk as a map, and the digipet needs core.
8. A map with grass, a grass common-event to battle, a mart (shop), a gym (switch + transfer). Needs: Transfer, Shop, Battle as real gameplay, not state files (gap list in `TILESETS-EVENTS-AND-GAME-CLONES.md` section 6). *Exit:* a person walks onto grass via the relay, a battle window appears, wins, gold changes.
9. The same game in pc-hq 3D (camera model is ported; use the 3D GPU daemon). *Exit:* same world, 3D view, relay-driven.

**Phase D: the other clones, each reusing A-C**
10. **Minecraft**: click-voxel to event (gap 2), gravity/move-route, craft UI. Mineclonia guides exist.
11. **CDDA / mutaclysm**: original C game at `101.mutaclsym…/ops`; rebuild mechanics as events (`MUTACLYSM-AS-EVENTS-DESIGN.md`, step 1 = item/recipe schema).
12. **Civ**: range overlay (builtin cell tint + BFS) is the stated priority; the desktop is the map, pals are units and cities.
13. **GTA / 007**: need a tileset/DIR pack first; 007 needs a one-page design from the owner before any code.

**Phase E: the economy (parallel with C/D, after step 4)**
14. Pick one economy to make multiplayer-real: **the auction**. Order: `auction_state` ledger op with a harness (step 1 of its design) → window → play-money ledger (`PLAY-ECONOMY` step 1) → chain price (`chain_*` ops) behind the chain signing gate.
15. **Cross-machine transport**: replace 127.0.0.1 hardcoding in `palnet_peer.c`. Plan only today (`cross-machine networking plan`). *Exit:* two machines (the Mac is `10.0.0.144`) see one auction.
16. WSR/XOD tournament goes live against real WSR (its own roadmap lists the steps; mock fitness only so far).

**Phase G: long term, creative tools (do not start before Phase C works)**
17. **`103.media-studio`** holds a DAW (`103.daw`), an image editor (`103.img-editor`), a 3D editor (`103.3d=blender-clone`), a video editor (`103.vid-edit`) and `100.tts-point-2-anything`: 54 files, **unported prototypes**. The plan is `MEDIA-STUDIO-XHTPM-PORT.md` (written 2026-09-03; "next agent starts at §9"): migrate each into the house shape (static `.xhtpm` + a projector + `khtpm_core_render.+x`), *not* a rewrite of the C editor. `MUSIC-PLAYER-HQ-DESIGN.md` (design only) is the small first step on the audio side, and `&.widgits/music-daemon` already generates and synthesizes game music (headless-tested, **nobody has listened to it**).
18. **What is thin, and where the owner expects more:** these tools have not been fleshed out for **navigation use** (numbered nav, keyboard reachability, relay driving). A tool that cannot be driven through the relay cannot be verified by anyone, so for each tool the first deliverable is: every control reachable by nav number or key, listed in a table, with a harness that drives three of them through the relay. Then the image editor's AI add-on (Stable Diffusion) is **blocked on hardware** (no GPU on this machine) and stays a later item.

**Phase F: ship it** (see `INSTALL-STORE-ACCOUNTS-AND-USER-DATA-COMPLICATIONS-2026-10-08.md`): make all of the above installable by someone else without touching the owner's data.

---

## 8. Hard rules from `AGENTS.md` and house experience (violating these loses work)

- **Commit your own files only, by explicit pathspec**: `git commit -m ... -- <paths>`. Never `git add -A`. Never sweep runtime files (`module_parent.pid`, `*.pdl` state, logs, `cli_io_state.txt`).
- **Your branch is `grok`.** Do not commit to `main`, `claude`, or another tool's branch. Do not merge, cherry-pick across branches, push, or force-delete unless the owner says so.
- **Prefer one worktree per agent** (`git worktree add …`), cut from the *current* tip, never from a stale base (one agent worked 424 commits behind and produced nothing).
- **Never `git stash`**, never `git reset` with anyone else's staged files.
- **User desk data is not code.** `xyzfs/users/` is live data (entities, histories, phones, wallets). Never `git add` it, never `checkout/switch/reset --hard/clean` a live checkout across a branch that tracks it (git will delete the files; it once deleted the owner's API key files). Merge in a scratch worktree. Never push `user/*` branches.
- After any reset, branch switch or bulk script: `git ls-files --deleted | grep -v /pieces/sessions/ | wc -l`. More than a few dozen means the tree was wiped; stop and read the incident doc.
- Keys, wallets, chat histories: never print, commit, or paste them.
- Lane split: the `opencode-fix` lane owns network worker/fetch code; the `opencode` lane owns row layout. Don't edit their files. `bv_menu_input.c` is the placer/camera merge point.
- Free models only; no paid credits.
- **Report faithfully.** If a test fails, say so with the output. If you skipped a step, say so.

---

## 9. What to send back after each step

A short note with: what changed (file list), the harness name and `passed=N failed=0`, the mutant that failed, the PNG/frame path, the text-state path, and an **Unverified** list. Use the status marks (✅🟡📐❌). Put it in the book next to the design doc and link it from `08-roadmap/00-INDEX.md`.

## 10. Decisions that belong to the owner (ask, don't guess)

First game after Eden (I recommend Pokemon-on-desk); whether Stop game should ask before despawning; the retention numbers; the goals-board shape; 007's design; whether `AUTO_PROMOTE` ever turns on (today: off, autonomy 0 everywhere); which chain to use for prices and the signing gate.

## 11. Where things are

Book: `#.#.calendar-dox/!.HQ-IQ-BOOK/`. Key reads: `02-architecture/CENTROID_GOLD_STD.md`, `03-pitfalls/OPERATIONAL-LANDMINES.md`, `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md`, `08-roadmap/NB-DEBUG-QUICKREF.md`, `08-roadmap/design-docs/DOC-STATUS-AND-PATHS-FORWARD-2026-10-08.md`, `EDEN-STATE-RETENTION-DESIGN-2026-10-08.md`, `PIECECRAFT-HQ-GAME-EDITOR-AND-PLAY.md` sections 6-9 (Transfer/Shop/Battle). Reports: `XO/14.oct8/`. Night lessons (audio): `1-1.HARNECIENT.SMOL/NIGHT_40_*` and `NIGHT_41_*`.
