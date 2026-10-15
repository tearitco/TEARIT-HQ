# `auto` and `auto-studio`: design (2026-10-14; DESIGN ONLY, nothing below is built)

Owner's request: add a system to users, **`auto` (on/off)** and **`auto-studio`**, where auto-studio sets automatic control over the **entire house account**; control is by **pipeline relay injection**; the brains are whatever is available (Gemma over the LAN, an API, tomom) and **the sub-drivers are trained**. This document says what that means in this house, how to build it in slices that each prove themselves, and how to keep it safe.

Checked before writing: no `auto` / `auto-studio` concept exists in the repo (grep over `*.md`/`*.pdl`: only unrelated "autopilot" words in piececraft and wsr-pal files).

## 1. Core idea in one paragraph
Every window in this house already has the three things an automaton needs: a **state surface** (each manager publishes `state/ui.txt`-style text), an **action surface** (the per-process relay file `#.desktop/entity_menu_history/<pid>.txt`, written to by humans' real input and by `relay_drive`), and a **checkable macro format** (`relay_drive run <cases.pdl>`: STEP rows with expectations). `auto` is a conductor that closes the loop: **observe state, decide a verb, inject it through the relay, verify with an expectation, write a ledger row**. The deciding brain is tiered and cheap-first; small "sub" drivers are trained from the ledger. A user's `auto` flag decides whether the conductor may act for that user at all; `auto-studio` widens it from one chosen window to every window that has opted in.

## 2. The two switches (per user)
| Switch | Meaning | Default |
|---|---|---|
| `auto` = off | nothing acts; the conductor is not running for this user; status shows OFF | **off** |
| `auto` = on | the conductor may act, **only** on windows/verbs the user named in `scope` | off |
| `auto-studio` = on | `auto` is also applied to **every opted-in window of the account**, scheduled by the studio (priorities, CPU/load, hosts). Turning it on implies a studio session record; turning it off or the kill switch ends all of it at once | off |

Rules that never change: **neither switch can turn itself on** (only a human action or an explicit owner-signed row can); `auto-studio` requires `auto` = on; there is a kill file `<user>/auto/STOP` that the conductor checks before every action (checked with an append-only marker or a plain existence test, not mtime).

Storage: a file `auto.pdl` beside the user's `meta.txt` (house plain pipe format), e.g.
```
SECTION | KEY | VALUE
AUTO | state | off
AUTO | studio | off
AUTO | scope | sfx-hai,voice-fx          # windows allowed when studio is off; ignored when studio is on (manifests decide)
AUTO | budget_actions_per_min | 20
AUTO | budget_llm_calls_per_hour | 60
AUTO | brain_order | rules,mac-gemma,tomom   # cheap first; each tier optional
AUTO | owner_gate | accept,promote,spend,send,arm-mic,delete   # verbs that ALWAYS need a human
```
**User data rule (AGENTS.md)**: `xyzfs/users/` is private data on local orphan branches, saved by `button.sh save-data`. `auto.pdl` lives there and is never added to a code branch; the **code** (conductor, ops, manifests) lives in the HOUSE. An agent user (e.g. `claude-0001`) gets its own `auto.pdl`; so do pets and bots if they are users. The owner's account (`jb`) is the only one whose switches the owner flips; agent accounts' switches may be flipped by the owner on request only.

## 3. The window side: manifests (opt-in, per app)
A window is controllable only if its app ships `auto.manifest.pdl` next to its `button.sh`. It states, in the house pipe format:
```
APP    | sfx-hai | window 1060x700 | button.sh run | stop | state/renderer.pid
STATE  | ui | state/ui.txt | marker=state/ui.txt        # what to read; marker = growth of this append-only file signals change
VERB   | make    | press label:Make this sound | expect state/ui.txt contains "done:" | safety=free
VERB   | play    | press play                       | expect state/ui.txt contains "msg=playing" | safety=free
VERB   | accept  | press label:Accept               | ...                                   | safety=OWNER   # never automatic
SKILL  | smoke   | tests/sfx-hai-smoke.pdl          # existing relay case = a named multi-step skill
CHECK  | judge   | ops/+x/sfx_make --check          # numeric judge that can fail
```
Why a manifest and not "drive whatever": a window that was not built for it stays untouchable; every verb carries its own expectation, so a failed action is detected, not assumed; `safety=OWNER` verbs are enumerated by the app author and enforced by the conductor.

## 4. The conductor: components (all C or pal; no Python except one-off converters)
Names are proposals; check `ops/` for collisions before creating.
| Piece | Does | Pattern |
|---|---|---|
| `auto_ctl` (op) | `on|off|studio on|studio off|status|stop`; edits `auto.pdl` only when a human-origin arm token is present; writes the STOP file | like `button.sh` verbs, C op |
| `auto_observe` (op) | reads a window's STATE files, returns a compact observation row; detects change by **marker growth** (house rule), not mtime | cursor polling like the prisc ledgers |
| `auto_act` (op) | one verb into one window: allow-list check, budget check, STOP check, `relay_drive press ...`, then the manifest `expect`; returns PASS/FAIL | wraps `relay_drive`; fork+exec with watchdog (the house's `run_op` pattern, never `popen` without a timeout: that froze the VM once) |
| `auto_decide` (op) | observation + enumerated legal verbs in, **one verb out**; runs the brain tiers (section 5) | stateless; every decision is a ledger row |
| `auto_studio.pal` | the scheduler loop (prisc): picks the next (window, goal) by priority and load, calls observe/decide/act, sleeps; backs off when load average is high; offloads heavy verbs to debil/Mac | `.pal` orchestrates, C ops do work, append-only ledger = source of truth (PRISC-OPS-ARCHITECTURE.md) |
| `auto-hq` window (x11-hq, khtpm) | the human face: big ON/OFF, `studio` toggle, STOP button, live ledger tail, per-window enable list, verdict queue. Built last | manager + projection like sfx-hai |

**Ledger** (append-only, per user, in `<user>/auto/ledger.txt`):
`ACT | ts | window | verb | tier | args | result PASS/FAIL | ms | note` and `DECIDE | ts | window | observation hash | candidates | chosen | tier | valid 0/1`. This file is also the **training data** (section 6). Reading it by identity (our own session id), never "newest on disk".

## 5. Brains, tiered, cheapest first (free models only)
- **T0 rules** (pal/C): deterministic if-this-state-then-that-verb tables plus the existing relay case files. Costs nothing, never hallucinates. Most of the value early.
- **T1 small local-network model**: Gemma on the Mac (`ai_backend.pdl`: `gemma_lan_url`, currently `gemma3:270m`). A 270M model is weak: use it **only as a chooser among enumerated verbs** with a strict output (one verb id from the list, or `none`). Anything not in the list is discarded and logged as `valid=0`. Measure the validity rate on a fixed set of observations before trusting it (slice A4).
- **T2 tomom**: the house's own hand-built LLM pipeline (`#.Z.HUMAN_LLM/3.stage.llm.tomom...`, `&.hq-apps/tomom-hq`): dormant, not missing. Use when its output format fits the chooser contract; verify it runs before depending on it.
- **T3 a bigger API model**, free providers only (no paid credits; OpenRouter free quota is per account): rare planning calls (write a goal into verbs), never in the per-action loop; budgeted by `budget_llm_calls_per_hour`.
- **T9 human**: the verdict queue. Any `owner_gate` verb stops here.
The tiers are a **cascade with caching**: identical observation hash plus goal reuses the previous valid decision (logged as `tier=cache`).

## 6. "Training the subs" (what that honestly means here)
A "sub" is a small, specialised driver for one window or one goal (e.g. `sub-sfx-variants`, `sub-voice-intel`, `sub-img-batch`): a manifest, a rules table, a prompt/bank, and a harness. It is **not** gradient-training a model on this weak machine. The house's own learning pattern is already proven in miniature (img-hai bank tuning): **propose a candidate edit, trial it as an overlay, score it with a harness, promote only on evidence, keep the old one**. Applied to subs:
1. Every `ACT`/`DECIDE` row plus the window's judge result is a labeled example (action in a state, outcome PASS/FAIL, harness score).
2. The attrition loop (memory: TEARIT + bank + tomom + FSM/GOAP; harness rows are Watch records) proposes edits: a rule row added, a prompt example added, a verb ordering changed.
3. A candidate sub runs on the same observation set as the incumbent (replay from the ledger, no live window needed for the first pass), harness-scored. **Promotion needs a measured win AND, where the quality is subjective (picture, sound), a human verdict.** Negative-test the scorer first (a deliberately bad candidate must lose).
4. Optional real learning later: fine-tune/distill a small model **off this machine** (debil/Mac), from ledger data, with the owner's consent for any data that includes their voice or screen.
Delegation flywheel in memory applies: spec + locked harness + cheap workers; a pilot (q001) already passed.

## 7. Safety rails (non-negotiable; every one is testable)
1. Off means off: with `auto`=off, **zero** relay lines are written and no LLM is called (negative test: run the conductor against a window, diff the relay file before and after).
2. Allow-lists: only manifest VERBs, only enabled windows; `safety=OWNER` verbs refused with a ledger row. Default `owner_gate` includes: Accept/promote (verdicts), anything that spends or moves wallet/chain value, sending messages outside the house, **arming the microphone**, deleting, restarting or moving the owner's windows (hotbar etc. are the owner's), editing `xyzfs/users` of another user, `git` operations, killing processes.
3. Budgets: actions per minute, LLM calls per hour, wall time per studio session; exceeding = pause + visible banner, never silent.
4. STOP: the STOP file, the window button, and `auto_ctl stop` all end every loop within one tick (test with a timer).
5. **Load awareness** (this machine ran at load average ~20): the studio refuses to start local heavy verbs above a threshold and offloads to debil/Mac; nice/ionice for local heavy work.
6. No secrets in manifests/ledger/prompts; the ledger never stores the process environment, key files or recordings. Audio/screens captured only when a human armed it.
7. **Relay caveat**: relay-driven success does not prove real-hardware focus/grab behaviour (memory `relay-testing-may-mask-real-focus-bugs`). `auto` may be trusted for state-level verbs; focus/grab bugs still need a human run.
8. Agent accounts: `auto` on an **agent** account does not inherit the owner's permissions; each user's `owner_gate` and scope are its own.

## 8. Build slices (each ends with evidence, per the house rule; do them in this order)
| Slice | Build | Proof (must include a negative) |
|---|---|---|
| A0 | `auto.pdl` schema + `auto_ctl` (on/off/status/stop) + STOP file + ledger writer | off: conductor run writes 0 relay lines (diff); STOP mid-loop halts within 1 tick; malformed `auto.pdl` = treated as off |
| A1 | `auto_act` on **sfx-hai** with its manifest: verbs `make`, `play`; allow-list, budget, expect | `make` PASS from a clean window; a verb not in the manifest refused; a deliberately failing expectation reports FAIL; budget exceeded pauses |
| A2 | `auto_observe` with marker-growth detection (sfx-hai ui.txt) | change detected on growth, no false change on idle (count over 60 s); missing file reads as "unknown", never as zero |
| A3 | T0 rules driver loop on sfx-hai: generate N variants of a recipe, keep those passing `sfx_make` checks, ledger every step | N variants made unattended, 0 owner clicks; recipes that must fail (silent/clipped) are rejected by the judge; ledger row per action |
| A4 | T1: `auto_decide` using Mac gemma as chooser over enumerated verbs | validity rate on a fixed 30-observation set (report the number, expect it to be imperfect with a 270M model); invalid outputs never reach `auto_act` |
| A5 | studio scheduler `auto_studio.pal` over 3 manifests (sfx-hai, voice-fx, img-hai queue), load-aware, offload via ssh to debil/Mac | two windows driven in one session; load threshold test (synthetic load makes it defer); STOP ends both |
| A6 | sub training: replay candidate edits over ledger rows, harness-scored, promote-on-evidence | a known-bad candidate loses; a known-good wins; promotion writes an audit row; old version retained |
| A7 | `auto-hq` window + taskbar entry + screenshot for the owner | relay case on the window itself: toggling shows OFF/ON truthfully; STOP button case |
| A8 (consent first) | shadow learning from the owner's own relay history (section 3 of file 03) | only after the owner agrees; produces macro proposals, never actions |

Each slice: build, relay/shell case, negative test, doc, commit (own paths only), then show the owner a PNG or state file. Docs not progress on their own.

## 9. Open decisions for the owner (answer these before A5)
1. Which accounts get the switches in the first release: `jb` only, agent accounts, pets?
2. The default `owner_gate` list (section 2) — add or remove verbs?
3. Is it acceptable for the studio to drive windows while the owner is at the machine (focus-follows-mouse and display-wide grabs can fight a human)? Proposal: studio pauses when it sees human input on the relay.
4. Which tier-3 free API providers are approved, and any data that must never leave the house.
5. Where the owner wants the `auto-hq` entry (the hotbar and menu positions are theirs).
