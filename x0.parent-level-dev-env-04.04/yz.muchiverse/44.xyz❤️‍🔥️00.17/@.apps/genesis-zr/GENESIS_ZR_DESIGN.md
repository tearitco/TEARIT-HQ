# 🌍🖥️ GENESIS-ZR — DESKTOP MULTIPLAYER FARMING & TRADING, VISUAL

> **PURPOSE:** Desktop/visual version of genesis-txt. Same multiplayer farming game, same ledger, **different rendering layer** using desktop avatars, live land plots, and shared exchange board.
>
> **NOT BUILDING YET.** This doc plans the visual layer without changing game logic. Read genesis-txt first; this describes how to **visualize** it.
>
> **⚠️ CORRECTION (2026-08-01):** Same correction as my-chara-zr — this is the real CHTPM `.chtpm`/`piece.pdl`/menu_input system rendered via `chtpm_rgb_render`/`gl_mirror` (GL primary per xyzos-standards §35), not a separate "compose_gl.c" render architecture. See `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` §4 for the proven pattern this must follow. Revisit before building.

---

## 📖 1. THE VISION

Same multiplayer game as **genesis-txt** (2-4 players, farm/mine/exchange over 365 days):
- 🧑 Multiple **avatars on screen** (one per human player), draggable, with status bars.
- 🌾 **Shared "world" view:** all land plots visible (whose farm is whose?).
- 💰 **Price ticker:** live commodity prices updated as trades happen.
- 💱 **Exchange board:** open orders visible to all players, animated order matching.
- ⏳ **Synchronized time:** all players see the same day/time, animated calendar.
- 🤖 **AI players:** appear as AI avatars on screen (or as notifications when they act).

**Technical:** Same ledger + ops as genesis-txt. Additional **render layer** converts multi-player state → visual elements. Uses muchi-pals infrastructure for multi-window coordination.

---

## 🏗️ 2. DESIGN PHILOSOPHY

### **Same Logic, Different Skin**

- **genesis-txt** = ledger-driven multiplayer game, text renderer (view.txt per player).
- **genesis-zr** = same ledger-driven game, **visual renderer** (GL world view + avatars + shared widgets).

**All ops stay the same.** Only the **render layer** differs:
- `compose.c` (in txt) → renders to text view.txt for each player.
- `compose_gl.c` (in zr) → renders to GL world window + player status windows + exchange board + price ticker.

**Consequence:** The **master_ledger.txt is 100% identical** between txt and zr versions. Record a game in txt, replay in zr (or vice versa) with no data conversion.

---

## 🎨 3. VISUAL COMPONENTS

### **🌍 WORLD VIEW (Central window, shared)**

A top-down or isometric view of the shared farming world:

```
┌────────────────────────────────────┐
│ 🌍 GENESIS-TXT — WORLD (Day 5)    │
├────────────────────────────────────┤
│                                    │
│  [Alice's Farm] [Bob's Farm]      │  ← Player territories
│   🌾 🌾 🌾      ⬜ 🌱 🌾          │
│   🌾 🌾 🌾      🌾 🌾 🌾          │
│                                    │
│  [AI-1 Farm]    [AI-2 Farm]       │
│   🌱 🌾 ⬜      🌾 🌾 🌾          │
│   🌾 🌾 ⬜      ⬜ 🌾 ⬜          │
│                                    │
│  💰 Market Prices (Day 5):        │
│  Grain: 2.50 ↑ | Silver: 45.00 ↓│
│  Wheat: 3.00 ↑ | Gold: 400.00 ↗│
│                                    │
└────────────────────────────────────┘
```

**Features:**
- **Grid layout:** Each player gets a territory (e.g., top-left, top-right, bottom-left, bottom-right).
- **Plot visualization:** Empty (⬜), growing (🌱), ripe (🌾) with animated transitions.
- **Prices ticker:** Live updates as orders match.
- **Turn indicator:** Shows "Alice's turn (2/5 actions used)" or similar.

**Interactions:**
- Click a plot → details window (shows owner, crop type, days until harvest).
- Hover over price → show recent trade history for that commodity.

