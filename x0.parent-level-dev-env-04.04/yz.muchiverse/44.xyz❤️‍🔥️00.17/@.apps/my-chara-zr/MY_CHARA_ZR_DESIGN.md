# 🧑🖥️ MY-CHARA-ZR — DESKTOP VISUAL VERSION OF MY-CHARA-TXT

> **PURPOSE:** Desktop/visual version of my-chara-txt. Same farming game, same ledger, **different rendering layer** using desktop windows, drag-drop, and live avatars.
>
> **NOT BUILDING YET.** This doc exists to plan what the visual layer adds without changing the game logic. Read my-chara-txt first to understand the base game; this document describes how to **visualize** it.
>
> **⚠️ CORRECTION (2026-08-01):** my-chara-txt's own design was corrected to use the real, proven **CHTPM nav pattern** (named `.chtpm` screens + `href` navigation + `${piece_methods}`/`piece.pdl` + per-screen PAL modules), not a flat "compose renders an ASCII box" model. Per `xyzos-standards §35` (GL is primary, ASCII secondary; widgets are GL-only spawn profiles of the SAME app, not a separate render path), my-chara-zr is **not** a wholesale different rendering architecture from my-chara-txt — it is the SAME `.chtpm`/`piece.pdl`/menu_input system with `chtpm_rgb_render`/`gl_mirror` painting the GL surface instead of (or alongside) the ASCII renderer. "compose_gl.c" below should be understood as `mychara_compose_frame` writing the same `view.txt`/`piece.pdl` data that the RGB pipeline consumes, not a hand-built alternate render function. Revisit this doc's rendering sections against my-chara-txt's finished implementation before building.

---

## 📖 1. THE VISION

Same game as **my-chara-txt** (one character, farm/mine/trade/inventory):
- 🌾 Farm wheat/corn on owned land plots.
- ⛏️ Mine silver/gold.
- 💱 Exchange with NPCs.
- 🪟 Inventory window (items as widgets).
- ⏳ Day progression.
- 📊 Health bar, time display.

**But with visual elements:**
- **Live avatar on desktop:** Your character appears as a draggable window/entity on the screen. Can minimize it (→ folder image), drag it around.
- **Land plots as visual representations:** Plots appear as mini-fields in the avatar window or as separate "field" windows.
- **Animated growth:** Crops grow visually as pixels/icons change state (empty → sprouting → ripe).
- **Item inventory as draggable objects:** Inventory items can be dragged into the avatar window to use/consume.
- **Context menus:** Right-click avatar → [Farm] [Mine] [Exchange] [Inventory] etc.

**Technical:** Same ledger + ops as my-chara-txt. Additional **render layer** converts state → visual elements. Uses existing infrastructure from 01.muchi-pals (window management, drag-drop, asset rendering).

---

## 🏗️ 2. DESIGN PHILOSOPHY

### **Same Logic, Different Skin**

- **my-chara-txt** = ledger-driven game, text renderer (view.txt).
- **my-chara-zr** = same ledger-driven game, **visual renderer** (GL windows, avatar entity on desktop).

**All ops stay the same.** Only the **render layer** differs:
- `compose.c` (in txt) → renders to text view.txt.
- `compose_gl.c` (in zr) → renders to GL windows + avatar positioning.

**Consequence:** The ledger is 100% identical between txt and zr versions. A ledger saved in txt mode can be replayed in zr mode (or vice versa) with no data loss.

---

## 🎨 3. VISUAL COMPONENTS

### **🧑 AVATAR ENTITY (Main window)**

A draggable window representing the character:

```
┌─────────────────────┐
│ 🧑 Adam (my-chara)  │  ← Draggable, can minimize
├─────────────────────┤
│                     │
│  ❤️  75/100        │  ← Health bar
│  💰 420 coins       │  ← Money
│                     │
│  [🌾] [⛏️] [💼]  │  ← Quick-action buttons
│  Farm  Mine  Trade  │
│                     │
│  Day 5, Morning     │  ← Time display
│                     │
└─────────────────────┘
```

**Actions:**
- **Left-click buttons** → execute action (opens modal or sub-window).
- **Right-click avatar** → context menu (Farm, Mine, Exchange, Inventory, Sleep, etc.).
- **Drag avatar** → move it around desktop (just for visual organization, no game effect).
- **Click minimize** → collapses to a folder icon (can reopen).

### **🌾 LAND PLOTS WINDOW**

When user clicks Farm or the [🌾] button, a separate window opens:

```
┌──────────────────────────────┐
│ 🌾 My Plots (Adam)           │
├──────────────────────────────┤
│ Plot 0: [🌾 Wheat][HARVEST]  │  (mature, clickable to harvest)
│ Plot 1: [🌱 Growing...][▪▪▪▪░]│  (2 days left, visual growth bar)
│ Plot 2: [⬜ Empty]           │  (clickable to plant)
│                              │
│ [Wheat seed: 10] [Corn seed: 5]
└──────────────────────────────┘
```

