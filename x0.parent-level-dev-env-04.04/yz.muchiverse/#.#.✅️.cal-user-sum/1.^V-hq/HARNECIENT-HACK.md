# HARNECIENT HACK — tool-like use out of non-tooled models

> **THE COMPANY'S BREAD AND BUTTER.** Our entire product's selling point: give users
> tool-like use (read/edit/exec/research/write documents) out of models that have NO
> native tool support (gemma3:270m, gemma3:1b, stable-code:3b — anything).
> **The API never finds out.** We never send a `tools` field, never use native
> `tool_calls`. We prompt the model as a plain-text chat, parse its plain-text reply
> tolerantly, and the harness does all the actual tool work. Hand-hold every step;
> fall back on everything.
>
> Reference implementation to read FIRST: `@.apps/my-lawyer/` (live-verified, this is
> the origin of the name). Core source: `ops/mylawyer_case_worker.c`,
> `ops/mylawyer_judge_worker.c`, `ops/json_parser.c`, `ops/connect_op.c`,
> `pieces/registry/personas/*.txt`, plus `data/cases/1/*.txt` for real gemma output.

## The idea in one line

A NON-tooled model still produces useful plain text; the harness makes that text
*behave* like tool calls by (a) extracting it tolerantly, (b) deciding deterministically
which tool to run, (c) running it, and (d) folding the real result back in — with a
fallback for every step. Users experience "the AI read a document / wrote a document /
searched a corpus", and the model was never asked to do any of it.

## Why it exists (the measured facts)

- `gemma3:270m`/`1b` **cannot reliably follow a `TOOL:` text format** — it hallucinates
  tool lines and echoes the format back (KPI became "no TOOL: hallucination").
- `stable-code:latest` (3B) **server-rejects** any request containing a `tools` field:
  `{"error":"...does not support tools"}`.
- Even a native-tool 8B (`llama3-groq-tool-use:8b`) makes parallel tool calls and
  guesses edit args before a read result exists.
