# ⚔️⚔️ TSC_ELO — TRUE SWORDS CLASH ⚔️⚔️
### 🏹️ A Multiplayer/AI Duel Layer on Top of the TSOTS Verse Game 🛡️

> **DRAFT FOR REVIEW** — READ THIS BEFORE WE BUILD ANYTHING 🧐
> This is the design doc for the *next* version of our game. It is **not code yet**.
> Your job right now: read it, and tell me where we agree and where we disagree. 🖊️💬

---

## 📖 1. WHAT ARE WE BUILDING? (THE DREAM 🦄)

We already built **TSOTS OG** 🎮 — the single-player "scramble the verse, sort it back into order" game (now at `+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00/`).

Now we level it up into a **TRUE SWORDS CLASH** ⚔️ duel:
- 🥊 **Two fighters** face off (think RPG duel, spiritual-warfare themed).
- 📜 Every turn you fight by **solving a verse puzzle** (the Mana Challenge) to earn **Mana** ✨.
- 💫 You spend **Mana** to cast **Miracles** 🔥🩹⚡ that damage/heal/apply status to your foe.
- 🏆 **First to drop the enemy HP to 0 wins.**
- 🤖 The opponent can be a **human** 👤 (hot-seat) or a **computer AI** 🧠 — or even **computer vs computer** 🤖🤖 (watch the bots duel!).

And the whole thing runs on the house's **master ledger architecture** 📋 — the same pattern `101.ledger-player-npc-simple+3` uses to drive AI ops.

---

## 🗂️ 2. REFERENCE SOURCES (WHERE THE RECIPES COME FROM 👩‍🍳)

| # | Source | What we take from it |
|---|--------|---------------------|
| 1️⃣ | `+.TSOTS-ALPHA-OMEGA/RULES/True_Swords_Clash_Rules.txt` | 📜 The actual duel rules: HP/mana, Mana Challenge, Miracles, Status Effects, Win State |
| 2️⃣ | `101.ledger-player-npc-simple+3` | 📋 The **master ledger architecture**: `config.txt` state + `data/master_ledger.txt` event log + human/computer turn ops + game_dispatch tick |
| 3️⃣ | `&.widgits/WIDGIT_BIBLE.md` | 🪟 The **widget pattern**: two-program model, GL windows, runtime ledger, widget cmd bus (inbox/status) |
| 4️⃣ | `@.apps/text-editor-xyz/button.sh` | 🚀 The **combined app launcher** pattern: host app + widgets launched together, symlinked assets, cleanup traps |
| 5️⃣ | Our own `TSOTS-OG+01.00` (in `+.TSOTS-ALPHA-OMEGA/`) | 🎮 The **Mana Challenge engine** (deal/input/compose) — already built and tested! |

> ✍️ **HOUSE SPELLING NOTE:** it's **WIDGIT** (with an `i`) — the bible is explicit about this! Get it right in every file/comment. 🔤

---

## 🏗️ 3. THE BIG ARCHITECTURE (READ THIS TWICE 👀)

### 🧱 The Two-Program Pattern (from WIDGIT_BIBLE §1)

A widget is **never** a subprocess or thread of the host. They are **two completely separate programs**, communicating **only through plain text files** 📁 (no sockets, no pipes, no shared memory).

```
🧑‍💼 HOST (the game app)                      🪟 WIDGITs (separate programs)
├── own session dir                          ├── own session dir each
├── own system/ (binaries)                   ├── own system/ (binaries)
├── own ops/ (C tools)                       ├── own ops/ (C tools)
├── own PAL loop + prisc+x                   ├── own PAL loop + prisc+x
├── own keyboard_input + ASCII renderer      ├── own GL window (gl_mirror + chtpm_rgb_render)
└── PID: AAAA                                └── PID: BBBB / CCCC
```

### 📋 The Two Ledgers (don't confuse them! ⚠️)

