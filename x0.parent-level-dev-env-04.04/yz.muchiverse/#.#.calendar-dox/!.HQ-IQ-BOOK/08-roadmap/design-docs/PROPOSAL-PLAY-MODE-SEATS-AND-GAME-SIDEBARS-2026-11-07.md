# 🎮🪟 Watch the AI play, or play as the human: seats, one set of hooks, and game sidebars that replace the editor's in Play mode (design, 2026-11-07)

💌 *For the owner, and any agent picking this up. Written by Claude Sonnet 5.5. Labels: **SAID** you told an agent · **READ** in a named file · **OBS** I ran or counted it · **DOC** my reasoning, not run. **Nothing in sections 3 to 7 is built.***

## 0. 🎯 Short answers
1. **Does deskopoly open in rpg-edit? Yes (OBS, today).** Section 1 has the window picture. It showed the neutral board only (no game played), with no sidebar of its own.
2. **Watching AI play and playing as the human are the same thing with a different driver.** Give each seat a driver (`pet:<style>`, later `ai`, or `human`) and one `tick` that asks the seat's driver for its next verb and then performs it through the same verbs and ledger a click uses. A human seat simply makes `tick` wait. Everything else (decision rows, the bank, harness replay) is shared. (Section 3.)
3. **The sidebar swap you describe has most of its parts already.** The editor's sidebars are rows whose visibility is a **state key**. What is missing is a **mode** (edit or play) and a **profile** per mode that flips those keys, plus a way for a **game to declare its own bars**. Recommended: use the in-game layouts plan (2026-10-06) as the long-term home and prove the idea first with the mechanism that works today. (Section 4.)
4. **First game sidebar to build: "Seats"** (who plays, whose turn, Roll / Buy / Skip / End for the human seat, speed and pause). It is also the best test of the whole design. (Section 5.)

## 1. ✅ What I tested today: does it open in rpg-edit? (OBS)
* I linked `deskopoly` into the editor's maps (the launcher does this itself for every `game.pdl`, READ in `board/open_board.sh`), saved `deskopoly:board` as the last book with the editor's own `edit_last.sh`, and launched the editor through `cpu_gate`.
* The editor reported `book_label=book:deskopoly`, `page_label=page:board`. I dumped the **real window** (1300x802, found by size, `dump_frame_png_op`): the toolbar shows `book:deskopoly` and `page:board`, the engine's overlay prints `deskopoly:board`, and the 3D view draws the ring from the page I generated (orange-diamond Start corner, dirt, planks, parquet, boards, rough stone, sand; raised ring over a lower green middle).
* Evidence: `XO/31.Nov/nov-7/4.deskopoly-in-the-rpg-edit-window.png`. A flat 2D preview of a **played** game (12 owned tiles with owner markers, counts equal to `status`) is `5.deskopoly-flat-preview-after-a-played-game.png`.
* **Cleanup, verified:** I closed the window by its pid, ended the leftover engine by pid after checking its session folder was my launch's, confirmed no related process remained, and restored `state/last_book.txt` (hash `7c34618740b19edc` before and after, content `book=rpg-space page=bridge`). The `deskopoly` link in the editor's maps folder is left in place (additive; the launcher recreates it anyway).
* **Not shown:** ownership markers **inside the window**. Redrawing the page while the engine runs needs a reload path I did not use. The markers are proven in the file and the flat preview only.
* Cost while open: load rose to about 1.7 from 0.4 on an otherwise quiet machine with your video player open; I did not measure idle CPU of the engine separately.

