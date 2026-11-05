# AI roadmap: micro-AI, delegation, in-house models and data (answers to the owner, 2026-11-05)

Labels: **SAID** owner 2026-11-05 ("ai is the entire goal, automation etc."); **READ** in a named doc; **DOC** my reasoning, not run. This doc **collects and orders** what the house already designed; it adds little new design on purpose (design-choice test: one authority per fact).

## 1. Is there a roadmap? Yes, in pieces; this is the one page that orders them
| Piece | Where (READ) | State |
|---|---|---|
| North star: the **learning flywheel**; micro-task contract `TASK \| id \| family \| input \| output \| judge \| budget \| min_grade`; router `local student -> cheap free API -> mid -> strong -> claude`; worker grade = Laplace (reward+1)/(reward+punish+2); graduation KPI | `16.game/VISION-CLARIFIED-2026-10-27.md` section 8; `08-roadmap/design-docs/DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` | designed; a first pilot (q001) passed; exam verbs (`chem_exam.pal`, 21/0) built |
| Harness as the judge, store and scoring | `HARNESS-DELEGATION-PIPELINE.md`, `HARNESS-STORE-AND-SCORING-DESIGN.md`, `HARNESS-BEHAVIOR-BANK-DESIGN.md`, `17.ai/HARNESS-AS-GAME-BRAIN.md` | judge built; **no caller** |
| API registry and caps (never burn the free quota) | `17.ai/API-REGISTRY-AND-CAPS.md` | exists |
| h-ai relay / lab (Harnecient) | `HARNECIENT-H-AI-RELAY.md`, `H-AI-LAB-DESIGN.md` | built in part |
| Describe -> score -> store loop (LLMUD) | `LLMUD-HACK.md`, `17.ai/AI.md` | designing |
| Attrition model (owner definition 2026-10): TEARIT + bank + tomom + FSM/GOAP; Watch -> DESCRIBE (fixed line) -> validator -> human review; promotion undesigned | memory `attrition-model-definition`; `ROBOT-CHAT-BLUEPRINT.md` | designing |
| tomom (hand-built in-house LLM pipeline, dormant) | `#.Z.HUMAN_LLM/3.stage.llm.tomom...` (outside this tree; found 2026-10) | exists, dormant |
| Roadmap with milestones | `16.game/ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md` | exists |

## 2. Does the meta AI (tomom / attrition / Tear-it training) belong in this roadmap? Yes, as the spine
Everything in the TSOTS/Elo roadmap that needs a judgement is a micro-task in this system: **rated bots** (M2) are routed micro-tasks graded by Elo; **vehicle design proposals** are tasks judged by `check_vehicle` and a smoke drive; **gym lanes** are tasks with scored targets; **event-page writing** is a task judged by a referee harness. So the AI track is not a side project: every milestone below adds a **judge**, and a judge is what lets a cheap model be tried safely.

## 3. How to make the most of micro-AI and minimize tokens (DOC; ordered by payoff)
1. **Rules before models.** A deterministic check or table answers many "AI" questions for free; the model only sees what rules cannot decide. (House rule already: models pick inside a fixed shape.)
2. **Fixed-shape, one-line outputs.** A task whose answer is one enumerated line costs tens of tokens and is machine-judged. Free-text answers are the expensive, unjudgeable ones.
3. **Router with a graded ladder; start at the cheapest tier** (local Gemma on the Mac, then free APIs, then a stronger model) and escalate only on a judged failure; record every run as a `DATA` row. Never loop to a rate limit.
4. **Small, packed context.** Give the model the one catalog row or the one state file, not the book. Generated "context packs" per task family (the zoo/museum generators in `GYMS-ZOOS-MUSEUMS...` produce exactly these); use prompt caching for the fixed prefix on APIs that support it.
5. **Cache and dedupe.** Identical input -> stored output (the dataset is also a cache); canonicalize inputs so near-duplicates hit.
6. **Distill from verified rows.** Only rows that passed a locked judge become training examples; train the smallest model that holds the grade for that family; the router retires the expensive tier when the cheap one keeps its grade over N rows (**graduation**). KPI: share of tasks served in-house per family, API calls per family trending to zero.
7. **Batch and schedule heavy work** (training, big evals) through `cpu_gate` and offload GPU work to the Mac/debil, not this weak machine.
8. **Measure tokens per task family** in the `DATA` row (tokens in/out, tier, verdict) so "minimal tokens" is an observed number, not a belief.

