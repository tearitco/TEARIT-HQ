# Q014: Groq free-tier limits, observed 2026-10-07 (from response headers; no guessing)
Method: one tiny chat call (max_tokens 8, prompt "Reply with the single word ok") per model, only `x-ratelimit-*` / `retry-after` headers read. Raw: `observed.json` (headers only, no key).

| model | limit-requests | limit-tokens | remaining after 1 call | reset-requests | reset-tokens |
|---|---|---|---|---|---|
| openai/gpt-oss-120b | 1000 | 8000 | 999 req / 7915 tok | 1m26.4s | 637ms |
| openai/gpt-oss-20b | 1000 | 8000 | 999 req / 7915 tok | 1m26.4s | 637ms |
| qwen/qwen3.8-27b | 1000 | 8000 | 999 req / 7974 tok | 1m26.4s | 194ms |

Reading (interpretation, label it as such): 1000 requests per DAY (1m26.4s = 86400 s / 1000 = time to regain one request) and 8000 TOKENS PER MINUTE. Each of the three models showed 999 remaining after one call each, so the request bucket looks PER MODEL (not verified by a second call on the same model). Not observed: the actual 429 response and its `retry-after`.
Consequences for the fleet: requests are plentiful (1000/day/model vs OpenRouter's 50/day/account); the binding limit is TOKENS PER MINUTE: the q019 pilot used 4,444 tokens in one call = more than half a minute's budget. Plan: cap `max_tokens` per attempt, space attempts, and let the quartermaster treat tokens as its own unit (`LIMIT | groq | tokens | 8000 | 60`, `LIMIT | groq | requests | 1000 | 86400`).
