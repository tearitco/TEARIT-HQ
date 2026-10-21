# Recap and handoff — Sonnet, sprint of 2026-10-17 to 2026-10-18

Branch `claude`, pushed through `0d3f23c56` (before this note). Games touched: df-hq, cdda-hq, mcl-hq (hardening also in rpg-pet, civ-rpgce, dsr-rpg, gta-hq, tcg-rpgce, compile-checked only).

## What landed (in order)

| Commit | What | Doc |
|---|---|---|
| `e460e4d6b` | small gaps: View row removed from 8 standalone games; pet-trainer pinned left info bar; bug_bounty items 2 and 4 closed | bug_bounty.md |
| `f0689bbd9` | CPU quota guard `$.crypts/cpu-quota.sh` + `ops/cpu_cap.c` (tier 1 systemd, tier 2 own limiter, tier 3 nice) | 03-pitfalls/hq-cpu-safety.md §7 |
| `50c2a2541` | scripted playthrough harnesses (pal + data) for df/cdda/mcl, `playthrough_op`, `COPYDIR` verb | 06-testing/GAME-PLAYTHROUGH-HARNESSES.md, 17.ai/HARNESS-AS-GAME-BRAIN.md |
| `1ebd8f01c` | crash hardening of the game daemons + S1 ledger compaction (`ge_compact`, archive files, hard cap 256 MB) | 03-pitfalls/hq-crash-hardening.md |
| `c97af4a8a` | decision ledger + daily review (`state/decisions.txt`, `review` verb) | 16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md |
| `0d3f23c56` | ledger wired into the behavior bank (`state/bank/dec_<game>_<action>.behavior.pdl`) with a read-back: pet retarget chance reads the walk weight (`decpct` verb) | same doc, last two sections |

## Evidence (observed, not assumed)

* Final regression `XO/run_dec2.out` (01:17, 2026-10-18) ALL DONE, 0 failures: test_decisions, chat, log, cdda c1/c2, mcl blocks, doorday, lock, tabs cdda/mcl/df, fuzz (FUZZ_KILLS=10), s1 x3.
* Playthrough: df 36, mcl 43, cdda 39 checks, 0 failures. Ledger compaction: 0 failures x3 incl. 42 MB ledger and kill -9 rounds. Fuzz: 71 PASS.
* A mutant with the read-back removed fails exactly the 6 read-back checks.
* Two older outputs show failures and were superseded: `XO/run_final.out` (00:02, test_tabs_df-hq 8 failures) and an early test_decisions run (00:30, mcl-hq rotation, 2 failures). Both were followed by green runs; the **cause of neither was proven** (timing under load suspected for the tabs one). If either returns, run that suite alone and look.

## Not tested (do not assume)

* A live daemon with compaction, decision rows or the bank running while pets walk. Windows must be relaunched to pick up the installed binaries.
* Long-run behavior of a pet population; whether retargeting sooner is actually better (no judge exists).
* rpg-pet, civ-rpgce, dsr-rpg, gta-hq, tcg-rpgce beyond a clean compile.
* mcl-hq mobs and vehicles (none exist). Real clicks on cdda/df action rows, df Build dropdown, mcl Inventory dropdown not seen on screen.

## Owner-only (needs sudo)

* Delegate the cpu controller: `/etc/systemd/system/user@.service.d/delegate.conf` with `Delegate=cpu cpuset io memory pids` (enables tier 1). Details in hq-cpu-safety.md §7.
* Install the `linux-surface` kernel; check for a Surface Laptop 3 firmware update.
* The cause of the four hard freezes (2026-10-15..17) is **unproven**: no panic, OOM or disk error in the logs; ACPI "No valid trip points".

## Next steps, in order of value

1. **Real AI step:** per-actor weights (today the weight is per game), more choices reading their weight, a score/reason column, and a judge that checks whether a changed weight helped.
2. **Grok's input:** which tab data to build first (df stockpiles+haul, df build/designation rules, cdda loot/wear/hunger/skills, mcl hotbar+chests) and the DOOR/DAY wording. Listed in `2026-10-17/from-sonnet-tabs-built-next-data.md`.
3. Dropdown rows are only about 13 characters wide (shared renderer change, needs the owner's go-ahead). Shop tab unchanged. pet-trainer has no automated suite.
4. Playthrough harness limits: a failed phase does not stop later phases; weights are keyed by game+phase only; nothing calls the AI hook yet (pet, tomom, auto).
5. Stale `lc_clock` only after kill -9 (bounded, one per game folder).

## Traps worth knowing

* Every game verb is a fresh process with the same rand seed, so scratch pets never pick targets: give them `tx=`/`ty=` in `pet_<id>.txt`.
* `grep -c` on a missing file prints nothing; on zero matches it prints 0 and exits 1. Use `if [ -f ]; then grep -c ... || true; else echo 0; fi`.
* `timeout` puts its child in a new process group and escapes the CPU cap; use `--foreground`.
* Run the test suites one at a time (scratch names collide). test_ledger_compact hits the 400 s limit inside a combined run; run it alone (`ROUNDS=5`).
* `rm -rf $VAR` is blocked by the safety check; use literal absolute paths.
* Test scripts must only kill processes they started. Never `pkill -f` / `pgrep -f` from a tool shell.

## Left alone on purpose (other agents' work, not mine)

`rezip-house.sh`, `_shared-lib/.gitignore`, `bv_gpu_raymarch.c`, `16.game/00-INDEX.md`, grok's `16.game/RULES-*.md` and `from-grok-menus-data-and-layout.md`, many `prisc+x` / `chtpm_*` binaries and 1.TPMOS files, `board/.hq_manager/`, `piececraft-hq/ops/pc_generate_chunk.c`. Never staged by me.

Tests live in `/home/no/Desktop/github/work/XO` (scratch, not in git): test_decisions.sh, test_ledger_compact.sh, test_fuzz_state.sh, test_tabs_*.sh, run_dec2.out.