| Ledger | File | Schema | Purpose |
|--------|------|--------|---------|
| 🏠 **Runtime ledger** | `<house>/<current_xyzfs>/home/runtime/ledger.txt` | `timestamp\|event\|type\|project_id\|session_root\|pid\|display_name\|inbox_path` | 👋 Who's online? Discovery. `ledger_append ONLINE/OFFLINE`, `ledger_peers <type>` |
| 🎲 **Game master ledger** | `TSC_ELO/data/master_ledger.txt` | `timestamp\|epoch\|player\|turn\|word\|action_type` | 🧾 The game's immutable event history. Every action ever taken, append-only |

The **game master ledger** is the referee's truth 📜. The **runtime ledger** is how widgets find each other 📡.

### 📨 The Widget Cmd Bus (from WIDGIT_BIBLE §7)

```
Widget generates a command (e.g. MODE:HvC)
  → writes to host's inbox:  pieces/system/widget_cmds/inbox.txt
    → host's drainer loop reads inbox.txt
      → host executes command
        → host writes ACK to widget's status.txt
```

We'll extend the house bus commands (NEW/SAVE/SAVE_AS/LOAD/PING) with **match commands**:
`MATCH:<MODE>`, `RATING:<opponent_elo>`, `PLAYER:<name>`, `START`, `FORFEIT`, `PING`.

---

## ⚔️ 4. TRUE SWORDS CLASH RULES (the actual duel 🥋)

Straight from `True_Swords_Clash_Rules.txt` — mapped onto our verse engine:

### 💚 HP / ✨ Mana
- Each fighter starts with **HP 100**, **Mana 0**.
- On your turn you pick **ONE** of two primary actions:
  1. **Gather Mana** (The Mana Challenge) — or —
  2. **Execute a Miracle** (attack/heal/status)

### 📜 THE MANA CHALLENGE (= our existing TSOTS gameplay! 🎯)
- Choose an amount **2 to 10** (that's how much mana you're trying to earn).
- You are shown **N shuffled verses** (N = mana requested).
- You must **sort them into original chronological order** (bible order) — exactly our scramble game!
- ✅ Success → you gain that mana. ❌ Failure → no mana, **your turn ends**.

> 🎉 **Huge win:** our TSOTS engine already does this (deal → round.txt + solution.txt → digits input). We **reuse it as-is** — it IS the Mana Challenge. 🎊

### 💫 MIRACLES (the combat moves)
| Miracle | Cost | Effect (from rules) |
|---------|------|---------------------|
| 🙏 Hail Mary | 0 | Flip a coin: Heads = double your mana; Tails = lose all mana |
| ⚔️ Slash | 1 | 2 HP damage; roll d6, on a 6 enemy starts **Bleeding** (-1 HP/turn) |
| 🔥 Fire | 2 | Roll d6 → that many HP damage |
| 😤 Berserk | 3 | Flip a coin: Heads = 13 HP damage; Tails = miss |
| 💚 Heal | 4 | Restore 10 HP to self + remove all negative status |
| ⚖️ Judgement | 5 | Flip a coin: Heads = steal ALL enemy mana |
| 🐍 Poison | 6 | Enemy gains **Poison** (-2 HP/turn) |
| 🦗 Locust | 7 | Enemy gains **Locust** (-1 mana/turn) |
| 😇 Blessing | 8 | Self gains **Blessing** (+1 mana/turn) |
| 👼 Angel | 9 | Steal 10 HP (heal self 10, damage enemy 10) + clear own negative status |
| 🗡️ Holy Sword | 10 | 25 HP damage; roll d6, on a 6 enemy starts **Bleeding** |

### 🌡️ STATUS EFFECTS (applied at the start of every turn)
| Status | Effect |
|--------|--------|
| 🩸 Bleeding | -1 HP at start of your turn |
| 🐍 Poison | -2 HP at start of your turn |
| 🦗 Locust | -1 mana at start of your turn |
| 😇 Blessing | +1 mana at start of your turn |

> Heal 💚 or Angel 👼 clears negative effects.