**Interactions:**
- Click empty plot → choose crop (wheat/corn) → plant.
- Click growing plot → shows progress bar + harvest date.
- Click mature plot → harvest (adds grain, removes plot).
- Drag seeds from inventory into a plot? (Drag-drop experiment, optional.)

### **📦 INVENTORY WINDOW**

Opening inventory shows items as draggable objects or clickable list:

```
┌─────────────────────┐
│ 📦 Inventory (Adam) │
├─────────────────────┤
│ [Grain x30]         │  Clickable / draggable
│ [Silver x5]         │
│ [Gold x1]           │
│ [🛏️ Bed]           │
│ [🚽 Toilet]        │
│ [🚿 Shower]        │
└─────────────────────┘
```

**Interactions:**
- Click item → use it (grain → eat, bed → sleep, etc.).
- Drag item onto avatar? → use it (experimental drag-drop).
- Right-click item → context menu (use, drop, info).

### **💱 EXCHANGE WINDOW**

When opening exchange with NPCs:

```
┌────────────────────────────────┐
│ 💱 Market Exchange             │
├────────────────────────────────┤
│ Prices Today (Day 5):           │
│  Grain: 💰 2.5 per unit        │
│  Silver: 💰 45.0 per unit      │
│                                │
│ [BUY] [SELL] [CANCEL]          │
│                                │
│ Your open orders:              │
│  (none)                        │
└────────────────────────────────┘
```

---

## 🖥️ 4. WINDOW MANAGEMENT (FROM MUCHI-PALS)

**Existing infrastructure (01.muchi-pals-🥚️-13.01):**
- Window spawning / closing.
- Drag-drop between windows.
- Window minimize → folder icon.
- Context menus on right-click.

**Reuse pattern:**
- my-chara-zr **uses the same window infrastructure** but launches from the my-chara-zr app instead of the muchi-pals zoo.
- Avatar entity is a "living entity" window (like a pet, but not a pet yet — just a window).
- Plots/inventory/exchange are sub-windows (child or sibling windows).

**Future:** Eventually, the avatar could be a **real entity on the desktop** with more lifelike behavior (wanders, performs actions autonomously, etc.) — that's the full "pet/ZR" version with desktop integration. For now, it's just a rich window.

---

## ⚙️ 5. RENDERING ARCHITECTURE

### **Dual Render Path**

```
my-chara-txt:
  config.txt + ledger
      ↓
  compose.c (text renderer)
      ↓
  view.txt (ASCII HUD)
      ↓
  terminal display

my-chara-zr:
  config.txt + ledger (SAME DATA)
      ↓
  compose_gl.c (visual renderer)
      ↓
  Avatar window + Plot windows + Inventory window (GL)
      ↓
  desktop display
```

**Key insight:** The **ledger is shared.** Only the rendering changes.

### **Compose Process (Visual)**

`compose_gl.c` reads:
1. `config.txt` (game state, day, health, money).
2. `pieces/system/plots.txt` (plot states: empty/growing/ripe).
3. `pieces/apps/player_app/inventory.txt` (items owned).
4. `pieces/display/avatar_pos.txt` (where did user drag the avatar last?).

Then renders:
- Avatar window at avatar_pos.txt coordinates.
- Sub-windows for plots, inventory, exchange (positioned relative or stacked).
- Status bars (health, progress).
- Time display.

---

## 📁 6. DIRECTORY LAYOUT — `my-chara-zr/`

Same as my-chara-txt, **plus:**

```
my-chara-zr/
├── MY_CHARA_ZR_DESIGN.md           ← you are here
├── button.sh                       ← launcher (spawn avatar window + setup)
├── scripts/
│   └── build.sh                   ← compile ops + compose_gl
├── default_op.txt                 ← op registry (same as txt, but compose_gl instead of compose)
├── system/                         ← symlinks from 01.muchi-pals (window mgmt, drag-drop, gl_mirror)
├── ops/
│   ├── farm_plant.c  ... (same as txt)
│   ├── compose_gl.c   ← DIFFERENT: GL renderer instead of text
│   └── +x/
├── pal/
│   └── main_loop_gl.pal           ← loop: tick + compose_gl (not compose)
├── pieces/
│   ├── system/                    ← config.txt, plots.txt (same as txt)
│   ├── display/                   ← avatar_pos.txt (draggable position), gl windows list
│   ├── apps/player_app/           ← view.txt + inventory window state
│   ├── registry/fonts/            ← glyphs (same or extended for visual)
│   ├── assets/                    ← [NEW] PNGs/SVGs for crops, items, UI
│   │   ├── crops/                 ← wheat_empty.svg, wheat_growing.svg, wheat_ripe.svg, etc.
│   │   ├── items/                 ← grain.svg, bed.svg, toilet.svg, etc.
│   │   └── ui/                    ← health_bar.svg, money_icon.svg, etc.
│   └── system/widget_cmds/        ← (optional, if using widgets for setup; simpler: just context menu)
└── data/
    └── master_ledger.txt          ← SAME ledger as txt version
```

