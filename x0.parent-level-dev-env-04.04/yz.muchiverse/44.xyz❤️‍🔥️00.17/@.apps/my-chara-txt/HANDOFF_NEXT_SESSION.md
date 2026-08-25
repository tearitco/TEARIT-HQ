# Handoff: my-chara-txt — Automation Retrofit Session (2026-08-02)

**From:** Sonnet 5
**To:** Next agent picking up my-chara-txt or genesis-txt work — assume ZERO context, read this whole doc first.
**Status:** Farm/Mine screens (built prior session) + a new **automation/supervision layer** are all built and **live-verified through the real `button.sh run` entry point**, including a full unattended game-completion run. `user-walkthru.txt` (same directory) has been updated with manual test steps — the user is doing a human walkthrough after this handoff, so **do not assume anything is broken just because state looks mid-game** when you pick this up; read `pieces/system/config.txt`/`plots.txt` fresh before touching anything.

Also in scope this session (sibling work, not covered in depth here): two new full design docs, `@.apps/civ-txt/CIV_TXT_DESIGN.md` and `@.apps/tactics-txt/TACTICS_TXT_DESIGN.md` — both share this exact automation model (§7 of each doc) verbatim. **Read those two docs' own §7 before touching automation anywhere else in this house — the design is meant to be copied, not reinvented per-project.**

---

## 1. WHAT WAS BUILT THIS SESSION

