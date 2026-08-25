# 🏗️ BUILD PLAN: MY-CHARA-TXT + MYNE-QRYPTO TANDEM (2026-08-01)

## 🛑 2026-08-01 MID-BUILD ARCHITECTURE CORRECTION — READ FIRST

**What happened:** P1 skeleton was built (directories, `default_op.txt`, `button.sh`, `scripts/build.sh`, and stub `.c` files for both `my-chara-txt` and `myne-qrypto`) using a WRONG interface model — a flat "one `compose` op writes an ASCII HUD box directly to `view.txt`, single-letter keys (F/M/S/I/E) trigger one-op-per-action" design. The user flagged this was not the real house interface pattern before P2 (real `compose_frame`/`menu_input`) was written.

**Real, proven pattern (confirmed via direct file read, both running precedent the user has seen):**
- `014.wsr-pal💸️📌️+2/pieces/chtpm/layouts/wsr_main_menu.chtpm` + `wsr_trade_menu.chtpm`
- `01.muchi-pals-🥚️-13.01/pieces/chtpm/layouts/main.chtpm` + `store.chtpm` + `pal/store_module.pal` + `ops/muchi_menu_input.c`

Key elements: named `.chtpm` screens navigated via real `href` buttons (never op-simulated screen switches — xyzos-standards §18); `${piece_methods}` auto-generates numbered buttons from a `piece.pdl` METHOD table that gets **regenerated per call** for dynamic lists (plots, inventory) rather than hand-authoring a separate screen per list state; ONE PAL module per screen (`<screen>_module.pal`), copying the proven idle-sync + `screen_changed`-diffed loop shape (only recompose when something actually changed — Pitfall 48); ONE shared `<project>_menu_input` op (METHOD-table dispatcher, reads current screen from `current_layout.txt` fresh every call — never separately tracked) + ONE shared `<project>_compose_frame` op (writes `view.txt` → substituted into `${game_map}`) per project, not one op per verb.

**Action taken:** All 5 design docs (`my-chara-txt`, `my-chara-zr`, `genesis-txt`, `genesis-zr`, `myne-qrypto`) updated with this correction. `my-chara-txt`'s design doc §4/§6/§7/§8 were fully rewritten to the real pattern (done). The other 4 docs got a correction note pointing back to `my-chara-txt`'s §4 as canonical — their own detailed sections still need a full rewrite pass when each is actually built.

**⚠️ STALE, NEEDS REWORK:** The P1 stub files already written on disk (`my-chara-txt/ops/compose.c`, `tick.c`, `farm_plant.c`, `farm_harvest.c`, `mine.c`, `store_buy.c`, `store_sell.c`, `inventory_use.c`; `myne-qrypto/ops/qrypto_*.c`; both `default_op.txt` registries; both `button.sh`) all reflect the WRONG model and must be redone to match: `.chtpm` layouts under `pieces/chtpm/layouts/`, `piece.pdl` files, per-screen PAL modules under `pal/`, and the `mychara_menu_input`/`mychara_compose_frame` op pair (per `MY_CHARA_TXT_DESIGN.md` §6/§7) instead of the current one-op-per-verb stub set. **Do not build further on top of the current stubs — rewrite them first.**

