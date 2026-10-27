# API registry and caps (living record; created 2026-10-27)

Owner 2026-10-27: "there should be docs on last api testing; if not, make sure we keep better agent dox." Finding: the house has docs on **what free models can do** and **how delegation works** (`02-architecture/OPENROUTER-MODEL-COMPETENCY.md`, `13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md`, `AI-BACKENDS-DISAMBIGUATION.md`), but **no record of rate caps or of probe runs**. This file is that record. Every probe adds a dated row; nothing here is guessed.

## Rules (house)
* Free providers and the Mac's Ollama only (memory: free-models-only; all Ollama calls go to the Mac URL in `#.desktop/ai_backend.pdl`). OpenRouter's free quota is **per account**.
* Probe niced, one provider at a time, stop at the first refusal; record the number, the error text and the time. Never put keys in this file.
* The router reads the cap rows (design: VISION-CLARIFIED section 8); a cap row older than 30 days is stale and is re-probed.

## Registry (what is known; "unknown" = never probed)
| Provider / model | Cost | Where configured | Daily cap | Per-minute cap | Last probed | Notes |
|---|---|---|---|---|---|---|
| Mac Ollama `gemma3:270m` (10.0.0.144:11434) | free | `ai_backend.pdl` gemma_lan_url / gemma_lan_model | none (own hardware) | CPU only | 2026-10-27 `api/tags` reachable (curl exit 0; the model list includes other models) | micro-task candidate #1 (chemistry exam taker) |
| OpenRouter free models | free | open-hai (see delegation guide) | unknown | unknown | never | quota per account; competency in OPENROUTER-MODEL-COMPETENCY.md |
| Other free APIs (HORN/ghost workers) | free | `^.hai-horn` | unknown | unknown | never | see DELEGATION-FLYWHEEL design |

## Probe log (append one row per run: date, provider, calls made, first refusal, error text, resulting cap)
| Date | Provider | Calls | Refusal | Text | Cap recorded |
|---|---|---|---|---|---|
| (none yet) | | | | | |

## Per-task worker grades (filled by the exam taker and later tasks)
| Task family | Worker | n | reward | punish | grade | Updated |
|---|---|---|---|---|---|---|
| (none yet) | | | | | | |