## 4. Proprietary in-house data, models and learnings (DOC)
- **The dataset is the asset.** Append-only `DATA` rows (`task|input|output|verdict|tier|lane|ts`), verified rows only, compaction ledger (dedupe, hard negatives kept, stale retired). Keep it in the house, never in third-party services beyond what a call must send; the free-model rule and the Mac-Ollama rule stand.
- **Models are artifacts too:** small weights files, router rows, task families and their judges are data-only packs, versioned by content hash, shippable through the harness store later.
- **Private data stays private.** Wallets, chat, user desks (`xyzfs/users`, `user/*` branches) are never training data without an explicit owner decision.
- **Honest expectation about "our own coding agents":** a general coding model trained in-house is a large, GPU-heavy project (L-XL, likely not feasible on this hardware alone). A realistic target is **narrow in-house agents** for the house's own small languages and families (event pages, `.pdl` rows, pal ops, vehicle designs, gym lanes), each with a locked judge, distilled from verified rows and escalating to a larger API model on failure. Start there; revisit general coding only if the data and graduation numbers justify it.

## 5. Does this go on the blockchain? Recommendation: mostly no, with one useful exception (DOC; decision D22, PROPOSED)
- **Why not the data:** training data, prompts and weights are large, private, and updated often; an append-only chain is slow and expensive for that, and putting private rows on a chain makes them hard to retract.
- **What fits the house chain** (designed in `CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md`, `PAL-CHAIN-META-AND-SUBCHAINS.md`; **nothing there is built for this**): small **receipts**. For example a content hash of a model pack or dataset version, a graduation record ("family X served in-house at grade G over N rows"), a rated result for the Elo trust gate (roadmap M3), or a purchase of a pack in the Store. That gives provenance and tamper evidence without carrying the data.
- **Economy angle** (SAID earlier, chain as bank and Fed): paying for compute, buying packs and rewarding good judged contributions can use the chain; keep it separate from the learning loop until the economy exists.
- **Test:** one authority per fact (the data file is the authority; the chain stores only its hash), scalable, reversible; house shape = hashes in a ledger row.

## 6. Roadmap (the AI track; each step has a judge gate)
| Step | What | Gate |
|---|---|---|
| A0 | Inventory and one page of truth: confirm which flywheel parts run (router rows, `DATA` rows, pilot q001); record tokens per call | a table of observed numbers (not assumed) |
| A1 | Give the harness-as-judge its **first caller**: a rated-bot move picker for True Swords Clash (M2) | harness: bot ladder monotonic; calls logged with tier and tokens |
| A2 | Router + `DATA` plumbing for two families (chemistry exam, TSC move) using the Mac Gemma only; API quota reserved for escalation | graduation measured on one family; never burn the quota |
| A3 | Context-pack generators (the zoo/museum generators) for each family | tokens per task drops by a measured amount |
| A4 | Distill the first small in-house model from verified rows (tomom path or a small fine-tune on the Mac/debil) | beats the rule bot on the family judge; falls back automatically on regression |
| A5 | Narrow in-house agents: event pages, `.pdl` rows, vehicle designs, gym lanes, each with judge | per-family graduation KPI |
| A6 | Chain receipts for packs and graduation (after the chain economy exists) | receipt verifies against the file hash |
| A7 | Revisit a general in-house coding agent | only if A4-A5 numbers justify |
Ties: M2 (rated bots), M10 (vehicle designs), M11/M12 (gyms are the eval set). Footprint: training and evals are heavy: one at a time, niced, offloaded.

## 7. Open questions
1. Is "Harnecient" the product name for the router and judge layer, or only the h-ai relay? 2. Which APIs and which Gemma size on the Mac are the official tiers today (`API-REGISTRY-AND-CAPS.md`)? 3. May verified `DATA` rows ever leave the house (a shipped pack), and under what screening? 4. Is a chain receipt for model packs wanted soon, or only after the economy?
