# 🗡️ TSC_ELO — PROGRESS REPORT au2 🏗️
### 🎯 *True Swords Clash — Master-Ledger Duel + Match Setup WIDGIT*

> 💌 au1 → au2: **P1 host ⚔️ + P2 Match Setup WIDGIT 🪟 now talk end-to-end.**
> Setup chosen in the GL window → host starts the duel → ACK comes back to the window.
> Both windows finally sit **side by side** instead of stacked 🥞.

---

## 🏁 TL;DR — WHERE WE STAND

| 🧩 Piece | 📍 Status |
|---|---|
| 🖥️ Host program (TTY duel screen) | ✅ done + live-verified |
| 🪟 Setup WIDGIT (own GL window, own PAL, own ops) | ✅ done + live-verified |
| 🔌 Widget ↔ host cmd bus (`inbox/status/pending`) | ✅ done + live-verified |
| 🧮 ELO chain (xyzfs ratings, K=32, `E_A` expected-score) | ✅ resolver wired, live-verified |
| 🏆 Duel loop (tick/AI/deal/compose) | ✅ skeleton live (in-game screen reached) |
| 🌟 Miracle, 🗺️ map/arena, 💾 full match persistence | 🔜 next |

---

## ✅ WHAT'S DONE (the whole enchilada 🌮)

### 🖥️ THE HOST (app profile)
- 🏁 **Launch:** `./button.sh run` → session dir → orchestrator 🎼 + parser 🧠 + GL mirrors 🪞.
- 🔤 **TTY screen:** `pal/main_loop_chtpm.pal` → `tsc_compose` → `view.txt` → parser renders
  the **TRUE SWORDS CLASH** title, match mode line, ledger log line, and action bar
  (`1:strike 2:heavy 3:heal 4:block`).
- 🧠 **Persistent module:** `prisc+x` runs `main_loop_chtpm.pal` forever (idle-sync → diff → dispatch).
- 👾 **Referee:** `tsc_tick` runs every tick (16667µs) — status effects, winner check,
  **dispatches `tsc_ai` for computer turns**.
