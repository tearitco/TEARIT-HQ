# KNOWN ISSUES

Open defects, each with how it was confirmed. "Fixed" items are kept briefly
because the *reasoning* is the useful part — several were false beliefs written
into the code as comments.

---

## FIXED

### `prisc+x` could not run any op on Windows (quoting)

`system/prisc+x.c` built each op command as `'%s' %d` — `/bin/sh` syntax — but
the Windows branch runs it through `popen()` -> MinGW `_popen` -> `cmd.exe /c`,
and `cmd.exe` does not treat `'` as a quote character. It looked for a file
literally named `'C:\...\ops\+x\wsr_menu_input.+x'`, apostrophe included.

Symptom: `The filename, directory name, or volume label syntax is incorrect.`
repeated ~12x per loop, and the pal VM ran on silently dispatching nothing.

Confirmed by: building both variants and watching stderr go from 12 errors to
empty while `history_cursor` advanced 0 -> 1.

**This is why a manual `button.ps1 run` looked healthy** — the orchestrator
spawns ops by its own path, so only the pal relay path was affected. The
symptom pointed at the relay; the fault was in the interpreter.

### `cmd.exe` cannot run a forward-slashed `.+x` path

`ops/wsr_menu_input.c` `shell_cd_run()` had a loop that did nothing but
`break`, commented:

    /* leave forward slashes — cmd accepts them for MinGW bins */

That is false. `cmd.exe` will not resolve a forward-slashed name for a real PE
whose extension is `.+x` rather than `.exe`, so **every `RUN:` row in the whole
project silently failed on Windows**.

Confirmed by, directly:

    cmd /c "cd /d <root> && ops/+x/player_trade.+x corp_AFL sell 5"
      -> 'ops' is not recognized as an internal or external command
    cmd /c "cd /d <root> && ops\+x\player_trade.+x corp_AFL sell 5"
      -> Sold 5 shares of AFL at $115.04 (proceeds $575.20)

Fixed by converting the first token's slashes to backslashes. Only the first
token, so arguments legitimately containing a forward slash are untouched.

### `$(bash ...)` in a piece.pdl RUN row

`projects/wsr-pal/pieces/wsr_trade_menu/piece.pdl` uses:

    RUN:./ops/+x/player_trade.+x $(bash scripts/active_corp.sh) buy 10

`cmd.exe` has no `$(...)` expansion, so `player_trade` received the literal
tokens `$(bash` and `scripts/active_corp.sh)` as arguments and looked for
`projects/wsr-pal/pieces/$(bash/state.txt`.

Fixed by resolving the active corporation in C on Windows
(`active_corp_index` indexing sorted `corp_*` dirs — the same mapping the
`.sh`/`.ps1` use). Faster than shelling out to PowerShell per click. **Linux is
untouched**, per the house rule that Linux stays canonical.

### `button.ps1 sim-key` could not start at all

Line 196 passed the same file to both `-RedirectStandardOutput` and
`-RedirectStandardError`. PowerShell refuses that outright:

    This command cannot be run because "RedirectStandardOutput" and
    "RedirectStandardError" are same.

and the following `Stop-Process` then failed on a null `$proc`. Fixed: two
distinct temp logs, a null guard, and both tails echoed so failures are visible.

### `system/prisc+x.c` was missing

`scripts/build.ps1:49` compiles `system\prisc+x.c`, and that file did not exist
in this tree — only `system/prisc+x` (an **ELF/Linux** binary, `7F 45 4C 46`)
and `system/prisc+x.exe` (PE). `check` passed only because the stale `.exe` was
checked in, so the build step was failing silently.

Same situation for `orchestrator` (ELF + .exe, no `.c`).

The canonical `&.widgits/_shared-lib/system/prisc+x.c` is now installed.

---

## OPEN

### No shareholder registry

`ops/corp_apply_finances.c` says it in its own comment:

> Dividend payout: real cash cost. Only routes to the player's own wallet if
> THEY are the owner — we don't track a full shareholder registry, so a
> dividend on an AI/independent corp just leaves the corp (honest
> simplification, not silently swallowed — named here).

`Shareholder List` is nav 26 on the main menu and is a `STUB`.

Consequence: a dividend on an AI-owned corporation leaks out of the economy —
money is destroyed rather than redistributed. The project is honest about it,
which is why it is the top of `ROADMAP.md` rather than a silent gap.

### The frame reports commands it issued, not results