**Before writing another line of code**, also read (only partially read so far, large files):
- `#.haiku+/!.xyzos-pitfalls+1.txt` (2581 lines total, only read lines 1-1006 — RESUME AT LINE 1007)
- `#.haiku+/!.xyzos-standards+1.txt` (262KB, too large for one read — grep for the relevant section headers first, especially §6/§12/§13/§16/§18/§21/§35 referenced directly in `muchi_menu_input.c`'s own header comment)

---

## 📋 PHASED TANDEM BUILD STRATEGY (interface details below are STALE — see correction above; phase table/exit-criteria shape still holds)

Build both **my-chara-txt** (game) and **myne-qrypto** (blockchain service) in parallel, testing integration early.

---

## 🎯 PHASES AT A GLANCE

| Phase | my-chara-txt | myne-qrypto | Integration | Exit Criteria |
|-------|---|---|---|---|
| **P1** 🥇 | Skeleton: build.sh, ops registry, data dirs, PAL loop | Skeleton: build.sh, QTC/QTH data dirs, node startup | (none) | Both projects compile, dirs exist |
| **P2** 🌾 | Compose (text render), basic farm/mine ops framework | QTC node working (reuse 041.pal-chain), blockchain.txt growing | (none) | My-chara renders a frame, QTC mines blocks |
| **P3** ⛏️ | Farm action: plant/harvest implemented | QTC ops callable (qrypto_mine_qtc), wallet daemon responds | **FIRST INTEGRATION**: my-chara calls qrypto_mine_qtc | My-chara mines, wallet updates, ledger records |
| **P4** 🤝 | Exchange ops (buy/sell grain), exchange.txt matching | QTC wallets + tx tracking working, prices trackable | (none) | Two players trade, prices emerge |
| **P5** 🪙️ | Mining UI widget (GPU select, odds preview) | QTH node skeleton (PoS validator selection) | (none) | QTH blocks generated, validator list grows |
| **P6** 🔒 | Staking UI widget, stake action | QTH staking ops (qrypto_stake_qth) | **SECOND INTEGRATION**: my-chara calls qrypto_stake_qth | My-chara stakes, validators.txt updated, rewards earned |
| **P7** 📊 | Full audit: replay game + blockchain ledgers | Audit tools: replay QTC/QTH, cross-check state | **PARALLEL AUDIT**: both ledgers verify together | Game + blockchain state match replayed state |

---

## 🔍 TESTABLE CHECKPOINTS

### **P1 Complete → Can verify:**
```bash
# Both projects have directory structure
ls -la @.apps/my-chara-txt/ | grep -E "button.sh|build.sh|default_op.txt|data|pal"
ls -la @.apps/myne-qrypto/ | grep -E "button.sh|build.sh|default_op.txt|data|nodes"

# Both compile without warnings
cd @.apps/my-chara-txt && ./scripts/build.sh
cd @.apps/myne-qrypto && ./scripts/build.sh
```

### **P2 Complete → Can verify:**
```bash
# My-chara renders a frame
cd @.apps/my-chara-txt && button.sh run
# Should show: view.txt displays "Day 1 | Health 100 | Money 500 | Grain 10"

# QTC node mines blocks
cd @.apps/myne-qrypto && button.sh start_qtc
# Should show: blockchain.txt growing, new blocks appended every ~30s
```

### **P3 Complete → Can verify (FIRST TESTABLE INTERFACE):**
```bash
# Start both
cd @.apps/my-chara-txt && button.sh run &
cd @.apps/myne-qrypto && button.sh start_qtc &

# User mines
echo "mine_qtc alice 2" > @.apps/myne-qrypto/pieces/tmp/mine_request.txt

# Check result
cat @.apps/my-chara-txt/data/master_ledger.txt | tail -1
# Should show: "2026-08-01|1|mine|qtc:50" or "qtc:0"

cat @.apps/myne-qrypto/data/qtc/wallets/alice.txt
# Should show: balance increased (or unchanged if unlucky)
```

---

## 📝 IMPLEMENTATION STRATEGY

### **Day 1 (Aug 1) — P1 + P2 Skeleton**
1. Create both projects' directory trees + ops registries.
2. Write `build.sh` for both (compile stubs, symlink system/).
3. Write `button.sh` launchers (start/stop nodes, cleanup).
4. Set up PAL loops (tick + compose for my-chara; tick for myne-qrypto).
5. Create initial data structures (config.txt, blockchain.txt, wallets/).

**Exit:** Both projects build cleanly, directories exist.

### **Day 2 (Aug 2) — P2 Compose + QTC Node**
1. Implement `compose.c` for my-chara (render config.txt → view.txt).
2. Implement basic `tick.c` (advance day, apply decay).
3. Adapt 041.pal-chain code into myne-qrypto/nodes/qrypto_node_qtc.c.
4. Test QTC node mining blocks (reuse proven 041.pal-chain logic).

**Exit:** My-chara renders a frame; QTC mines blocks independently.

### **Day 3 (Aug 3) — P3 Integration (CHECKPOINT)**
1. Implement `farm_mine.c` op in my-chara (calls qrypto_mine_qtc).
2. Implement `qrypto_mine_qtc.c` op in myne-qrypto (file-based IPC to daemon).
3. Test end-to-end: my-chara player mines → daemon processes → wallet updates → ledger records.

**Exit:** First real integration works. **User tests interface here.**

### **Days 4-7 (Aug 4-7) — P4-P6 Expansion**
1. Exchange matching (P4).
2. QTH node + PoS (P5).
3. Staking integration (P6).

### **Day 8+ (Aug 8+) — P7 Audit**
1. Replay tools.
2. Cross-validation.
3. Full audit suite.

---

## 🔗 INTEGRATION POINTS (Critical)

### **P3: Mining Integration**

**File-based IPC:**
```
my-chara-txt/pieces/tmp/mine_request.txt
├─ address=alice
├─ gpu_count=2
└─ coin_type=qtc

myne-qrypto/pieces/tmp/mine_response.txt
├─ status=ok
├─ block_found=true
├─ reward=50
├─ block_num=42
└─ nonce=12345
```

**Ops involved:**
- `farm_mine.c` (my-chara) → reads inventory, writes mine_request.txt, polls mine_response.txt.
- `qrypto_mine_qtc.c` (myne-qrypto) → calls daemon, gets result, writes mine_response.txt.
- Daemon (qrypto_node_qtc) → performs PoW, updates blockchain.txt + wallets/, signals completion.

### **P6: Staking Integration**

**Same file-based pattern:**
```
my-chara-txt/pieces/tmp/stake_request.txt
├─ address=alice
├─ amount=100
└─ coin_type=qth

myne-qrypto/pieces/tmp/stake_response.txt
├─ status=ok
├─ staked_ok=true
└─ validator_slot=1
```

---

## 📊 DATA FLOW (Audit Trail)

```
User action in my-chara:
  ↓
farm_mine.c writes mine_request.txt
  ↓
qrypto_mine_qtc.c polls, calls daemon
  ↓
qrypto_node_qtc (daemon) mines block
  ↓
Updates blockchain.txt (⛓️ ledger)
Updates wallets/alice.txt (💰 wallet)
  ↓
Writes mine_response.txt
  ↓
farm_mine.c reads response
  ↓
Updates my-chara inventory
  ↓
Appends to master_ledger.txt (📜 game ledger)
  ↓
compose.c renders updated state to view.txt
```

**Two audit trails, one data flow:**
- 🔗 `myne-qrypto/data/qtc/blockchain.txt` — immutable chain
- 💰 `myne-qrypto/data/qtc/wallets/alice.txt` — derived state (can replay from blockchain)
- 📜 `my-chara-txt/data/master_ledger.txt` — game events
- 🎮 `my-chara-txt/pieces/system/config.txt` — game state (can replay from game ledger)

**Audit:** Read both ledgers top-to-bottom, re-compute both states, verify they match.

---

## 🚀 NEXT STEPS

1. ✅ Designs written (all 5 docs done).
2. ⏳ **NOW:** Start P1 skeleton + P2 basics.
3. 🧪 **Checkpoint P3:** First integration (mining) works. User tests.
4. 📈 Continue P4-P7 based on feedback.

---

## 📌 SUCCESS CRITERIA

- ✅ Both projects compile without warnings.
- ✅ My-chara renders a text HUD.
- ✅ QTC node mines blocks (reuses 041.pal-chain).
- ✅ My-chara can call qrypto_mine_qtc, get result, update ledger. (P3 checkpoint)
- ✅ Two independent audit trails stay in sync (P7).

---

## 🎯 GOAL

By P3 checkpoint (Aug 3), user can:
1. Start a my-chara game.
2. See character + inventory.
3. Click "Mine" button.
4. See random result (block found or not).
5. See wallet update + game ledger entry.
6. Verify both ledgers are consistent.

**This proves the architecture works. Then expand.**

---

**Last updated:** 2026-08-01  
**Author:** Claude Haiku (Auto-Start Build)  
**Status:** Starting P1 skeleton now.
