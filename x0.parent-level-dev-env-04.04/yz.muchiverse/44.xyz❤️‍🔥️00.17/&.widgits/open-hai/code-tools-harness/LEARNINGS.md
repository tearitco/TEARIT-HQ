# LEARNINGS — open-hai native tools & model probing

**Purpose:** hard-won facts so future agents don't burn hours re-deriving them.
**Owner:** open-hai work, 2026-08-12. Raw evidence: `api-test-results.md` + `results/`.
Harness (pure C): `run_native_tools.c` (build: `cc -O2 -Wall -Wextra -o run_native_tools run_native_tools.c`).

## 0. THE HARNECIENT HACK — read this FIRST
The company's real bread-and-butter: **tool-like use out of NON-tooled models by never
telling the API we want tools.** Live reference: `@.apps/my-lawyer` gets `gemma3:270m`
to read + write real case documents — plain `/api/chat` calls (no `tools` field), a
persona file that forbids structure, simple plain-text questions, a tolerant
response parser (`json_parser.+x`, strips ` ```json ` fences + dot-path extraction),
deterministic app-side tool dispatch, real-file folding, and a fallback for every step.
This is the properly-engineered version of what my naive text-JSON experiment lacked
(hand-holding). Canonical doc: `au11-hq/HARNECIENT-HACK.md`.

**PROVEN on this harness (2026-08-12): the `harnecient_*` scenarios pass 9/9 across
`stable-code:latest` (3B), `gemma3:1b`, AND `gemma3:270m`** — deterministic read, edit
(compiles + prints 15), and run, with zero `tools` fields sent. By contrast the flaky
approaches measured in the same session: 8B native read-then-edit FAILS (parallel calls,
guessed search, hallucinated success); text-JSON tool-call requests on 1B/270m/3B are
nondeterministic and hallucinate. **Use the Harnecient pattern going forward so users
get tool-like read/edit/run on non-tooled models — it is the reliable option.**

## 1. The 3B CANNOT do native tool calls. Don't re-test it.
`stable-code:latest` (3B, family stablelm): Ollama refuses any request with a `tools`
field — `{"error":"...does not support tools"}` server-side. This is not a routing or
prompt problem. There is NO model-side tool calling on the 3B, period.
Also confirmed: prompting it to emit tool-call JSON as text fails (it rambles about
writing `subprocess` wrappers instead of calling anything).

## 2. The agent-45 pattern is NOT "model emits tool calls" — it's deterministic + wrappers.
gemma 270M/1B too small to follow a `TOOL:` format → agent-45 does tool detection
**before any LLM call**: `gemma_strategy.c detect_tool()` (keyword match on the user's
message) → `strategy_execute_a.c` → the app's own `ops/+x` wrapper binaries execute it.
A `TOOL:`-style prompt hack WAS tried and REMOVED (models hallucinated tool lines and
echoed the format back; the KPI became "no TOOL: hallucination in last 12 turns").
**Consequence:** h-ai already has this hack (detect_tool → approve/deny → ops) and it is
model-agnostic — it works identically whether the chat backend is gemma-270m or the 3B,
because the model never participates in tool calling. This IS the Harnecient Hack (see
`au11-hq/HARNECIENT-HACK.md`); my-lawyer's `gemma_ask()`+`json_parser` round-trip is the
polished, live-verified form of it.

## 3. Pure C only — no Python in house tooling.
House rule: embedded/ops code is pure C (`-Wall -Wextra` clean). The first harness was
written in Python; it was ported to C (`run_native_tools.c`) and the `.py` deleted.
Write C first.

## 4. The 8B tokenizer corrupts emoji-laden absolute paths.
House tree dirs are emoji-named (`🤖️🪤️🏠️/...`). `llama3-groq-tool-use:8b` regenerated
garbled `\uXXXX` escapes for those paths → FileNotFoundError. **Work fixtures must live
under a plain-ASCII path** (harness uses `/tmp/code-tools-harness-work/`). Durable
results stay in the house tree (`results/` under the harness dir).

## 5. `llama3-groq-tool-use:8b` (8B) — native `tool_calls` WORK, with real caveats.
Verified end-to-end through the harness (each PASS is `gcc && ./sample`-verified):
- `read_file` — PASS
- `exec_cmd` (compile+run) — PASS
- `edit_file` with the exact search/replace block provided — PASS (applies, compiles, prints 15)
- `edit_file` in a "read then edit" agent sequence — FAIL x2: the model emits `read_file`
  AND `edit_file` as **parallel calls in one message**, so edit args are guessed before
  the read result exists (`for (int i = 0; i < N-1; ...` vs the real `i = 1`), the
  exact-block match is rejected, and it then **hallucinates success**.
  → For safe editing: give the exact block in the prompt, or gate edit behind a
  read-confirmation step.

## 6. JSON quirks that WILL bite a C (or any non-`json.loads`) parser:
- **`\uXXXX` escapes:** the 8B emits `\u0026\u0026` for `&&`, `\u003c`/`\u003e` for
  `<`/`>` (the 3B does too). A raw-string extractor must decode `\uXXXX` (plus
  `\\ \" \n \r \t`) or commands/paths silently break. (`json_decode()` in the harness.)
- **`arguments` arrives as an object OR a quoted JSON string** depending on the model —
  handle both when echoing tool_calls back into the payload.

## 7. Harness C-port bugs that took three round-trips (now fixed):
- Missing `n++` when appending the final assistant turn → the final answer was silently
  dropped from results and verdicts read empty content.
- Verdict for `run` looked at the wrong message (must scan all tool-role msgs for the
  exec output, not the last msg).
- Lesson: after every port, **diff the verdicts against a known-good run** before trusting
  the new harness. Parity check caught all of the above.

## 8. Ollama tool-loop protocol (confirmed working, matches gem-dev):
assistant msg with `tool_calls` → harness executes locally → append `{"role":"tool",
"content":<result>}` → re-query with full history. `model` is REQUIRED in the body
(missing it → HTTP 400 `model is required`). Endpoint: `http://10.0.0.144:11434/api/chat`.

## 9. fopen-on-directory crash (harness bug found + fixed 2026-08-12):
Linux `fopen()` on a DIRECTORY SUCCEEDS, `fseek(SEEK_END)` returns 0, and `ftell()`
returns `LONG_MAX` (0x7fffffffffffffff). `read_whole()` then did `malloc(LONG_MAX+1)`
→ segfault. Triggered when the 3B's rambling post_hack reply embedded a fake
`summary = {"tool":"read_file","args":{"path": file_path},...}` Python dict that the
parser accepted, and the unparseable `path` resolved to the WORK directory. Fixed:
`read_whole()` rejects `sz<=0 || sz>8MB`. Lesson: never trust paths/sizes from model
output; always bounds-check file sizes before allocation.

## 9. Documented-elsewhere, do not re-discover:
- Agent-45 full history: `045.muchi-pal-agent🤖️+1++/#.dox/jul-21-gemma-fix.txt`,
  `walkthru-j30.txt`, `!.GRAND-PLAN-TOOL-SCAFFOLD.txt`.
- gem-dev working standard: `1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev/`
  (model `llama3-groq-tool-use:8b`, tool loop in `manager/gem-dev_manager.c`,
  payload builder `ops/src/gemini_payload_builder.c`).
