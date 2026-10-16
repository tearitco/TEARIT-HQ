# Dev log: `auto` slices A0-A3 built and proven (2026-10-14, evening)

Design: `17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md` (status section appended at the end). Code: `44.xyz.01.00/&.widgits/auto/`. First manifest: `&.hq-apps/sfx-hai/auto.manifest.pdl`. Class: `1-1.HARNECIENT.SMOL/NIGHT_50_THE_CONDUCTOR_AND_THE_OFF_SWITCH.txt`.
**What this is**: the first runnable part of the conductor, driving ONE real window (sfx-hai) through its relay, with rules only (no model). **What it is not**: the studio, any brain, any user account of the owner's, or anything heard by a person.

## 1. What was built
| Piece | File | Does |
|---|---|---|
| shared helper (text-include) | `ops/auto_common.c` | pipe-row parser, `auto.pdl` keys (anything but exactly `on` is off), ledger append, STOP check, per-minute action count |
| `auto_ctl` (A0) | `ops/auto_ctl.c` | `status / on / off / studio-on / studio-off / stop / clear-stop`; `on`, `studio-on`, `clear-stop` need `--human`; `studio-on` needs `on`; `off` also turns studio off; STOP file; one `CTL` ledger row per change |
| `auto_act` (A1) | `ops/auto_act.c` | one manifest verb into one window: checks (auto on, STOP absent, scope or studio, verb in manifest, safety not OWNER, `owner_gate`, budget) then `relay_drive press`, then the manifest expectation; `ACT` / `REFUSE` ledger rows. Exit 0 PASS, 1 FAIL, 4 refused, 5 budget, 6 window not running |
| `auto_observe` (A2) | `ops/auto_observe.c` | reads a window's STATE files; change by **marker (size) for append files, hash for rewritten files**; missing file = `unknown`, exit 3 |
| `auto_plan` (A3) | `ops/auto_plan.c` | a list of `DO | verb | count` rows through `auto_act`; stops at the first FAIL or refusal; `PLAN` row |
| `case_run` | `ops/case_run.c` | generic proof runner for ops: `CASE | name | exit | must contain | must NOT contain | command` (`~` writes `|` in the text fields) |
| proofs | `tests/auto_cases.pdl`, `tests/neg_manifest.pdl`, `tests/neg_dead.pdl`, `tests/dead.pid` | 34 cases |
| manifest | `&.hq-apps/sfx-hai/auto.manifest.pdl` | verbs make, play, freq_up, freq_down (free); owner_yes/no, claude_yes/no (OWNER = never automatic); STATE ui (rewrite) and ledger (append); `READY` and `NOTE` rows |
Manifest additions over the design: `READY | verb | file | text | timeout` (wait before pressing) and `NOTE | verb | file | field` (copy a field of the file's last line into the ACT row). Tests used a **scratch user dir**, never `xyzfs/users`.

## 2. Proof results (final run, fresh sfx-hai window)
`case_run tests/auto_cases.pdl`: **34 pass, 0 fail** (A0 14, A1 13, A2 4, A3 3). The ones that matter:
- **Off means off, measured**: with auto off, `make` refused and the window's relay file had the same byte count before and after. Same check for unknown verb, owner-gated verb and STOP file (refused, relay untouched).
- `on` without `--human` refused and the state stayed off; `studio-on` while off refused; STOP refuses `on` until a human clears it; a garbage `auto.pdl` reads as off.
- `make` PASS twice in a row (second one cannot be satisfied by stale text because success = the append-only ledger **grew** and the new last row has no FAIL); an expectation that can never be met reports **FAIL**; a dead pid file gives exit 6; a budget of 1 stops the second action (exit 5).
- Observe: idle looks say `same` (3 in a row), a ledger append reads `changed`, a missing file reads `unknown` (exit 3), never zero.
- A3 (rules only, no owner clicks): 3 x (freq_up, make) = 6 steps, `PLAN | 6 | 6 | 0 | 0`; the three sounds are distinct (ledger notes `coin_pickup_f1195_d220`, `..._f1315_d220`, `..._f1447_d220`); each make took about 1.8-1.9 s; a plan whose first verb is owner-gated stops after 1 step and never runs `make`.
Also rerun after the fixes below: sfx-hai smoke 18/18 (was 15), voice-fx smoke 38/38 (was 35).

## 3. How the first runs went (the honest part)
| Run | Result | Cause |
|---|---|---|
| 1 | 17 pass / 16 fail | my parser: the last field of a pipe row was cut at its next `|`, so any command containing a pipe broke (`ac_split`). Fixed in the shared helper |
| 2 | 29 / 4 | one test-design mistake (dead-window manifest named another app, so scope refused first) and a real window problem below |
| 3 | 30 / 3 | a `READY` wait I had just added timed out: `busy=1` for good |
| 4 | **34 / 0** | after the fix below |
### Real bug found by the proofs, introduced by me earlier today
The **▶ Play** handler I added to sfx-hai and voice-fx (`aa730d5d4`) called `signal(SIGCHLD, SIG_IGN)`. SIG_IGN is inherited by every later child, so after any Play press: `sfx_make` could not `waitpid` its own renderer (reported "failed: sfx_render rejected"), and the manager could never reap its make child (stuck "still making the last sound", `busy=1` forever). voice-fx had the identical patch, so **Say after Play was probably broken in the window the owner had open** (not reproduced there; same code). The play smoke case only pressed Play after Make, never Make after Play, so it passed. Fix: the play spawn is now a double fork (the player is orphaned and reaped by init; SIGCHLD stays default). Regression steps added: sfx-hai smoke (Make after Play must reach `busy=0` with no "failed:"), voice-fx smoke (Say after Play must not show "still making"), and `auto_cases` ("make still PASSes after a play press"). **Negative evidence for the regression case**: it is exactly what failed in run 3 before the fix; I did not re-break the code on purpose afterwards.
### A window can be "done" and still busy
sfx-hai appends its ledger row and the `done:` line **before** it plays the sound, and the manager only reaps the child afterwards, so a second `make` right after a PASS is dropped. Window-side fix: the manager now writes `busy=0|1` into `state/ui.txt`; conductor-side: the manifest `READY` row makes `auto_act` wait for `busy=0` before pressing. **Rule for new manifests: if a verb can be dropped while busy, the window must publish a busy flag.**

## 4. Limits (do not oversell)
- One window (sfx-hai), rules only. No model was called; A4 (Gemma chooser) and everything after are not built. `auto-hq`, the studio scheduler and the account menu do not exist.
- `--human` is a convention, not security (any caller can type it). Real enforcement is the A7 window button.
- The relay is not real hardware input (focus/grab bugs would not show).
- Tests make sounds **aloud** (each sfx `make` plays through the speakers; ~10 sounds per run). Sound quality was not assessed: "valid and distinct" means the numeric checks passed and the names/frequencies differ.
- A3's `freq_up` nudges accumulate in the window's working copy (+10% per press; sfx-hai working copies live in `state/sfx2.pdl`).
- Scratch user dir only; no real account has `auto.pdl`.
- Windows were restarted for these tests: sfx-hai (mine) and voice-fx (the one I had left open; state `voices2.pdl` and verdicts were cleared by the smoke case's clean start).

## 5. Next (in order)
1. Manifests for **voice-fx** (verbs say, play, knobs; READY needs a busy flag there too) and **img-hai** (queue/batch; Accept stays OWNER) so one plan can span windows.
2. A nightly runner: every relay case + `auto_cases` on debil or the Mac (needs the windows there; or run the op-only cases there), a green/red map file.
3. A4: `auto_decide` with Gemma over the LAN as a chooser, validity-rate measured on a fixed observation set.
4. A5: `auto_studio.pal` scheduler over three manifests, load-aware. A6, A7 per the design.
5. Decide the owner questions in the design (accounts, owner_gate defaults, running while the owner is at the machine).
