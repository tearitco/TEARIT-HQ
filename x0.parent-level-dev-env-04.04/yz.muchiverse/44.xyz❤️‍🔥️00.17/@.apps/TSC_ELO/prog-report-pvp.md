# 🗡️ TSC_ELO — PROGRESS REPORT pvp ⚔️
### 🎯 *Two subharnesses, one duel: PvP over the house P2P peer*

> 💌 au2 → pvp: **P1–P5 network proofs, driven through the WIDGIT's REAL input chain.**
> One harness launches 2 subharnesses; the duel runs **player-vs-player over the house
> `palnet_peer`** — not on one screen. The setup WIDGIT on each side is a real peer of its host.

---

## 🏁 TL;DR — WHERE WE STAND

| 🧩 Piece | 📍 Status |
|---|---|
| 🧭 FSM driver (`fsm-driver.sh`) | ✅ written (scenario-agnostic engine) |
| 🕹️ Duel scenario (`pvp_duel.sh`) | ✅ written (BOOT→PRESENCE→CHALLENGE→ACCEPT→MOVE→CONVERGENCE→DONE) |
| 🧹 P1 Presence | ✅ live-verified (2 `tsc_duel` peers in `net/presence/`) |
| ✉️ P2 CHALLENGE wire | ✅ live-verified (B's inbox gets A's `CHALLENGE:…`) |
| ✉️ P2 CHALLENGE ledger | ✅ live-verified (B ledger `via=net`) |
| 🤝 P3 ACCEPT wire | ✅ live-verified (state=playing, A inbox gets `|ACCEPT`) |
| ⚔️ P4 A→B MOVE | ✅ live-verified (turn 0→1, B inbox + B ledger) |
| ⚔️ P4 B→A MOVE | ✅ live-verified (turn 1→2, A inbox + A ledger) |
| 📚 P5 ledger convergence | ✅ live-verified (4 events agree on both ledgers) |
| 🧭 **Full harness run** | ✅ **OVERALL: PASS** (exit 0, ~30–60s) |
| 🐛 ledger.txt-created-as-directory | ✅ **root-caused + fixed** in `tsc_setup.c` AND `tsc_net.c` |
| 🐛 CHALLENGE state skipped | ✅ fixed (`h_presence` returned ACCEPT, skipping P2) |
| 🔌 Proof archive | ✅ per-side artifacts (`ledger_A/B.txt` …) + `proof/latest` |
| ⚡ CPU guards | ✅ 30fps caps, `nice`, wallclock cap, kill-on-quit trap |

---

## ✅ WHAT'S DONE