**Asset files (new):** Simple SVGs or PNGs for visual elements. Can be minimal (just colored squares for crops, text labels for items).

---

## 🎮 7. PLAYER INTERACTION FLOW

```
1️⃣ USER DRAG AVATAR
   Avatar window moved to new position
   → compose_gl updates avatar_pos.txt

2️⃣ USER CLICK [🌾] FARM BUTTON
   → farm_plant op called
   → opens "Select Plot" window
   → user clicks plot 0 (empty)
   → plant wheat (cost 10 grain)
   → config.txt updated, ledger entry added
   → compose_gl re-renders
   → plot 0 now shows [🌱 Growing]

3️⃣ USER RIGHT-CLICK INVENTORY ITEM (GRAIN)
   → context menu appears: [Use] [Drop] [Info]
   → click [Use]
   → eat op called
   → health += 50, grain -= 1
   → compose_gl re-renders health bar + inventory

4️⃣ TIME ADVANCES (day_end)
   → tick op runs
   → plots grow, health decays
   → compose_gl updates all windows
   → user sees plot progress bar advance
```

---

## 🔗 8. CROSS-REFERENCES & ECOSYSTEM

### Relationship to my-chara-txt:
- **my-chara-txt:** Text-based, `view.txt` render, simple.
- **my-chara-zr:** Visual-based, GL windows render, same ledger + ops.
- **Choose txt or zr at launch** (or later, mix them — same ledger = can switch).

### Relationship to muchi-pals:
- **01.muchi-pals-🥚️-13.01:** Living pet entities on desktop with drag-drop, window mgmt.
- **my-chara-zr:** Uses muchi-pals' window infrastructure + asset rendering, but launches as a standalone app (not part of the zoo ecosystem yet).
- **Future:** Could integrate my-chara-zr entities INTO the muchi-pals zoo (so your character lives in the zoo).

### Relationship to genesis-zr:
- **genesis-zr** extends my-chara-zr to multiplayer. Same GL rendering, but multiple avatars on screen + multi-window exchange board.

---

## 🤔 9. DESIGN DECISIONS / OPEN QUESTIONS

1. **Window layout:** Should avatar always be top-left, or remember last drag position? (Currently: remember in avatar_pos.txt.)
2. **Sub-windows:** Should plots/inventory/exchange be child windows (tied to avatar) or independent sibling windows?
3. **Drag-drop:** Can user drag seeds/items from inventory window into plot window to plant? (Nice-to-have, not required for P1.)
4. **Asset simplicity:** Just colored boxes + text labels, or invest in pixel art? (Start simple.)
5. **Integration with muchi-pals:** Should my-chara-zr entities eventually live in the muchi-pals zoo, or stay standalone?
6. **Minimize behavior:** Does minimized avatar → folder image on desktop, or just hide the window? (Folder image is cooler but more complex.)

---

## 🚀 10. BUILD ORDER (After my-chara-txt is complete)

| Phase | What we build | Verified by |
|-------|---------------|-------------|
| **P1** 🥇 | Window framework: avatar window + plot window, drag-drop works | avatar draggable, plots window opens |
| **P2** 🎨 | Render assets: crop icons (empty/growing/ripe), item icons | visual state renders correctly |
| **P3** 🌾 | Farm actions: click plot → plant/harvest, visuals update | farming works, crops grow visually |
| **P4** ⛏️ | Mine + inventory: mine works, inventory window shows items, items droppable | all visual interactions work |
| **P5** 💱 | Exchange window: open exchange, show prices, buy/sell + visuals | trading works, prices display |
| **P6** ✨ | Polish: animations (growth), context menus, minimize to icon | feels alive, window mgmt smooth |
| **P7** 🔄 | Ledger parity test: same ledger as txt version, replay in both modes | txt and zr versions stay in sync |

---

## 🏁 11. TL;DR — THE 30-SECOND VERSION

- **Same game as my-chara-txt**, but visualized on desktop with windows + drag-drop.
- Avatar is a draggable window. Plots/inventory/exchange are sub-windows.
- **Rendering layer changes (GL), logic stays the same** — same ledger, same ops.
- Reuses window management from muchi-pals (drag-drop, minimize).
- **Not building yet**, but this doc locks down how visual layer integrates.

🖥️ Visual layer coming later. 🎨