The menu prints `Ran: <command>` for a selected METHOD row without checking
the op's exit status. For the equity rows this is now moot because the op's
stdout is captured and surfaced — an unaffordable buy correctly shows
`Insufficient cash: need $1150.40, have $1038.30.` But **any new `RUN:` row
added later will hit the same trap**, and will look like it works. Assert on
state files, not on the success message.

### chtpm digit accumulator drops fast keystrokes

A `2`,`9`,Enter nav jump was silently dropped at ~1.2 s between keys and worked
at 2 s. Not diagnosed further — worked around by timing. Worth a real look in
`chtpm_parser_pal.c`'s `digit_accum` reset logic if nav ever feels unreliable.

### 22 of 31 main-menu methods are `STUB`

Including `Shareholder List`, `List Portfolio`, `Financial Profile`,
`Earnings Report`, `Research Report`, `Chart`, `Auto`, `Watchlist`, `Private`.
`PRIVATE` is notable: `corp_ipo.c` exists and is wired through the new-corp
wizard, so IPO is reachable but not from its own menu entry.

### No payroll anywhere in the ops

Zero matches for payroll / wage / employee in `corp_apply_finances.c`. It exists
in `SOCIETY-ECONOMY-ARCHITECTURE.txt` as design only.

### Player trades have no price impact

`ops/player_trade.c` deliberately does not move `stock_price`; only
`corp_update_price.c` does, once per End Turn. Flagged in the source as a v1
simplification. Consequence: you can buy unlimited shares at a fixed price, so
the market cannot currently be arbitraged or crashed by player action.

### `system/orchestrator.c` has no source in this tree

Same shape as the prisc+x problem: ELF + `.exe` checked in, `.c` absent. The
Windows path is therefore unfixable from this tree.

---

## Deferred on purpose, for a human to reconcile

- **`prisc+x.c` has drifted from canonical, and the fix is untracked.**
  Two facts, both verified, and they are not the same thing:

  1. The house `.gitignore` (lines 113-116) deliberately vendors exactly one
     `prisc+x.c`, at
     `44.xyz.01.00/&.widgits/_shared-lib/system/prisc+x.c`, which is
     explicitly whitelisted with `!**/_shared-lib/system/prisc+x.c`. Every
     per-project copy is ignored on purpose. **This is working as designed,
     and the other machine is not missing anything** - it gets the canonical
     tracked file. The earlier note in `PROGRESS.md` describing the file as
     "missing entirely" is wrong and should not be trusted.
  2. Separately, a Windows op-spawning fix made on 2026-09-28 lives in an
     untracked copy at `WSR_PAL-PREFERED/system/prisc+x.c`. That copy is 1520
     lines against the canonical 1492, and the diff is **purely additive, 28
     lines, 0 removals** - the `_WIN32` double-quoting of op paths plus its
     explanatory comment. No canonical behaviour is modified.

  The fix is real and it works: without it, `cmd.exe` treats `'` as a literal
  filename character, every op spawn dies with "The filename, directory name,
  or volume label syntax is incorrect", and it hits the pal relay path only -
  which is why `button.ps1 run` looked healthy while no keypress ever
  dispatched. See `PROGRESS.md` for the original diagnosis.

  **The cost of leaving it here:** the fix is invisible to any other checkout
  and will be lost if this working tree is cleaned. It is not force-added,
  because doing so would vendor a divergent per-project copy, which is exactly
  what the consolidation rule exists to prevent. Reconciling it means folding
  the change into the canonical `_shared-lib` file, not committing this one.

  Left alone deliberately on 2026-09-29 at the user's direction: the other
  machine is already running a working `prisc+x`, and the canonical file is
  shared by every project in the repo, so the change deserves a deliberate
  decision rather than a drive-by from a WSR task.

---

## House-level traps (not bugs in this project, but they cost time here)

- **Stale duplicate files.** The live `piece.pdl` is under
  `projects/wsr-pal/pieces/<piece_id>/`; a disagreeing copy sits at
  `pieces/<piece_id>/`. `wsr_menu_input.c:140` reads the `projects/` one.
- **Two relays, two formats.** See `DRIVING.md`. Getting this wrong produces a
  false "the relay is broken".
- **Two different cursors.** `history_cursor` in
  `pieces/apps/player_app/state.txt` is the relay cursor. `cursor=` in
  `pieces/wsr_main_menu/state.txt` is the menu selection cursor. Reading the
  wrong one produces a false "it works".
- **Filesystem mtimes are useless for version ordering** in this repo (OneDrive
  checkout artifacts). Use `git log --date`.
- **PowerShell cannot run a `.+x` binary through a pipeline**, and cannot be
  given the same file for both stdout and stderr redirects.
