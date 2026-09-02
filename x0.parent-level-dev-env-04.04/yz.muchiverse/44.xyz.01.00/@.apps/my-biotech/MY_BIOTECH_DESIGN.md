# 🧪🔬 MY-BIOTECH — RESEARCH, DISCOVER, AND SELL COMPOUNDS (LLM-DRIVEN)

> **PURPOSE:** A `my-chara-txt`-shaped game (same CHTPM playstyle, same proven pattern — see `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` §4) where the domain is **chemistry research** instead of farming or mining. The player **buys chemical elements**, **researches compounds** by querying a real local LLM (`gemma3:270m` via LAN Ollama), **accumulates a personal corpus** of discovered facts, and **sells breakthrough compounds** (drugs, pesticides, foods, tools — mostly the first two) on a market.
>
> **Complexity note (stated honestly, per direct user request):** this is the most complex of the `my-chara-txt` sibling family so far — it's the first one where the CORE gameplay loop, not a rare optional flourish, is a real LLM call per turn. Read `%.harnesses/xo-human.md` in full before touching this doc's FSM/decision-mode sections — it documents the real, proven precedent (the `decision_mode` chassis, the "deterministic-before-LLM" doctrine, the corpus-file convention) this design is built on, not invented fresh.

---

## 📖 1. THE VISION

One character, one `main.chtpm`, turn-based — same shape as every sibling in this family. The actions:

- 🧪 **Research** — spend a turn attempting a discovery. The character's own "research agent" (FSM-driven, weighted-random over the player's corpus + owned elements) constructs a prompt, queries `gemma-lan`, and — if the response is usable — writes a REAL, growing `dossier.txt` document (§3/§5) recording the compound's name, ingredients, use case, effect, side effect, and market price. This is how a "breakthrough" happens — and the document is real, sitting in the directory, readable at any time.
- 🏛️ **FDA Review** (new, §3) — once a dossier has enough sections, a Gemma "regulator" reads the REAL document and renders APPROVED or REJECTED, appended to the dossier itself as its own final line — same "judge reads a real document" pattern as the sibling `my-lawyer` game.
- 🛒 **Store** — buy chemical elements (the raw ingredients research consumes and compounds are made of). Same shape as `my-chara-txt`'s Store, themed to a periodic-table-style inventory instead of seeds.
- 💱 **Market** — sell discovered compounds. APPROVED compounds sell at full listed price; REJECTED ones sell at a black-market discount instead of being unsellable (§3, §9). Same shape as `genesis-txt`'s exchange design (or the simpler NPC-fixed-price model from `my-chara-txt`'s original Store — not yet decided, see §9).
- 📚 **Corpus** — view the player's own accumulated raw-text knowledge (every fact learned from every Research action, successful or not — even failed/inconclusive research turns can still teach the corpus something small).
- 📦 **Inventory** — owned elements + discovered compounds not yet sold.
- ⏳ **EndTurn**.

**NPCs run the identical research loop, automated** — same FSM, same weighted-random corpus selection, same real Gemma calls, just with no human waiting on the result. This is a direct extension of `%.harnesses/xo-human.md`'s `decision_mode` chassis (§2 below).

---

## 🏗️ 2. REFERENCE SOURCES & GROUNDING (read before designing further — nothing below is invented without a real anchor)