### 🏆 WIN STATE
- Reduce opponent HP to **0** → **VICTORY** 🎉
- Or a player **FORFEITS** → other player wins 🏅

---

## 🧩 5. COMPONENTS — WHO TALKS TO WHOM

### 🪟 W1 — SETUP WIDGIT (THE FIRST WIDGET 🥇)
This is what you asked for first — the **mode selector**.

**Screen:** a GL window with a menu:
```
⚔️ TRUE SWORDS CLASH — MATCH SETUP
  🎖️ YOUR RATING: 1000 (Padawan)          ← read from your xyzfs
  [ 🥊 1. HUMAN vs HUMAN        ]
  [ 🧠 2. HUMAN vs COMPUTER     ]
  [ 🤖 3. COMPUTER vs COMPUTER  ]
  [ 🎚️ 4. OPPONENT RATING (1000) ]         (AI strength = ELO number, modes 2 & 3)
  [ ▶️ 5. START MATCH           ]
```
**Behavior:**
- 👆 User navigates with arrows + Enter (or digit keys).
- ✅ Selection writes match config **into the host game** via the **widget cmd bus** (`MATCH:HvC` + `RATING:1200` + `START`).
- ➡️ That widget **passes data to the game** — exactly the "setup widget passes data to the game" flow you described.
- 📊 The opponent rating IS the difficulty — there is no separate "difficulty 1-3" menu anymore. Higher ELO = stronger AI (see §6.5).

### 🧑‍💼 H — HOST GAME APP (the referee + the human-facing dueling UI)
- Runs the master ledger + config state machine.
- Launches/relays to the human player widget when it's a human's turn.
- **CvC mode**: host is a silent referee — both widgets auto-play, host just renders the battle.

### 👤 H1 — HUMAN PLAYER WIDGIT
- Shows the **Mana Challenge** (scrambled verses via our engine) when it's your turn.
- Captures digit input → submits the sorted order → **result written to the master ledger**.
- After mana gained, shows the **Miracle menu** (choose your move) → ledger.
- This is basically our existing game screen, extracted into a widget window. 🎮→🪟

### 🤖 A1 — AI PLAYER WIDGIT (the computer brain, via master ledger!)
- Reads the master ledger + config, decides what to do, performs its turn by **appending its action to the ledger**.
- **Strength = ELO rating** (chess-style). The AI is parameterized by the rating it's given (default 1000):
  - 🔢 **Rating → skill curve:** every AI reads the *same* challenge; its **expected score** against a human is set by the ELO formula (§6.5). Concretely the AI's solve-accuracy, preferred mana amounts, and Miracle choices are all tuned to its target rating (e.g. 1000 = moderate, 1800+ = near-perfect solver).
  - 🎖️ **Tier names are only cosmetic** — a 1000-rating AI is "Padawan" and a 1700+ is "Grandmaster", but that's just a name badge derived from the number. The number does the work.
- **KEY IDEA from `101`:** the AI *is* a peer on the ledger. It doesn't cheat — it reads the same state every other player sees and acts through the same `word_turn_input`-style op. 🧠📋
- 🧾 After a duel, the AI's rating is updated by the same `tsc_elo` op as a human's (it has a real ELO in the ratings file, so CvC bots can climb too).

### ⚙️ OPS (C tools, all registered in `default_op.txt`)
| Op | What it does | Source pattern |
|----|--------------|----------------|
| `tsc_setup` | Reads setup cmd-bus inbox → writes `config.txt` (mode, players, ratings) | `game_init` in 101 |
| `tsc_deal` | **Reuse our `tsots_deal`**: pick N verses, scramble, write `round.txt` + `solution.txt` | ours ✅ |
| `tsc_input` | **Reuse our `tsots_input`**: digit answer → ledger entry, success/fail | ours ✅ |
| `tsc_tick` | The referee: advance turn, apply status effects, check win | `game_tick`/`game_dispatch` in 101 |
| `tsc_miracle` | Resolve a Miracle (RNG coin/dice, damage/heal/steal, status) → ledger | new |
| `tsc_ai` | AI decision + move → ledger (strength = its ELO rating) | `computer_turn` in 101 |
| `tsc_elo` | Load/update ELO ratings for players (read+write player xyzfs ratings file) | new |
| `tsc_compose` | **Reuse our `tsots_compose`**: render HP/mana bars + battle + verses to `view.txt` | ours ✅ |
| `ledger_append` | Runtime ledger ONLINE/OFFLINE (copy from file-menu) | file-menu ✅ |
| `ledger_peers` | Runtime ledger discovery (copy from file-menu) | file-menu ✅ |

