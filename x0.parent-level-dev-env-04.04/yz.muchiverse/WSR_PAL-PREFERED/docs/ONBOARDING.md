# ONBOARDING — for a new agent picking up wsr-pal

Written 2026-09-29 after a session that lost real time to four separate
misdiagnoses, each of which is now documented so you do not repeat them.

Read this whole file before you change anything. It is roughly 10 minutes and
it is the difference between an hour of work and a day.

---

## 0. Read the house skill first

    Skill: khtpm-house-standards

This project is a `.chtpm` + manager/renderer app. That skill is **mandatory**
reading before you touch a `.chtpm`, a manager/render pair, or the relay. Its
opening line is a story about an agent spending a whole session rewriting a file
whose correct replacement was already sitting in the same directory. Assume that
is about to happen to you too, and check for the real thing first.

---

## 1. What this project actually is

A Wall Street Raider-style stock market sim. The player is an economic actor
with cash and a portfolio. There are 52 AI corporations, plus governments,
population, real estate, weather and a derivatives market.

It is **not** a fork of the legacy `MSR-DEPRACATED/` k32 tree. That tree is a
much simpler, printf-menu program that never implemented its own dividend,
payroll or AI systems. If you are sent to k32 by mistake, `../MSR-README.md`
has the file:line evidence for why it was deprecated.

---

## 2. The two most dangerous traps

### 2a. Two renderers, two relays, two key formats

This is the #1 time sink. There is not one input path, there are two, and they
read **different files with different formats**.

| | renderer | relay file | line format |
|---|---|---|---|
| **default** (`button.ps1 run`) | `chtpm_parser_pal` | `pieces/keyboard/history.txt` | `KEY_PRESSED: N` |
| **pal mode** (`-Pal`, `sim-key`) | `prisc+x` + `pal/main_loop.pal` | `pieces/apps/player_app/interact_relay.txt` | bare int |

`orchestrator.c:420-436` launches `chtpm_parser_pal` and **never launches
`prisc+x`**. If you write bare ints into the pal-mode file during a default run,
**nothing happens and you will conclude the relay is broken.** It is not.

Full request flow for the default path:

    pieces/keyboard/history.txt        (KEY_PRESSED: N)   <- you write here
      -> chtpm_parser_pal resolves nav / digits / Enter
      -> writes pieces/apps/player_app/interact_relay.txt
      -> pal/main_loop_chtpm.pal polls that, calls `wsr_menu_input <n>`
      -> wsr_menu_input runs the piece.pdl METHOD command
      -> compose_frame / hit_frame rewrite pieces/display/current_frame.txt

`keyboard_input.c:209,220` and `gl_mirror.c:220,221` write to **both** relays
on purpose, so a real human keystroke reaches whichever renderer is live. Only
an agent has to pick the right one.

### 2b. Stale duplicate files

The **live** `piece.pdl` is `projects/wsr-pal/pieces/<piece_id>/piece.pdl`.
A stale copy sits at `pieces/<piece_id>/piece.pdl` and **disagrees** — the stale
trade one lists only futures/options/loans, the live one lists Buy/Sell/Short/
Cover. `wsr_menu_input.c:140` reads the `projects/` path.

Before concluding "X is not wired", confirm the path the code actually opens.

---

## 3. Driving it (minimum viable)

    # 1. start it
    .\button.ps1 run          # add $env:NO_GL=1 for headless

    # 2. read the CURRENT nav numbers - do not assume 1..N
    Get-Content pieces\display\current_frame.txt

    # 3. send a key (default mode)
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 49"   # '1'
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 13"   # Enter

    # 4. for multi-digit nav, send each digit with >= 2s between, confirm
    #    the accumulator in the frame's "Active [^]:" line, THEN Enter.

**Use 2-second gaps.** At 1.2 s a `2`,`9`,Enter sequence was silently dropped.
Watch `Active [^]: 2` then `Active [^]: 29` before committing.

`pieces/display/current_frame.txt` is plain text and is the cheapest way to
assert on state. Prefer it over decoding a PNG. `scripts\k3_frame_capture.ps1
-Topic <name> -KeyCodes 49,13` does injection plus receipt capture in one step.

Never truncate the relay files — they are append-only and cursor-based.
`orchestrator.c:400,402` truncates them on a clean launch.

---

## 4. Build

    .\button.ps1 compile      # 35 ops binaries + system binaries
    .\button.ps1 check        # verify
    .\button.ps1 kill

`button.ps1 run` sets `SKIP_ORCH_COMPILE=1` on purpose: without it every launch
recompiles for 30-60 s and looks like a hang.

To rebuild one op after editing it:

    gcc ops\your_op.c -I"ops\lib" -o "ops\+x\your_op.+x" -lm

Two things that will bite you:

- **The op binary has no `.exe`.** `build.ps1` emits `ops\+x\name.+x`. That is
  the house name and `cmd.exe` will not run it by a forward-slashed path. See
  `KNOWN-ISSUES.md`.
- **PowerShell will not run a `.+x` binary through a pipeline** ("Cannot run a
  document in the middle of a pipeline"). Use `Start-Process -FilePath ... -Wait
  -PassThru -RedirectStandardOutput $o -RedirectStandardError $e` with two
  *different* files. Passing the same file to both is refused outright.

---

## 5. Where the state lives

Flat `key=value`, 52 corps:

    projects/wsr-pal/pieces/corp_<TICKER>/state.txt
        cash, stock_price, book_value, shares_outstanding, market_cap,
        shares_held, owned_by, decision_mode, industry, last_action

Player side:

    projects/wsr-pal/pieces/player_you/state.txt          cash=<float>
    projects/wsr-pal/pieces/player_you/holdings.txt       <ticker>|<shares>
    projects/wsr-pal/pieces/player_you/transactions.txt   <ticker>|<action>|<shares>|<price>|<date>

`holdings.txt` is the de-facto shareholder ledger: one line per ticker, and
**negative means an open short position**. `transactions.txt` is the
append-only trade history. Both are written by `ops/player_trade.c`.

Ops are separate processes that communicate only through these files. There is
no shared state and no linking between them.

---

## 6. What is NOT done (so you don't assume it is)

- **No shareholder registry.** `Shareholder List` is nav 26 and a `STUB`.
  `ops/corp_apply_finances.c` says so itself: "we don't track a full shareholder
  registry, so a dividend on an AI/independent corp just leaves the corp".
  This is the current top task — see `ROADMAP.md`.
- **No payroll in the ops at all.** Zero hits for payroll/wage/employee in
  `corp_apply_finances.c`. It exists in `SOCIETY-ECONOMY-ARCHITECTURE.txt` only.
- **22 of 31 main-menu methods are `STUB`.**
- **Player trades move no price.** `player_trade.c` explicitly does not move
  `stock_price`; only `corp_update_price.c` does, once per End Turn. That is a
  deliberate v1 simplification, flagged in the source.

---

## 7. House conventions you must not break

- Never `git reset` or `git stash` in a shared checkout — other agents are live
  and share one `.git/index`. Stage only your own paths, and run
  `git diff --cached --name-only` before committing.
- Commit to **your own tool's branch** (`kilo` for this agent). Never to
  `main` or another agent's branch.
- Never sweep unrelated modified files into a commit. This checkout routinely
  has 300+ live runtime-state modifications from other agents.
- `AGENTS.md` belongs to another agent. Never stage, restore or edit it.
- Never report "done" without a fresh run and real evidence (a state diff, a
  frame capture, a log line). A clean compile is not evidence.
