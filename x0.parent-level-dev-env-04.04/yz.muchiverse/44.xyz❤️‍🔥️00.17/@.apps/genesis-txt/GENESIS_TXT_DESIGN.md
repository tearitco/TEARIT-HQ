# 🌍 GENESIS-TXT — MULTIPLAYER FARMING & TRADING, TEXT-BASED, LEDGER-AUDITED

> **PURPOSE:** A multiplayer farming/trading game. Multiple human players and/or computer AI, competing to maximize wealth through farming, mining, and exchange.
>
> **Complexity level:** Simpler than 014.wsr-pal (no corporations, no R&D, no stock markets), but more complex than my-chara-txt (now has player-vs-player trading, AI opponents, simultaneous turns).
>
> **Read my-chara-txt's design first,** then come back here. genesis-txt **extends** my-chara-txt by adding multiplayer, order books, and AI.
>
> **⚠️ CORRECTION (2026-08-01):** Any section below describing a flat "compose op writes an ASCII HUD to view.txt" render with ad-hoc single-key actions is **superseded**. The real, proven interface is the **CHTPM nav pattern** — named `.chtpm` screens, `href` navigation, `${piece_methods}` buttons generated from a `piece.pdl` METHOD table, one PAL module per screen, a shared `*_menu_input`/`*_compose_frame` op pair per project. See `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` §4 (rewritten to match wsr-pal's `wsr_main_menu.chtpm`/`wsr_trade_menu.chtpm` and muchi-pals' `main.chtpm`/`store.chtpm`/`store_module.pal` — both real, running precedent) for the exact pattern. genesis-txt's own screens (world view, exchange board, per-player status) should follow that same shape: `genesis_menu_input`/`genesis_compose_frame`, one `.chtpm` + one PAL module per screen, multiplayer turn state read fresh from `current_layout.txt`/`config.txt` rather than tracked separately. Sections below written before this correction (ops list, directory layout, session flow) need a pass to match — do that pass when genesis-txt build actually starts, using my-chara-txt's finished implementation as the direct template.

---

## 📖 1. THE VISION

A **shared farming/trading world** with:
- 2-4 **human players** (local or networked, each with their own widget terminal)
- 0-N **computer players** (AI, running headless, no visual window)
- 0-N **background NPCs** (hardcoded traders; optional, for flavor)