## 2. 🧱 What exists today (READ and OBS)
| Thing | Fact | Where |
|---|---|---|
| Editor sidebars | three **hand-written rows** in the board template: `side-l` (pets), `side-p` (palettes), `side-r` (chat). Each is `class="canvas-overlay-left/right ..."` with `show="${side_left}"` / `${side_pal}` / `${side_right}` | `rpg-edit-hq/board/pchq-board.xhtpm` lines 113, 125, 143 |
| Their switch | state keys in `board/state/sides.txt` (`side_left=`, `side_pal=1`, `side_right=1`); toggled by `layout-studio/ops/+x/side_toggle.+x`; the left slot is shared by pets and palettes through `pal_side` ("a state-key switch, not an href stack", owner OK 2026-11-02) | `pal_side.c` header; `sides.txt`; `state/prefs.txt` (`sidebar_start=layout`, `sides_history.pdl`) |
| "Play" today | `play` and `stop` **start and stop the world** (`VERB play | native | start the world (plays events/play)`); the toolbar shows `Player: playing`. That is the simulation running, **not an edit-versus-play screen mode** | `rpg-edit-hq/verbs.pdl`; the window picture |
| Edit versus play as a UI mode | **I found none** (I read the verbs, the sidebar rows and the prefs; I did not read the daemon source end to end) | |
| In-game layouts plan | user-made menus and bars drawn **inside** the board as fragments: `<overlay src>`, anchors (`left`, `right`, `bottom-centre`...), saved layouts called by name through the event-command registry (`layout.show/hide/toggle`), a studio of ops first and a window second. Phase 0 (overlay strip) and 0b (sandbox: `<overlay src>` splice, `canvas-overlay-right` column, test menu) are **done**; phases 1 to 8 are **proposed** | `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` |
| The owner's export rule | an exported game "starts in PLAY mode: no Play menu, and probably none of the editor buttons ... those buttons should really be layout custom events, so devs can build their own visually scripted menus" | `16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md` section 7b |
| Possession and drivers | one character is driven by input at a time (`ctl` flag, `control` verb, INT mode, the relay); the rest run the pet step (D49 section 3e) | `petslike_core.c`, `rpg-edit-hq/verbs.pdl` |
**So:** "add sidebar menus through layouts" is already the plan; "swap them when Player starts" is the new part.

## 3. 🤖🧑 Seats: one driver slot, same hooks (DOC)
**Principle (D49, "one door"):** nothing plays the game except by appending rows through verbs the game already knows. A human click, a pet's step, an AI, a harness: same door.
**Design for deskopoly (extends what I built today):**
* **Seats as data:** `state/seats.txt` rows `SEAT | p1 | pet:bold`, `p2 | pet:cautious`, `p3 | human`, ... (today the style is in `rules.pdl`; a seat points at a style or at `human`). A new verb **`seat <player> <driver>`** changes a seat **mid-game** (your "take over any pet" decision from D49) and writes a ledger row, so a replay knows who was in charge when.
* **One `tick`:** `tick` reads whose turn it is and the seat's driver. A **pet driver** returns its next verb (roll, then buy or skip by its style, then end_turn) and `tick` performs it through the **same code the CLI verbs use**; a **human driver** makes `tick` print `waiting: p3 human` and do nothing. `autoplay` becomes "tick until over".
* **The strongest proof that the hooks are shared (can fail):** an all-pet game run by `tick` ends with a ledger **byte-identical** to today's `autoplay` (seed 7: 334 events, hash `128baad3c4609438`). A second case: a human seat that issues the same verbs a pet would produces the same ledger as that pet.
* **Legal actions as data:** the `ui` projection (below) publishes `can_roll`, `can_buy`, `can_skip`, `can_end` from the replayed state, so the interface can show or hide a button because **the hard state says it is legal**, not because UI code guessed. That is your "use hard state to improve UX" goal in its simplest form.
* **`ui` verb:** writes `state/ui.txt` (key=value, atomic) from the replayed ledger: `turn`, `current`, per-seat cash and position, `can_*`, the last N ledger rows as log lines, `driver_<id>`. The window loads it as a vars file (the pattern the hotbar already uses, READ in the layouts plan section 2).
* **Watching:** a timer calls `tick` at a set speed (tuning data) while the world is **playing**; `stop` pauses it. A human seat waits for a click. Same loop whether you watch four pets or play one seat against three.
* **Learning stays the same:** every pet decision is already a `DEC` row the bank digests (verified today: buy 0.9286, skip 0.6667 from hand counts). A human seat's choices can be logged the same way (outcomes ok / blocked), so a human can *teach* the bank, if you choose (decision 5).

