# ⚖️📜 MY-LAWYER — RESEARCH, WRITE A CASE, ARGUE IT BEFORE A GEMMA JUDGE

> **PURPOSE:** A `my-chara-txt`-shaped game (same CHTPM playstyle, same proven pattern) where NPCs (and the government) post lawsuits — to prosecute or defend. The player picks a side, chooses to **settle** or **go to court**, and if going to court, their character's own research AI **writes a real case document** (a plain-text file, sitting in the directory, growing turn by turn — the player can open and read it directly, it is never hidden state) by researching law/precedent via Gemma-LAN calls and deterministic tool calls. When both sides are ready, a **Gemma judge reads both real case documents** and picks a winner. Winning earns money; money can fund a **run for political office**; higher office grants a real, mechanical **bias** in both settlement negotiation and judging.
>
> **This is the third and most complex sibling in the family** — it reuses and extends real, working, LIVE-VERIFIED patterns from `my-biotech` (the corpus/research loop, the async background-worker fix) rather than inventing fresh. Read `@.apps/my-biotech/MY_BIOTECH_DESIGN.md` in full first, especially §2-4, plus its real implementation (`ops/mybiotech_research_worker.c` — the exact async pattern this design copies from day one, not retrofitted after a blocking mistake like `my-biotech` had to be).

---

## 📖 1. THE VISION

One character, one `main.chtpm`, turn-based CHTPM nav — same shape as every sibling. The actions:

- 📋 **Docket** — browse lawsuits NPCs (and the government) have posted, wanting a prosecutor or defender. Pick one up.
- ⚖️ **Case Screen** (per active case) — choose **Settle** (negotiate an immediate outcome, no research needed, lower payout but lower risk and instant) or **Go to Court** (requires building a real case first).
- 🔬 **Research** (same corpus-growing loop as `my-biotech`, themed to law instead of chemistry — laws, precedents, statutes) — grows the character's personal legal corpus, same IQABOD-style plain-text convention.
- ✍️ **Build Case** — THE key new mechanic (§3): the character's own research agent writes a REAL case document, one argument point at a time, via Gemma calls + deterministic tool calls (search corpus, search precedent, cite law), each addition visibly appended to a real file the player can open directly (`data/cases/<case_id>/<side>_case.txt`).
- 🧑‍⚖️ **Present to Judge** — once both sides are ready, a Gemma judge reads BOTH real case documents (not a summary, not internal state — the actual files) and picks a winner (§4).
- 🏛️ **Run for Office** — spend accumulated money to campaign for a political office. Higher offices grant a real bias in settlement leverage and judging outcomes (§5) — a deliberate, explicit corruption/influence mechanic, not hidden.
- 📦 **Inventory / Corpus / EndTurn** — same shape as every sibling.

---

## 🏗️ 2. REFERENCE SOURCES & GROUNDING (what's reused vs. genuinely new)

| Source | What we reuse |
|---|---|
| **`@.apps/my-biotech/MY_BIOTECH_DESIGN.md` + its real implementation** (built and live-verified same session) | The ENTIRE research loop: weighted-random topic selection, simple plain-text (never structured-JSON) Gemma prompts, `connect_op.+x`/`json_parser.+x` round trip, IQABOD-style corpus file convention, and — critically — the ASYNC background-worker pattern (`ops/mybiotech_research_worker.c`, PID-tracked, status-file-polled) that fixed `my-biotech`'s own synchronous-blocking mistake. `my-lawyer` builds this pattern in from day one — every Gemma-calling action (Research, Build Case, Present to Judge) is a background worker from the start, never a blocking call in `menu_input.c` directly. |
| **`%.harnesses/xo-human.md`** | The `decision_mode` chassis (`corp_decide.c`) — NPC opposing counsel uses this exact tiering (preset/weighted/llm) to decide how strong a case to build, same as any other automated entity in this house. |
| **`045.muchi-pal-agent🤖️+1`'s `gemma_strategy.c`/`strategy_execute_a.c`** | The REAL "deterministic tool detection before any LLM call" pattern — `my-lawyer`'s own tool calls (search corpus, search precedent, cite law) are plain deterministic dispatch (keyword/topic match → run the tool → fold result into context), never asking Gemma to emit a `TOOL:` format itself. Confirmed necessary: `gemma3:270m` can't reliably do that (same finding `my-biotech`'s design leaned on). |
| **`corp_decide.c::llm_choice()`** | The exact shape for the JUDGE call (§4) — a simple, keyword-extractable prompt with a hard fallback if the LLM response is unusable, not a structured-output requirement. |
| **Direct user correction, this session** | `gemma3:270m` (not a larger model) is expected to be fine for judging — outcome quality is meant to hinge on each side's own corpus/case competency and office-held bias, not raw judge-model size. `gemma3:1b` was pulled onto the LAN box and registered as an optional model in `045.muchi-pal-agent🤖️+1`'s registry (`gemma-lan-1b`) per direct instruction, but is NOT a hard dependency for `my-lawyer`'s judge — `gemma3:270m` (`GEMMA_LAN_URL`/`GEMMA_LAN_MODEL`, same constants `my-biotech` already uses) is the default. |