**Core mechanics:**
- 🌾 Each player **farms** (owns plots, plants crops, harvests).
- ⛏️ Each player **mines** (extracts silver/gold).
- 💱 **Exchange ledger** — players post buy/sell orders for grain, wheat, silver, gold. Orders match by price and quantity (player-vs-player, not player-vs-NPC).
- 📊 **Prices** driven by supply/demand from the exchange (not hardcoded like my-chara-txt's NPC prices).
- 🤖 **AI players** use a decision_mode chassis (simple heuristics, not LLM) to decide what to farm/mine/trade.
- ⏳ **Time is roguelike:** all N players take 5 actions each, THEN time ticks by 1 day (not simultaneous actions; sequential turns, but synchronized day advancement).

**Win condition:** After N days, highest wealth (money + asset value) wins. Or longest survival (no starvation).

---

## 🏗️ 2. REFERENCE SOURCES & PATTERNS

| Source | Pattern we reuse |
|--------|------------------|
| **my-chara-txt** | Ledger architecture, config.txt state, PAL loop, op dispatch, asset types (grain, wheat, silver, gold) |
| **@.apps/TSC_ELO/** | Master ledger, turn alternation (turn % num_players), epoch model, AI player ledger ops |
| **014.wsr-pal💸️📌️+2** | Decision_mode chassis (preset/weighted/llm for AI difficulty), price formula from supply/demand, multiple entities ticking |
| **&.widgits/WIDGIT_BIBLE.md** | Two-program pattern (host + widgets), widget cmd bus for setup (W1), player widgets (P1-P4) |
| **file-menu ops** | ledger_append, ledger_peers (runtime ledger discovery — who's online?) |

**Key difference from my-chara-txt**: genesis-txt has **player-driven prices** (exchange ledger matching), **AI opponents**, and **simultaneous multi-player state**. Much closer to WSR's model than my-chara-txt.

---

## 🌍 3. GAME SETUP (WIDGET W1 — SETUP SCREEN)

**Widget W1 (separate program):** A text menu asking:

```
🌍 GENESIS-TXT — WORLD SETUP
  How many total players? [2-4] → 4
  How many are human? [0-4] → 2
  How many are computer? [auto] → 2
  Starting money per player? [default 500] → 500
  Game length in days? [10 / 365 / custom] → 365
  AI difficulty? [easy / medium / hard] → medium
  
  [▶️ START GAME]
```

**Behavior:**
- W1 writes match config to the **host game** via the **widget cmd bus** (`players:4`, `humans:2`, `start_money:500`, etc.).
- Host writes to `config.txt` (game-wide state).
- Host spawns player widgets (P1, P2 for humans; AI runners for computer players).

---

## 🌍 4. THE WORLD (DATA MODEL)

### 📋 `config.txt` — GAME STATE

```
num_players=4
humans=2
computers=2
ai_difficulty=2               # 0=easy, 1=medium, 2=hard
start_money=500
day=1
max_days=365
current_epoch=1
current_turn=0                # (0-19 for 4 players × 5 actions each)
game_state=playing            # playing | game_over
show_ledger=0
```

### 👥 `players.txt` — ALL PLAYER STATE (one line per player)

```
player_id|name|type|health|money|grain|wheat|silver|gold|status
0|Alice|human|100|450|30|10|5|0|alive
1|Bob|computer|95|500|25|8|3|2|alive
2|Charlie|human|85|480|20|5|10|1|alive
3|Diana|computer|100|500|15|12|2|0|alive
```

This mirrors TSC_ELO's `config.txt` player lines, extended with inventory items.

### 🧾 `master_ledger.txt` — APPEND-ONLY EVENT LOG

```
timestamp|day|epoch|player_id|turn_num|action_type|details
2026-08-01T12:00:00|1|1|0|0|plant|wheat:plot_0
2026-08-01T12:01:00|1|1|0|1|mine|silver:1
2026-08-01T12:02:00|1|1|0|2|order_post|buy:grain:10:price:2.0
2026-08-01T12:03:00|1|1|1|0|plant|corn:plot_0
2026-08-01T12:04:00|1|1|1|1|order_post|sell:wheat:20:price:3.0
2026-08-01T12:05:00|1|1|2|0|eat|grain:5
2026-08-01T12:10:00|1|1|0|3|day_end|
```

**turn_num** = current_turn % 5 (which of the 5 actions is this for this player in this round).

### 📊 `exchange.txt` — OPEN ORDERS (like a stock exchange)

```
order_id|player_id|side|commodity|qty|price_per|posted_day|status
1|0|buy|grain|10|2.0|1|open
2|1|sell|wheat|20|3.0|1|open
3|0|buy|silver|5|50.0|1|open
4|2|sell|gold|2|500.0|1|open
```

Orders sit here until matched (best-price-first matching). Matched orders are appended to ledger and removed from this file. **This drives prices**: average of recent trades = "market price."

---

## 🎮 5. TURN STRUCTURE (ROGUELIKE, SEQUENTIAL TURNS)

**Each "round":**
1. **Round starts.** Round display: "Round 1/73 (73 days × 5 actions each)" or similar.
2. **Player 0 takes 1-5 actions** (farm, mine, order, eat, idle; up to 5). Each action → ledger entry.
3. **Player 1 takes 1-5 actions.**
4. **Player 2 takes 1-5 actions.**
5. **Player 3 takes 1-5 actions.**
6. **All players done.** Tick happens: day increments by 1, health decays, orders may match, ledger entry "day_end".
7. **Check end condition:** day >= max_days? If yes, game_over. If no, go to step 1.

**Why sequential?** Simpler to audit. Player 0's action can't be retroactively changed by Player 1's action. Ledger is pure append, no merge conflicts.

---

## ⚙️ 6. ACTIONS & LEDGER ENTRIES

### 🌾 FARM
Same as my-chara-txt: plant wheat/corn (costs 10 grain), harvest when ripe.

### ⛏️ MINE
Same as my-chara-txt: RNG roll, 70% silver / 30% gold.

### 💱 POST AN ORDER (on exchange)

**Buy Order:**
```
Player says: "I want to buy 10 grain at $2 per unit"
→ exchange_post_order "buy" "grain" 10 2.0
→ ledger: "...|order_post|buy:grain:10:price:2.0"
→ order added to exchange.txt with status=open
```

**Sell Order:**
```
Player says: "I want to sell 20 wheat at $3 per unit"
→ exchange_post_order "sell" "wheat" 20 3.0
→ ledger: "...|order_post|sell:wheat:20:price:3.0"
→ order added to exchange.txt with status=open
```

**Matching logic:**
- After each ledger action, `exchange_tick` runs.
- Matches buy orders (highest price) with sell orders (lowest price), best-price-first.
- If buy_price >= sell_price, **MATCH!** → both orders update to status=filled, ledger entry for each: "order_fill".
- Inventory updates for both players.

### 📖 CANCEL AN ORDER
```
Player says: "Cancel my buy order #1"
→ exchange_cancel_order 1
→ ledger: "...|order_cancel|order_id:1"
→ order removed from exchange.txt
```

### 🍽️ EAT
```
Player: "Eat 5 grain"
→ eat_action 5
→ health += (5 × nutritional_value); grain -= 5
→ ledger: "...|eat|grain:5:health_gain:50"
```

### ⏳ END TURN / IDLE
```
Player: "Pass my turn" or timeout
→ no action added to ledger
→ next player's turn starts
```

---

## 🤖 7. AI PLAYER BEHAVIOR

**AI is NOT connected to a widget.** It runs headless, reading the ledger + exchange + game state, making decisions.

**Decision cycle (called each time it's the AI's turn):**

1. **Read current state:** config.txt, players.txt, exchange.txt, master_ledger.txt.
2. **Evaluate goals:** 
   - Easy mode: Plant if have grain, mine if have time, buy grain if cheap, sell ores if expensive.
   - Medium mode: Plan ahead (farm early, trade mid-game, consolidate late-game).
   - Hard mode: Hedge prices, detect demand trends, coordinate with other players' likely moves.
3. **Pick action:** e.g., "Post sell order for 5 silver at $45 (below average price to sell fast)."
4. **Execute:** Append ledger entry, update exchange.txt or players.txt as needed.
5. **Return control** to next player.

**Implementation:** Use decision_mode chassis (like WSR does):
- **Preset mode** (difficulty 0): hardcoded heuristics, instant.
- **Weighted mode** (difficulty 1): weighted scoring of options (farm vs. mine vs. trade), instant.
- **RL mode** (difficulty 2): learned from prior games, instant (if trained; otherwise fallback to weighted).

**LLM mode is NOT used here** (see 014.wsr-pal's GAME-AI-SPEED-DOCTRINE: LLM is too slow for routine decisions).

---

## 📊 8. PRICE DISCOVERY (SUPPLY & DEMAND)

**How prices emerge:**
- Exchange.txt holds all open orders.
- After each matched trade, ledger records the price.
- `exchange_compose` calculates market price as: average of last N trades (or OHLC if you want fancier).
- Displayed to all players (so they can see "grain is at $2.50 today").

**No hardcoded prices** like my-chara-txt. Prices are **emergent** from player behavior.

---

## ⚙️ 9. OPS (C tools, registered in `default_op.txt`)

| Op | Purpose |
|----|---------|
| `setup_game` | W1 widget → parse config → write config.txt, players.txt |
| `compose` | Render HUD to view.txt (for human players); show prices, orders, standings |
| `tick` | Advance day, apply hunger/status, check game end |
| `farm_plant` | Farm action |
| `farm_harvest` | Farm action |
| `mine` | Mine action |
| `exchange_post_order` | Add order to exchange.txt + ledger |
| `exchange_cancel_order` | Remove order from exchange.txt + ledger |
| `exchange_tick` | Match buy/sell orders, update ledger + inventory |
| `exchange_compose` | Render order book + price chart to view.txt |
| `eat_action` | Eat item, heal, ledger |
| `ai_decide` | AI reads state, picks action (calls appropriate ops internally) |
| `ledger_append` | Utility for appending to master_ledger.txt |
| `ledger_peers` | Runtime ledger discovery (who's still online?) |

---

## 🪟 10. WIDGETS (FOUR PROGRAMS)

### **W1 — SETUP WIDGET**
- Input: num_players, humans, computers, start_money, max_days, ai_difficulty.
- Output: writes config.txt via cmd bus.
- Then exits; game begins.

### **P1-P2 — PLAYER WIDGETS (for humans)**
- Each human player has their own GL window.
- Shows HUD: their health, money, inventory, current day, my turn / waiting for others.
- Shows exchange book: ask/bid spread, my orders.
- Input: keyboard → farm, mine, order, eat, pass.
- Separate programs, communicate with host via cmd bus (like TSC_ELO's H1).

### **A1-A4 — AI RUNNERS (for computer players)**
- No visual window.
- Headless program that runs ai_decide once per turn.
- Reads ledger, decides, updates ledger + exchange.
- Like TSC_ELO's A1 (AI player widget) but headless.

### **HOST GAME (referee)**
- Runs the main loop: tick + compose (for HUD render).
- Manages config.txt, players.txt, exchange.txt, master_ledger.txt.
- Calls exchange_tick to match orders.
- Doesn't render anything (no window); just orchestrates.

---

## 📁 11. DIRECTORY LAYOUT — `genesis-txt/`

```
genesis-txt/
├── GENESIS_TXT_DESIGN.md           ← you are here
├── button.sh                       ← launcher (start game, spawn widgets, cleanup)
├── scripts/
│   └── build.sh                   ← compile ops
├── default_op.txt                 ← op registry
├── system/                         ← symlinks from 014.wsr (prisc+x, parser, renderer, gl_mirror, keyboard_input)
├── ops/
│   ├── setup_game.c  compose.c  tick.c
│   ├── farm_plant.c  farm_harvest.c  mine.c
│   ├── exchange_post_order.c  exchange_cancel_order.c  exchange_tick.c  exchange_compose.c
│   ├── eat_action.c  ai_decide.c  ledger_append.c  ledger_peers.c
│   └── +x/          ← compiled binaries
├── pal/
│   └── main_loop_chtpm.pal        ← tick + compose loop
├── pieces/
│   ├── system/                    ← config.txt, players.txt, exchange.txt
│   ├── display/                   ← view.txt (host HUD)
│   ├── apps/player_1_app/ ... player_4_app/  ← one per human player
│   ├── registry/fonts/ascii/     ← glyphs
│   └── system/widget_cmds/       ← inbox.txt, status.txt (cmd bus)
└── data/
    └── master_ledger.txt          ← immutable event log
```

---

## 🎮 12. FULL GAME SESSION (END-TO-END)

```
1️⃣ START
   button.sh run
   ├─ Host game app starts (waits on setup)
   └─ W1 setup widget opens GL window

2️⃣ SETUP
   User clicks: 4 players, 2 human, medium difficulty, 365 days
   ├─ W1 writes config.txt via cmd bus
   ├─ Host writes players.txt (Alice, Bob, Computer 1, Computer 2)
   ├─ Host spawns player widgets P1 (Alice), P2 (Bob)
   ├─ Host spawns AI runners A1, A2 (headless)

3️⃣ DAY 1, ROUND 1
   Alice takes 2 actions:
   ├─ plant wheat on plot 0
   └─ mine (gets silver)
   
   Bob takes 1 action:
   ├─ post buy order: 10 grain @ $2.50
   
   AI 1 takes 3 actions:
   ├─ plant corn on plot 0
   ├─ mine (gets gold)
   └─ post sell order: 15 wheat @ $3.00
   
   AI 2 takes 2 actions:
   ├─ post buy order: 5 gold @ $400
   └─ eat grain
   
   → All actions appended to ledger, in order.
   → exchange_tick runs: no orders match yet (prices don't align).
   
4️⃣ END OF ROUND 1
   → tick runs
   → day becomes day 2
   → all players' health decays by 5 (hunger)
   → plants grow (now at day 2/4 until harvest)
   → ledger: "...|day_end|"
   
5️⃣ DAY 2 onwards
   Repeat until day 365.
   
6️⃣ GAME OVER
   Calculate final wealth: money + (grain × price) + (wheat × price) + (silver × price) + (gold × price)
   Winner: highest wealth. 🏆
```

---

## 🚀 13. BUILD ORDER

| Phase | What we build | Verified by |
|-------|---------------|-------------|
| **P1** 🥇 | Skeleton: setup widget (W1), config.txt, host loop | setup menu works, config.txt written |
| **P2** 👤 | One human player (P1): HvC game (human vs 1 AI), farming + mining | human can farm/mine, ledger entries correct |
| **P3** 💱 | Exchange: post/cancel orders, order book rendering | orders appear in exchange.txt, display works |
| **P4** 🤖 | Exchange matching: buy/sell orders match, prices emerge, inventory updates | two players trade, inventory updates both sides |
| **P5** 🎮 | Multiple human players: 2 humans vs 2 AI, full round cycling | all 4 players take turns, day ticks correctly |
| **P6** 🧠 | AI difficulty tuning (easy/medium/hard decision tiers) | AI plays differently at each difficulty |
| **P7** 📊 | Full audit: ledger replay, standings render, final wealth calc | restart, replay ledger, final state matches |
| **P8** ✨ | Polish: price chart, order expiry (old orders auto-cancel), stats screen | nice-to-haves working |

---

## 🔗 14. CROSS-REFERENCES & ECOSYSTEM

### Stepping stones:

- **my-chara-txt** ← Start here! Single player, simplest ledger ops. Understand this first.
- **genesis-txt** ← You are here. Extends my-chara-txt with multiplayer + exchange.
- **genesis-zr** → Next step after text is working. Same ledger, visual avatars on desktop.
- **014.wsr-pal💸️📌️+2** → After genesis works, this is the full stock-market sim to learn from.

### Patterns reused:

- **Ledger + config pattern** from TSC_ELO (master_ledger.txt + config.txt).
- **Decision_mode chassis** from WSR (preset/weighted for AI difficulty).
- **Order book + price matching** is our own addition (WSR uses fixed prices, not emergent).
- **Roguelike turn structure** from WSR (tick all entities, advance day).

---

## 🤔 15. OPEN QUESTIONS / DESIGN DECISIONS

1. **Player limit:** Hard cap at 4, or allow more? (4 is nice for GL window tiling, but scalable?)
2. **Order expiry:** Should old orders expire after N days? Or sit until manually cancelled?
3. **Initial inventory:** All players start with only money. Should some start with grain/tools?
4. **Starvation mechanics:** If health hits 0, is the player eliminated, or just badly off?
5. **AI coordination:** Should AIs have shared goals (compete vs. humans) or fully independent?
6. **Price volatility:** How much random variance day-to-day? Currently: emergent from orders only.
7. **Game-end event:** Just highest wealth, or any other win condition (e.g., first to 10k)?

---

## 🏁 16. TL;DR — THE 30-SECOND VERSION

- **2-4 players** (human + AI), **365-day game** (configurable).
- **Farm, mine, post buy/sell orders.** Roguelike turns: each player takes up to 5 actions, then day ticks.
- **Orders match on an exchange ledger** → prices emerge from supply/demand, not hardcoded.
- **AI uses decision_mode** (easy/medium/hard heuristics) to play competitively.
- **Fully ledger-auditable:** every action is an append-only entry. Replay = read ledger.
- **Simplest multiplayer farming game**, bridges my-chara-txt (single player) and WSR (full market sim).

**Next step:** Agree on open questions, build from my-chara-txt's skeleton, then extend.

🌍 Ready to settle a new world? Let's go. 🌾
