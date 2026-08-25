# xo-human.md — "Exo Harness": AI loops (FSM/weighted/RL/LLM) that can drive OR co-pilot a human player's slot

**House:** `44.xyz…`
**Date:** 2026-08-01
**Status:** guidance / research only — nothing built yet
**Audience:** whoever picks up `my-chara-txt` / `genesis-txt` AI-player work next

**Written in response to a direct question:** did this session use a formal testing harness? **No** — `my-chara-txt`'s P2 checkpoint was verified with raw `bash` + manual key injection against `pieces/keyboard/history.txt`, not a reusable `button.sh`-entry harness. That's a real gap; §5 below covers closing it using tooling that **already exists and is project-agnostic**, not hypothetical.

The user's actual ask: guidance for a harness that can run **external AI loops (RL/BT/FSM, even an LLM via Gemma) to play the farming/trading games alongside human players — and that a human player can hand control of THEIR OWN slot to, mid-game, if they choose.** That's the "exo" in `xo-human` — an exoskeleton the human can step into or out of, not a separate opponent bolted on the side.

---

## 1. The good news: this exact mechanism already exists, proven, in `014.wsr-pal💸️📌️+2`

Read `014.wsr-pal💸️📌️+2/ops/corp_decide.c` in full before building anything — it is the single most relevant piece of code in this entire house for this task. It is NOT a design doc, it's real, working, tested code, and it already does almost exactly what's being asked for, just for stock-trading corporations instead of farm characters.