### 🧭 THE FSM DRIVER (`test-harn-same/fsm-driver.sh`)
- `fsm_register NAME TIMEOUT_S HANDLER` → state table; `fsm_run()` advances until `DONE`.
- `FSM_TICK_S=0.5`, `FSM_MAX_TRANSITIONS=120` (deadlock guard).
- Handlers run in a **subshell** → persisted vars live in `.fsm-state`
  (`fsm_set` / `fsm_load_state`); handler stdout = logs + **LAST line is the next state**;
  stderr = debug (can't become a bogus transition).

### 🕹️ THE DUEL SCENARIO (`test-harn-same/scenarios/pvp_duel.sh`)
```
BOOT → PRESENCE → CHALLENGE → ACCEPT → A_MOVE → B_VERIFY → B_MOVE → A_VERIFY → CONVERGENCE → DONE
```
- Discovers the 2 host sessions by mtime; maps each widget session via its
  `pieces/system/focus.txt`.
- Copy-run proof artifacts → `proof/pvp-<ts>/` **before** cleanup.
- P1–P5 asserted exactly per `TSC_P2P_PVP.md`.

### 🎮 HOW MOVES TRAVEL THE REAL CHAIN
```
tk_focus_item digit-jump → Enter (KEY:n) → chtpm_parser_pal send_command
 → interact_relay.txt → setup_menu_input → <host>/widget_cmds/inbox.txt
 → tsc_setup 8 (drainer) → tsc_net broadcast → outbox → palnet_peer
 → remote inbox → remote tsc_net drainer → remote ledger
```
- Playing mode **falls through** non-quick-keys to the menu dispatch (plain digits can't
  reach `setup_menu_input` through the REAL chain — the parser consumes them for nav-jump).
- `piece.pdl` gained METHOD rows `Play: STRIKE|HEAVY|HEAL|BLOCK` → `MOVE_STRIKE|…`
  wired in `setup_menu_input.c` to enqueue `MOVE:<action>`.

### 🧹 PITFALLS BATTLED THIS SESSION
- **PITFALL 20** — `execl` doesn't shell-split: 6-arg launches.
- **PITFALL 21** — assert on REAL per-session net artifacts, never shared config.
- **PITFALL 54** — wait for non-empty `current_frame.txt` before render.
- 🪤 **Self-matching pkill:** `pkill -f orchestrator` matches the shell's OWN cmdline and
  kills the harness. Use the `"[o]rchestrator"` class trick.
- 🪤 **Shell CWD drift:** the persistent shell forgot its `workdir` after a hung command —
  every boot command now begins with an explicit `cd`.

---

## 🧯 THE BIG ONE — `ledger.txt` WAS A DIRECTORY 🐛
- **Symptom:** B_VERIFY timed out: B's inbox had A's `MOVE:heavy`, `tsc_net` WAS running
  (offset advanced, `applied.txt` dedupe worked) — but the ledger never appeared.
- **Root cause:** both `game_ledger_append` fns called `ensure_dir(path)` on the **file**
  path; `ensure_dir` mkdir'd **every** component including the last → a directory named
  `ledger.txt`. Then `fopen(path,"a")` failed silently (`if (!f) return;`).
- **Fix:** `ensure_dir` now strips the last `/` component and only builds parent dirs
  (both `tsc_setup.c:131` and `tsc_net.c:107`).
- ✅ **Re-verified live:** B's inbox gets A's CHALLENGE (twice — peer re-delivers), but
  `applied.txt` dedupes → **exactly one** ledger line:
  ```
  1785805511-2915516-1|tsc_elo-tsc_duel-2910833|P1_dbg|CHALLENGE:P1_dbg|net
  ```
- 🪤 **Bonus insight:** the peer re-delivers wire lines (dup in inbox), the ledger is the
  deduped truth — assert convergence on **ledger**, never raw inbox.

### 🔌 IDENTITY FIX (earlier this session)
- `tsc_net` CHALLENGE now writes its payload name as `player_1_name` (was `player_2_name`);
  ACCEPT writes the acceptor's identity from the wire `user` field (config is shared, roles
  fixed, **wire carries names**).

### 🧯 SECOND BUG CAUGHT BY THE FULL RUN — CHALLENGE WAS BEING SKIPPED 🐛
- **Symptom:** the run log jumped `PRESENCE -> ACCEPT` with NO P2 proof line; the
  game started from a bare ACCEPT and the ledger never showed a CHALLENGE.
- **Root cause:** `h_presence` returned `ACCEPT` instead of `CHALLENGE`, so the
  FSM never entered the CHALLENGE state and P2 never ran (and the P3/P4 lines
  silently passed on a challenge-less game).
- **Fix:** `h_presence` now returns `CHALLENGE`; P4b/P4d also tightened to assert
  the SPECIFIC move (`MOVE:$A_MOVE` / `MOVE:$B_MOVE`) in the peer's ledger, not
  just *any* `via=net` line (PITFALL 21 discipline).
- ✅ **Re-verified live:** full run now shows every P1–P5 line and **OVERALL: PASS**;
  the archived ledgers agree on **4 events**: `CHALLENGE`, `ACCEPT`,
  `MOVE:block`, `MOVE:heal` — each `local` on its actor's side and `net` on the
  peer's.

