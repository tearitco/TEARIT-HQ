# 🧑 MY-CHARA-TXT — A SINGLE CHARACTER, TEXT-BASED DATA-FLOW AUDIT

> **PURPOSE:** The simplest possible farming/trading game. One player, one character, zero multiplayer complexity. This exists to **test and audit the ledger architecture and data flow** before scaling to genesis-txt (multiplayer) or visual layers (my-chara-zr).
>
> **Read this first** if you're trying to understand the whole ecosystem. Everything builds on the patterns proven here.

---

## 📖 1. THE VISION

A **single character** in a text-based world where time advances in **days**. The character can:
- 🌾 **Farm** — plant crops on owned land plots
- ⛏️ **Mine** — extract silver or gold
- 💼 **Store** — manage inventory (see items, use consumables)
- 💱 **Exchange** — buy/sell with NPCs at market prices (no other players; this is single-player)
- 📊 **Inventory** — view owned items and use them (bed → sleep, etc.)
- ⏳ **EndTurn** — advance time by one day

**Health bar** shows current state. **Auto-eat** triggers if critically low on food. **Game ends** after a set number of days (e.g., 10 days for a short run, 365 for a full year).

The whole system is **ledger-driven**: every action (plant seed, harvest, eat food, trade) appends to `master_ledger.txt`. This makes replay, audit, and debugging trivial — just read the ledger top-to-bottom.

---

## 🏗️ 2. REFERENCE SOURCES & PATTERNS