### 1a. Farm + Mine screens (functional gameplay, no automation)
- `farm.chtpm` / `pal/farm_module.pal`: plant (costs 10 grain, 3-day growth) → harvest (50 grain wheat / 60 corn) cycle across 3 plots, dynamic `piece.pdl` regenerated every render.
- `mine.chtpm` / `pal/mine_module.pal`: single Mine action, 70%/30% silver/gold RNG.
- `pieces/system/plots.txt` — new persistent state file (plot state/crop/harvest_day per plot). **Had to be added to `button.sh`'s session-symlink list** — it wasn't originally, which silently broke farm state persistence across sessions until fixed (a real bug, already fixed, see button.sh's `ln -sf` block).

### 1b. Automation / supervision layer (the actual ask this handoff is about)
Three independent, orthogonal dials, all in `pieces/system/config.txt`:

| Field | Values | Meaning |
|---|---|---|
| `supervision_mode` | `manual` / `semi` / `full` | Manual = status quo, human plays every key. Semi = auto-executes ONE action then pauses (`paused_for_confirmation=1`) until a `CONTINUE_AUTO` command. Full = runs unattended, self-throttled to ~1 real action/sec. |
| `decision_mode` | `0`-`3` (preset/weighted/rl/llm) | Maps onto the real, proven chassis from `014.wsr-pal💸️📌️+2/ops/corp_decide.c`. **Only `0` (preset) is actually implemented** — 1/2/3 currently fall back to preset logic (an honest, working fallback, not a stub that crashes). |
| `risk_level` | `1`-`10` | Wired as a field, read/settable, but **not yet consulted by any decision logic** — the preset tier doesn't use it. Real hook for a future `weighted` tier. |

New op: `ops/mychara_ai_decide.c` — the decision brain. Self-gates to a fast no-op unless `supervision_mode != manual` and not currently paused; throttles real actions to ~1/sec via a `last_auto_tick` timestamp field, so Full mode is observable and never floods the ledger. Preset logic priority: harvest any ripe plot → plant if grain≥10 and a plot is empty → **else END_TURN** (this fallback is load-bearing: growing→ripe transitions and health decay only happen inside END_TURN's own tick logic, so without this fallback automation would spin forever never advancing a day — a real correctness fix made mid-session, not a style choice).

New screen: `automation.chtpm` / `pal/automation_module.pal` — its own widget (3rd button on `main.chtpm`, after Farm/Mine), shows current dial state, lets you toggle `supervision_mode` and (once Semi triggers) resume via `CONTINUE_AUTO`. **Critically: `mychara_ai_decide` is called from every screen's own idle loop (`main_module.pal`, `farm_module.pal`, `mine_module.pal`, `automation_module.pal` all got the same one-line addition to their `no_key:` branch), not just automation's** — so Full/Semi automation keeps progressing regardless of which screen a human happens to be looking at, matching the widget-separation principle (checking status shouldn't require babysitting one specific screen).

### 1c. New commands in `mychara_menu_input.c`
`SET_SUPERVISION:<mode>`, `SET_DECISION_MODE:<n>`, `SET_RISK:<n>`, `CONTINUE_AUTO` — all simple `write_kv` calls, no new mechanism.

---

## 2. LIVE VERIFICATION PERFORMED THIS SESSION (real, not simulated)

Per house standing rule (Pitfall 21): always test through the real `button.sh run` entry point with real key injection, never just an op-level test. Done here, plus **extra CPU-safety rigor** because the user explicitly said a prior session crashed on CPU:

- Every test wrapped in `timeout N bash button.sh run` (hard self-terminating ceiling), `ps aux` CPU%-checked at every step (never exceeded ~2%, matching the pre-existing manual-mode baseline).
- **Manual**: confirmed zero drift (day/last_auto_tick unchanged) over several seconds of idle.
- **Semi**: confirmed exactly ONE `[AUTO] ...` action fires, then `paused_for_confirmation=1` holds indefinitely until `CONTINUE_AUTO`.
- **Full**: ran a fresh game from Day 1 all the way to `game_state=game_over` (Day 11, past `max_days=10`) **fully unattended**, cycling harvest/plant/day_end coherently in the ledger, CPU flat throughout. **Confirmed it correctly stops taking actions the instant `game_state=game_over`** (no further ledger lines despite continued wall-clock time) — this was the single most important safety property to verify, since a supervision loop that keeps "acting" after game-over would be exactly the kind of runaway the user was worried about.
- Always followed with `bash button.sh kill` + `ps aux` re-check + `rm -rf pieces/sessions` before starting the next test. No process was ever left running between tests.

**Game state was reset to fresh (Day 1, Manual mode, empty plots) at the end of this session**, and `data/master_ledger.txt` was cleared (old contents backed up to `data/master_ledger.pre-automation-test.bak.txt`) so the user's own upcoming walkthrough starts clean.

---

## 3. WHAT'S NOT BUILT / HONEST GAPS

- `weighted`/`rl`/`llm` decision tiers (1/2/3) are unimplemented — real future work, not urgent (per `%.harnesses/xo-human.md`'s own speed doctrine, `llm` should stay rare/opt-in anyway, never the default).
- `risk_level` (1-10) is wired but inert — needs a real `weighted` scoring function to actually consume it.
- No automated harness scenario for automation yet (`test-harn-same/` only covers the original End Turn flow) — `demo_automation.sh` covering Semi-pause and Full-completion would be the natural next addition, reusing the same `tk_inject_key.c`/`tk_assert_contains.c` primitives already in that directory.
- Store/Inventory screens still don't exist (unchanged from before this session).
- No Save/Load (documented pre-existing gap, `sv-ld-ses-strat.md` has the real fix plan) — automation runs against the same single persistent `config.txt`/`plots.txt` manual play does, no isolated save slots.

---

## 4. THE GENESIS-TXT QUESTION — SHOULD THIS ESCALATE THERE NOW?

**Recommendation: yes, and genesis-txt's own design doc already anticipated this — it's not scope creep, it's closing a gap that doc already named.**

Read `@.apps/genesis-txt/GENESIS_TXT_DESIGN.md` §7 ("AI PLAYER BEHAVIOR") and §10 ("WIDGETS") before starting. Concretely, that doc already specifies:
- `ai_difficulty=0/1/2` (easy/medium/hard) in its own `config.txt` — **this is the same `decision_mode` axis** my-chara-txt just proved live, genesis-txt's doc just named it `ai_difficulty` instead. Recommend renaming/aligning to `decision_mode` for consistency across the house rather than keeping a parallel, differently-named field for the same concept.
- Separate **P1-P2 player widgets** (for humans) and **A1-A4 AI runner widgets** (for computer players) — this is genesis-txt's own multiplayer-native version of the exact widget-separation principle my-chara-txt's `automation.chtpm` just proved for a single player. The AI runners are effectively N simultaneous instances of what `mychara_ai_decide.c` does for one player.

**What genuinely needs new design work, not just a copy-paste** (per `%.harnesses/xo-human.md` §4's own still-open questions, unchanged by this session):
1. Should a mid-game human→AI hand-off be visible to OTHER players (e.g. "Alice's character is now AI-controlled")? No precedent for this in the single-player case.
2. Should genesis-txt's easy/medium/hard difficulty be three genuinely different `weighted`-tier tunings, or `preset`/`weighted`/`weighted-tuned` as three distinct tiers? Still not decided.
3. Genesis-txt has REAL other players whose turns must be waited on — my-chara-txt's supervision loop only ever had one entity to gate; genesis-txt's `civ_turn_dispatch.c`-equivalent (a sequential or simultaneous multi-player turn dispatcher, TBD which — see that project's own §5) needs its OWN park-and-wait logic per player slot, not a single global gate. This is real, non-trivial extension work, not a mechanical copy.

**Concrete first step if you pick this up:** don't build genesis-txt's full multiplayer turn dispatcher yet — first just port `mychara_ai_decide.c`'s exact preset-tier shape (harvest→plant→end_turn priority) into a `genesis_ai_decide.c` stub that operates on ONE `players.txt` row at a time, prove that single-player-shaped port works through genesis-txt's existing (if any) test harness, THEN tackle the harder multi-player turn-dispatch question as its own increment. Ask the user before committing to sequential-vs-simultaneous multiplayer turns if that's not already settled elsewhere — it wasn't re-litigated this session.

---

## 5. FILES TOUCHED THIS SESSION (for a fast diff/audit)

```
@.apps/my-chara-txt/
├── ops/mychara_ai_decide.c          NEW
├── ops/mychara_menu_input.c         MODIFIED (new commands + PLANT/HARVEST/MINE from prior session)
├── ops/mychara_compose_frame.c      MODIFIED (automation screen render branch + farm/mine from prior session)
├── pal/automation_module.pal        NEW
├── pal/main_module.pal              MODIFIED (ai_decide call in no_key branch)
├── pal/farm_module.pal              MODIFIED (same)
├── pal/mine_module.pal              MODIFIED (same)
├── pieces/chtpm/layouts/automation.chtpm   NEW
├── pieces/chtpm/layouts/main.chtpm  MODIFIED (Automation button)
├── pieces/system/plots.txt          NEW (prior session) — now also correctly symlinked per-session
├── pieces/system/config.txt         MODIFIED (new automation fields, reset to fresh Day 1 at session end)
├── projects/my-chara-txt/pieces/automation/   NEW dir (piece.pdl regenerated at runtime)
├── button.sh                        MODIFIED (plots.txt symlink fix, fresh-config template updated, binary check list updated)
├── scripts/build.sh                 MODIFIED (mychara_ai_decide.c build line added)
├── default_op.txt                   MODIFIED (mychara_ai_decide registered)
├── user-walkthru.txt                MODIFIED (new §4a automation section, updated known-gaps)
├── MY_CHARA_TXT_DESIGN.md           MODIFIED (new §10a automation retrofit note)
└── HANDOFF_NEXT_SESSION.md          NEW (this file)

@.apps/civ-txt/CIV_TXT_DESIGN.md     NEW (full 4X design, see its own §7 for shared automation doctrine)
@.apps/tactics-txt/TACTICS_TXT_DESIGN.md   NEW (10x10 tactics battler design, same §7)
```

---

---

## 6. ADDENDUM (same day, later): GL mirror, test harnesses, and a HIGH-PRIORITY confirmed bug

### 6a. GL/RGB mirror window — now real, confirmed working by the user directly

`system/gl_mirror.c` (the actual GLUT window-opening binary) was missing entirely before this addendum — only `chtpm_rgb_render.c` (writes pixels to a file, no window) existed, copied in from an earlier session but never paired with the binary that actually displays them. Ported the real, proven `gl_mirror.c` from `101.mutaclsym🧟‍♂️️+18.01/system/gl_mirror.c` (717 lines, GLUT+X11), along with the real ASCII glyph font registry it needs (`pieces/registry/fonts/ascii/`, ~1.2MB, copied from the same source).

**A real, pre-existing bug in mutaclysm's own `gl_mirror.c` was found and fixed** (in this project's copy only, mutaclysm's own file untouched): a comment at line 364 contained a nested `/* 30 fps cap */` inside an outer block comment, which terminates the outer comment early and breaks compilation. Fixed by rewording to avoid nested comment markers.

`button.sh`'s `run` action now launches `gl_mirror` + `chtpm_rgb_render` as background daemons, gated on `[ -z "$NO_GL" ] && [ -n "$DISPLAY" ]` (skips gracefully if unavailable — **test harness scenarios should always pass `NO_GL=1`**, see §6b), waiting for the first real frame before launching (avoids a known black-window race). Window title personalized to `"my-chara-txt RGB mirror"` (was hardcoded `"mutaclsym RGB mirror"`, a copy-paste leftover). **The user directly confirmed seeing the window open** — this is real, tested, working, not just theoretically wired.

**Separate, NOT built**: the tactics-txt "View Board" mutaclysm-style ASCII/emoji map-as-its-own-persistent-GL-window (discussed with the user, deferred until `board.chtpm` exists in tactics-txt — pure text-mirror only was this pass's scope).

### 6b. Test harnesses added — `test-harn-same/scenarios/`

Two new scenarios alongside the pre-existing `demo_end_turn.sh` (which itself needed a fix, see below): `demo_farm_mine.sh` (plant/harvest/mine) and `demo_automation.sh` (Manual/Semi/Full supervision, including a full unattended Full-mode run to `game_state=game_over` with CPU verified low throughout). All three individually verified PASS. Run any one directly:

```bash
cd @.apps/my-chara-txt/test-harn-same
bash scenarios/demo_end_turn.sh      # or demo_farm_mine.sh / demo_automation.sh
```

**`./button.sh all` (running all three back-to-back) hung/stalled after the first scenario in this session's own testing** — each scenario individually passes cleanly and quickly, but the sequential "all" runner did not progress to scenario #2 within a 280s bound. Root cause not chased down (a real, open item, not faked as fixed) — possibly related to §6c's own relay-consumption bug affecting session/process cleanup timing between scenarios, or an unrelated `bash "$s"` process-handoff issue in `test-harn-same/button.sh`'s own loop. **Recommend running scenarios individually until this is root-caused.**

**`demo_end_turn.sh` needed a real fix this session** (not from `NO_GL`/GL work — a pre-existing test broke because of THIS session's own earlier Farm/Mine/Automation additions): it assumed a bare `Enter` press activates End Turn because End Turn used to be the only/default-selected item on `main.chtpm`. Since Farm/Mine/Automation buttons were added earlier this session, the default cursor now sits on Farm (item 1) instead — the old test's bare-Enter keystroke was silently activating the wrong screen. Fixed to explicitly select item 4 (digit `'4'` + Enter) before pressing End Turn. **Lesson for future UI changes**: adding new nav items to a screen changes what a bare `Enter` press does everywhere that screen is the entry point — check existing harness scenarios for this exact assumption before adding new buttons to any screen.

All scenarios' own session-readiness polling loops were fixed to `grep` for real screen content (e.g. `"M Y - C H A R A"`) rather than just checking `[ -f current_frame.txt ]` — the latter is satisfied by `chtpm_parser_pal`'s own transient `"[Map Loading...]"` placeholder frame too, causing intermittent spurious failures on the very first assertion of a run. This same fix was independently needed in civ-txt's and tactics-txt's own new scenarios (see their own handoffs) — likely worth applying to any OTHER existing `test-harn-same` scenario in this house that predates this finding.

### 6c. HIGH-PRIORITY CONFIRMED BUG — interact_relay.txt consumption-position issue, affects the whole CHTPM family, not just this project

**Found and reproduced multiple times while writing this session's new harness scenarios (civ-txt, tactics-txt, and here) — this is real, confirmed, NOT yet root-caused, and almost certainly present in every other project using this house's shared `chtpm_parser_pal.c`/`prisc+x.c` pattern (wsr-pal, muchi-pals, pal-chain, mutaclysm, etc. — none of THEIR own harnesses were re-tested against it this session, but the mechanism is in shared code, not project-specific code).**

**The bug, as observed:** navigating between CHTPM screens via a real `<button href>` can cause one or more STALE relay entries (previously-consumed keypresses from an EARLIER screen) to be re-dispatched against the NEW screen's own `piece.pdl`, resolving to whatever numbered METHOD row the stale entry's value happens to coincidentally match. Critically, **this is not a fixed "one phantom action per navigation"** — it compounds. A test that repeatedly ping-pongs between two screens (e.g. main↔farm in a wait-for-ripening loop) saw `day` inflate from an expected ~4 to over 55 within just 8 loop iterations — strong evidence that each newly-launched screen's own `prisc+x` process re-reads `interact_relay.txt` from position 0 every time, re-executing an ever-growing backlog of already-handled history rather than resuming from wherever the previous screen's own module left off.

**Impact on THIS session's work:** civ-txt's own setup→main transition hit this too (confirmed harmless there only because the replayed action, `CONFIRM_START`, is idempotent). Tactics-txt's setup→main transition hit it visibly (one phantom `END_TURN` fires immediately on arrival, confirmed and now asserted-as-expected in `demo_setup_and_battle.sh` — see that project's own handoff). My-chara-txt's own farm↔main↔mine round-trips hit it severely enough that `demo_farm_mine.sh` had to be redesigned to avoid repeated round-trip navigation entirely (seeding an already-ripe plot directly on disk instead of waiting through the UI) rather than trying to assert an increasingly-unpredictable exact day count.

**This was NOT introduced by this session's own new code** — it lives in the shared `chtpm_parser_pal.c`/`prisc+x.c` read_history/relay mechanism, copied unmodified from `041.pal-chain⛓️` into every project in this family. **Real, valuable future work**: root-cause exactly how `interact_relay.txt`'s consumption position is tracked across a screen-switch (does the position reset live in the file itself, in a PAL VM register that doesn't survive a process restart, or somewhere else?), and fix it so a freshly-launched screen's module starts consuming relay events from wherever the PREVIOUS screen's module left off, not from position 0. Given the shared, security/stability-relevant nature of this file, **do not attempt this fix casually** — it touches code every CHTPM-based project in this house depends on.

**Until fixed, the practical guidance for anyone writing a new test scenario in this house:** avoid designing scenarios around repeated back-and-forth screen navigation if you need to assert exact state (day counts, turn counts, etc.) — either stay on one screen for the bulk of a test (as `demo_automation.sh` now does deliberately), or seed/assert state directly on disk rather than waiting through multiple UI round-trips, or use bounded polling with tolerant assertions rather than exact-count predictions.

### 6d. Game state reset again after this addendum's own testing

Config/plots/ledger were reset to fresh (Day 1, Manual, empty plots, empty ledger) at the end of this addendum too — multiple harness runs since §1-5 left real state changes on disk.

---

*End of handoff. Game state was left fresh (Day 1, Manual, empty plots) for the user's own walkthrough — check `pieces/system/config.txt` before assuming anything about current game progress.*
