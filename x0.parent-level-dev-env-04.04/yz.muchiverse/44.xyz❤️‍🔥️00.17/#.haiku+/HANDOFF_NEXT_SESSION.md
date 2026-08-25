# Handoff: Session 2026-08-01/02 → Next Session

**From:** Sonnet 5 (this session started as Haiku 4.5, user switched mid-session with `/model`)
**To:** Next Haiku session (assume ZERO context — read this whole document before touching any file)
**Date:** 2026-08-02 (session spanned 08-01 into 08-02)
**Status:** `my-chara-txt` P2 checkpoint COMPLETE, LIVE-VERIFIED, and has a real regression harness (10/10 PASS) — plus the user has independently begun adding a `farm.chtpm` screen (visible in `pieces/chtpm/layouts/main.chtpm`/`mychara_menu_input.c`/`mychara_compose_frame.c`, not yet documented in this handoff — check its current state before assuming §7's "not built" framing still holds). `myne-qrypto/qtc/` (the QTC backend) is COMPLETE and thoroughly tested (21/21 PASS). **`myne-qrypto` itself — the player-facing GAME — does not exist yet**, corrected in §11. **TWO NEW SIBLING PROJECTS this session, `my-biotech` (built through P3, live-verified, real Gemma research loop) and `my-lawyer` (design doc only, not built)** — see §12/§13 (new). **A significant, house-wide finding about small local LLMs and judge/verdict tasks was made and documented at the standards level — read `PITFALL 69` in `!.xyzos-pitfalls+1.txt`, `§42` in `!.xyzos-standards+1.txt`, and `#.haiku+/!.gemma-judge-tomo&iqa.md` before building ANY judge/verdict/classification mechanic against a small local model, in any project.**

---

## 0. READ THIS SECTION FIRST — WHAT KIND OF PROJECT THIS IS

This house (`44.xyz…/`) builds small, self-contained "apps" that all share ONE proven UI architecture called **CHTPM** (a nav-menu system, NOT a flat text dump, NOT a web UI, NOT anything you'd guess from the name alone). Before you write a single line of code in ANY of these projects, you must understand CHTPM, because **an earlier pass in this exact session got it wrong once already** (see §2 below) and had to redo real work as a result. Do not repeat that mistake.

**Before doing ANYTHING else, read these two files in full:**
1. `#.haiku+/!.xyzos-pitfalls+1.txt` — 2581 lines, real bugs already found and fixed across this house, with root causes. **This session only read lines 1–1006.** If you are the next session, either continue reading from line 1007, or at minimum grep it for keywords relevant to whatever you're about to touch before you touch it.
2. `#.haiku+/!.xyzos-standards+1.txt` — 262KB, too big to read in one shot. Use `grep -n "^===\|^SECTION\|^§"` on it to find section headers, then read the specific sections relevant to your task. Section §35 (GL vs ASCII), §18 (screen switching), §23 (session isolation), §41 (per-screen modules) are all directly relevant to everything below.

If you skip these and "helpfully" invent your own interface pattern (flat ASCII HUD, single-letter hotkeys, custom render loop, whatever seems reasonable), **you will be wrong**, and the user will have to stop you and make you redo it, exactly like happened once already in this session.

---

## 1. WHAT THIS SESSION DID, IN ORDER (context you need to make sense of the file tree)

1. **Moved `TSC_XYZ` → renamed `TSC_ELO` → moved into `@.apps/TSC_ELO/`.** This is a duel-game design doc (`TSC_DESIGN.md`), unrelated to the rest of this handoff except as a directory-move precedent. Not touched further this session.

2. **Wrote 4 new design docs** for a simpler farming/economy game family, all under `@.apps/`:
   - `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` — single-player, text-based, the "sanity test" / data-flow audit app. **Build this first.**
   - `@.apps/genesis-txt/GENESIS_TXT_DESIGN.md` — multiplayer (2-4 players, human+AI) version of the same game, with a real order-book exchange.
   - `@.apps/my-chara-zr/MY_CHARA_ZR_DESIGN.md` — future desktop/visual (GL window, drag-drop) version of my-chara-txt. **NOT built, not started.**
   - `@.apps/genesis-zr/GENESIS_ZR_DESIGN.md` — future desktop/visual version of genesis-txt. **NOT built, not started.**

3. **Added a 5th design doc + real implementation** for a blockchain layer these games can mine/trade against:
   - `@.apps/myne-qrypto/MYNE_QRYPTO_DESIGN.md` — describes two coins: **QTC** (proof-of-work, scarcity-based, mirrors real `041.pal-chain⛓️`) and **QTH** (proof-of-stake + a PAL-script smart-contract VM, a genuinely NEW design with no existing precedent in this house — **not built at all yet**, design doc only).
   - `@.apps/myne-qrypto/qtc/` — a REAL, WORKING, independently-running copy of `041.pal-chain⛓️`'s entire codebase (system/ops/pal/pieces/chtpm), copied wholesale, **completely unmodified** (see §4 for why, this was an explicit user decision).

4. **Mid-session architecture correction (IMPORTANT, read §2 below in full).** An early build pass wrote the WRONG interface for `my-chara-txt` (flat ASCII HUD + single-letter keys). The user caught this, we read real proven precedent (`014.wsr-pal💸️📌️+2`, `01.muchi-pals-🥚️-13.01`, and `041.pal-chain⛓️` itself), corrected all 5 design docs, then **rewrote the P1 skeleton from scratch** to match the real pattern.

5. **Built and LIVE-VERIFIED a working P2 checkpoint for `my-chara-txt`** — real CHTPM screen, real key injection, real state mutation, real ledger append, all through the actual `button.sh run` entry point (not a shortcut/op-level-only test — see §6 for the full verified trace).

---

## 2. THE CHTPM ARCHITECTURE — WHAT IT ACTUALLY IS (read this before writing any UI code, anywhere in this house)

**The wrong model (do NOT do this):** one `compose` op writes a hand-drawn ASCII box straight into the rendered output, single-letter keys (F/M/S/I/E) each map to one dedicated op.

**The real model (confirmed from THREE independently-built, working projects — `014.wsr-pal💸️📌️+2`, `01.muchi-pals-🥚️-13.01`, `041.pal-chain⛓️`):**

- The UI is a set of **named screens**, each a `.chtpm` XML-ish layout file under `pieces/chtpm/layouts/`. Example, read this file directly to see it for real: `041.pal-chain⛓️/pieces/chtpm/layouts/wallet_main.chtpm`.
- Screens **navigate to each other via real `<button href="...">` tags**. Screen switching is handled ENTIRELY by the shared `chtpm_parser_pal.c` binary. **An op must never simulate a screen switch itself** — that's a named house rule (`xyzos-standards §18`).
- Each screen has its **own dedicated PAL module** (a tiny script in `pal/<screen>_module.pal`), declared in that screen's `<module>` tag. This is `xyzos-standards §41`'s "per-screen module" convention.
- Each screen's numbered action buttons are **auto-generated** by the `${piece_methods}` placeholder, sourced from a `piece.pdl` file (a plain key-value table with `METHOD | <label> | <COMMAND string>` rows) living at `projects/<project_id>/pieces/<screen_id>/piece.pdl`.
- **Dynamic/variable-length menus (a list of farm plots, a list of inventory items) are NOT separate hand-built screens.** They are the SAME screen's `piece.pdl` file, **regenerated on every render call** by the compose op, so the number of buttons changes but the screen identity doesn't. (Real precedent: muchi-pals' `pets.chtpm` — one file serves both "no pet selected" and "pet selected" states this way.)
- **One shared dispatcher op per project** (e.g. `chain_menu_input.c`, `muchi_menu_input.c`, and now `mychara_menu_input.c`) reads which screen is current fresh every call from `pieces/display/current_layout.txt` (**never separately tracked mutable state** — this is deliberate, re-derive it every time), reads that screen's `piece.pdl`, maps the pressed key to a numbered row, and dispatches on that row's `COMMAND` string (a big if/else chain of `strcmp`).
- **One shared render op per project** (`chain_compose_frame.c`, `mychara_compose_frame.c`) writes ONLY `pieces/apps/player_app/view.txt` — **never** `pieces/display/current_frame.txt` directly (that file has exactly one legitimate writer, `chtpm_parser_pal.c` itself — writing to it from two places causes a real, previously-diagnosed flicker/corruption bug, "ONE WRITER RULE", pitfalls #14/#15/#18/#19). `${game_map}` in the `.chtpm` layout is what substitutes `view.txt`'s content into the final rendered frame.
- The PAL module's own loop (`pal/<screen>_module.pal`) has a specific, proven shape you should COPY EXACTLY, not reinvent:
  ```
  li x1, 0
  li x9, 0
  <project>_menu_input x9
  <project>_compose_frame
  hit_frame
  read_pos x7, "pieces/display/<project>_screen_changed.txt"

  loop:
  li x9, 0
  <project>_menu_input x9
  read_pos x8, "pieces/display/<project>_screen_changed.txt"
  beq x7, x8, check_key
  addi x7, x8, 0
  j render

  check_key:
  read_history pieces/apps/player_app/interact_relay.txt x2, x1
  beq x2, x0, no_key
  <project>_menu_input x2
  j render

  no_key:
  sleep 30000
  j loop

  render:
  <project>_compose_frame
  hit_frame
  sleep 30000
  j loop
  ```
  This ONLY recomposes when something actually changed (a screen switch, OR a real relayed keypress) — **never unconditionally every tick**. An earlier version of a sibling project did that and flooded `frame_history.txt` at ~33 renders/sec with nothing happening (`Pitfall 48`). Copy this shape, don't "simplify" it.
- **No quit path inside any PAL module.** Quitting the whole session is `chtpm_parser_pal.c`'s own job (top-level Ctrl+C). If a module's own script tries to intercept a quit key, it kills that module while the parser keeps relaying into a file nothing reads anymore — a real, previously-hit "frozen, unresponsive" bug (`Pitfall 10`).

**Live, real examples to read directly if any of the above is unclear (all three are proven, running code, not drafts):**
- `014.wsr-pal💸️📌️+2/pieces/chtpm/layouts/wsr_main_menu.chtpm` + `wsr_trade_menu.chtpm`
- `01.muchi-pals-🥚️-13.01/pieces/chtpm/layouts/main.chtpm` + `store.chtpm` + `pal/store_module.pal` + `ops/muchi_menu_input.c`
- `041.pal-chain⛓️/pieces/chtpm/layouts/wallet_main.chtpm` + `ops/chain_menu_input.c` + `ops/chain_compose_frame.c` (the DEEPEST-read precedent this session — read `chain_menu_input.c` and `chain_compose_frame.c` in full before writing any new menu_input/compose_frame op; `my-chara-txt`'s own ops are near-line-for-line structural copies of these two files with the domain logic swapped in)

---

## 3. WHY THE FLAT-ASCII VERSION GOT BUILT FIRST (so you don't repeat it)

Early in this session, before this corrected understanding existed, a P1 skeleton was written for `my-chara-txt` and `myne-qrypto` using ONE `compose` op per project that hand-drew an ASCII box directly, with single-letter keys (F/M/S/I/E) each triggering a SEPARATE dedicated op (`farm_plant.c`, `mine.c`, `store_buy.c`, etc — one op per verb). **This was wrong and has already been fully deleted and replaced.** If you see any reference to this shape anywhere (old memory, a stale note, an old design-doc paragraph not yet corrected), it is WRONG — trust §2 above instead.

The user caught this by asking "do you understand [wsr-pal's real interface]?" and "you're not emulating the nav-based chtpm interface structure" before much damage was done, and had the smarter model (Sonnet 5, via `/model`) read the real precedent directly before allowing any further code. **The lesson for you: if you are about to design ANY interface in this house and haven't personally read at least one real, running `.chtpm` + `piece.pdl` + `*_menu_input.c` triple first, stop and go read one. Do not guess.**

---

## 4. THE MYNE-QRYPTO / QTC DECISION (explicit user ruling, do not re-litigate)

The design doc originally imagined `myne-qrypto` as a brand-new daemon we'd write from scratch, talking to `my-chara-txt` via request/response files (`mine_request.txt` → `mine_response.txt`). **This is NOT what got built, and should not be built that way.**

Instead: `041.pal-chain⛓️` already exists, is fully real (genuine SHA-256 proof-of-work, halving reward schedule, hard total-supply cap, real `openssl` crypto — not a simulation), and already has its own complete CHTPM UI (login/signup/wallet/send/receive/mining-status screens). The user's own direct ruling on this (read literally, don't paraphrase):

> "we dont need to rename at all. its better if we can reuse ops directly, since our goal is to oneday have shared ops folder for the whole house" — followed by "obviously u do the surface rename in game tho"

**What this means concretely:**
- `@.apps/myne-qrypto/qtc/` is a **byte-for-byte, unmodified copy** of `041.pal-chain⛓️`'s `system/`, `ops/`, `pal/`, `pieces/chtpm/`, `projects/pal-chain/`, `default_op.txt`, `scripts/build.sh`, `button.sh` (only `data/` and `wallets/` are freshly emptied — this is a NEW, independent chain instance, not sharing ledger state with the original pal-chain project).
- Internally, the code still says `"cones"` / `"millicones"` (the currency unit) and `project_id=pal-chain` (an internal path-matching identifier, invisible to the player, used to find `projects/pal-chain/pieces/<screen>/piece.pdl`). **Do not rename these inside the reused code.** Renaming `project_id` without renaming the whole `projects/pal-chain/` directory tree WILL break path resolution — this was tried and reverted once already this session.
- **"QTC" branding belongs ONLY in `my-chara-txt`'s own presentation layer** — i.e., when `my-chara-txt` displays a message like "You mined 50 QTC," it is calling the SAME unmodified `chain_miner.+x` underneath, against a ledger that still internally calls the unit "cones." The rename is cosmetic and happens at the boundary where `my-chara-txt` talks about the result, not inside the reused engine.
- The house-wide direction this is pointing toward: a future single shared `shared-ops/` folder that every project (including `my-chara-txt`, `myne-qrypto`, `genesis-txt`, etc.) pulls the SAME binaries from, rather than each project keeping its own local copy. That doesn't exist yet — for now, local copies per project (matching `041.pal-chain⛓️`'s own `scripts/build.sh` header comment convention) are correct and expected.
- **QTH (the proof-of-stake + smart-contract coin) has NO existing precedent anywhere in this house.** It is real, new design work, described in `MYNE_QRYPTO_DESIGN.md` §4/§10, and has not been started at all. When you build it, it should still use the SAME CHTPM pattern (§2 above) for its own UI — new domain logic, same proven interface shape.

**Verified this session:** `@.apps/myne-qrypto/qtc/` builds clean (`bash scripts/build.sh` → `build ok`) and `bash button.sh check` shows all 14 binaries present (`system/prisc+x`, `system/chtpm_parser_pal`, `system/orchestrator`, `ops/+x/chain_miner.+x`, etc). It was NOT run end-to-end this session (no real login/mine/send test performed) — only build-verified. **Running a real login→wallet→mine test through `myne-qrypto/qtc/button.sh run` is real, valuable, un-done work** for whoever picks this up next.

---

## 5. CURRENT FILE-TREE STATE (as of end of this session — verify with `ls` before trusting, things may have moved)

```
@.apps/
├── TSC_ELO/
│   └── TSC_DESIGN.md                      (duel game, unrelated, untouched this session)
├── my-chara-txt/                          ← BUILT, P2-VERIFIED, see §6
│   ├── MY_CHARA_TXT_DESIGN.md             (corrected design doc — §4 has the real interface spec)
│   ├── location.txt
│   ├── button.sh                          (session-isolated launcher, modeled on pal-chain's own)
│   ├── default_op.txt                     (registers mychara_menu_input + mychara_compose_frame)
│   ├── scripts/build.sh
│   ├── system/                            (prisc+x, keyboard_input [PATCHED, see §6.1],
│   │                                        renderer, chtpm_parser_pal, chtpm_rgb_render,
│   │                                        orchestrator — all .c sources + compiled binaries,
│   │                                        copied from 041.pal-chain⛓️/system/)
│   ├── ops/
│   │   ├── mychara_menu_input.c           (THE dispatcher — modeled on chain_menu_input.c)
│   │   ├── mychara_compose_frame.c        (THE renderer — modeled on chain_compose_frame.c)
│   │   └── +x/                            (compiled binaries)
│   ├── pal/
│   │   └── main_module.pal                (main.chtpm's dedicated module)
│   ├── pieces/
│   │   ├── chtpm/layouts/
│   │   │   └── main.chtpm                 (the ONLY screen built so far)
│   │   └── system/config.txt              (persistent game state: day/health/money/grain/...)
│   ├── projects/my-chara-txt/
│   │   ├── pieces/main/piece.pdl          (ONE method row: "End Turn" → END_TURN)
│   │   └── manager/                       (gui_state.txt lives here at runtime)
│   └── data/
│       └── master_ledger.txt              (real, append-only, has 2 real day_end lines in it
│                                            right now from this session's live test — see §6)
├── my-chara-zr/
│   └── MY_CHARA_ZR_DESIGN.md              (corrected design doc, NOT built)
├── genesis-txt/
│   └── GENESIS_TXT_DESIGN.md              (corrected design doc, NOT built)
├── genesis-zr/
│   └── GENESIS_ZR_DESIGN.md               (corrected design doc, NOT built)
└── myne-qrypto/
    ├── MYNE_QRYPTO_DESIGN.md              (corrected design doc)
    ├── location.txt
    └── qtc/                               ← BUILT, BUILD-VERIFIED (not runtime-tested), see §4
        └── (byte-for-byte copy of 041.pal-chain⛓️'s system/ops/pal/pieces/projects/
             default_op.txt/scripts/button.sh, with system/keyboard_input.c PATCHED,
             see §6.1, fresh empty data/ + wallets/)
```

**Also relevant, NOT under `@.apps/`:**
- `#.resume/aug-1-haiku.md` — an earlier, now-PARTIALLY-STALE build-plan doc from this same session. It documents the WRONG P1 model (§3 above) and a phased plan that assumed request/response file IPC for myne-qrypto (§4 says that's not how it went). Read it for the phase-numbering shape (P1→P7) if useful, but do not trust its interface-level details — this handoff supersedes it. Consider merging/retiring that file in a future session.

---

## 6. THE LIVE-VERIFIED P2 CHECKPOINT (exact commands + exact results, so you can reproduce or extend it)

This is the single most important thing to understand before writing more code: **the full real stack was launched through the actual user entry point and a real key was injected, not just an op tested in isolation.** Per `Pitfall 21` (a real, house-wide standing rule): testing an op directly, or testing in an isolated synthetic environment, PROVES NOTHING about whether the real `button.sh run` launch path actually works — a project can have perfect op-level logic and still be completely broken for a real user if the launcher wiring is wrong (this exact class of bug bit `044.pal-chat-irc` once, documented in `Pitfall 20`). So: always test through `button.sh run` at least once, for real, before calling anything "working."

### 6.1 A pre-existing bug had to be patched first (Pitfall 22 / Pitfall 51)

`system/keyboard_input.c` (copied from `041.pal-chain⛓️`, itself copied from a shared lineage across ~11 projects in this house) has a `read_key()` function that busy-spins at ~100% CPU when stdin isn't a real terminal (i.e., any headless/backgrounded/API-sandbox test run — exactly what an agent testing session looks like). `Pitfall 22` documented and "fixed" this once, but `Pitfall 51` found the fix was incomplete: the `usleep(20000)` throttle was placed AFTER the hard-error return path instead of before it, so the busy-spin still happens on that specific path. **This session applied the Pitfall 51 fix to BOTH copies** (`my-chara-txt/system/keyboard_input.c` and `myne-qrypto/qtc/system/keyboard_input.c`) — moved the `usleep(20000)` to run before the `if (nread == -1 ...) return -1;` check, so every non-1 read result gets throttled regardless of which branch caused it. **If you copy `keyboard_input.c` from anywhere else into a new project, check for this exact bug pattern before running headless** (`grep -n -A3 "while ((nread = read" system/keyboard_input.c` — the `usleep` line must come BEFORE the `if (nread == -1...) return -1;` line, not after).

### 6.2 The exact test performed (reproduce this to verify current state, or extend it)

```bash
cd "@.apps/my-chara-txt"
bash button.sh run < /dev/null > /tmp/mychara_run.log 2>&1 &
# waited ~3s for the stack to come up, confirmed via:
ps aux | grep -E "prisc\+x|chtpm_parser_pal|orchestrator|keyboard_input"
# → saw all 4 processes live, CPU usage LOW (0.0-1.0%, not pegged — confirms the §6.1 fix worked)

SESSION=$(ls -td pieces/sessions/*/ | head -1)
cat "$SESSION/pieces/apps/player_app/view.txt"
# → showed real rendered state: "Day 1 / 10 | Health: 100/100 | Money: 500 | Grain: 10"
cat "$SESSION/pieces/display/current_frame.txt"
# → showed the FULL composed frame including the auto-generated button:
#     "[>] 1. [End Turn (advance 1 day)]"
#   (this button came from piece.pdl's ${piece_methods} substitution — proves the
#    dynamic menu-generation path works, not just the static chrome)

# Real key injection, per _.0.aigent-testing-k3.txt's documented format:
echo "[2026-08-01 23:35:00] KEY_PRESSED: 13" >> "$SESSION/pieces/keyboard/history.txt"
# (13 = Enter; the only menu item is already the default cursor position, so
#  Enter alone activates "End Turn" — this is what a real player would do)

sleep 2
cat data/master_ledger.txt
# → "2026-08-01T23:34:03|1|day_end|health:95"   (REAL, appended, not simulated)
cat pieces/system/config.txt | grep -E "day=|health="
# → day=2, health=95   (REAL state mutation, persisted to disk)

# Repeated the injection a 2nd time to confirm repeatability/stability:
echo "[2026-08-01 23:36:00] KEY_PRESSED: 13" >> "$SESSION/pieces/keyboard/history.txt"
sleep 2
cat data/master_ledger.txt
# → BOTH lines present, append-only confirmed:
#     2026-08-01T23:34:03|1|day_end|health:95
#     2026-08-01T23:34:37|2|day_end|health:90
cat pieces/system/config.txt | grep -E "day=|health="
# → day=3, health=90

# Cleanup:
bash button.sh kill
rm -rf pieces/sessions
```

**What this proves, concretely:**
- Real `.chtpm` layout parsing + `${game_map}` + `${piece_methods}` substitution works.
- Real per-screen PAL module loop (idle-sync, screen_changed diffing, no unconditional spam) works.
- Real key injection through the documented `pieces/keyboard/history.txt` mechanism reaches the module.
- The `mychara_menu_input` dispatcher correctly resolves a keypress → `piece.pdl` row → `END_TURN` command.
- Real state mutation (day/health in `config.txt`) persists correctly.
- Real ledger append (`data/master_ledger.txt`) is genuinely append-only and accumulates correctly across multiple turns.
- Automatic re-render after a state change works (view.txt reflected the new day/health without any extra manual step).
- CPU stays low in headless mode (the §6.1 patch is working).

**What this does NOT yet prove / is NOT built:** farm/mine/store/inventory screens (only "main" with one "End Turn" button exists), any connection between `my-chara-txt` and `myne-qrypto`/QTC (no mining action exists in-game yet), any multiplayer anything (`genesis-txt` is 100% unbuilt), any visual/GL layer (`*-zr` projects are 100% unbuilt), QTH (100% unbuilt, design-only).

---

## 7. WHAT TO DO NEXT (concrete, in order)

1. **Read `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` in full**, especially the corrected §4/§6/§7/§8. This is the authoritative spec for what screens/actions still need building (farm, mine, store, inventory).

2. **Add the `farm.chtpm` screen next** (the design doc §4 already has a worked example of its `piece.pdl` shape — regenerated per-call based on plot state: empty/growing/ripe). This is the first screen that needs a DYNAMIC `piece.pdl` (regenerated by the compose op every render, not static like `main`'s single "End Turn" row) — read muchi-pals' `ops/muchi_menu_input.c` header comment (the PETS SCREEN section) for the real precedent on how a single screen serves multiple dynamic states.

3. **When you get to the "Mine" action**, this is where `my-chara-txt` and `myne-qrypto/qtc` connect for the first time. The design doc's original request/response-file IPC idea (`mine_request.txt`/`mine_response.txt`) was written BEFORE the decision in §4 above to reuse pal-chain's ops directly — reconsider whether that's still the right integration shape, or whether `my-chara-txt`'s own `mychara_menu_input.c` should just shell out to `myne-qrypto/qtc/ops/+x/chain_miner.+x` directly (background-launched, PID-tracked, exactly like `chain_menu_input.c`'s own `CHAIN_START_MINING` handler does — read that function, lines ~352-405 of `041.pal-chain⛓️/ops/chain_menu_input.c`, it's the real, proven, live pattern for "launch a background daemon from a menu action, guard against double-launch via a `.pid` file, track it, stop it with SIGTERM"). This has NOT been decided yet — **ask the user before building it**, don't assume.

4. **Run a real end-to-end test of `myne-qrypto/qtc/button.sh run` on its own** (create a wallet, log in, start mining, check mining status) before wiring it to `my-chara-txt` — right now it's only build-verified, never runtime-tested. Follow the same rigor as §6 (real entry point, real key injection, not op-level shortcuts).

5. **`genesis-txt` and the `*-zr` projects are untouched.** Their design docs each have a correction note at the top pointing back to `my-chara-txt`'s finished implementation as the template to follow once building starts — don't build these until `my-chara-txt` itself is more complete, per the user's own stated "build my-chara-txt first, tandem-test myne-qrypto alongside it" plan.

6. **Retire or merge `#.resume/aug-1-haiku.md`** into this handoff once you're confident nothing useful is left in it that isn't already captured here — it currently has some stale/superseded content per §5 above.

---

## 8. HOUSE-WIDE STANDING RULES YOU MUST NOT VIOLATE (collected here for convenience, all sourced from the pitfalls/standards docs read this session)

- **Never** test a feature only via direct op invocation or an isolated synthetic setup and call it "verified" — always run at least one pass through the real `button.sh run` entry point with real key injection (`Pitfall 21`).
- **Never** write to `pieces/display/current_frame.txt` from an op — that file has exactly one legitimate writer, `chtpm_parser_pal.c` (`Pitfall 14/15/18/19`, "ONE WRITER RULE"). Compose ops write `view.txt` only.
- **Never** simulate a screen switch from inside an op — always a real `<button href="...">` (`xyzos-standards §18`).
- **Never** unconditionally recompose every tick — only when a real key was relayed or a `*_screen_changed.txt` marker actually grew (`Pitfall 48`).
- **Never** give a PAL module its own quit-key path — quitting is `chtpm_parser_pal.c`'s job alone (`Pitfall 10`).
- **Always** derive "which screen is current" fresh from `pieces/display/current_layout.txt` on every dispatch call — never track it separately in mutable state.
- **Always** check `system/keyboard_input.c` for the Pitfall 22/51 busy-spin pattern before running any project headlessly for the first time.
- Use PAL's `exec` instruction to call a op from a `.pal` script — there is **no `op` instruction**, that's a silent no-op typo (`Pitfall 13`).
- If a project's directory path contains a shell metacharacter (`&`, `;`, `|`, `$`, backtick, unescaped space) and its `prisc+x` binary predates 2026-07-29, custom ops will silently fail with zero visible error (`Pitfall 50`) — check the `prisc+x.c` copy's `exec_custom_op()` for a `shell_quote()` call if this ever seems to be happening.

---

## 9. WHO TO ASK IF STUCK / WHAT'S ALREADY BEEN DECIDED (don't re-litigate these)

- **Interface architecture:** CHTPM nav pattern, per §2. Settled, proven three times over. Not up for reinterpretation.
- **QTC reuse strategy:** unmodified copy of `041.pal-chain⛓️`, no renaming inside the reused code, cosmetic-only rename at `my-chara-txt`'s own presentation boundary. Settled, per §4, direct user ruling.
- **Build order:** `my-chara-txt` first, `myne-qrypto` in tandem (test both, but `my-chara-txt` leads). `genesis-txt`/`*-zr` come after. Settled, direct user instruction earlier in this session.
- **How the Mine action should integrate `my-chara-txt` ↔ `myne-qrypto`:** **NOT settled.** See §7 item 3. Ask the user.
- **QTH design:** described in the design doc, not yet built, not yet validated against any real running precedent (unlike QTC). Treat it as a genuine open design problem, not a settled spec, when you get to it.

---

---

## 10. REAL REGRESSION HARNESSES NOW EXIST (added after §1-9 were first written, same session)

The user asked directly "did you run a testing harness?" — the answer at that point was no (§6 was ad-hoc bash). Both projects now have a real `test-harn-same/` harness, modeled directly on `045.muchi-pal-agent🤖️+1/test-harn-same/`'s own proven shape (thin `button.sh` + reused project-agnostic `ops/tk_*.c` primitives + `scenarios/*.sh`). Read `%.harnesses/xo-human.md` §5 for the full rationale before extending either.

### 10.1 `my-chara-txt/test-harn-same/` — `demo_end_turn.sh`, 10/10 PASS

Reproduces §6.2's exact manual trace as a real, re-runnable script:
```bash
cd @.apps/my-chara-txt/test-harn-same
bash button.sh demo
```
Asserts: baseline day/health render correctly, real Enter keypress → ledger gets a `day_end` line, `config.txt` day/health mutate correctly, frame re-renders with new day, repeated across 2 turns for stability, ledger stays exactly append-only (no dupes/loss), and `keyboard_input` CPU stays low (regression guard for the Pitfall 51 fix — §6.1). Proof (before/after frame snapshots + final ledger) saved to `proof/harness-<timestamp>/` each run.

**Not yet covered by this scenario:** farm/mine/store/inventory (those screens don't exist yet — add new `scenarios/demo_*.sh` files as each screen gets built, same shape).

### 10.2 `myne-qrypto/qtc/test-harn-same/` — TWO scenarios, both PASS

**`demo_login_screen_smoke.sh` (10/10 PASS):** proves the reused `041.pal-chain⛓️` engine launches, spawns all 4 real processes, and renders the correct login screen from its NEW location (`@.apps/myne-qrypto/qtc/`, fresh empty `data/`+`wallets/`).

**`demo_signup_login_wallet.sh` (11/11 PASS, added later same session):** the REAL signup→login→wallet-screen flow, closing what was originally flagged as a known research gap. `<cli_io>` multi-field navigation was researched directly from `system/chtpm_parser_pal.c` source (not guessed) and is now fully understood and encoded in this scenario:

- `cli_io` fields ARE numbered nav items in the SAME list as buttons (`is_interactive()` includes `cli_io`), and render with the identical `[prefix] N. Label: [value]` bracket+number format — so `tk_focus_item.c`'s digit-jump-by-label mechanism works on them completely unmodified.
- **Fill sequence, confirmed correct and now proven live:** digit-jump to the field → `Enter` (13) **activates** typing mode → type characters (each one live-syncs to `gui_state.txt` under that field's `target_id`, not just on commit) → **`ESC` (27), NOT another Enter, deactivates** the field ("KISS: ESC just deactivates, NEVER clears input" — a second Enter on a non-empty buffer commits but explicitly stays active, per the parser's own comment: "STAY ACTIVE: Do NOT deactivate the input element").
- **Two real bugs found and fixed while building this scenario (both in the SCENARIO, not the underlying engine):**
  1. `wallets/<wallet_id>` is a **directory** containing `wallet.txt`, not a flat file — an assertion checking `[ -f wallets/$WALLET_ID ]` will always wrongly fail even on a genuinely successful signup. Check `[ -d wallets/$WALLET_ID ] && [ -f wallets/$WALLET_ID/wallet.txt ]` instead.
  2. A `<cli_io target_id="wallet_id_input">` field's typed value **persists across screens that share that same `target_id`** (real, confirmed `chtpm_parser_pal.c` behavior — `login.chtpm` and `signup.chtpm` both use `target_id="wallet_id_input"`). Re-typing into an already-filled field without clearing it first **appends** instead of replacing, producing garbage (`"harnesstest1785653794harnesstest1785653794"`), which made login legitimately fail with "No such wallet." — a real, live-caught bug in the test scenario's first draft, not the engine. Fix: send ~30 backspace (127) keystrokes to clear a field before typing into it, unconditionally, regardless of whether you expect it to already be empty.

```bash
cd @.apps/myne-qrypto/qtc/test-harn-same
bash button.sh demo               # runs the smoke test (default)
bash scenarios/demo_signup_login_wallet.sh   # runs the full real flow
bash button.sh all                # runs both, prints a KPI summary
```

**Still not covered:** start-mining / check-balance / send-cones flows. The same `cli_io` fill mechanic now proven for signup/login applies directly to `send_screen.chtpm`'s two fields (`to_wallet_input`, `amount_input`) — extending this scenario or adding a sibling one should be straightforward now that the mechanic is proven, not a research problem anymore.

### 10.3 A cosmetic discrepancy you'll see and shouldn't worry about

Both `bash button.sh demo` runs exit with `Exit code 143` at the very top of the tool output, even on a full PASS. This is SIGTERM (128+15) propagating from the scenario's own `cleanup()` trap killing the backgrounded session — it happens AFTER `=== OVERALL: PASS ===` has already printed correctly. The scenario's own real exit code (checked via `$?` inside `button.sh all`'s summary loop) is correct; only the outer shell-tool's reported exit code is noisy. Not investigated further this session — if it bothers a future session, look at whether `trap cleanup EXIT` should also explicitly `exit $FAIL` at the very end rather than relying on the trap's own implicit propagation.

---

## 11. MAJOR CORRECTION (2026-08-02): `myne-qrypto` IS ITS OWN GAME, NOT A SERVICE — READ THIS BEFORE TOUCHING `myne-qrypto/` AT ALL

Everything in §4 and the original `MYNE_QRYPTO_DESIGN.md` framed `myne-qrypto` as "not a game," a headless blockchain service that `my-chara-txt` would call into via file-based request/response IPC (`mine_request.txt` → `mine_response.txt`). **This was wrong**, caught by direct user correction after the user tried to actually play `myne-qrypto/qtc/` and found no player-facing entry point matching `my-chara-txt`'s style. Direct user words: *"its supposed to be like my-chara, but with blockchain mining and exchange instead of farming... player will choose which coins 2 mine, and get to buy more asics. and exchange them on the exchange. its to be very simliar in playstyle and gui to mychara/ genesis-txt"*.

**`MYNE_QRYPTO_DESIGN.md` has been fully rewritten to reflect this — read the current version, not this summary, before building anything.** The short version:

- **`myne-qrypto` is its own standalone player-facing game**, structurally identical in playstyle to `my-chara-txt`: one character, one `main.chtpm`, turn-based CHTPM nav. Actions: **Mine** (choose QTC or QTH, using owned ASICs), **Store** (buy more ASICs), **Exchange** (trade QTC/QTH), **Inventory**, **EndTurn**.
- **`myne-qrypto/qtc/`** (everything built and tested this session — the full unmodified `041.pal-chain⛓️` copy, §10.2's harnesses) is now understood as the **backend engine** the game shell's Mine/Exchange actions shell out to (via `popen()`/`system()`, same pattern `chain_menu_input.c` itself already uses internally for its own `CHAIN_START_MINING` handler) — NOT the player-facing interface. Its own login/wallet/mining-status screens remain real, working, useful dev/debug tooling for auditing the chain directly, but a player of myne-qrypto-the-game never navigates to them directly.
- **Nothing that was built or tested needs to be redone or thrown away** — the QTC backend engine and its two harness scenarios (21/21 PASS) are exactly as valuable as before, just correctly understood now as the backend layer, not the whole product.
- **What's genuinely missing, and is the real next step:** the game shell itself. `myne-qrypto/` at the TOP level currently has no `button.sh`, no `system/`, no `.chtpm` screens, no `myne_menu_input.c`/`myne_compose_frame.c` — none of it exists yet. Building it is a direct, low-risk copy of `my-chara-txt`'s own already-proven P1/P2 build (same `system/` sources, same per-screen PAL module shape, same shared-menu-input/shared-compose-frame op-pair pattern) — not new design work, just the fourth application of a pattern proven three times already (wsr-pal, muchi-pals, pal-chain) plus once more live in `my-chara-txt` itself.
- **One real open design question, NOT decided, flagged in the design doc's new §10.5:** does buying ASICs actually need to change `chain_miner.+x`'s real mining odds (requires either launching N parallel miner processes per ASIC owned, or modifying the reused engine — which conflicts with the "don't rename/modify reused ops" principle from earlier this session, see the `feedback-reuse-ops-dont-rename` memory note), or are ASICs flavor/display-only for now? **Ask the user before implementing the Mine/Store screens' actual logic** — this materially changes what those ops need to do.

**Concrete first step for whoever picks this up:** copy `my-chara-txt`'s own `system/`, `scripts/build.sh`, `button.sh` (session-isolation pattern), `pal/main_module.pal` shape, and `pieces/chtpm/layouts/main.chtpm` shape into `myne-qrypto/`, swap the domain (Mine/Store/Exchange instead of Farm/Mine/Store/Inventory — note "Mine" already existed as a `my-chara-txt` action name too, don't confuse the two projects' own action lists), get a real `button.sh run` showing a `main.chtpm` with at least one working action, verified the same rigorous way `my-chara-txt`'s own P2 checkpoint was (§6.2) — real entry point, real key injection, not an op-level shortcut.

---

## 12. `my-biotech` — chemistry research via REAL Gemma-LAN calls, P1+P2+P3 DONE and LIVE-VERIFIED (updated same session, was P2-only earlier)

A fourth sibling in the `my-chara-txt` family (`@.apps/my-biotech/MY_BIOTECH_DESIGN.md`). Same CHTPM playstyle as every sibling, but the core loop makes a real, live call to a local LAN Gemma model (`gemma3:270m` via Ollama, `http://10.0.0.144:11434` — confirmed reachable this session, NOT guaranteed to stay that way, re-verify with `curl --max-time 5 <url>/api/tags` before trusting it in a future session) every "Research" turn, not as a rare/optional flourish.

**Full FSM now built and live-verified** (`ops/mybiotech_research_worker.c`): pick a random element → HYPOTHESIZE (name a real compound) → 4 separate ENRICH calls (use_case/effect/side_effect/price, each appended as `[Section]` to a REAL, player-visible per-compound dossier at `data/research/<compound>/dossier.txt`, IQABOD-style corpus summary line also appended) → FDA_REVIEW (see §14, real regulatory verdict + honest reasoning appended to the dossier) → RECORD (`discovered_compounds.txt`). `test-harn-same/demo_research_and_end_turn.sh` (11/11 PASS) and `test-harn-same/demo_fda_review_discrimination.sh` (a second, standalone harness specifically proving the judge mechanic discriminates correctly, 6/6 PASS) both exist and pass.

**Real, ASYNC from the start of this update — the earlier "synchronous, blocks the whole module" limitation is FIXED**: `mybiotech_menu_input.c`'s `RESEARCH` command now launches `ops/mybiotech_research_worker.c` as a detached, PID-tracked background process (same pattern as `041.pal-chain⛓️`'s own `chain_miner.+x`), returns immediately, and `mybiotech_compose_frame.c` polls `data/research_status.txt` to show live "⏳ Researching... [step] (Ns elapsed)" progress. Live-verified non-blocking: firing Research then End Turn with ZERO delay between them, both completed correctly within the same second.

**Two real bugs found and fixed, worth knowing about if you see either pattern again:**
1. `projects/my-biotech/pieces/mybiotech_menu/` didn't exist on first build — `write_kv()`'s `fopen(path, "w")` fails silently (no crash, no stderr) when the parent directory is missing, so the real Gemma call/corpus-append/ledger-append all worked but `last_message` never persisted. **General lesson for any sibling project:** any op writing to `projects/<project_id>/pieces/<piece_id>/` needs that piece's own directory to actually exist first — this failure mode produces zero error output anywhere.
2. `research_status.txt`/`research.pid` originally lived under session-scoped `pieces/system/` — if a player quit while research was in flight, those files got deleted with the session even though the worker kept running, orphaning it. Moved to `data/` (symlinked to the real, persistent project root in every session) and live-verified the fix by killing a session mid-research and confirming the worker still completed correctly.

**What's genuinely NOT built yet** (design doc §7, P4 onward): Store (buy elements, weight tracking), Market (sell compounds — APPROVED at full price, REJECTED at a black-market discount, exact discount not decided), the Corpus/Dossier viewer screens, NPC researchers, weighted-random element selection (still picks uniformly from 5 hardcoded elements — no `elements.txt`/ownership/weight tracking). Design doc §9 has 7 explicit open questions, none decided.

---

## 13. `my-lawyer` — DESIGN ONLY, not built. A third sibling: research law, write a real case document, argue before a Gemma judge

`@.apps/my-lawyer/MY_LAWYER_DESIGN.md` (written this session, zero code). NPCs post lawsuits, player picks prosecute/defend, chooses settle-or-court, and if going to court, the character's own research agent writes a REAL, growing case document (`data/cases/<case_id>/plaintiff_case.txt`/`defendant_case.txt`) via Gemma research + deterministic tool calls (search corpus, search precedent) — same "player can open and watch the document build" transparency as `my-biotech`'s own dossiers, extended to a genuinely new mechanic (a document built from MULTIPLE calls/tool-uses over several turns, not one research attempt). A judge then reads BOTH real documents and picks a winner. Winning earns money; money buys political office; office grants a real, explicit, LOGGED bias in both settlement negotiation and judging (a deliberate corruption/influence mechanic, not hidden).

**Docket generation, not yet in the design doc — real user instruction, capture before building:** the pool of available lawsuits should be bulk-generated by Gemma (~30 diverse civil/government/corporate cases), refilling monthly with a cap (not infinite/on-demand) — "there's only so many a month." Add this to §1/§6 area of the design doc before building the Docket screen.

**§4 (the judge) is already written to apply §14's own describe-don't-classify pattern from the start** — read that section before implementing, it explicitly warns against the "A or B, answer with just one letter" version `my-biotech` had to fix after the fact. **The exact keyword/scoring shape for a two-document COMPARISON (not `my-biotech`'s one-document classification) is flagged as real, scoped, NOT-yet-designed follow-up work** — don't assume `my-biotech`'s own `classify_description()` can be copied verbatim, it needs a real comparison-shaped scorer.

---

## 14. A SIGNIFICANT, HOUSE-WIDE FINDING: small local LLMs can't reliably self-classify into a verdict, even when they demonstrably know the content — READ THIS BEFORE BUILDING ANY JUDGE/VERDICT MECHANIC, ANYWHERE

**Full detail lives in THREE places, read all three before touching any judge/verdict/classification code against a small local model:**
1. `PITFALL 69` in `#.haiku+/!.xyzos-pitfalls+1.txt` — the real incident, root cause, fix, with exact measured numbers.
2. `§42` in `#.haiku+/!.xyzos-standards+1.txt` — the standing house-wide rule + a concrete 5-step checklist for any future project.
3. `#.haiku+/!.gemma-judge-tomo&iqa.md` — the wider theory doc (emoji-heavy per direct request), extending the core finding to `045.muchi-pal-agent🤖️+1`'s existing tool-detection pattern, `ROADMAP-models.txt` §11.2's future RL judge-loop plan, BT/FSM shared-vocabulary design, and — most speculatively, explicitly flagged as untested theory, not confirmed result — what it might mean for `IQABELLA`'s (design-only) teacher/student distillation harvest and `T@Q`/`tomom@qroq`'s own per-curriculum expert-weights training.

**One-paragraph summary for anyone in a hurry:** asking `gemma3:270m` to directly classify content into a binary verdict (`"Answer APPROVED or REJECTED"`) was live-measured as unreliable — wrong 2/3 times on an obviously-lethal test compound in `my-biotech`'s own FDA_REVIEW mechanic, even reframed as SAFE/DANGEROUS, even with the correct option listed first (ruling out order bias), and it produced literally zero real reasoning when asked to explain its own verdict. The SAME model, asked instead to openly DESCRIBE the same content ("Describe the safety concerns in one sentence"), gave real, accurate, content-aware answers 6/6 times. **Fix: never ask a small local model to self-classify — ask it to describe, then run your own small, hand-written, auditable keyword/sentiment scorer over its real response text to derive the verdict.** This matched a bigger model's own accuracy (`gemma3:1b`, pulled onto the LAN box this session and registered in `045.muchi-pal-agent🤖️+1`'s `model_list.txt` as `gemma-lan-1b`, also tested and also worked) WITHOUT needing to upgrade — direct user framing worth repeating: *"i think its a big deal that we try to make it work b4 going up 2 larger model."* Try describe+deterministic-score FIRST on whatever model is already in use, every time, before reaching for a bigger one.

**Fully implemented and re-verified in `my-biotech`** (`ops/mybiotech_fda_verdict.c` — the standalone, directly-testable version; `ops/mybiotech_research_worker.c` — the production FDA_REVIEW step, both carry the exact same `classify_description()` keyword scorer, kept in sync manually per this house's no-shared-headers convention). **Explicitly NOT yet applied to `my-lawyer`'s own judge** (§13 above) — that project has zero code, the design doc is written with the pattern already in mind, but the real two-document comparison scorer still needs to be built and tested from scratch.

---

*End of handoff. If anything above conflicts with what you observe on disk, trust what you observe on disk and flag the discrepancy — this document describes state as of 2026-08-02 (session spanned 08-01 into 08-02), things may have moved since.*