- 🕹️ **Input:** `tsc_input` maps relayed keys `1..4` → Mana-Challenge actions →
  hands off via `pieces/system/player_action.txt` → `tsc_deal` executes (no-op on AI's turn).
- 📜 **Ledger:** `ledger_append` logs every event; `ledger_peers` lists live peers.

### 🪟 THE SETUP WIDGIT (widget profile — a REAL separate program)
- 🏁 Launched by the host with `setsid` → **own** session, **own** `system/`, **own** `ops/`,
  **own** PAL loop, **own** GL window. 🚫 no `keyboard_input`, 🚫 no ASCII renderer.
- 🧩 Ops:
  - `setup_set_focus` 🎯 — writes `focus.txt` (host session root + inbox/status paths).
  - `setup_enqueue_cmd` 📨 — the **only** writer of the host inbox.
  - `setup_menu_input` 🔢 — METHOD-table dispatch (SET_MODE / SET_ELO / SET_NAME / START).
  - `setup_compose_frame` 🖼️ — the **only** writer of `view.txt` (renders state + "Host says:").
- 🔄 PAL loop: `setup_menu_input x9(0)` pre-sync → compose → `read_history` relay →
  dispatch → recompose. 30ms cadence.
- 🎨 GL: `gl_mirror` (wsr-pal build, **with** `interact_relay` forwarding) + `chtpm_rgb_render`.
- 📋 Layout: `pieces/chtpm/layouts/setup_main.chtpm` + `projects/setup/pieces/setup/piece.pdl`
  (4 METHOD rows) → parser renders `[ ]`/`[>]` arrow-nav menu. ⌨️ Digits + Enter execute.

### 🔌 CMD BUS (widget ➜ host)
```
setup_menu_input 🔢 → setup_enqueue_cmd 📨 → <host>/pieces/system/widget_cmds/inbox.txt
host drainer (every ~0.2s): tsc_setup 8
   → reads inbox, persists pending to pending.txt 💾, applies on START
   → writes ACK to <host>/.../widget_cmds/status.txt
widget compose reads status.txt → shows "Host says: MATCH STARTED…" ✅
```

### 🧮 ELO CHAIN
- 📍 Ratings live in the **player's xyzfs** (`<house>/<xyzfs>/home/games/tsc_ratings.txt`),
  resolved via `house_root.txt → current_login.txt → first user → 1000 fallback`.
- ⚖️ Chess-style: **K=32**, new players start **1000 vs 1000**, tiers cosmetic.
- 🎓 `tsc_elo get <name>` / `resolve` / (update) subcommands.

---

## 🎬 EXPECT-FX WALKTHROUGH 🚶
> *"What should I expect each function to do?"* — the honest tour, op by op.

### 🚀 EXPECT on launch (`./button.sh run`)
1. 🪟 **TWO windows open** — host TTY/ASCII screen + Setup WIDGIT GL window **beside it**
   (`GL_MIRROR_X/Y` keeps them from stacking). *(live-verified: +680 vs +198)*
2. 🩺 Host screen shows `Waiting for the SETUP WIDGIT…` + `game_state=waiting_setup`.
3. 🪟 Widget window shows the **MATCH SETUP** box: Mode / Opponent ELO / Your name /
   Host line / last message.

### ⌨️ EXPECT while typing in the WIDGIT window
| Key | Expect | Why |
|---|---|---|
| ⬆️⬇️ arrows | `[ ]` ↔ `[>]` focus marker moves on the method list | parser owns focus nav |
| 🔢 digits | jump-focus to a method row | parser index nav |
| ⏎ Enter on a row | dispatches `KEY:<n>` → relay file → `setup_menu_input` | parser `send_command` → `inject_raw_key` |
| 🔁 Enter on **SET_MODE** | mode cycles `HvH→HvC→CvC`, line updates, `MATCH:<mode>` enqueued | cycle + enqueue |
| 💯 Enter on **SET_ELO** | rating +100 (wrap 400..3000), `RATING:<n>` enqueued | cycle + enqueue |
| 🧍 Enter on **SET_NAME** | name cycles, `PLAYER:<name>` enqueued | cycle + enqueue |
| 🏁 Enter on **START MATCH** | `START` enqueued, widget says "waiting for match to begin…" | enqueue |

### 🤖 EXPECT from the host drainer (`tsc_setup`)
- 💾 **`MATCH:` / `RATING:` / `PLAYER:`** → persisted to `widget_cmds/pending.txt`
  (the 0.2s tick can split the widget's separate enqueues — pending survives the gap).
- 🏁 **`START`** → reads accumulated pending → writes config:
  `mode=`, `game_state=playing`, player names/types/ratings snapshot (HP/mana/status reset),
  ledger `setup` line, ACK to `status.txt` → **resets pending.txt to defaults**.
- 📡 **`PING`** → `PONG host alive`.

### 🗡️ EXPECT once playing (host PAL loop)
- 🧠 `tsc_input x2` relays a key → `tsc_input` writes `player_action.txt`.
- 🎲 `tsc_deal` consumes it (strike 3⚔️ / heavy 6🔥 / heal +5💚 / block 🛡️; +1 mana/turn, cap 10).
- 🤖 `tsc_tick` → if computer's turn → `tsc_ai` (heal if low 🩹 / heavy if lucky 🎲 / strike else) → applies same math → checks winner.
- 🖼️ `tsc_compose` re-renders after every change; screen shows the battle.

### 📈 EXPECT from the ELO engine (the literal **E_X** fx)
```
expected_score(A vs B):   E_A = 1 / (1 + 10^((R_B − R_A)/400))   📐
rating update:            R'_A = R_A + K·(S_A − E_A),  K = 32    ⚖️
new players:              1000 vs 1000                           🌱
```
- 🤖 AI strength is *tuned to its rating* — a 1800+ bot is a near-perfect solver; 1000 is a friendly sparring partner. *(full AI curve = next phase)*

---

## 🧯 BUGS CAUGHT BY THE LIVE TEST (au2) — all fixed ✅

1. 🐛 **Drainer statelessness race** — `MATCH:`/`RATING:` landing in an earlier 0.2s tick than
   `START` were silently dropped → host started **HvH** when the widget had set **CvC**.
   💊 **Fix:** `pending.txt` persistence across invocations, reset on START. *(live re-verified:
   CvC + 1100 now survives to the START tick.)*
2. 🪟 **Overlapping GL windows** — two glut windows, same default spot, widget hidden behind host.
   💊 **Fix:** `GL_MIRROR_X/Y` env support in wsr-pal `gl_mirror.c`; widget compiles its own copy
   from that source (keeps the real `interact_relay` relay!) and opens at `680,90`.
3. 🧹 **Key-0 pre-sync seeding empty values** — `read_kv_str_local` clobbers its buffer to `""`
   when the file is missing, so the seed wrote `mode=`. 💊 **Fix:** hardcoded defaults in the
   `key==0` branch (button.sh already seeds, this is belt-and-suspenders).

---

## 🧯 KNOWN QUIRKS / GOTCHAS 🪤

- 📝 `interact_relay.txt` holds the **ASCII code as a decimal** (`'2'` → `50`), and menu ops
  are invoked with that integer. Don't test menu ops by hand with `2` — you'll wonder why
  nothing fires. 🫠
- 🖼️ `gl_mirror` MUST be the **014.wsr-pal** build (writes `interact_relay.txt`). The
  mutaclysm/045 copy does NOT relay GL keys → widget feels frozen. ❄️
- 🔤 Glyphs are a **local copy per project** (95 files). Missing = invisible text, no error.
- 🧭 PITFALL 54: wait for `current_frame.txt` non-empty BEFORE `chtpm_rgb_render`, else the
  GL window is black forever. ⬛
- 💾 Persistent `config.txt` holds the last match's state across runs (by design) — right now
  it's left at `playing` from the au2 live test. `rm pieces/system/config.txt` to re-seed
  `waiting_setup` for a fresh demo. 🧹
- 🧭 Host's in-game screen currently shows `[Map Loading…]` — the arena/map is the next task,
  not a bug. 🗺️

---

## 🕹️ HOW TO RE-TEST THE FULL LOOP

```bash
./button.sh run            # ⚔️ both windows open side-by-side
# in the WIDGIT window:
#   ⏎ SET_MODE  ×2        # → HvC  (or whatever)
#   ⏎ SET_ELO   ×1        # → 1100
#   ⏎ SET_NAME  ×1        # → Player2
#   ⏎ START MATCH         # 🏁
./button.sh kill           # 🧹 cleanup
```

**Watch for:**
- 🪟 Two windows, not stacked.
- 🏁 Host flips `waiting_setup → playing` with **the mode you actually picked**.
- 📨 Widget shows `Host says: MATCH STARTED: HvC (you 1000 vs 1100)…`.

---

## 🚀 NEXT STEPS

1. 🗺️ **Map / arena screen** (the `[Map Loading…]` placeholder → real duel battlefield).
2. 🧠 **AI phase 2** — rating-tuned solve-accuracy, mana preference, Miracle choices per §6.5.
3. 🌟 **`tsc_miracle` real implementation** — winner's Miracle pick, re-enter duel with effect.
4. 💾 **Full match persistence** — win/loss ledger → ELO update (K=32) written back to xyzfs.
5. 🎨 Widget polish — status read-back refresh cadence, window sizing.

---

*📚 Companion docs: `TSC_DESIGN.md` (the full spec) · `QUICK_STANDARDS.md` (house rules cheat sheet).*