**What's genuinely NEW, no existing precedent to lean on:**
- The **case-document-as-real-file** mechanic (§3) — no sibling project writes a growing, player-visible document as its core artifact. Closest analogue is the corpus file itself, but a case document is structured (argument points, citations) rather than a flat fact list.
- The **judge-reads-two-real-files-and-picks-a-winner** mechanic (§4) — a genuinely new shape for a Gemma call (compare two documents, not classify one question).
- The **office/election ladder + bias mechanic** (§5) — entirely new design, no precedent anywhere in this house.

---

## ✍️ 3. BUILD CASE — THE CORE NEW MECHANIC (a real, player-visible, growing document)

### **Why a real file, not internal state**

Direct user requirement: *"the characters ai should literally beable to write a 'case' from doing gemma research + toolcalls; and player can even see the document in the directory."* This is a deliberate continuation of this whole project family's stated purpose (`my-chara-txt`'s own "sanity test, data-flow audit") — nothing about a character's reasoning process should be a black box. The case document IS the audit trail for "why did this side win or lose."

### **File location and format**

```
data/cases/<case_id>/
├── case_meta.txt          # plaintiff, defendant, claim summary, status
├── plaintiff_case.txt     # the plaintiff-side lawyer's real, growing document
└── defendant_case.txt     # the defendant-side lawyer's real, growing document
```

**`plaintiff_case.txt` / `defendant_case.txt` — plain text, human-readable, grows one argument point per Build Case turn:**
```
CASE FOR: State v. Adam Chen (Case #4)
SIDE: Plaintiff

[Argument 1] Precedent: Smith v. Jones (1987) established that
negligence requires a demonstrated duty of care. The defendant's own
prior corpus entry (researched turn 2) confirms this standard applies
to commercial contracts specifically.

[Argument 2] Statute: Commercial Code §14.3 requires written notice of
breach within 30 days. Evidence suggests defendant received notice on
turn 1 - see corpus entry "notice served march 3".

[Closing] The plaintiff has demonstrated both a clear duty of care and
timely notice under §14.3 - the defendant's own conduct satisfies every
element of the claim.
```

This is a REAL file. The player can navigate to a `Case Viewer` screen (or literally open the file outside the game) and read exactly what their character's research agent has argued so far, at any point mid-turn — not a summary, the actual document.

### **The FSM (per Build Case action, run as an async background worker from the start)**

```
SELECT_ANGLE
  → weighted-random pick of 1 unexplored angle relevant to this case's
    claim (same weighting heuristic as my-biotech §3 - untrained,
    simple recency/success scoring, NOT IQABOD's trained RL system)
  ↓
TOOL_CALL (deterministic dispatch, NO LLM involved in choosing which
           tool - matches gemma_strategy.c's own real precedent)
  → search_corpus <angle>: grep the character's own corpus file for
    prior relevant research (cheap, instant, no network)
  → if corpus has nothing usable: search_precedent <angle> - THIS one
    DOES call gemma-lan (simple prompt: "Name one plausible legal
    precedent case relevant to <angle>. Just the case name, one line."
    - same simple-prompt discipline as my-biotech's own research call,
    never asking for structured output)
  ↓
WRITE_ARGUMENT_POINT
  → format whatever the tool call returned into one "[Argument N]"
    paragraph (a plain string-template fill, NOT another gemma call -
    keep every step as cheap/deterministic as possible, reserve real
    LLM calls for the two genuinely open-ended steps: search_precedent
    and the final CLOSING call)
  → append directly to data/cases/<case_id>/<side>_case.txt (real file
    write, visible to the player immediately)
  ↓
(repeat SELECT_ANGLE -> WRITE_ARGUMENT_POINT for a tunable number of
 rounds - not decided how many, see §7 open questions)
  ↓
CLOSING (once the player marks the case ready, or a round cap is hit)
  → ONE more gemma call: "Here is a set of legal arguments: <the whole
    document so far>. Write one short closing paragraph that ties them
    together persuasively. Plain text, no markdown, 2-3 sentences."
  → appended as a "[Closing]" paragraph
  → case_meta.txt status updated to "ready"
```