### **👥 PLAYER STATUS WINDOWS (Multiple, one per human player)**

Each human player gets a status sidebar (can be docked or floating):

```
┌──────────────┐
│ 🧑 Alice     │
├──────────────┤
│ ❤️  75/100   │
│ 💰 450       │
│ 🌾 30        │
│ 🌽 10        │
│ ⚒️  5        │
│ ✨ 1         │
│              │
│ [TURN: 2/5]  │
│ [Farm]       │
│ [Mine]       │
│ [Trade]      │
│ [Pass]       │
└──────────────┘

┌──────────────┐
│ 🧑 Bob       │
├──────────────┤
│ ❤️  95/100   │
│ 💰 500       │
│ 🌾 25        │
│ 🌽 8         │
│ ⚒️  3        │
│ ✨ 2         │
│              │
│ [WAITING...] │
└──────────────┘
```

**Features:**
- Health/money/inventory at a glance.
- Turn counter (if it's your turn, shows action buttons).
- Can be minimized to small icon.

### **💱 EXCHANGE BOARD (Shared widget)**

A dedicated window showing the live order book:

```
┌────────────────────────────────────┐
│ 💱 EXCHANGE BOARD (Day 5)          │
├────────────────────────────────────┤
│ BUY ORDERS          | SELL ORDERS  │
├─────────────────────┼──────────────┤
│ Alice: 10 grain@2.5 | Bob: 20 wheat@3.0
│ AI-1:  5 gold@400   | Alice: 5 silver@50
│ Bob:   3 silver@45  |
│                     |
│ RECENT TRADES:      │
│ [Day 5] Alice bought 10 grain @ 2.50
│ [Day 4] Bob sold 15 wheat @ 3.00
│ [Day 4] AI-2 bought 5 gold @ 395
│
└────────────────────────────────────┘
```

**Features:**
- Live bid/ask spread (left = buy orders, right = sell orders).
- Orders highlight when you posted them.
- Recent trade log (scroll history).
- Animated when new orders posted or matched (flash, sound effect?).

**Interactions:**
- Click order → cancel it (if yours).
- Double-click to see full details.

### **⏳ GAME CLOCK WIDGET**

A small, shared widget showing game time:

```
┌──────────────────┐
│ ⏳ DAY 5, ROUND 1│
│ (Alice: 2/5)     │
│ (Bob: waiting)   │
│ (AI-1: done)     │
│ (AI-2: done)     │
│                  │
│ [PAUSE] [SPEED ▶]│
└──────────────────┘
```

**Features:**
- Shows current day + round + who's taking turns.
- Optional pause/speed controls (let time run automatically or step through turns).

---

## 🖥️ 4. MULTI-WINDOW COORDINATION (FROM MUCHI-PALS)

**Window ecosystem:**
- **World window:** Central, always visible (or alt-tab to).
- **Player status windows:** One per human, docked left side (or floating).
- **Exchange board:** Right side, or separate tab.
- **Clock widget:** Top corner, always visible (or minimized).

**Window manager (reused from muchi-pals):**
- Windows can be dragged, resized, minimized to icons.
- Docking suggestions (snap to edges, align with other windows).
- Focus switching via keyboard (Tab or number keys).
- All window positions saved to `pieces/display/window_layout.txt` between sessions.

---

## ⚙️ 5. RENDERING ARCHITECTURE

### **Dual Render Path (Like my-chara-zr)**

```
genesis-txt:
  config.txt + players.txt + exchange.txt + ledger
      ↓
  compose.c (text renderer)
      ↓
  view.txt per player (ASCII HUD)
      ↓
  terminal display (one per player)

genesis-zr:
  config.txt + players.txt + exchange.txt + ledger (SAME DATA)
      ↓
  compose_gl.c (visual renderer)
      ↓
  World window + status windows + exchange board (GL)
      ↓
  desktop display (shared + per-player)
```

**Key:** Same ledger, same state files. Only rendering changes.

### **Compose Process (Visual)**

`compose_gl.c` reads:
1. `config.txt` (game state, current day, current player's turn).
2. `players.txt` (all player status: health, money, inventory).
3. `exchange.txt` (open orders + recent trades).
4. `master_ledger.txt` (to compute prices from recent trades).
5. `pieces/display/window_layout.txt` (where should windows be positioned?).

Then renders:
- World view window (territory + plots + price ticker).
- Player status windows (one per human).
- Exchange board window.
- Game clock widget.
- Animated transitions (order matching, crop growth).

---

## 📁 6. DIRECTORY LAYOUT — `genesis-zr/`

Same as genesis-txt, **plus:**

```
genesis-zr/
├── GENESIS_ZR_DESIGN.md            ← you are here
├── button.sh                       ← launcher (spawn world + player windows)
├── scripts/
│   └── build.sh
├── default_op.txt                  ← ops (same as txt, but compose_gl instead of compose)
├── system/                         ← symlinks from muchi-pals (window mgmt, drag-drop, gl_mirror)
├── ops/
│   ├── farm_plant.c  ... (same as txt)
│   ├── compose_gl.c   ← DIFFERENT: GL renderer
│   └── +x/
├── pal/
│   └── main_loop_gl.pal           ← tick + compose_gl
├── pieces/
│   ├── system/                    ← config.txt, players.txt, exchange.txt (same as txt)
│   ├── display/                   ← world_view.txt, exchange_board.txt, window_layout.txt
│   ├── apps/
│   │   ├── world_app/            ← [NEW] shared world state
│   │   ├── player_1_app/ ... player_4_app/  ← per-player status
│   │   └── exchange_app/         ← [NEW] shared exchange board state
│   ├── registry/fonts/           ← glyphs
│   ├── assets/                   ← [NEW] PNGs/SVGs for crops, avatars, UI
│   │   ├── crops/                ← wheat_empty.svg, wheat_growing.svg, wheat_ripe.svg
│   │   ├── avatars/              ← [NEW] alice.svg, bob.svg, ai_1.svg, ai_2.svg
│   │   ├── commodities/          ← grain.svg, silver.svg, gold.svg
│   │   └── ui/                   ← health_bar.svg, price_up.svg, price_down.svg
│   └── system/widget_cmds/       ← (optional setup widget cmd bus)
└── data/
    └── master_ledger.txt         ← SAME ledger as txt version
```

---

## 🎮 7. PLAYER INTERACTION FLOW (Example)

```
1️⃣ WORLD LOADS
   compose_gl renders:
   → world window (4-territory farm view)
   → 4 player status windows on left (Alice, Bob, AI-1, AI-2)
   → exchange board on right
   → clock widget top-center

2️⃣ ALICE'S TURN BEGINS
   clock shows "Alice: 1/5"
   Alice clicks [Farm] button in her status window
   → action_handler passes control to farm_plant op
   → UI shows "Select a plot to farm"

3️⃣ ALICE CLICKS PLOT (0,0) IN WORLD
   → farm_plant validates, updates players.txt (Alice's inventory)
   → ledger entry added
   → exchange_tick runs (no orders matched)
   → compose_gl re-renders
   → world window updates: Alice's plot (0,0) now shows [🌱 Growing]
   → Alice's status window updates: grain -= 10

4️⃣ BOB'S TURN (While Alice finishes her 5 actions)
   clock shows "Bob: waiting" (dimmed)
   Bob can click [Farm] to start his turn, or pass

5️⃣ ORDERS MATCH ON EXCHANGE
   Alice posts: sell 10 grain @ 3.0
   Bob posts: buy 10 grain @ 3.0
   → exchange_tick matches them
   → ledger entries: order_post (Alice), order_post (Bob), order_fill (both)
   → exchange board animates: orders flash, "TRADE EXECUTED" notification
   → prices update in world window (price history)

6️⃣ END OF DAY (All players done)
   clock shows "Day 6" (incremented)
   → tick runs: plants grow, health decays, prices recalc
   → compose_gl updates all windows
   → world view shows plots aged +1 day
   → status windows show health decrements

7️⃣ GAME ENDS (Day 365 reached)
   → final wealth calculated
   → standings window: 1st Alice (5000g), 2nd Bob (4500g), 3rd AI-1, 4th AI-2
   → option to replay or new game
```

---

## 🔗 8. CROSS-REFERENCES & ECOSYSTEM

### Relationship to genesis-txt:
- **genesis-txt:** Text-based, multiple view.txt files per player.
- **genesis-zr:** Visual-based, GL world + windows.
- **Same ledger, same ops, different render path.**

### Relationship to my-chara-zr:
- **my-chara-zr:** Single-player visual farming (avatar + plots + inventory).
- **genesis-zr:** Multi-player version (multiple avatars + shared world + exchange board).
- **Can reuse window management code, but genesis-zr is more complex** (multi-entity coordination).

### Relationship to muchi-pals:
- **01.muchi-pals-🥚️-13.01:** Desktop pet ecosystem (living entities, drag-drop, zoo).
- **genesis-zr:** Uses muchi-pals' window infrastructure, but is a standalone game (not zoo-integrated yet).
- **Future:** Could integrate genesis-zr avatars INTO the muchi-pals zoo (so your farm is a living entity on the desktop).

### Relationship to WSR:
- **014.wsr-pal:** Full stock-market sim with corporations, R&D, taxes.
- **genesis-zr:** Farming + commodity exchange (simpler domain, but same multi-entity ledger pattern).
- **genesis-zr is good practice for WSR's render layer** (multiple entities, live prices, order book).

---

## 🤔 9. DESIGN DECISIONS / OPEN QUESTIONS

1. **World view style:** Top-down grid (like Stardew Valley), isometric, or abstract map? (Start simple: 2×2 grid for 4 players.)
2. **Avatar representation:** Simple colored squares + initials, or invest in pixel-art avatars? (Start simple.)
3. **Animation speed:** Instant updates (crops grow instantly when day ticks) or animated transitions (crop grows smoothly over 1 second)? (Animated feels better but more complex.)
4. **AI notification:** When AI player acts, should we show a log message ("AI-1 planted wheat"), or silently update the world? (Show log for clarity.)
5. **Multiplayer sync:** Should players see each other's actions in real-time (as they happen), or only at day-end? (Real-time is more engaging, but requires network sync if remote players.)
6. **Exchange auto-scroll:** Should exchange board auto-scroll to recent trades, or let player scroll manually? (Auto-scroll with option to freeze.)
7. **Replay feature:** Should genesis-zr support replaying a saved ledger (fast-forward through a game)? (Nice-to-have, mirrors my-chara-txt's replay.)

---

## 🚀 10. BUILD ORDER (After genesis-txt is complete)

| Phase | What we build | Verified by |
|-------|---------------|-------------|
| **P1** 🥇 | Window framework: world window + player status windows, docking | windows layout works, can be resized |
| **P2** 🌍 | World rendering: territories, plots (empty/growing/ripe), price ticker | world view shows all 4 players' farms, plots update |
| **P3** 👥 | Player status windows: health/money/inventory, action buttons | status displays correctly for each player |
| **P4** 💱 | Exchange board: order book display, recent trades log | orders visible, trades logged |
| **P5** 🎮 | Player actions: farm/mine/order from world view, all visuals update | full game playable via GUI |
| **P6** 🤖 | AI integration: AI players appear, move updates world + exchange | all 4 players (2 human + 2 AI) play, world updates |
| **P7** ⏳ | Game clock + pacing: time ticks visually, day/round display, pause/speed | time displays, day advancement visible |
| **P8** ✨ | Polish: animations, notifications, price chart, standings screen | feels alive, end-game standings shown |
| **P9** 🔄 | Ledger parity test: same ledger as txt version, replay in both modes | txt and zr versions stay in sync |

---

## 🏁 11. TL;DR — THE 30-SECOND VERSION

- **Same multiplayer farming game as genesis-txt**, but visualized on desktop.
- **Shared world view** showing all 4 players' territories + price ticker.
- **Individual player status windows** + shared exchange board + game clock.
- **Rendering layer changes (GL), logic stays the same** — same ledger, same ops.
- Reuses window management from muchi-pals (drag-drop, docking).
- **Not building yet**, but this doc locks down how visual layer scales to multiplayer.

🌍 Shared world is coming. 🤝