## 4. 🪟 Modes and sidebars (DOC)
**Three different things must not be mixed (they are today):**
| Thing | Today | Proposed name |
|---|---|---|
| world running | `play` / `stop` | keep (`running`) |
| input goes to the game | `In: ON/OFF` | keep (`In`) |
| **which screen set is shown: editor or player** | not found | **new: `ui_mode = edit | play`** |
**A bar** = a layout fragment with a small `layout.pdl`: `slot` (left, right, top, bottom), `modes` (`edit`, `play`, or both), `owner` (`editor` or `game:<book>`), `default` (on/off), `feed` (its vars file), and whether it can be minimised. **A profile** is data per mode: which bars are on in each slot.
**How the swap works with no renderer change (the idea that fits today's code):** the rows are already `show="${key}"` over state keys. A new op **`ui_mode <edit|play>`** (same shape as `side_toggle`) writes the `side_*` keys from the mode's profile, and remembers each bar's minimise state per mode (the `sides_history.pdl` idea). The editor's bars have `modes=edit`; a game's bars have `modes=play`.
**Where a game's bars come from:** the **book**. A book lists its bars (`@.apps/<book>/layouts/<id>/` plus a row per bar, in `game.pdl` or a `layouts.pdl`), exactly the folder shape the layouts plan already proposes. When the editor opens a book, it can list that book's bars; Play shows them and hides the editor's.
**Slots and conflicts (decision, data not code):** the left slot is a radio group today (pets or palettes). Generalise to **slot groups**: `exclusive` (one at a time) or `stack`. Two bars wanting the same exclusive slot: the profile says who wins.
**Exported game:** no editor profile is shipped; it starts in `play` (the owner's rule, section 2). The same bars, no editor chrome.
**Two stages (so the idea is proven before the big work):**
* **Stage 1, today's mechanism:** add the game's bars as **hand-written rows** in the board template gated by `show="${side_game_*}"`, plus the `ui_mode` op and a profile file. Proves the swap, the feed and the buttons. It is the same hand-written shape `side-r` has now, so it needs no new renderer feature, but it **does** hard-wire a deskopoly row into the editor's template, which the house rule says not to do for the long term.
* **Stage 2, the layouts plan:** load the bars as `<overlay src>` fragments (layouts plan phase 3), call them by name (phase 4), author them in the studio (phases 5 to 6). Stage 1's rows are then deleted, not kept.
**Design-choice test (AGENTS.md):** *one authority per fact:* the bar's slot and modes live in its `layout.pdl`, the profile in one data file. *Modular:* no per-game code in the editor (stage 2). *Scalable:* the tenth game adds a folder. *Long term:* it is where the layouts plan and the owner's export rule already point. *House shapes:* state keys, op, `.pdl`, vars file, fork/exec. *Cost and risk:* stage 1 is cheap and temporary and I label it with its removal condition (stage 2 phase 3); stage 2 touches the renderer (`khtpm_core_render.c`), so the khtpm house standards apply (read the gold standard first, clip never translate, nav numbers on visible rows only).

## 5. 🧩 First game bar: Seats (what you would see) (DOC)
```
 SEATS               p1 rex   pet:bold      cash 3888
 [pause] [speed]     p2 fenn  pet:cautious  cash 3956   <- turn
                     p3 gus   HUMAN         cash 3484
 [Roll] [Buy] [Skip] [End]   (only the legal ones show)
 last: p2 rolled 3,4 -> tile 7: for-sale
 [take over p2]  [hand back p3]
```
Everything in it comes from the `ui` feed and the verbs; no rule lives in the bar.

## 6. 🪜 Phases (each ends with a proof that can fail; nothing started)
| # | What | Proof |
|---|---|---|
| **A1** | `seats.txt`, `seat`, `tick`, `ui` verbs in `deskopoly.c`; `autoplay` = tick until over | all-pet `tick` game = today's ledger byte for byte; human seat makes `tick` wait; a human issuing the pet's verbs gives the pet's ledger; an illegal verb is refused and appends nothing; `ui.txt` equals `status` |
| **A2** | take over / hand back mid-game | a ledger row records it; replay reproduces who drove each move; a seat change out of turn is refused |
| **B1** | `ui_mode` op + profile file (state keys only) | a harness case flips `edit` to `play` and the `side_*` keys equal the profile; flipping back restores the editor's remembered state (hash) |
| **B2** | stage 1: Seats bar as rows in the board template, gated by `ui_mode` | window picture in edit (editor bars) and in play (Seats bar), found by size and dumped; a relay click on Roll appends one ledger row and the picture changes |
| **C** | tick driven by the editor daemon at a set speed, pause on `stop` | idle CPU with the window open measured **with the browser running** (house rule) and put in the commit |
| **D** | stage 2: bars as `<overlay src>` fragments, listed by the book | the Seats bar is loaded from `deskopoly/layouts/seats/`; the template row is deleted |
| **E** | studio ops author a game bar | an agent builds a bar from the command line (layouts plan phase 5) |
A1, A2 and B1 need no window and no renderer change. B2 needs the editor window and the CPU gate; I will ask before launching it.

## 7. ⚠️ Costs and risks
* **Ticking:** one process per tick is a fork per tick; the house already notes about 95 forks per second per open board from its prisc loops. A watched game should tick **inside the existing daemon at about 1 per second, or slower**, not from a new loop; measure first.
* **The editor template is shared** (the khtpm shared-layout caution): any edit to `pchq-board.xhtpm` or a layout path must keep window-size mutations idempotent and be gated per window.
* **Stage 1 puts a game's row in the editor's template.** Allowed only as a labelled stopgap with a removal condition.
* **A new mode touches input routing** (`In:`, possession, Esc backs out one level). `ui_mode` must not change any of those; Esc should still release possession first, then leave `In`.
* **Unverified:** whether `side_toggle` keys can be set from a second op without racing the daemon's own writes; whether the board reloads a page that changes under it (for live ownership markers).

## 8. ❓ Decisions for you
1. **Mode names and the Player menu:** is `Player` the edit/play switch, with today's `play`/`stop` kept as pause/resume of the world? (I recommend yes.)
2. **Stage 1 first, or go straight to layout fragments?** I recommend stage 1 for deskopoly only, because it proves the swap in about a day of work and is deleted at stage 2.
3. **Where a book declares its bars:** `game.pdl` rows, or a `layouts.pdl` beside `rooms.pdl`? (I recommend `layouts.pdl`: `game.pdl` is read by several tools.)
4. **Left-slot policy:** exclusive group (as pets or palettes today) for game bars too?
5. **Should a human's choices feed the decision bank** (teaching it), or only pets' choices?
6. **Tick speed and who owns it:** the editor daemon at 1 per second as the default?

## 9. 🔗 References
`@.apps/deskopoly/{board,rules,page,verbs}.pdl`, `ops/deskopoly.c` · `rpg-edit-hq/board/{pchq-board.xhtpm,open_board.sh,state/sides.txt}`, `ops/{pal_side.c,edit_last.sh,edit_open.sh}`, `verbs.pdl` · `layout-studio/ops/+x/side_toggle.+x` · BOOK: `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md`, `16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md`, `08-roadmap/design-docs/HANDOFF-NOV-7-ONE-VOCABULARY-PETS-ACT-2026-11-07.md` and `PROPOSAL-ONE-DOOR-DRIVING-AND-THE-MASTER-LEDGER-2026-11-07.md` · khtpm rules: skill `khtpm-house-standards` · XO: `XO/31.Nov/nov-7/` (documents 0 to 3, pictures 4 and 5)
*Nothing was sent to an outside service. The lore stays local.* 💛