**The `decision_mode` chassis** (an integer field, `0`–`4`, stored per-entity in that entity's own `state.txt`):

| Mode | Name | What it does |
|---|---|---|
| `0` | **preset** | Trivial hardcoded threshold rule. Instant, zero cost. |
| `1` | **weighted** | A REAL ported formula (`fundamental_value()` — book value × market-cap multiplier × leverage factor × risk-bias factor, blended 70/30 with price momentum). Not a stub — genuinely reused from `analysis_loop.c`'s own real stock-pricing logic. |
| `2` | **rl** | Currently a STUB that falls back to `weighted`'s logic. A real placeholder for a future learned policy, honestly labeled as not-yet-real in the code's own comments. |
| `3` | **llm** | A real, live LAN call to `gemma3:270m` via Ollama (see §2 below for exactly how). |
| `4` | **human** | **Parks.** If no human decision has been queued yet (`human_decision` field empty), it prints `"[human] waiting for input (run: button.sh choose buy|sell|hold)"` and returns WITHOUT taking any action. The entity's turn simply waits. The instant a human runs `button.sh choose buy`, that gets read, consumed, and the turn resolves. |

**Why mode 4 IS the "exo harness" hand-off mechanic, already built:** a human player literally just sets `decision_mode=4` on their own entity's `state.txt` to take manual control, or sets it to `0`/`1`/`3` to hand control to an automated tier — **mid-game, at will, no special "hand-off" code needed at all.** The park-and-wait behavior of mode 4 IS the human-in-the-loop mechanic. This is not a new thing to design; it's a config value to read and branch on, already proven.

**Direct implication for `my-chara-txt`/`genesis-txt`:** when the "Mine"/"Farm"/"Trade" action screens get built, each player slot (in `genesis-txt`'s multiplayer case) or the single character (in `my-chara-txt`'s case, if someone wants to watch an AI play solo) should carry the exact same `decision_mode` field, and the exact same park-and-wait pattern for mode 4. Don't invent a different mechanic — copy `corp_decide.c`'s shape line-for-line, swap in farm/mine/trade decisions instead of buy/sell/hold.

---

## 2. The real "Gemma + tools" pattern (from `045.muchi-pal-agent🤖️+1`) — and the ONE thing to get right

The user named `045.muchi-pal-agent🤖️+1` as the demonstrated precedent for "external AI loops, like gemma + tools." Read `agent-summary-claude.txt` in that project (157 lines, a real live-test report, not a design doc) before building anything LLM-driven here. **The single most important finding in it:**

> "Gemma is too small to reliably follow a TOOL: format, so tool detection is fully deterministic and keyword-based, decided BEFORE any LLM call."

**Concretely, the real (not hypothetical) architecture is:**

1. `gemma_strategy.c` — reads the user's raw text, does **deterministic keyword matching** (e.g. "list"/"dir"/"files"/"show" → `detected_tool=list_dir`, `selected_strategy=A`). **No LLM call happens at this stage.**
2. `strategy_execute_a.c` — if a tool was detected, **pre-executes it synchronously**, renders the result straight into the frame, BEFORE any LLM call.
3. `send_message.c` — the actual LLM chat call, separate and optional. In the live test this even *failed* (bad API key) and the tool-execution path still worked correctly, because it doesn't depend on the LLM succeeding.

**Do not build native function-calling / tool-schema plumbing for a small local Gemma model in this house — it has already been tried, found unreliable, and replaced with deterministic pre-detection.** If an "exo harness" needs to decide "should I mine or should I farm right now," the right shape is: cheap deterministic rule/weighted-score decides FIRST (same as `corp_decide.c`'s `decide_from_value()`), and an LLM call is only invoked for a genuinely judgment-shaped question, with a hard fallback to the deterministic result if the LLM call fails or returns garbage — exactly like `corp_decide.c`'s own `llm_choice(..., int fallback)` parameter already does.

**The actual mechanics of a real LLM call in this house** (from `corp_decide.c::llm_choice()`, live-proven, copy this shape):
- A plain-text **persona file** (`pieces/registry/personas/decide_trade.txt` in wsr-pal's case) is the system prompt — just a text file, no schema.
- A one-shot request is built as a JSON blob (`{"model":"gemma3:270m","stream":false,"messages":[{"role":"system",...},{"role":"user",...}]}`) and written to a temp file.
- `ops/+x/connect_op.+x <url> <request.json> <response.json>` does the actual HTTP POST (to a **LAN** Ollama instance, `http://10.0.0.144:11434/api/chat` in the wsr-pal case — check what LAN address is live/current before reusing this verbatim).
- `ops/+x/json_parser.+x <response.json> 'message.content'` extracts the reply text.
- The reply text is lowercased and substring-matched for expected keywords (`"buy"`, `"sell"`, `"hold"`) — **not parsed as structured JSON/function-call output.**
- **No conversation history, no tools[] array** — every call is a fresh, stateless, one-shot classification. This is deliberate, matches `GAME-AI-SPEED-DOCTRINE`'s hard rule (see §3) that LLM calls must stay off the routine per-tick path.

---

## 3. The hard speed rule (find and read the doctrine doc — this session couldn't locate it, you might)

Multiple files this session referenced a `GAME-AI-SPEED-DOCTRINE.txt` (cited inside `014.wsr-pal💸️📌️+2/SOCIETY-ECONOMY-ARCHITECTURE.txt` as: *"AI APIs = training/rare-moment only, never the routine in-game path"*, and *"a live LLM call averaged ~935ms in this session's own testing"*). **This session searched for that exact file and could not find it at the expected path** (`44.xyz…/GAME-AI-SPEED-DOCTRINE.txt`) — it may be under a different `44.xyz…` variant directory, may have been renamed, or may no longer exist. **Find it before building an exo harness that drives more than one or two entities.** The rule it states, paraphrased from citations found in `corp_decide.c`'s own header comment and `SOCIETY-ECONOMY-ARCHITECTURE.txt`:

- `decision_mode=3` (llm) must **never** be the default/routine tier across many entities — at ~935ms per call, ticking even a modest number of AI players via LLM would make a multiplayer round take unacceptably long.
- `weighted` (mode 1) is the real, default in-game mechanism for automated players.
- `llm` stays valid, real code — for training/distillation data generation, or genuinely rare "big decision" moments — never the per-tick workhorse.

**Direct implication for `genesis-txt`'s AI difficulty tiers** (already named in `GENESIS_TXT_DESIGN.md` §7 as easy/medium/hard): these should map onto `preset`/`weighted`/`weighted-with-better-tuning`, **not** "easy=preset, hard=llm." If you want an LLM-tier AI player as a genuinely optional, opt-in, slower "watch it think" mode, that's fine and matches this house's existing pattern — just don't make it the default or the thing that runs every tick for every AI player in a 4-player game.

---

## 4. Concrete shape for `my-chara-txt`/`genesis-txt`'s exo harness (recommendation, not yet decided)

Once the farm/mine/store screens exist (see `my-chara-txt`'s own `MY_CHARA_TXT_DESIGN.md` §7 next-steps), the natural place for this is a new shared op, modeled directly on `corp_decide.c`:

- **`mychara_ai_decide.c`** (single-player) / **`genesis_ai_decide.c`** (multiplayer) — reads `decision_mode` from the player's own state, branches exactly like `corp_decide.c`:
  - `preset` — trivial rule ("if grain < 20, farm; if have ore, sell it; else mine").
  - `weighted` — a real scored decision across farm/mine/trade options (health urgency, inventory levels, current prices) — this is genuinely new logic to write (no existing "farm sim AI" precedent to port, unlike `corp_decide.c`'s lucky reuse of a real stock-pricing formula), but the SHAPE (a scoring function → threshold → action) should mirror `decide_from_value()`.
  - `llm` — same `connect_op.+x`/`json_parser.+x`/persona-file/keyword-match pattern as §2, used sparingly (e.g., only for the "which Miracle to cast" kind of judgment call in `TSC_ELO`, or "should I take a risky trade" in `genesis-txt`'s exchange — NOT for routine farm/mine ticks).
  - `human` — **exact same park-and-wait mechanic as `corp_decide.c` mode 4.** A human player's own turn simply doesn't resolve until they act through the real UI (a real keypress through the real CHTPM screen, per this session's own P2-verified pattern) — no separate "hand control back" button needed, because a human acting through the UI IS the hand-off in reverse.
- **The human "give control to the exo harness" action** is then just: set `decision_mode` away from `4` (a real menu action, e.g. a `piece.pdl` row "Auto-Play (AI takes over)" → `COMMAND SET_DECISION_MODE:1`). Setting it back to `4` takes control back. This needs no new mechanism beyond what already exists.

**Genuinely open, not yet decided — ask the user before building:**
- Should a mid-game hand-off be visible to OTHER players in `genesis-txt` (e.g. "Alice's character is now AI-controlled")? `corp_decide.c` has no precedent for this since wsr-pal corporations don't have a multiplayer-visibility concern the same way.
- Should the `weighted` tier's scoring formula be tunable per-difficulty (matching `genesis-txt`'s existing easy/medium/hard framing), or should difficulty instead select between `preset`/`weighted`/`weighted-tuned` as three distinct tiers?
- Whether `llm` tier should be available to human-controlled slots too (e.g., "let Gemma suggest my next move" without taking full control) — `corp_decide.c` has no such half-way mode; would be new design.

---

## 5. The formal testing-harness gap (the actual "did you use a harness" question)

This session's `my-chara-txt` P2 test was real (went through the real `button.sh run` entry point, real key injection, per Pitfall 21) but was **ad-hoc bash**, not a reusable harness. This house DOES have a real, proven, reusable harness shape — it just wasn't used this session. Two conventions exist, both real:

1. **Per-project `test-harn-same/`** (e.g. `045.muchi-pal-agent🤖️+1/test-harn-same/`) — a thin `button.sh` (actions: `compile`, `demo`, `all`) + `ops/` holding **project-agnostic** key-injection/assertion primitives + `scenarios/*.sh` holding the actual test sequences (one scenario per feature, each prints `PASS:`/`FAIL:` lines and an `OVERALL` verdict).

2. **Cross-project `%.harnesses/<pair>/`** (this very directory — e.g. `%.harnesses/file-menu+mutaclysm/`) — same shape, used when the thing under test spans more than one project's own directory (exactly `my-chara-txt` ↔ `myne-qrypto` once the Mine action connects them, or a future exo-harness that needs to drive `genesis-txt` across multiple player sessions).

**The genuinely useful, reusable part: `045.muchi-pal-agent🤖️+1/test-harn-same/ops/` already has FOUR project-agnostic key-injection ops** — `tk_inject_key.c`, `tk_type_text.c`, `tk_focus_item.c`, `tk_assert_contains.c` — described in that project's own `button.sh` header comment as "byte-identical copies, project-agnostic." **These can likely be copied directly into a new `%.harnesses/xo-human/` or `my-chara-txt/test-harn-same/` without modification** — read them before writing your own key-injection helper from scratch (this session's own P2 test manually echoed into `pieces/keyboard/history.txt` by hand, which works but duplicates what these ops already do more robustly).

**Update (2026-08-01, later same session): both stood up and passing.** `my-chara-txt/test-harn-same/` (`demo_end_turn.sh`, 10/10 PASS) and `myne-qrypto/qtc/test-harn-same/` (`demo_login_screen_smoke.sh` + `demo_signup_login_wallet.sh`, 10/10 + 11/11 PASS) both exist now, using exactly this shape. The `qtc` harness's second scenario also closed the `<cli_io>` multi-field-navigation research gap originally named here — see `#.haiku+/HANDOFF_NEXT_SESSION.md` §10.2 for the confirmed mechanic (digit-jump → Enter activates → type → **ESC (not Enter) deactivates**, values persist across screens sharing a `target_id` so clear-before-type is required) and two real scenario bugs found+fixed along the way (`wallets/<id>/wallet.txt` is a directory, not a flat file; the persist-across-screens behavior). This same mechanic now directly unlocks `send_screen.chtpm`'s two-field flow (`to_wallet_input`/`amount_input`) as a straightforward extension, not a research problem.

Once `myne-qrypto` integration and/or `genesis-txt` multiplayer exist, graduate the cross-project parts into `%.harnesses/xo-human/` alongside this doc.

---

## 6. Reading list, in priority order, for whoever builds this

1. `014.wsr-pal💸️📌️+2/ops/corp_decide.c` — the decision_mode chassis itself, read in full.
2. `045.muchi-pal-agent🤖️+1/agent-summary-claude.txt` — real live-test report on the deterministic-keyword-before-LLM pattern.
3. `045.muchi-pal-agent🤖️+1/ops/gemma_strategy.c` + `ops/strategy_execute_a.c` — the actual keyword-detection + pre-execute code (not read in full this session — read before copying the pattern).
4. `045.muchi-pal-agent🤖️+1/test-harn-same/ops/tk_*.c` — the four reusable key-injection primitives.
5. Find and read `GAME-AI-SPEED-DOCTRINE.txt` (see §3 — location not confirmed this session).
6. `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` and `#.haiku+/HANDOFF_NEXT_SESSION.md` — for where `my-chara-txt` currently stands (P2-verified, farm/mine/store/inventory screens not yet built — see that handoff before wiring any AI decision-making into a screen that doesn't exist yet).

---

*This document is guidance/research, not a build plan — nothing described here has been implemented. If you build the `mychara_ai_decide.c`/`genesis_ai_decide.c` op described in §4, please update this file with what actually got built vs. what changed from this plan.*
