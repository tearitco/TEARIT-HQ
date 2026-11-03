# API registry and caps (living record; created 2026-10-27)

Owner 2026-10-27: "there should be docs on last api testing; if not, make sure we keep better agent dox." Finding: the house has docs on **what free models can do** and **how delegation works** (`02-architecture/OPENROUTER-MODEL-COMPETENCY.md`, `13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md`, `AI-BACKENDS-DISAMBIGUATION.md`), but **no record of rate caps or of probe runs**. This file is that record. Every probe adds a dated row; nothing here is guessed.

## Rules (house)
* **NEVER probe a free API to exhaustion.** On 2026-10-27 a probe looped to the first 429 and burned the whole day's free quota (owner: "save the api for when you really need it"). A cap is learned from ONE call's `X-RateLimit-*` headers or from the rows below. The API is the escalation tier; bulk micro-tasks go to the Mac Gemma.
* Free providers and the Mac's Ollama only (memory: free-models-only; all Ollama calls go to the Mac URL in `#.desktop/ai_backend.pdl`). OpenRouter's free quota is **per account**.
* Probe niced, one provider at a time, stop at the first refusal; record the number, the error text and the time. Never put keys in this file.
* The router reads the cap rows (design: VISION-CLARIFIED section 8); a cap row older than 30 days is stale and is re-probed.

## Registry (what is known; "unknown" = never probed)
| Provider / model | Cost | Where configured | Daily cap | Per-minute cap | Last probed | Notes |
|---|---|---|---|---|---|---|
| Mac Ollama `gemma3:270m` (10.0.0.144:11434) | free | `ai_backend.pdl` gemma_lan_url / gemma_lan_model | none (own hardware) | CPU only | 2026-10-27 `api/tags` reachable (curl exit 0; the model list includes other models) | micro-task candidate #1 (chemistry exam taker) |
| OpenRouter free models (key in open-hai state; account is free tier, $0.20 lifetime usage) | free | open-hai / `^.hai-horn/ops/horn_chat_openrouter.c` | **50/day stated by the API header (`X-RateLimit-Limit`); 83 calls actually succeeded before the first refusal** (discrepancy unexplained) ; shared across all `:free` models (a second model was refused too); +10 credits would unlock 1000/day | no per-minute limit hit (30 calls in 69 s, then 53 more, all 200) | 2026-10-27 | quota per account; competency in OPENROUTER-MODEL-COMPETENCY.md |
| Groq `openai/gpt-oss-120b` (also `gpt-oss-20b`) | free | `&.widgits/open-hai/state/raw_groq.txt` (gitignored), `horn_chat_backend` provider 1 | **1000 requests/day** (header `x-ratelimit-limit-requests`) | 8000 tokens/min (header) | 2026-11-02 | works: 200 and "ok" from 120b. `gpt-oss-20b` is a reasoning model: with `max_tokens` 8 it returns an empty reply (200), give it 300+. |
| Poolside `poolside/laguna-xs-2.1`, `laguna-s-2.1` | free (account) | `raw_poolside.txt` (gitignored), `horn_chat_backend` provider 2 | unknown | **60/min** (header) | 2026-11-02 | key authenticates and `/v1/models` lists both, but every chat call returns HTTP 400 "Model version ... is deactivated. It needs to be activated before running predictions": **the account owner must activate a model in the Poolside console.** Nothing to fix on our side. |
| NVIDIA direct (build.nvidia.com) | free tier (unverified) | none | unknown | unknown | never | no key in the house; not wired. The nvidia free slugs on OpenRouter now answer 404 "unavailable for free" (2026-11-02), so they cost money there. |
| Other free APIs (HORN/ghost workers) | free | `^.hai-horn` | unknown | unknown | never | see DELEGATION-FLYWHEEL design |

## Findings of the 2026-10-27 probe
* Daily cap is **per account across all free models** and resets at the epoch in `X-RateLimit-Reset` (midnight UTC style). Budget rule for the router: assume **50/day** (the stated number) and treat extra as a bonus. The probe itself spent today's whole free quota: **do not probe again before the reset** and never on a day the owner needs the free API.
* `GET /api/v1/key` shows `is_free_tier`, usage and a deprecated rate_limit field; it does not show the daily count, so the cheapest check is one call and reading the headers.
* Consequence for the flywheel: the free API tier supports **tens of micro-tasks a day**, so Gemma 270M on the Mac (free, uncapped) must carry the bulk and the API is reserved for escalations and judging.

## Probe log (append one row per run: date, provider, calls made, first refusal, error text, resulting cap)
| Date | Provider | Calls | Refusal | Text | Cap recorded |
|---|---|---|---|---|---|
| 2026-10-27 | OpenRouter `nvidia/nemotron-3-ultra-550b-a55b:free`, max_tokens=1 prompt "hi", niced | 83 ok (30 + 53), the next refused | HTTP 429 | `Rate limit exceeded: free-models-per-day. Add 10 credits to unlock 1000 free model requests per day`; headers Limit=50 Remaining=0 Reset=epoch-ms 1793145600000 | daily cap reached; per-minute not reached |
| 2026-11-02 | Groq 120b + 20b, Poolside xs + s, OpenRouter `nvidia/nemotron-3-super-120b-a12b:free` | 6 (one each, prompt "Reply with exactly: ok") | Poolside 400 (deactivated), OpenRouter 404 | Groq ok; Poolside deactivated on the account; OpenRouter slug paid-only now | Groq 1000/day + 8000 tokens/min; Poolside 60/min |

## Per-task worker grades (filled by the exam taker and later tasks)
| Task family | Worker | n | reward | punish | grade | Updated |
|---|---|---|---|---|---|---|
| (none yet) | | | | | | |