| Source | Pattern we reuse |
|--------|------------------|
| **014.wsr-pal💸️📌️+2** (`pieces/chtpm/layouts/wsr_main_menu.chtpm`, `wsr_trade_menu.chtpm`) | **The real, proven CHTPM nav interface** — `.chtpm` layout files, `${piece_methods}` auto-generated buttons, `href` screen navigation, `${game_map}` render substitution |
| **01.muchi-pals-🥚️-13.01** (`pieces/chtpm/layouts/main.chtpm`, `store.chtpm`, `pal/store_module.pal`, `ops/muchi_menu_input.c`) | Per-screen PAL module pattern, `piece.pdl` METHOD-table dispatch, the idle-sync/screen_changed-diffed PAL loop shape, the "dynamic list = regenerated piece.pdl, not a fake screen" convention |
| **@.apps/TSC_ELO/** (True Swords Clash) | Master ledger (append-only event log), config.txt state, op registry conventions |
| **#.haiku+/!.xyzos-pitfalls+1.txt** & **!.xyzos-standards+1.txt** | House-wide gotchas (idle-loop frame spam, atomic writes, quit-path ownership) — **read before writing any op or PAL script here** |

**Key correction (2026-08-01):** An earlier pass of this doc described a flat "compose.c writes an ASCII HUD box directly to `view.txt`" render model with ad-hoc single-key actions (F/M/S/I/E). That is **not** how this house's interface actually works, and does not match anything proven running. The REAL pattern — confirmed directly from wsr-pal (which the user has seen running) and muchi-pals — is a **navigation-based CHTPM interface**: named `.chtpm` screens, `piece.pdl`-driven method menus, and a dedicated PAL module per screen. §4 and §7 below now describe that real pattern.

**Key difference from TSC_ELO**: TSC_ELO is PvP with a master referee. **my-chara-txt is PvE** (player vs environment) — the "opponent" is just NPC market prices and random harvests. Much simpler.

---

## 🌍 3. THE WORLD (DATA MODEL)

### 📋 `config.txt` — AUTHORITATIVE GAME STATE

```
game_id=my-chara-001
player_name=Adam
day=1
max_days=10
health=100
money=500
grain_in_inventory=10
silver_in_inventory=0
gold_in_inventory=0
game_state=playing          # playing | game_over
last_action=
```

**Why this design:** `config.txt` is the snapshot for rendering. The **ledger** is the immutable audit trail. Together they're the same pattern TSC_ELO uses: state file + event log.

### 🧾 `master_ledger.txt` — APPEND-ONLY EVENT LOG

Every action ever taken, one line per action, pipe-delimited:

```
timestamp|day|action_type|details
2026-08-01T12:00:00|1|harvest|wheat:50:plot_0
2026-08-01T12:05:00|1|eat|grain:10
2026-08-01T12:10:00|1|mine|silver:2
2026-08-01T12:15:00|1|trade|sell:wheat:20:price:5
2026-08-01T12:20:00|1|day_end|
2026-08-02T00:00:00|2|status_tick|health:95
```

**Never deleted, only appended.** Replay = read ledger, re-apply every action in order.

---

## 🎮 4. THE REAL INTERFACE — CHTPM NAV SCREENS (not a flat HUD)

The game is a set of **named screens** (`.chtpm` layout files under `pieces/chtpm/layouts/`), navigated via `href` buttons, exactly like wsr-pal's `wsr_main_menu.chtpm → wsr_trade_menu.chtpm` and muchi-pals' `main.chtpm → store.chtpm`. There is **no single flat HUD** — each screen shows its own menu + its own render.

### Screens (first cut)

| Screen file | Shows | Reached from |
|---|---|---|
| `main.chtpm` | Day/health/money summary, nav buttons | (entry point) |
| `farm.chtpm` | List of owned plots (empty/growing/ripe) as `${piece_methods}` rows | `main.chtpm` → `[Farm]` |
| `mine.chtpm` | Mine action + last result | `main.chtpm` → `[Mine]` |
| `store.chtpm` | Buy/sell rows per commodity | `main.chtpm` → `[Store]` |
| `inventory.chtpm` | Owned items as clickable rows (use item) | `main.chtpm` → `[Inventory]` |

### Example: `main.chtpm` (modeled directly on `wsr_main_menu.chtpm` / muchi-pals' `main.chtpm`)

```xml
<panel time_reactive="true">
    <module>system/prisc+x pal/main_module.pal</module>
    <interact src="pieces/apps/player_app/interact_relay.txt" />
    <text label="+============================================================+" /><br/>
    <text label="|                   M Y - C H A R A                          |" /><br/>
    <text label="+============================================================+" /><br/>
    <text label="${game_map}" /><br/>
    <text label="+============================================================+" /><br/>
    <button label="Farm" href="pieces/chtpm/layouts/farm.chtpm" /><br/>
    <button label="Mine" href="pieces/chtpm/layouts/mine.chtpm" /><br/>
    <button label="Store" href="pieces/chtpm/layouts/store.chtpm" /><br/>
    <button label="Inventory" href="pieces/chtpm/layouts/inventory.chtpm" /><br/>
    <text label="+============================================================+" /><br/>
    <text label=" [ctrl+c] Quit" /><br/>
</panel>
```

`${game_map}` is populated by that screen's own `compose_frame` op writing to `pieces/apps/player_app/view.txt` (day/health/money summary) — **never** a hand-rolled ASCII box drawn directly by the op into the rendered layout itself. Screen **navigation** (main→farm, farm→main) is a plain `href` — handled entirely by `chtpm_parser_pal.c`, never simulated by an op (xyzos-standards §18).

### Per-screen PAL module (modeled on `store_module.pal`)

Each screen gets its **own** PAL module string in its `<module>` tag, and its own tiny loop — proven shape, copied structurally from `store_module.pal`:

```
li x1, 0

li x9, 0
mychara_menu_input x9
mychara_compose_frame
hit_frame
read_pos x7, "pieces/display/mychara_screen_changed.txt"

loop:
li x9, 0
mychara_menu_input x9
read_pos x8, "pieces/display/mychara_screen_changed.txt"
beq x7, x8, check_key
addi x7, x8, 0
j render

check_key:
read_history pieces/apps/player_app/interact_relay.txt x2, x1
beq x2, x0, no_key
mychara_menu_input x2
j render

no_key:
sleep 30000
j loop

render:
mychara_compose_frame
hit_frame
sleep 30000
j loop
```

**Only recompose when something actually changed** (screen_changed marker diff, or a real relayed key) — per Pitfall 48, an unconditional per-tick `compose_frame` floods the frame history and pegs CPU. **No quit path inside the PAL script** (Pitfall 10) — quitting is chtpm's job, never the module's.

### `piece.pdl` — the METHOD table a screen's buttons are generated from

Dynamic/variable-length menus (list of plots, list of inventory items) are **not** separate hand-authored screens — they are `${piece_methods}`-generated buttons from a `piece.pdl` file that the compose/menu_input ops **regenerate on every call**, exactly like muchi-pals' `pets.chtpm` (one file serves both "no pet selected" and "pet selected" states by regenerating its own `piece.pdl` METHOD rows). Example, `farm`'s piece.pdl regenerated per plot state:

```
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | farm

METHOD       | Plot 0: Empty (plant wheat/corn)   | PLANT:0
METHOD       | Plot 1: Growing (2 days left)       | NOOP
METHOD       | Plot 2: Ripe! (harvest wheat)        | HARVEST:2
```

`mychara_menu_input` (this project's own menu-input op, modeled directly on `chain_menu_input.c`/`muchi_menu_input.c`) reads which screen is current from `pieces/display/current_layout.txt` (**never** separately tracked mutable state), reads that screen's `piece.pdl`, and dispatches the numbered key press to the matching `COMMAND` string (`PLANT:0`, `HARVEST:2`, etc.).

---

## 🎯 5. ACTIONS & LEDGER ENTRIES

### 🌾 FARM
- **Command:** Player selects a plot (0, 1, 2, ...). Plot state is either `empty`, `growing`, or `ripe`.
- **Mechanics:**
  - `empty` → player chooses wheat or corn → cost 10 grain from inventory → plot becomes `growing` with a harvest_day (current_day + 3).
  - `growing` → no action, just wait.
  - `ripe` → harvest for grain (wheat → 50 grain, corn → 60 grain).
- **Ledger:**
  ```
  2026-08-01T12:00:00|1|plant|wheat:plot_0
  2026-08-01T12:00:00|1|harvest|wheat:50:plot_0
  ```

### ⛏️ MINE
- **Command:** Player mines. Each mine action attempts to extract silver or gold.
- **Mechanics:** 
  - 70% chance → silver (+1), 30% chance → gold (+1). (Or different odds; tune as needed.)
  - Takes 1 turn.
- **Ledger:**
  ```
  2026-08-01T12:00:00|1|mine|silver:1
  ```

### 💼 STORE (NPC Auction/Market)
- **Buy:** Player sees average price of grain/wheat/silver/gold from a CSV or hard-coded price list. Player specifies qty and price they're willing to pay. If price meets threshold, trade happens.
- **Sell:** Player lists items for sale at a price. If NPC buys (hardcoded logic), it triggers a trade.
- **Ledger:**
  ```
  2026-08-01T12:00:00|1|trade|buy:grain:20:price_per:2.5
  2026-08-01T12:00:00|1|trade|sell:silver:5:price_per:10
  ```

**Simple NPC logic:** prices are fixed + small random variance per day. This is "hard-coded" NPC behavior, not AI; the exchange is **not** with other players yet (that's genesis-txt's job).

### 💱 INVENTORY
- **Open widget/menu:** Shows all items owned. Each item is a method. Click it (or type its digit) to use.
- **Consumables (grain, meat):** Using it removes 1 from inventory, heals/feeds character.
- **Equipment (bed, toilet, shower):** Using it triggers a status effect or action (rest → heal +10 HP, shower → hygiene buff, etc.).
- **Ledger:**
  ```
  2026-08-01T12:00:00|1|use_item|grain:1
  2026-08-01T12:00:00|1|use_item|bed:sleep
  ```

### ⏳ ENDTURN
- **Triggers:** Day advances by 1. All plots advance their growth. Status effects tick (health decrements if not eating). Weather/random events occur.
- **Ledger:**
  ```
  2026-08-01T12:20:00|1|day_end|
  2026-08-02T00:00:00|2|status_tick|health:95:hunger_damage:5
  ```

---

## ⚙️ 6. OPS (C tools, registered in `default_op.txt`) — REAL SHAPE

Modeled directly on `chain_menu_input.c` / `chain_compose_frame.c` (041.pal-chain) and `muchi_menu_input.c` / muchi-pals' compose op — **one shared `menu_input` + one shared `compose_frame` per project**, not one op per action. The menu_input op is the METHOD-table dispatcher; individual game verbs (plant, harvest, mine, buy, sell, use item) are **handled inside it** (or in small helper ops it calls), keyed off the `COMMAND` string from whichever `piece.pdl` row was selected — same shape as `chain_menu_input.c`'s own dispatch, not a separate op per button.

| Op | Purpose |
|----|---------|
| `mychara_menu_input` | **The** METHOD-table dispatcher. Reads `current_layout.txt` for current screen, reads that screen's `piece.pdl`, dispatches keycode → COMMAND (`PLANT:0`, `HARVEST:2`, `MINE`, `BUY:grain:10:2.5`, `USE_ITEM:bed`, etc.), performs the state mutation, appends to `master_ledger.txt` |
| `mychara_compose_frame` | Renders the CURRENT screen's data into `pieces/apps/player_app/view.txt` (substituted into `${game_map}`) AND regenerates that screen's `piece.pdl` if it's a dynamic list (plots, inventory) |
| `tick` | Advance day, apply hunger/health decay, check plant growth, auto-eat if needed, check win/loss — called from the PAL loop's own idle path, not per-keypress |
| `ledger_append` | Utility: append a line to `data/master_ledger.txt` (called internally by `mychara_menu_input`, not directly bound to a screen) |

**Reuse pattern:** If we already have a verse-scramble engine (TSC_ELO's `tsc_deal/input`), we could make farming require solving a puzzle to get bonus yield. For now, assume farming is deterministic (no puzzle).

---

## 📁 7. DIRECTORY LAYOUT — `my-chara-txt/`

```
my-chara-txt/
├── MY_CHARA_TXT_DESIGN.md      ← you are here
├── button.sh                   ← launcher (start game, cleanup)
├── scripts/
│   └── build.sh               ← compile ops
├── default_op.txt             ← op registry
├── system/                     ← symlinks from 014.wsr (prisc+x, chtpm_parser_pal, renderer, keyboard_input)
├── ops/
│   ├── mychara_menu_input.c   ← METHOD-table dispatcher (all game verbs)
│   ├── mychara_compose_frame.c ← per-screen render + dynamic piece.pdl regen
│   ├── tick.c  ledger_append.c
│   └── +x/        ← compiled binaries
├── pal/
│   ├── main_module.pal        ← main.chtpm's own module
│   ├── farm_module.pal        ← farm.chtpm's own module
│   ├── mine_module.pal        ← mine.chtpm's own module
│   ├── store_module.pal       ← store.chtpm's own module
│   └── inventory_module.pal   ← inventory.chtpm's own module
├── pieces/
│   ├── chtpm/layouts/
│   │   ├── main.chtpm  farm.chtpm  mine.chtpm  store.chtpm  inventory.chtpm
│   ├── system/                ← config.txt, plots.txt, current_layout.txt
│   ├── display/                ← mychara_screen_changed.txt
│   ├── keyboard/               ← history.txt
│   ├── registry/fonts/ascii/  ← glyphs
│   └── apps/player_app/       ← view.txt, interact_relay.txt, piece.pdl (per active piece)
└── data/
    └── master_ledger.txt      ← immutable event log
```

---

## 🎮 8. PLAYER ACTIONS FLOW (one full turn, real nav)

```
1️⃣ MAIN SCREEN
   → main.chtpm shown, ${game_map} populated by mychara_compose_frame
     ("Day 5 | Health 75 | Money 420")
   → player navigates (arrow/digit + Enter) to [Farm] → real href to farm.chtpm
   → chtpm_parser_pal.c handles the screen switch, launches farm_module.pal

2️⃣ FARM SCREEN LOADS
   → farm_module.pal's own idle-sync calls mychara_menu_input 0
   → mychara_compose_frame regenerates farm's piece.pdl from plots.txt
     (Plot 0: Empty, Plot 1: Growing, Plot 2: Ripe)
   → ${piece_methods} renders those as numbered buttons

3️⃣ INPUT (real key injection, not a hand-edited state file — Pitfall 21)
   → player selects "Plot 0: Empty (plant wheat/corn)" → keycode relayed
     via interact_relay.txt
   → mychara_menu_input reads current_layout.txt (== farm.chtpm), reads
     farm's piece.pdl, resolves keycode → COMMAND "PLANT:0"
   → validates (enough grain?), updates plots.txt, appends ledger entry
     "2026-08-01|5|plant|wheat:plot_0"
   → bumps pieces/display/mychara_screen_changed.txt

4️⃣ RENDER AGAIN
   → farm_module.pal's loop detects screen_changed diff → render branch
   → mychara_compose_frame re-regenerates piece.pdl (Plot 0 now "Growing")
   → ${piece_methods} shows the updated row

5️⃣ BACK TO MAIN → END TURN
   → player hrefs back to main.chtpm (real navigation, not simulated)
   → main screen has its own "End Turn" method row → COMMAND "END_TURN"
   → mychara_menu_input runs tick: plants grow, health decays (-5/day),
     auto-eat triggers if health < 20, day increments
   → ledger: "2026-08-01|5|day_end|"
   → check day >= max_days → game_state=game_over
```

---

## 🚀 9. BUILD ORDER (minimal, for audit/test)

| Phase | What we build | Verified by |
|-------|---------------|-------------|
| **P1** 🥇 | Skeleton: build.sh, system/, `main.chtpm` + `main_module.pal`, `mychara_compose_frame` renders a test frame via `${game_map}` | real `./button.sh run` shows main.chtpm, not just an op-level test |
| **P2** 🎮 | Farm action: plant wheat, update plot state, ledger entry | plant action works, ledger has entry |
| **P3** ⛏️ | Mine action: RNG roll, add ore, ledger | mine works, ore in inventory |
| **P4** 💼 | Store action: buy/sell with NPC at fixed price | buy/sell work, inventory + money update |
| **P5** 🔄 | Endturn: day ticks, plants grow, health decays | day increments, plants mature, health drops |
| **P6** 💪 | Inventory menu: use items (consumables disappear, equipment triggers action) | eating restores health, bed sleep works |
| **P7** ✅ | Full replay: restart game, read ledger, re-apply actions in order | ledger replay matches final state |
| **P8** 📊 | Audit tooling: script to dump ledger in readable format, compare to config.txt | audit passes |

---

## 🔗 10. CROSS-REFERENCES & ECOSYSTEM

### Relationship to other apps:

- **@.apps/TSC_ELO/** — Uses same ledger architecture, PAL loop, op dispatch. my-chara-txt **simplifies** TSC_ELO by removing PvP, AI, Miracles, combat. Start here, then level up to TSC_ELO if you want to understand combat + duel logic.

- **@.apps/genesis-txt/** — The **multiplayer version** of my-chara-txt. Same ledger, same ops, but: multiple players, exchange orders match between real players (not NPCs), AI difficulty tuning, turn alternation. Build my-chara-txt first to nail the ledger; then expand to genesis-txt.

- **@.apps/my-chara-zr/** — **Desktop visual version** of this game. Same ledger, same ops, different rendering (GL windows, drag-drop, live avatar on desktop instead of text HUD). Not building yet, but same data structures.

- **@.apps/genesis-zr/** — Visual version of genesis-txt (multiplayer farming with avatars). Same patterns, visual layer on top.

- **014.wsr-pal💸️📌️+2** — Full stock-market game. my-chara-txt is a **much smaller sibling** using the same decision_mode/ledger patterns but focused on farming, not trading.

---

## 🤖 10a. RETROFIT (2026-08-02): AUTOMATION / decision_mode / SUPERVISION LAYER — NOT YET BUILT, DESIGN ONLY

**Added retroactively, per direct instruction, after the same layer was designed fresh for `@.apps/civ-txt/` and `@.apps/tactics-txt/`.** My-chara-txt should carry the exact same automation model those two sibling docs specify in full (`CIV_TXT_DESIGN.md` §7 is the canonical writeup — read it, don't re-derive it here). Summarized for this project's own context:

- **Three independent dials, not yet implemented:**
  - **Supervision** (`manual`/`semi`/`full`) — Manual is today's status quo (you press every key). Semi runs one full automated day via `mychara_ai_decide.c`, then pauses (`paused_for_confirmation=1`) until you send `CONTINUE`. Full runs days back-to-back unattended.
  - **Risk** (1-10) — tunes an eventual `weighted`-tier scoring formula (e.g. how low grain/health can get before the automation prioritizes Farm/Mine over Store).
  - **Compute tier** (0=preset, 1=weighted, 2=rl-stub, 3=llm) — maps onto the real, proven `decision_mode` chassis from `014.wsr-pal💸️📌️+2/ops/corp_decide.c`. `llm` must stay rare/opt-in per `%.harnesses/xo-human.md`'s speed doctrine, never the routine per-day tier.
- **The concrete op**: `mychara_ai_decide.c`, modeled directly on `corp_decide.c` — reads `decision_mode` off `config.txt`, branches, and for `preset` starts as literally the trivial rule already named in `xo-human.md` §4 ("if grain < 20, farm; if have ore, sell it; else mine").
- **The hand-off mechanic needs zero new code beyond the op itself**: setting `decision_mode` away from `4`/human via a real `piece.pdl` METHOD row ("Auto-Play (AI takes over)" → `SET_DECISION_MODE:1`) IS taking-hand-off; setting it back to human IS taking control back. This is `corp_decide.c`'s own park-and-wait pattern, verbatim.
- **Widget-separation applies here too**: an `automation.chtpm` screen (dials only) should exist as its own widget, independent of `main.chtpm`/`farm.chtpm`/etc. — checking automation status should never require opening any gameplay screen.
- **Status quo**: none of this is built yet. Farm/Mine screens (§4-§9 above) exist and are manually verified; the automation layer is the next real increment on top, sharing its design 1:1 with civ-txt/tactics-txt rather than inventing a third variant.

---

## 🤔 11. OPEN QUESTIONS / DESIGN DECISIONS

1. **Farming difficulty:** Should planting cost a flat amount of grain, or require solving a puzzle (verse scramble) to earn bonus yield? (Reusing TSC_ELO's Mana Challenge engine?)
2. **Animal handling:** Scoped out for now, but should my-chara-txt include a single cow/chicken for future expansion, or keep it completely absent?
3. **NPC market prices:** Should they be fully deterministic (same price every day), or add daily variance (±10%) to make trading more strategic?
4. **Game end condition:** Exactly at max_days, or should there be a win state (e.g., reach 10,000 money)?
5. **Auto-eat threshold:** At what health% does it trigger? (Currently: health < 20.)

---

## 🏁 12. TL;DR — THE 30-SECOND VERSION

- One character, one text-based screen, roguelike turn-based time.
- **Farm → Mine → Store → Inventory → EndTurn**, rinse repeat.
- Every action appends to `master_ledger.txt` — audit trail is built-in.
- **Simplest possible farming game**, designed to **test the ledger and op architecture** before scaling to multiplayer (genesis-txt) or visual layers (my-chara-zr).
- Reuses TSC_ELO's ledger + PAL patterns, WAY simpler mechanics.

**Next step:** Agree on the open questions, then build Phase 1 (skeleton + compose).

🧑 Ready to farm? Let's go. 🌾