- But the SAME small models **reliably describe/complete short plain-text prompts**
  (measured: 6/6 describe vs 2/3 wrong direct-classify on my-biotech's FDA review).
- Conclusion baked into the hack: model = cheap text generator, never a tool router.
- **RE-CONFIRMED independently 2026-08-13** (`au11-hq/HARNESS-DELEGATION-PIPELINE.md`
  §12): a model-complexity classifier asked to output "SIMPLE"/"COMPLEX" directly
  failed across 3 prompt-engineering rounds and 2 models (gemma3:270m, gemma3:1b) —
  including a genuinely well-structured attempt (explicit rubric + system pre-prompt).
  Rebuilt as DESCRIBE (plain-sentence description) + a deterministic signal-counter —
  4/4 correct immediately after, same models, same test tasks. **This line's finding
  was already written down before that session started, and got violated anyway** —
  if you're about to ask a Harnecient model to output a category/label/index/enum
  directly, stop, this line is telling you it won't work reliably, no matter how good
  the prompt sounds.

## The six components (each one is required)

### 1. Plain API calls only — the API never knows
`POST /api/chat` with exactly `{"model":"<anything>","stream":false,"messages":[...]}`.
No `tools`, no `format`, no special schema. Works on ANY model. This is what makes the
product model-agnostic.

### 2. Persona file + simple plain-text question
A system persona file on disk (`pieces/registry/personas/lawyer_researcher.txt`,
`judge.txt`) that actively forbids structure:
```
You are a legal research assistant in a game. Give short, direct,
plain-text answers - one sentence or a single case name, never a list,
never markdown, never JSON. If you don't know, say "unknown" plainly.
```
Then a **simple** user question ("Name one plausible legal precedent case relevant to
<angle>. Just the case name, one line, no explanation."). Never demand structured
output from a small model — that is the original sin this hack exists to avoid.

### 3. Tolerant extraction — `json_parser.+x`
One robust primitive, reused everywhere (`gemma_ask()` in each worker): curl via
`connect_op.+x`, then extract `message.content` with `json_parser.+x` — which strips
` ```json ` fences defensively, walks dot-notation paths, and unescapes. The model's
output is treated as *text that may arrive wrapped in anything*, and the parser shrugs.

### 4. Deterministic tool dispatch & execution — the harness is the tool router
The model never picks a tool. `mylawyer_case_worker.c` picks the angle, greps the
corpus first (`search_corpus`, no network), calls gemma ONLY if the corpus has nothing
(`search_precedent`), and writes to the real case file itself (`case_doc_append`).
Same as `045.muchi-pal-agent`'s `gemma_strategy.c`: keyword/topic match → run tool →
fold result back in.

### 5. Real, player-visible artifacts
The "tool" writes REAL files (`data/cases/<id>/<side>_case.txt`) that grow turn by
turn and can be opened directly. The audit trail is the artifact. Never hidden state.

### 6. Hand-hold + fallback everywhere — the tolerance is the point
Every gemma step has a non-LLM fallback if the call fails or returns empty:
- precedent empty → `"General principle of %s applies to this dispute."`
- closing empty → `"The evidence and precedent above support this side's position."`
- judge text empty → score argument counts, then document length, then default.
And noisy output is **tolerated, not rejected**: live gemma 270M output included
`Precedent: * *Habeas Corpus*`, `**The Mill Case**`, `Case Name: *The Case of the
Consumed Artifact*` — all were formatted into `[Argument N]` lines anyway. The hack
never depends on the model being clean.

## Bonus rule: DESCRIBE, don't CLASSIFY (from PITFALL 69 / xyzos-standards §42)
Small models classify unreliably (my-biotech measured wrong 2/3) but describe
reliably. So the judge (`mylawyer_judge_worker.c`) asks gemma to **compare in its own
words**, then a deterministic, hand-authored keyword scorer
(`classify_comparison()`) derives the verdict from that real text, with
argument-count → length → default tiebreakers. Keep outcome decisions out of the LLM;
use the LLM for raw material.

## How to do it going forward (the recipe for any new feature)
1. Define the tools YOU need (read, edit, run, search corpus, write doc...).
2. Ask the non-tooled model ONLY short plain-text questions that a 270M can answer
   (a case name, a sentence, a comparison, a closing paragraph).
3. Extract its text with the tolerant parser primitive; never assume clean output.
4. Dispatch the actual tool deterministically; run it; append the real result to a
   real file or into the next prompt's context.
5. Give every single step a deterministic fallback so nothing can hang the app.
6. If you need a verdict, score the model's description yourself — never ask the
   model to classify.

## What this means for the ai-cell code-tools-harness work
The 3B `stable-code:latest` cannot do native tools and my earlier text-JSON experiment
lacked the hand-holding. The Harnecient Hack is the *properly engineered* version:
same idea, but with persona discipline, tolerant extraction, deterministic dispatch,
real-file folding, and fallbacks. For h-ai: adopt this pattern as the option that gives
users tool-like read/edit/run on non-tooled LAN models, exactly like my-lawyer gives
gemma read+write of case documents. See `&.widgits/ai-cell/code-tools-harness/LEARNINGS.md`.

## Measured proof (2026-08-12, code-tools-harness `harnecient_*` scenarios)
Full tool-like read/edit/run, **9/9 green, on every non-tooled model**, zero `tools`
fields sent:

| model | read | edit (compiles, prints 15) | run |
|---|---|---|---|
| `stable-code:latest` (3B) | PASS | PASS | PASS |
| `gemma3:1b` | PASS | PASS | PASS |
| `gemma3:270m` | PASS | PASS | PASS |

Real evidence (`results/gemma3_270m/harnecient_edit.json`): the 270m echoed the WHOLE
file inside a ` ```c ` fence instead of one line — the harness still extracted the
`i <= N` fix concept and applied the canonical line deterministically; compiles and
prints 15. Model noise tolerated, never trusted verbatim. Contrast in the same session:
naive text-JSON tool-call requests on 1B/270m/3B are nondeterministic and hallucinate
(the 3B invented `read_file ./example.txt` on a "say hello" prompt), and the native-tool
8B does parallel read+edit and hallucinates success. The hack is the reliable option.

## Grounding / further reading
- Live-verified implementation: `@.apps/my-lawyer/` (ops sources, personas,
  `data/cases/1/` real outputs) — and its sibling `@.apps/my-biotech/`
  (`mybiotech_research_worker.c`, `mybiotech_fda_verdict.c`, the 2/3-vs-6/6 measurement).
- Deterministic-dispatch precedent: `045.muchi-pal-agent🤖️+1`'s `gemma_strategy.c`
  / `strategy_execute_a.c` (the "tool beats model" pattern).
- Theory: `#.haiku+/!.gemma-judge-tomo&iqa.md`, `!.xyzos-pitfalls+1.txt` PITFALL 69,
  `!.xyzos-standards+1.txt` §42.