---

## 📦 6. THE GAME MASTER LEDGER — EXACT FORMAT 🧾

File: `TSC_ELO/data/master_ledger.txt` (append-only, `|`-delimited, same shape as `101`):

```
timestamp|epoch|player|turn|word|action_type
```

But we extend `word` → `action` field with rich payloads. Example match (HvC, player 1 = human 👤, player 2 = AI 🧠):

```
2026-08-01T12:00:00|1|Player1|0|deal:4:rank|referee
2026-08-01T12:00:00|1|Player1|0|mana:4123:ok|human_input
2026-08-01T12:00:00|1|Player1|0|miracle:fire:3|human_input
2026-08-01T12:00:00|1|Player2|1|mana:2413:fail|computer_auto
2026-08-01T12:00:00|1|Player2|1|miracle:slash:2|computer_auto
2026-08-01T12:00:00|1|Player1|2|mana:2314:ok|human_input
2026-08-01T12:00:00|1|Player1|2|miracle:heal:0|human_input
2026-08-01T12:00:00|2|Player2|3|status:poison|tick
2026-08-01T12:00:00|2|Player1|4|win:Player1|referee
```

- 🧾 Every action is a line. Nothing is deleted, ever (append-only). Replay = read the ledger top-to-bottom.
- 🔁 The **`turn`** field cycles `turn % 2` to alternate players (exactly like `101`'s `(current_turn % total_actors) + 1`).
- 🏓 The **`epoch`** field increments each full round (both players have gone) — like `101`'s epoch model.

### 📝 `config.txt` — THE AUTHORITATIVE STATE (mirror of `101`'s config.txt)

```
mode=HvC                    # HvH | HvC | CvC
num_players=2
epoch_length=100            # effectively "no limit", duel ends at 0 HP
current_epoch=1
current_turn=0
game_state=playing          # playing | victory | forfeit
last_input=
player_1_type=human
player_1_name=Player1
player_1_rating=1000        # loaded from player xyzfs ratings file (§6.5)
player_1_hp=100
player_1_mana=0
player_1_status=none        # none | bleeding | poison | locust | blessing
player_2_type=computer
player_2_name=SKYNET
player_2_rating=1000        # AI strength = this ELO number (§6.5)
player_2_hp=100
player_2_mana=0
player_2_status=none
show_ledger=0
```

> ⚡ The rules (`HP`, `Mana`, `status`) live in `config.txt`. The **history** lives in `master_ledger.txt`. State file = source of truth for rendering; ledger = source of truth for replay/audit. This mirrors `101` exactly.
>
> ⚖️ The **ratings** (`player_N_rating`) are snapshots copied in at match start from each player's xyzfs ratings file — the live source of truth for ratings lives outside this project (see §6.5).

---

## 📊 6.5 THE ELO RATING SYSTEM (THE CHESS WAY ♟️)

You said it perfectly: **new players start at 1000 vs 1000, just like chess websites.** 🏁 No more hand-picked "difficulty 1-3" — the difficulty IS the opponent's rating number.

### 🧠 The Algorithm (standard ELO, chess-style)
- Every player has one **rating** (default **1000** for new players).
- Expected score for A vs B:  `E_A = 1 / (1 + 10^((R_B − R_A)/400))`
- After a match:               `R_A' = R_A + K × (S_A − E_A)`
  - `S_A` = result (1 = win, 0.5 = draw, 0 = loss)
  - `K` = K-factor (e.g. **K = 32**, like chess.com's starter games; can drop to K = 16 after ~20 rated games to stabilize)
- Result: beat a higher-rated foe → big gain; lose to a lower-rated foe → big loss; two 1000s vs each other → winner +16 / loser −16. 📈📉

### 🎖️ TIERS ARE JUST NAMES (cosmetic badges, not difficulty!)
The ELO number does ALL the real work. Tier titles are derived from the number purely for flavor 🎭 — being called "Grandmaster" never changes how hard the AI plays.

| Rating range | Tier name | Badge |
|--------------|-----------|-------|
| 0–999 | Padawan | 🟢 |
| 1000–1199 | Apprentice | 🟡 |
| 1200–1399 | Knight | 🟠 |
| 1400–1599 | Master | 🔴 |
| 1600+ | Grandmaster (GM) | 🟣 |

> A 1000-rated bot is "Apprentice"; a 1700-rated bot is "GM" — but what actually matters is the number. Same formula, same rules, for every player (human AND bot). 🧠🤖

### 💾 Where Ratings Live — PLAYER XYZFS (your call, exactly as you said ✅)
Ratings are **not** stored inside the game project. Each player's rating lives in **their own xyzfs location**, following the exact same resolution chain as the runtime ledger (from `ledger_append.c`):

```
house_root.txt  →  0.user-pal👤️/00.login-signup/current_login.txt (current_xyzfs)
                →  <house>/<current_xyzfs>/home/runtime/ledger.txt   ← runtime ledger
                →  <house>/<current_xyzfs>/home/games/tsc_ratings.txt ← OUR RATINGS 🏆
```

- **File:** `<house>/<current_xyzfs>/home/games/tsc_ratings.txt`
- **Format:** one line per player, pipe-delimited:
  ```
  player_name|rating|wins|losses|draws|games_played
  Player1|1000|0|0|0|0
  SKYNET|1000|0|0|0|0
  ```
- 👤 Human players: identified by the name they type in setup. Their rating follows them across every project/machine that shares their xyzfs.
- 🤖 AI bots: ALSO get a real line in this file (so CvC bots climb the ladder and HvC opponents have a rating history). Bot ratings can start at 1000 too, or be seeded higher for "GM" challenge bots.
- **Op:** `tsc_elo` reads the file at match start (→ `config.txt` snapshots) and writes it back atomically at match end (`.tmp` + `rename`, same as `101`'s config rewrite).

### 🧾 ELO RESULT IS ALSO LEDGERED
The final result (and each player's rating delta) is appended to the **game master ledger** so every match is replayable AND auditable:

```
2026-08-01T12:00:00|3|Player1|9|result:win:1000->1016|rating_update
2026-08-01T12:00:00|3|SKYNET|9|result:loss:1000->984|rating_update
```

---

## 🎮 7. THE MATCH LIFECYCLE (ONE FULL DUEL, END TO END 🏁)

```
 1️⃣ START:  button.sh run
     ├─ setup WIDGIT opens its GL window 🪟
     └─ host game app starts, waits on widget cmd bus 📨

 2️⃣ PICK:   user navigates setup widget
     ├─ setup shows YOUR rating from xyzfs 🎖️
     ├─ MATCH:HvC
     ├─ RATING:1000              (opponent ELO = difficulty; default = your rating)
     └─ START  →  host writes config.txt (mode, ratings, players)

 3️⃣ TURN:   host loop = tsc_tick (PAL loop, like 101's main_loop_chtpm.pal)
     ├─ tsc_tick reads config.txt
     ├─ current player = (turn % 2) + 1
     ├─ apply status effects (bleeding/poison/locust/blessing)
     ├─ if current player is HUMAN 👤:
     │     tsc_deal (N verses, N = chosen mana 2-10)
     │     human widget shows scramble → digits → tsc_input
     │     success → tsc_miracle menu → resolve → ledger
     ├─ if current player is AI 🧠:
     │     tsc_ai reads ledger+config → picks move → ledger
     ├─ tsc_compose renders HP/mana bars + battle log to view.txt
     └─ check win (any HP <= 0?) → victory screen 🏆

 4️⃣ RATE:   match over (victory/forfeit) 🏁
     ├─ tsc_elo computes both players' new ratings (K=32 formula)
     ├─ updates player xyzfs ratings file (atomic .tmp+rename)
     └─ appends rating_update lines to the game master ledger

 5️⃣ REPLAY:  tsc_compose can show the full ledger (view_ledger-style) anytime
```

### 🔁 PAL loop (borrowed straight from `101/pal/main_loop_chtpm.pal`)

```
exec ./ops/tsc_compose
hit_frame
loop:
  exec ./ops/tsc_tick
  sleep 16667
  j loop
```

---

## 🗃️ 8. DIRECTORY LAYOUT — `@.apps/TSC_ELO/` 📁

```
TSC_ELO/
├── TSC_DESIGN.md                ← you are here 📄
├── button.sh                    ← combined launcher: host + setup widget (+ player widgets) 🚀
├── scripts/
│   └── build.sh                 ← compile ops, symlink system/, copy glyphs (mirror our button.sh) ⚙️
├── default_op.txt               ← op registry (tsc_* + ledger_append/peers + builtin halt/out)
├── system/                      ← symlinks/copies from 014.wsr-pal (prisc+x, parser, renderer, gl_mirror, rgb_render, keyboard_input)
├── ops/                         ← C sources + built ops/+x/*.+x
│   ├── tsc_setup.c  tsc_deal.c  tsc_input.c  tsc_tick.c  tsc_miracle.c  tsc_ai.c  tsc_elo.c  tsc_compose.c
│   ├── ledger_append.c  ledger_peers.c        (copied from &.widgits/file-menu)
│   └── +x/                    ← compiled binaries
├── pal/
│   └── main_loop_chtpm.pal     ← tsc loop (tick + compose)
├── pieces/
│   ├── chtpm/layouts/
│   │   ├── setup.chtpm        ← setup widget layout (W1) 🪟
│   │   ├── battle.chtpm       ← human player widget layout (H1) 🎮
│   │   └── (ai widget has no layout — headless brain, or small status window) 🧠
│   ├── registry/fonts/ascii/  ← 95 glyphs (copied, never symlinked! 🔤)
│   ├── system/                ← config.txt, round.txt, solution.txt, game_state.txt, house_root.txt
│   ├── display/               ← current_frame.txt, current_layout.txt, game_screen_changed.txt, renderer_pulse.txt
│   ├── keyboard/              ← history.txt
│   ├── apps/player_app/       ← view.txt, interact_relay.txt
│   └── system/widget_cmds/    ← inbox.txt, status.txt (cmd bus) 📨
├── data/
│   ├── master_ledger.txt      ← THE GAME LEDGER 🧾
│   └── (ratings do NOT live here — they live in player xyzfs, §6.5) 🏆
```

> 💡 `TSC_ELO/` (formerly `TSC_XYZ/`) sits in the **app store** (`@.apps/`), like `@.apps/text-editor-xyz`. It launches its own host + widgets and shares the existing system assets + `location.txt` bible path via symlinks. It is a self-contained app module, not a nested copy.

---

## 🚀 9. BUILD ORDER (HOW WE GET THERE, STEP BY STEP 👣)

| Phase | What we build | Verified by |
|-------|---------------|-------------|
| **P1** 🥇 | `TSC_ELO/` skeleton: build.sh, system/, pal loop, tsc_compose renders a test duel frame | frame renders, no warnings |
| **P2** 🪟 | **SETUP WIDGIT (first widget):** GL window, HvH/HvC/CvC menu, cmd-bus handoff to host, host writes config.txt | real key injection (PITFALL 21): select HvC → config.txt shows mode=HvC |
| **P3** 👤🤖 | **HvH hot-seat duel:** both players take turns via Mana Challenge + Miracle menu; HP/mana/status tracked; win state. Uses our existing deal/input engine | full HvH match to victory via injected keys, ledger lines correct |
| **P4** 🧠 | **AI widget + HvC:** tsc_ai plays at a given ELO rating (1000 default); tsc_elo loads/updates ratings in player xyzfs; full HvC match | HvC match, AI actually plays at its rating, ledger shows computer_auto + rating_update lines |
| **P5** 🤖🤖 | **CvC mode:** both players are AI widgets; host renders the auto-battle | CvC match completes to a winner; both bot ratings update; replay from ledger |
| **P6** ✨ | Polish: battle log view, status effect animations, forfeit key, match history, tier badges | FRAME_REPORT + live demo |

**Always:** PITFALL 21 (real key injection, never op-level-only tests), PITFALL 54 (parser-first GL ordering), zero-warning build bar. 🛡️

---

## 🤔 10. OPEN QUESTIONS FOR YOU (please answer with 👍/👎 or edits!)

1. ~~**📍 Location:**~~ ✅ **RESOLVED** — you moved it to the app store: `@.apps/TSC_ELO/` (renamed from `TSC_XYZ`). Doc updated to match.
2. **🪟 Setup widget = GL-only widget window**, separate from the main game window (two-program pattern) — or do you want the setup screen as just *another CHTPM layout inside the main game* instead? (House pattern says separate widget program; but it's your call.)
3. **🧠 AI fairness:** AI plays through the *same* ledger ops as humans (it reads the challenge, doesn't peek at `solution.txt`)? I think YES — that's the spirit of "master ledger handles our AI ops." Confirm? 🤝
4. ~~**🎚️ Difficulty:**~~ ✅ **RESOLVED** — difficulty = **ELO rating** (chess-style, start 1000, K=32), tiers are cosmetic names only. See §6.5.
5. ~~**🏆 End of match ELO:**~~ ✅ **RESOLVED** — ELO **is** the scoring: saved per-player to **their xyzfs** (`<house>/<current_xyzfs>/home/games/tsc_ratings.txt`), bots get ratings too. See §6.5.
6. **⏱️ Turn pacing in CvC:** bots should play instantly (no visible waiting) or with a small delay so you can watch the battle unfold?
7. **🖱️ Widget count:** setup widget (W1) + human widget (H1) + AI widget (A1) = 3 separate programs. OK? Or fold the human widget into the host screen?

---

## 🏁 11. TL;DR (THE 30-SECOND VERSION ⏱️)

- ⚔️ We're adding a **duel mode (True Swords Clash)** on top of our verse-scramble game, living at `@.apps/TSC_ELO/`.
- 🥇 **First widget = Setup** (HvH / HvC / CvC picker) that passes its choice **plus the opponent ELO rating** into the game via the **widget cmd bus**.
- 📋 Human and AI play through the **master ledger** (the `101.ledger-player-npc-simple+3` pattern): every action is an append-only ledger line, `config.txt` holds HP/mana/status/ratings.
- 🎮 The Mana Challenge **reuses our existing TSOTS deal/input engine** — we're not rewriting it.
- ⚖️ **Difficulty = ELO** (chess-style: new players 1000 vs 1000, K=32). Tier names (Padawan → GM) are **cosmetic only**. Ratings persist per-player in **their xyzfs** (`home/games/tsc_ratings.txt`), humans and bots alike.
- 🪟 Widgets are separate programs (WIDGIT Bible pattern) — host + setup + human + AI, all file-mediated.
- 🛠️ Build in phases: skeleton → setup widget → HvH → HvC → CvC → polish.

**Over to you, my liege 👑 — does this match the vision in your head? 🧠💭**

Emoji legend: 🪟 widget · 📋 ledger · 📨 cmd bus · ⚔️ combat · 📜 Mana Challenge · 🧠 AI · 🏆 win