**Async from day one** (learning directly from `my-biotech`'s own real mistake): `mybiotech_menu_input.c`'s original design blocked the whole PAL module for a single Gemma call; `my-lawyer`'s Build Case action does MULTIPLE Gemma calls in sequence (search_precedent + eventually closing) — blocking would be far worse here. `ops/mylawyer_case_worker.+x` runs the WHOLE FSM (however many rounds) as ONE detached background process, PID-tracked via `data/cases/<case_id>/<side>_worker.pid`, status polled via `data/cases/<case_id>/<side>_status.txt` (`running`, `current_step`, `rounds_done`) — `mylawyer_compose_frame.c` shows live progress ("⏳ Researching precedent for angle 'notice requirements'... round 2/4") the same way `my-biotech`'s fixed compose_frame shows "⏳ Researching...".

---

## 🧑‍⚖️ 4. THE JUDGE — READS TWO REAL FILES, PICKS A WINNER

> **⚠️ BUILD THIS RIGHT THE FIRST TIME — read `PITFALL 69` in `!.xyzos-pitfalls+1.txt` and `§42` in `!.xyzos-standards+1.txt` (and the fuller theory doc, `#.haiku+/!.gemma-judge-tomo&iqa.md`) BEFORE writing this code.** `my-biotech`'s own FDA_REVIEW mechanic originally asked `gemma3:270m` to DIRECTLY classify a verdict ("Answer APPROVED or REJECTED") and this was live-measured as genuinely unreliable — wrong 2/3 times on an obviously-lethal test case, even reframed, even with the "correct" option listed first, and producing ZERO real reasoning when asked to also explain itself. The fix (DESCRIBE, don't classify — ask gemma an open-ended comparison, then classify the real response text ourselves with a deterministic scorer) reached 6/6 correct on the SAME small model, no bigger model needed. `my-lawyer`'s own judge below is written with that fix already built in — do not implement the "A or B, answer with just one letter" version first and discover the same problem a second time.

**Once both sides mark their case "ready"** (or one side settles instead — see §6), the Present to Judge action:

1. Reads BOTH real files: `data/cases/<case_id>/plaintiff_case.txt` and `defendant_case.txt` (actual file contents, not a cached summary — if a player edited/re-ran research after marking ready, the judge sees whatever is really on disk at judgment time).
2. Builds an OPEN-ENDED COMPARE prompt — **never a direct "A or B" classify prompt** (same discipline as `my-biotech`'s own corrected FDA_REVIEW, §3 of that design doc):
   ```
   Here is Case A (Plaintiff):
   <plaintiff_case.txt content>

   Here is Case B (Defendant):
   <defendant_case.txt content>

   Compare the legal strength of these two cases in one or two sentences.
   ```
3. Run a small, auditable, hand-written keyword scorer over the REAL comparison text gemma actually wrote (not another LLM call, not a direct verdict word) — e.g. count occurrences of phrases favoring A ("plaintiff's case is stronger," "case A demonstrates," "more convincing on the plaintiff side") vs. phrases favoring B, similarly to `my-biotech`'s own `classify_description()` (`ops/mybiotech_fda_verdict.c`/`ops/mybiotech_research_worker.c` — read that real, working implementation directly as the template, this is the exact same pattern applied to a two-way comparison instead of a one-way safe/dangerous classification). **The exact keyword/scoring shape for a two-document COMPARISON (rather than a one-document CLASSIFICATION) is not yet designed in detail — this is real, scoped follow-up work, not a solved problem copied verbatim from my-biotech.** A live-tested starting point worth trying: score each document's own `[Argument N]` count and citation density as a cheap proxy alongside the keyword scorer, since `my-biotech`'s own finding was that gemma's real descriptive text tends to track genuine content quality even when its self-classification doesn't.
4. **Office-held bias applied AFTER the deterministic verdict, not baked into the prompt** (§5) — keeps the judge call itself honest/inspectable (the raw, unbiased verdict is always loggable before bias), with bias as an explicit, separate, auditable modifier step.
5. Real, tunable fallback if the deterministic scorer produces a genuine tie: a simple secondary tiebreak (e.g., whichever document is longer/has more `[Argument N]` points) — never crash, never leave the case stuck.
6. Result written to `case_meta.txt` (`status=judged`, `winner=plaintiff|defendant`, `raw_comparison_text=<gemma's real comparison>`, `raw_verdict=A|B`, `bias_applied=<office_bonus>`) and the game's own `data/master_ledger.txt` — the real comparison text is the player-visible "why," same transparency as `my-biotech`'s own `[FDA Verdict] APPROVED (<real description>)` line.

**Same context-length caution `my-biotech` already established:** gemma3:270m's own context window is bounded (32768 tokens per the earlier `curl /api/tags` listing's own `context_length` field — plenty of room for a few short case documents, but a case with dozens of rounds could eventually get large; not a P2 concern, worth remembering for later).