### 🔌 PROOF ARCHIVE
- Per-run `proof/pvp-<ts>/` now stores **per-side** artifacts
  (`ledger_A.txt`/`ledger_B.txt`, `inbox_A.txt`/`inbox_B.txt`,
  `frame_A/B.txt`, `wframe_A/B.txt`, `state_A/B.txt`, `presence/`, `config.txt`,
  `session.info`) — a flattened copy let B's ledger overwrite A's and silently
  destroyed the P5 evidence.
- `proof/latest` symlinks the newest archive.

### ⚡ CPU / SAFETY GUARDS (harness never melts a core)
- Booted services are frame-paced in source: renderer + chtpm_rgb_render
  `usleep(33333)` = **30 fps**, parser 60fps, keyboard 50fps, orchestrator 5–10Hz,
  `palnet_peer` event-driven `select()` (0% idle).
- FSM sleeps `FSM_TICK_S=0.5s` between retries (never busy-polls); helpers sleep
  after every injection.
- Bounded run: per-state timeouts (15–45s) + `FSM_MAX_TRANSITIONS=120` +
  overall wallclock cap `FSM_WALL_S=600s` (env-tunable).
- Hosts launch under `nice -n 10`; kill-on-quit trap `EXIT INT TERM` →
  `button.sh kill` → straggler sweep; pkill uses `[c]lass` syntax so it can't
  kill its own shell.

---

## 🧯 KNOWN QUIRKS / GOTCHAS 🪤

- 🪤 `tsc_net` is a one-shot (`tsc_net 8` drains ≤8 lines); the orchestrator wraps it in a
  loop that `fork`s a fresh one every ~200ms. `pgrep` for the transient child is flaky —
  check **offset/applied.txt** instead.
- 🪤 `tsc_answer` stdout = exactly one move word (`strike|heavy|heal|block`); `move_key`
  turns it into `tk_focus_item` on `Play: <MOVE>` + Enter.
- 🪤 A full `bash test-harn-same/button.sh pvp` run takes **~30–60s**; the cleanup trap calls
  `button.sh kill` on exit.

---

## 🕹️ HOW TO RE-TEST (the FULL harness)

```bash
bash test-harn-same/button.sh pvp      # 🔁 both subharnesses boot + duel + proofs → proof/pvp-<ts>/
bash test-harn-same/button.sh check    # 🔍 all system bins + ops executable
./button.sh kill                       # 🧹 cleanup
```

**Watch for:** `PRESENCE ✓` → `CHALLENGE ✓` → `ACCEPT ✓` → `MOVE A→B ✓` → `MOVE B→A ✓`
→ `CONVERGENCE ✓` → `DONE`.

---

## 🚀 NEXT STEPS

1. ✅ ~~Re-run `bash test-harn-same/button.sh pvp` to full P1–P5 PASS~~ — **done, OVERALL: PASS**.
2. ✅ ~~Archive `proof/pvp-<ts>/` artifacts~~ — **done**: per-side ledger/inbox/frame + `session.info`, `proof/latest`.
3. 📝 Fold the ledger-dir bug + CHALLENGE-skip bug + self-matching-pkill gotcha into `TSC_P2P_PVP.md`'s pitfall log.
4. 🔭 Next harness mile: play a full duel to a winner (0 HP → victory state) and a post-CONVERGENCE ELO write-back (K=32) — the duel so far proves the wire + ledgers in both directions, not yet the end-game.
5. 🗺️ Arena/map screen, 🤖 rating-tuned AI curve, 🌟 Miracle — game-side milestones still open (see `summary-walkthru.txt` §3).

---

*📚 Companion docs: `TSC_P2P_PVP.md` (PvP-over-P2P spec) · `TSC_DESIGN.md` (the full spec) ·
`QUICK_STANDARDS.md` (house rules cheat sheet).*