| Source | What we ground on it |
|---|---|
| **`%.harnesses/xo-human.md`** (written this same session) | The `decision_mode` chassis (`014.wsr-pal💸️📌️+2/ops/corp_decide.c`) — preset/weighted/rl/llm/human tiers, human-park-and-wait mechanic. My-biotech's own research agent (player's character AND every NPC) uses this exact chassis, with `llm` promoted from "rare judgment call" to "the actual core loop" (a real, explicit deviation from the doctrine's default — see §3's speed discussion for why this is still safe here). |
| **`045.muchi-pal-agent🤖️+1`'s own `gemma_strategy.c`/`strategy_execute_a.c`** (its own live-test report, `agent-summary-claude.txt`) | **Gemma 3 270M cannot reliably follow a structured format** ("too small to reliably follow a TOOL: format"). My-biotech's own Gemma prompts are deliberately simple, one-fact-at-a-time, plain-text — never "return JSON with these 6 fields," which would very likely produce malformed output from a 268M model. See §4. |
| **`014.wsr-pal💸️📌️+2/ops/connect_op.c` + `ops/json_parser.c`** (real, proven, confirmed reachable this session — `curl http://10.0.0.144:11434/api/tags` returned a real model list including `gemma3:270m`) | The REAL HTTP+JSON mechanism every Gemma call in this house already uses. `connect_op.+x <url> <request.json> <response.json>` (curl POST, 600s timeout — this LAN box's inference is slow/variable, up to ~118s observed for one completion). `json_parser.+x <file> <dot.path>` (handles ` ```json ` markdown-fenced responses too, a real Gemma habit). Reused as-is, not reinvented. |
| **`045.muchi-pal-agent🤖️+1/ROADMAP-models.txt` §11.0/§11.1** (IQABOD's real corpus pipeline, read directly — `main_orchestrator.c:160-320`) | **The real corpus file format in this house: `corpuses/<name>.txt`, RAW plain text, one short simple sentence per line, no special structure.** My-biotech's player/NPC corpus files follow this exact convention — not a database, not JSON, just appended plain-text lines. Real example already on disk elsewhere in this house: `corpuses/test_corpus.txt` is literally 5 short sentences, nothing more. |
| **`@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` §4 + its now-built, live-tested implementation** | THE CHTPM game-shell pattern (proven 3x: wsr-pal, muchi-pals, pal-chain; proven a 4th time live in `my-chara-txt` itself) — `.chtpm` screens, `piece.pdl` METHOD dispatch, per-screen PAL module, shared `*_menu_input`/`*_compose_frame` op pair. My-biotech's own game shell is a direct copy of this, same as `myne-qrypto`'s corrected design. |
| **`@.apps/myne-qrypto/MYNE_QRYPTO_DESIGN.md`** (corrected 2026-08-02, same session) | The "game shell vs. backend engine" split — my-biotech doesn't have a separate reusable backend engine to speak of (there's no `041.pal-chain`-equivalent for "chemistry research"), but the SAME principle applies to keeping the LLM-calling ops (`biotech_research.c` or similar) cleanly separated from the CHTPM dispatch/render pair, so the research mechanism is independently testable. |

**IMPORTANT — what NOT to reuse:** IQABOD's own trainable-embedding curriculum/RL system (`main_orchestrator.+x vocab_only`/`train`, `feedback_tx_module.c`, the `rl_curricula/<name>/` bank) is a genuinely separate, much heavier system (real neural weight training against tokenized corpora) — it is NOT what "weighted random selection from RL" means in this design. My-biotech's own "RL" is a **lightweight, non-trained weighted-random selection** (pick which corpus facts / which chemical elements to build this turn's prompt around, weighted by a simple score like recency or past-success-rate) — a scoring/selection heuristic, not a trained model. If a future session wants to genuinely train a learned policy for this, that would be a real, separate, much bigger undertaking building on IQABOD's actual ML pipeline — flagged here explicitly so nobody conflates the two.

---

## ⚗️ 3. THE RESEARCH LOOP — FSM + WEIGHTED SELECTION + REAL LLM CALL

### **Why LLM-as-core-loop is safe here, despite the doctrine's default-off stance**

`xo-human.md` §3 documents the real speed doctrine: LLM calls (~935ms typical, up to ~118s observed on this LAN box for one completion) must never be the routine per-tick path across MANY entities. My-biotech's own Research action:
- Is player-turn-gated (once per turn, not per-tick) — a turn-based game waiting a few seconds (or, worst case, ~2 minutes) for a research result is acceptable UX, unlike a real-time per-tick simulation.
- For NPCs: **this is the one place this design deliberately risks violating the doctrine, and it's flagged as a real open question, not silently ignored** — if `genesis-txt`-style multiplayer with several AI-controlled researchers each doing this every round, wall-clock cost adds up fast (4 NPCs × ~30s average = 2 minutes per round, worst case far longer). See §9 for the real mitigation options (batch/stagger NPC research calls, cap NPC LLM-tier usage, fall back non-LLM tiers for NPCs by default and reserve real LLM calls for the human player's own turn).

### **The FSM (per Research action)**

```
GATHER_INFO
  → weighted-random pick of 1-3 known chemical elements (owned or in corpus)
    + 1 random "domain hint" (drug / pesticide / food / tool - weighted
    toward drug/pesticide per the vision's own "mostly drugs, pesticides"
    framing)
  ↓
HYPOTHESIZE
  → build a SIMPLE, plain-text prompt (NOT asking for structured JSON -
    see §2's Gemma-270M finding): e.g. "Name one real chemical compound
    that could plausibly be made from: sulfur, nitrogen. It should be a
    pesticide or drug. Just the name, one line."
  ↓
QUERY_GEMMA (real connect_op.+x + json_parser.+x call, persona-driven)
  → if the response looks usable (non-empty, no obvious refusal/gibberish
    - a cheap heuristic check, not a strict parser), proceed
  → if not, this turn still succeeds partially: append what WAS learned
    (even "gemma had no answer for sulfur+nitrogen" is a real, if
    low-value, corpus fact) and the player gets a "no breakthrough this
    turn" message, not a hard failure
  ↓
ENRICH — **writes a REAL, growing, player-visible document, same pattern
         as `@.apps/my-lawyer/MY_LAWYER_DESIGN.md` §3's case documents**
         (direct user correction, 2026-08-02: "the biotech research
         should operate much the same way" - my-biotech's own research
         is NOT just flat corpus lines, it's a real per-compound
         dossier file the player can open and watch grow):
  data/research/<compound_name>/dossier.txt - one section appended per
  ENRICH sub-step, each its own SEPARATE simple Gemma call (never all
  fields in one structured request - a 270M model can't do that
  reliably, see §2):
  1. "What is <compound>'s primary use case? One short sentence."
     -> appended as "[Use Case] <answer>"
  2. "What is <compound>'s primary effect? One short sentence."
     -> appended as "[Effect] <answer>"
  3. "What is a known side effect of <compound>? One short sentence."
     -> appended as "[Side Effect] <answer>"
  4. "Estimate a plausible market price in dollars for one unit of
      <compound>. Just a number."
     -> appended as "[Market Price] <answer>"
  Each sub-step is a REAL file write, visible to the player immediately
  (same "open the file mid-research, watch it grow" transparency as
  my-lawyer's case documents) - not buffered internally and dumped once
  at the end. The player's own general corpus (data/corpus/player.txt)
  ALSO gets a one-line summary per sub-step (IQABOD-style short fact),
  so both artifacts exist: the flat corpus (quick lookup / weighting
  input for future SELECT_ANGLE-style choices) and the structured
  per-compound dossier (the real, detailed research record).
  ↓
RECORD
  → if enough sections came back usably (a real, tunable threshold - see
    §9), a NEW COMPOUND is recorded: name, ingredients, use, effect,
    side_effect, price, discovered_by, discovered_turn
  → appended to data/master_ledger.txt (game ledger) AND
    data/discovered_compounds.txt (the compound catalog, readable by
    the Market screen) - both DERIVED from the real dossier.txt, not a
    separate source of truth
  ↓
FDA_REVIEW — NEW, direct user request 2026-08-02 ("maybe we should have
             a judge to judge if the compound is correct? like fda or
             something"). Same shape as my-lawyer's own judge (§4 of
             that design doc): a Gemma call reads the REAL dossier.txt
             file (not a summary) and renders a verdict. Unifies both
             sibling games around the same "write a document -> Gemma
             judge reads the real document -> real consequence" pattern.
  → ⚠️ CORRECTED SAME SESSION, real finding, read PITFALL 69 in
    !.xyzos-pitfalls+1.txt / §42 in !.xyzos-standards+1.txt in full:
    asking gemma3:270m to DIRECTLY classify ("Answer APPROVED or
    REJECTED") was live-measured as unreliable - wrong 2/3 times on an
    obviously-lethal test compound, even reframed as SAFE/DANGEROUS,
    even with the danger option listed first (ruling out order bias),
    and producing ZERO real reasoning when asked to also explain
    itself. THE FIX, now the real implementation: DESCRIBE, don't
    classify. Ask gemma an open-ended question (what it's demonstrably
    good at - the same model gave real, accurate, content-aware
    descriptions 6/6 times on the identical test cases), then classify
    the REAL response text ourselves with a small, auditable,
    hand-written keyword scorer (`classify_description()` in both
    `ops/mybiotech_fda_verdict.c` and `ops/mybiotech_research_worker.c`)
    - never another LLM self-judgment. Live-verified 6/6 correct on
    gemma3:270m with this approach - no need for a bigger model
    (`gemma3:1b`, pulled onto the LAN box this session specifically to
    test as a fallback, also worked but proved unnecessary).
  → real prompt actually used: "Here is a research dossier for a
    proposed compound:\n<dossier.txt content>\nDescribe the safety
    concerns of this compound in one sentence."
  → OUR OWN deterministic scorer (not json_parser.+x keyword-matching
    gemma's own verdict word - gemma is never asked for a verdict word
    at all now) counts danger-words (toxic/fatal/lethal/dangerous/
    deadly/poison/banned/severe/risk/harm/hazard/death/unsafe/cardiac
    arrest/no antidote/weapon/carcinogen/overdose) vs. safe-words
    (safe/mild/beneficial/well-tolerated/none known/rare/generally
    safe/low risk/minimal) in gemma's real description text;
    safe_score > danger_score -> APPROVED, else (including ties) ->
    REJECTED (the honest, conservative default on inconclusive input)
  → verdict AND gemma's own real description appended to dossier.txt
    itself as a final "[FDA Verdict] APPROVED (<real description>)"
    line (same file, same transparency - the player reads the actual
    regulator reasoning, not a fabricated one) AND to
    discovered_compounds.txt as an `approval_status` field
  → REAL CONSEQUENCE (§6, Market): APPROVED compounds sell at full
    listed price through the normal Market screen. REJECTED compounds
    can still be sold, but only at a black-market discount (exact
    discount not decided - see §9) - a thematically honest fit for the
    game's own "mostly drugs, pesticides" framing (§1): an unapproved
    pesticide/drug is still sellable, just riskier/cheaper, not simply
    unusable
```

**Compounds are REAL chemistry, not fictional-but-plausible** (direct user correction, 2026-08-02: "the biotechs compounds needn't be fictional at all, gemma has plenty of knowledge of chemical compounds"). Earlier drafts of this design (and the sibling `my-lawyer` doc's own §7 open question about precedent-case realism) assumed a small local model would mostly hallucinate plausible-sounding-but-fake answers. For real chemical compounds/elements specifically, `gemma3:270m` has genuine training-data knowledge (chemistry is well-represented, unlike invented legal case citations) - so a discovered compound should be treated as the model's real, if imperfect, chemistry knowledge, not deliberately-fictional flavor text. This does NOT change any of the mechanism above (still simple plain-text prompts, still no structured JSON, still async/backgrounded) - only the FRAMING of what a discovery represents to the player.

### **Weighted-random selection (the "RL" the user asked for — lightweight, not trained)**

Each known chemical element (owned, or merely mentioned in the corpus from a past research attempt) gets a simple integer weight:
- `+1` weight per turn it sits unused (encourages eventually trying everything at least once — an epsilon-greedy-style exploration bias, not a learned policy)
- `+3` weight if it was part of a PAST successful compound discovery (encourages building on what worked — a simple positive-feedback heuristic, not backpropagation)
- `-2` weight (floored at a minimum, never fully excluded) each time it's picked, so the same 2 elements don't get picked every single turn

Pick 1-3 elements probabilistically weighted by these scores (same shape as `corp_decide.c`'s own `preset_choice()`/`decide_from_value()` — simple, deterministic-given-a-seed, auditable arithmetic, not a black box). This is intentionally simple and fully explainable — a real design choice given the emphasis this whole project family places on auditability (`my-chara-txt`'s own stated purpose is "sanity test, data-flow audit").

---

## 🧬 4. THE REAL GEMMA CALL (exact mechanism, grounded in §2's real precedent)

**Persona file** (`pieces/registry/personas/biotech_researcher.txt`, plain text, same convention as `corp_decide.c`'s `decide_trade.txt`):
```
You are a chemistry research assistant in a game. Give short, direct,
plain-text answers - one sentence or a single name/number, never a list,
never markdown, never JSON. If you don't know, say "unknown" plainly.
```

**Request construction** (same shape as `corp_decide.c::llm_choice()` — read that function directly before implementing):
```c
// build JSON request body -> temp file
// {"model":"gemma3:270m","stream":false,"messages":[
//   {"role":"system","content":"<persona text, escaped>"},
//   {"role":"user","content":"<this turn's simple question>"}
// ]}
system("'<root>/ops/+x/connect_op.+x' 'http://10.0.0.144:11434/api/chat' '<request.json>' '<response.json>'");
// json_parser.+x <response.json> 'message.content' -> the plain-text answer
```

**Confirmed reachable this session** — `curl http://10.0.0.144:11434/api/tags` returned a real model list including `gemma3:270m`. This means the P2 checkpoint for my-biotech CAN be a genuinely live-tested real LLM call, not a mocked/simulated one, unlike some of this session's earlier work where network reachability wasn't assumed.

**LAN address is NOT guaranteed stable** — `corp_decide.c` hardcodes `10.0.0.144`; verify this is still current before relying on it in a future session (a quick `curl --max-time 3 <url>/api/tags` check, same as this session did, is the right verification step — don't assume it's still reachable without checking).

---

## 🗂️ 5. DATA STRUCTURES

### `pieces/system/config.txt` — player state (same shape as `my-chara-txt`'s own)
```
game_id=my-biotech-001
player_name=Adam
day=1
max_days=10
health=100
money=500
game_state=playing
```

### `pieces/system/elements.txt` — owned chemical elements + weights
```
element|owned_count|weight
sulfur|3|4
nitrogen|1|6
carbon|5|2
```

### `data/corpus/player.txt` — IQABOD-style raw text corpus, one short fact per line (quick-lookup summary, NOT the detailed record — see dossier below)
```
sulfur and nitrogen can form thiodiazole
thiodiazole is used for crop pest control
thiodiazole causes mild skin irritation
gemma had no answer for carbon+oxygen
```

### `data/research/<compound_name>/dossier.txt` — the REAL, detailed, player-visible research document (§3 ENRICH — same pattern as `my-lawyer`'s case documents)
```
COMPOUND: thiodiazole
DISCOVERED FROM: sulfur, nitrogen

[Use Case] Used as a pest control agent in agricultural settings.
[Effect] Kills insects by disrupting their nervous system.
[Side Effect] Can cause mild skin irritation on contact.
[Market Price] Approximately $45 per unit.
[FDA Verdict] APPROVED
```

### `data/discovered_compounds.txt` — the compound catalog (pipe-delimited, append-only, DERIVED from each compound's own dossier.txt)
```
name|ingredients|use_case|effect|side_effect|price|approval_status|discovered_by|turn
thiodiazole|sulfur,nitrogen|crop pest control|kills insects|mild skin irritation|45|APPROVED|Adam|3
```

### `data/master_ledger.txt` — the game's own event ledger (same convention as every sibling)
```
timestamp|day|action_type|details
2026-08-02T10:00:00|3|research_attempt|elements:sulfur,nitrogen
2026-08-02T10:00:45|3|compound_discovered|thiodiazole
2026-08-02T10:01:00|3|corpus_append|4_lines
```

---

## 🖥️ 6. SCREENS (same CHTPM pattern as every sibling — `.chtpm` + `piece.pdl` + shared menu_input/compose_frame)

| Screen | Shows | piece.pdl METHOD rows |
|---|---|---|
| `main.chtpm` | Day/health/money summary | Research / Store / Market / Corpus / Inventory / EndTurn |
| `research.chtpm` | Live FSM state during a Research action ("Gathering info on sulfur, nitrogen..." → "Querying gemma-lan..." → result) | (mostly auto-advancing via the PAL module's own tick, not player-driven menu picks, once started) |
| `store.chtpm` | List of purchasable elements + prices | `BUY:<element>` per row |
| `market.chtpm` | List of discovered compounds + sell action | `SELL:<compound_name>` per row |
| `corpus.chtpm` | Scrollable view of the player's own corpus file (read-only) | (paging methods if the corpus grows long) |
| `dossier.chtpm` | Scrollable view of a SPECIFIC compound's `dossier.txt` (read-only) — reached from the Market/Research result screen by picking a compound | (paging methods if a dossier grows long) |

---

## 🚀 7. BUILD ORDER

| Phase | What we build | Status | Verified by |
|---|---|---|---|
| **P1** 🥇 | Skeleton: reuse `my-chara-txt`'s `system/`, `button.sh`, `main.chtpm` shape, one real screen | ✅ **DONE** | real `button.sh run` shows main.chtpm |
| **P2** 🧪 | Research action, SINGLE simple Gemma call (just the "name a compound" step), real connect_op+json_parser, corpus append | ✅ **DONE, LIVE-VERIFIED, ASYNC-FIXED** | real `gemma3:270m` round-trip via LAN Ollama (confirmed reachable, `http://10.0.0.144:11434`), corpus file grows with a real line, `test-harn-same/demo_research_and_end_turn.sh` 7/7 PASS. Real observed response quality is weak ("chlorine can be involved in Chlorine" — the model often just echoes the element name back) — expected given the model is only 270M params, not a P3 blocker, just a real quality ceiling worth knowing about before investing in the fancier ENRICH multi-step flow. Real bug found+fixed along the way: `projects/my-biotech/pieces/mybiotech_menu/` didn't exist, so `last_message` writes silently failed (fopen("w") on a missing parent dir just fails quietly) even though the gemma call/corpus-append/ledger-append all genuinely worked — `button.sh run` now `mkdir -p`'s that directory defensively. **2026-08-02: the original synchronous-blocking design is now FIXED** — `mybiotech_research_worker.+x` (new file) now runs the real Gemma call as a detached, PID-tracked background process (same pattern as `chain_miner.+x`), and `mybiotech_compose_frame.c` polls `pieces/system/research_status.txt` to show live "⏳ Researching... (Ns elapsed)" progress. Live-verified: firing Research then End Turn with ZERO delay between them, both completed correctly within the same second — the module never blocked. |
| **P3** 🔬 | Full FSM: multi-step ENRICH calls WRITING TO A REAL PER-COMPOUND `dossier.txt`, compound recording derived from the dossier, `discovered_compounds.txt`, FDA_REVIEW | ✅ **DONE, LIVE-VERIFIED** (2026-08-02). `mybiotech_research_worker.c` now runs the full FSM: hypothesize → 4 separate enrich calls (use_case/effect/side_effect/price, each appended as `[Section]` to a real `data/research/<compound>/dossier.txt`) → FDA_REVIEW (Gemma reads the real dossier, renders APPROVED/REJECTED, appended to the dossier itself) → record (`discovered_compounds.txt`, simplified to `name|element|approval_status|turn` rather than the fuller per-field schema originally sketched in §5 — the free-text use_case/effect/side_effect sentences live in the real dossier, not force-fit into pipe-delimited fields, avoiding an escaping/data-integrity risk). Live-tested end to end (`test-harn-same/demo_research_and_end_turn.sh`, 11/11 PASS): real dossier created, real FDA verdict appended, real catalog + ledger entries. **Two real bugs found and fixed along the way:** (1) `research_status.txt`/`research.pid` originally lived under `pieces/system/` (session-scoped, ephemeral) — if a player quit while research was in flight, those files got deleted with the session even though the background worker kept running, orphaning it and breaking the double-launch guard for the next session; moved to `data/` (symlinked to the real, persistent project root) and live-verified the fix by killing a session mid-research and confirming the worker still completed correctly. (2) The test harness's own readiness poll only checked that `current_frame.txt` existed, which could race and grab `chtpm_parser_pal`'s placeholder `[Map Loading...]` frame before the module's first real render — fixed to wait for actual rendered content (`grep -q "Corpus:"`). |
| **P4** 🛒 | Store: buy elements, weight tracking | elements.txt updates, weights shift after picks |
| **P5** 💱 | Market: sell discovered compounds | money increases, compound removed from unsold inventory |
| **P6** 📚 | Corpus screen: read-only paged view | corpus displays correctly, scrolls |
| **P7** 🤖 | NPC research agents (decision_mode chassis, §3's speed mitigation applied) | NPCs discover compounds autonomously, without blocking human turns excessively |
| **P8** 📊 | Audit: replay ledger + corpus, cross-check discovered_compounds.txt | audit passes |

---

## 🤔 9. OPEN QUESTIONS (ask the user before building past P2)

1. **NPC LLM-call cost mitigation** (§3): stagger NPC research across multiple game-ticks instead of all-at-once? Cap NPCs to `weighted`-tier only, reserve real `llm`-tier for the human player? Some hybrid? **Not decided.**
2. **"Enough fields to record a compound" threshold** (§3 RECORD step): all 4 enrich calls must succeed, or is a partial record (name + only 1-2 fields) still worth recording as a "preliminary" discovery? **Not decided.**
3. **Market pricing model**: NPC-fixed-price (simple, like `my-chara-txt`'s original Store) or real player-to-player order matching (like `genesis-txt`'s exchange)? **Not decided — my-biotech is currently designed single-player-first, so NPC-fixed-price is the more natural P1 fit, but flagging since the family's other multiplayer sibling (`genesis-txt`) uses order matching.**
4. **Duplicate discoveries**: if two different research attempts (player + NPC, or two separate turns) land on the same compound name, is that a real conflict to resolve (first-discoverer gets credit?) or are duplicates just allowed or silently merged? **Not decided.**
5. **Corpus size/pruning**: does the corpus grow forever, or is there a cap/summarization step once it gets large (relevant for prompt-length costs on later research turns, if past corpus entries ever get fed back into future prompts as context — not yet decided whether they should be)? **Not decided.**
6. **Black-market discount for REJECTED compounds** (§3 FDA_REVIEW, new): what's the actual discount vs. an APPROVED sale — flat percentage, or scaled by how "dangerous" the side effect sounds? Any risk mechanic (chance of a REJECTED sale getting the player fined/caught) or purely a price discount with no other consequence? **Not decided.**
7. **FDA judge bias/tiering**: should a player's own reputation (e.g., track record of past APPROVED compounds) ever bias future FDA_REVIEW calls, similar to how `my-lawyer`'s office-held bias affects its own judge? Or should the FDA judge stay a neutral, unbiased read of the dossier every time, in deliberate contrast to `my-lawyer`'s explicitly-biased judge? **Not decided — worth a real design conversation, since the two sibling judges currently differ on this axis (my-lawyer: bias is a named feature; my-biotech FDA: not yet decided either way).**

---

## 🏁 10. TL;DR — THE 30-SECOND VERSION

- **my-biotech is `my-chara-txt`'s chemistry-research sibling**: same CHTPM playstyle, different domain.
- **Research** = FSM (gather → hypothesize → query gemma → enrich → record → FDA review), weighted-random (untrained, simple heuristic) element selection, REAL `gemma3:270m` calls via the house's proven `connect_op.+x`/`json_parser.+x` mechanism — confirmed reachable this session.
- **Research writes a REAL, growing `dossier.txt` document per compound** (not just flat corpus lines) — same "player can open and watch it build" transparency as the sibling `my-lawyer` game's case documents.
- **Compounds are REAL chemistry** (gemma3:270m has genuine chemistry knowledge) — not deliberately-fictional flavor text.
- **An FDA-style Gemma judge reads the real dossier and renders APPROVED/REJECTED** — same "judge reads a real document" pattern as `my-lawyer`'s own judge. Real consequence: REJECTED compounds sell at a black-market discount instead of being unsellable.
- **Gemma 3 270M can't do structured JSON reliably** (house-confirmed finding) — every prompt is simple, one-fact-at-a-time, plain text, never "give me 6 fields as JSON."
- **Corpus = IQABOD-style raw text file**, one short fact per line, grows with every research attempt (successful or not) — a quick-lookup summary alongside each compound's own detailed dossier.
- **NPCs run the identical loop** via the `decision_mode` chassis, with a real, flagged, NOT-yet-solved cost concern about running real LLM calls for multiple NPCs every round.
- **Market/Store follow the same patterns as `my-chara-txt`/`genesis-txt`** — buy raw materials, sell what you make.

🧪 Ready to discover something? 🔬