---

## 🏛️ 5. OFFICE & BIAS — GENUINELY NEW DESIGN, NOT YET DECIDED IN DETAIL

### **The office ladder (simple, tunable — exact costs/tiers not decided, see §7)**

```
Tier 0: (none)               - no bias
Tier 1: Local Attorney       - small settlement leverage bonus
Tier 2: District Attorney    - moderate settlement + judging bias
Tier 3: State Attorney General - larger bias
Tier 4: (higher office, TBD) - largest bias
```

Running for office costs money (accumulated from winning cases). A real, explicit "spend money to buy influence" loop — the game's own honest name for what it's modeling.

### **Settlement bias**

When a player at a held office negotiates a settlement (§6), their office tier applies a real, visible multiplier/bonus to the settlement terms they can demand or must concede — e.g., a Tier 2 DA can settle for less risk to themselves and extract more from the other side than a Tier 0 player could. Exact formula not decided (§7).

### **Judging bias**

Applied AFTER the raw Gemma verdict (§4 step 4) — e.g., a simple probability nudge: if the player holds Tier 2+ office and the raw verdict favored the OTHER side by a narrow margin (not a landslide — exact "narrow" threshold TBD), there's a real, logged chance the recorded outcome flips in the office-holder's favor. **This must be an honest, visible mechanic** (logged in `case_meta.txt` as `bias_applied`), not silently baked into the judge's own reasoning — the whole point of showing the real case documents is transparency; the bias step should be equally transparent, not hidden inside the LLM call itself.

---

## 🤝 6. SETTLE VS. GO TO COURT

**Settle:** no research, no case-building, no judge call. An instant negotiation — resolved by a simple formula (case strength estimate from whatever corpus/office bias exists) rather than a real LLM judgment. Faster, lower variance, appropriate for a case the player doesn't want to invest turns in.

**Go to Court:** the full §3/§4 loop. Slower (multiple real Gemma calls, async background work spanning several turns), higher variance, higher potential payout.

**Open question (§7):** should NPCs on the OTHER side of a case ever refuse to settle / demand to go to court regardless of the player's preference? (Real litigation dynamics — not every case is settleable.) Not decided.

---

## 🗂️ 7. OPEN QUESTIONS (ask the user before building past groundwork)

1. **How many Build Case rounds** before a case is capped at "ready" (or can the player choose to stop early with fewer, weaker arguments)? Not decided.
2. **Settlement formula exact shape** — corpus size? office bias only? some blend? Not decided.
3. **Judging bias exact thresholds/probabilities** — what counts as "narrow margin," what's the flip probability at each office tier? Not decided.
4. **NPC opposing counsel** — always automated (decision_mode chassis), or can two human players face off in a `genesis-txt`-style multiplayer mode later? Not decided, but the architecture (real files, async workers) doesn't preclude it.
5. **Can a player abandon/lose a case they picked up** (bad research, ran out of turns) — what's the penalty, if any? Not decided.
6. **Precedent case realism** — `search_precedent`'s Gemma calls will invent PLAUSIBLE-SOUNDING but not necessarily REAL case names/citations (a 270M model has no reliable factual recall). Is this acceptable as "the character's own in-universe legal reasoning, not meant to be real-world-accurate" (matches `my-biotech`'s own compounds being fictional-but-plausible), or does this need a disclaimer/framing device in the UI? Leaning toward "acceptable, same framing as my-biotech" but not explicitly confirmed with the user yet.

---

## 🏁 8. TL;DR — THE 30-SECOND VERSION

- **my-lawyer is `my-biotech`'s legal-research sibling**: same corpus-growing research loop, same async-worker Gemma pattern (built in from day one this time, not retrofitted).
- **The real new idea:** a character's research agent writes a REAL, growing case document via Gemma calls + deterministic tool calls (search corpus, search precedent) — the player can open and read this file directly, at any time, as it's built.
- **Judge = `gemma3:270m` by default** (confirmed sufficient by direct user correction — outcome hinges on case quality + office bias, not judge-model size). Reads BOTH real case files, picks A or B with a simple, keyword-extractable prompt.
- **Office/bias is a new, explicit, LOGGED mechanic** — money buys political office, office buys real leverage in settlements and a real (transparent, auditable) thumb on the judging scale.
- **`gemma3:1b` was pulled onto the LAN box and registered** as an optional model in `045.muchi-pal-agent🤖️+1` (`gemma-lan-1b`) per direct instruction — available if wanted, not a hard dependency here.

⚖️ Ready to build a case? 📜
