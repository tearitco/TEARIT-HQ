# Task for Haiku: prove h-ai's LAN model(s) can actually do agent work

## Goal (one sentence)

h-ai currently sends plain prompts to Ollama and shows plain text
replies — nothing has proven the LAN model(s) can do TOOL USE, real
CODE WRITING, or other things expected of an actual AI agent. Find
out for real, using h-ai's own real UI (not a separate script), and
report exactly what works vs. doesn't.

## Read first

1. `&.widgits/open-hai/README.md` — how h-ai works, how to build/run/
   test it (relay injection, PNG-dump+receipt verification — don't use
   `xwd`/screenshots).
2. `&.widgits/open-hai/ONBOARDING.md` — recent history, what's real.
3. `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` (house root) — the LAN Ollama host's
   real credentials/model list. **Re-check this file's own model list
   is current before trusting anything below — it can drift.**
4. `045.muchi-pal-agent🤖️+1++/agent-45-aug3.md` — agent-45's own real
   tool loop (`list_dir`, `cmd_exec`, `edit_file`, `search_in_files`,
   etc.). This is the REFERENCE for what "an AI agent doing tool use"
   concretely looks like in this house — study it, don't invent a
   different shape from scratch.

## Real facts already known (verified via `/api/tags`, don't re-derive)

- `g_model_name` in `khtpm_open_hai_render.c` is hardcoded to
  `stable-code:latest` (3B, a real CODE-completion model, family
  `stablelm`). It has **no `tools` capability flag** in Ollama's own
  model metadata.
- `llama3-groq-tool-use:latest` (8B) **does** have a real `tools`
  capability flag — this is the one actual candidate on this LAN host
  for testing Ollama's native tool-calling API, if you want to test
  that path specifically.
- h-ai's own backend code (`send_to_ollama()` in
  `khtpm_open_hai_render.c`) calls `/api/generate` with a bare
  `{"model":..., "prompt":..., "stream":false}` payload — **no tools
  array, no tool-call parsing at all.** There is currently ZERO tool-
  use capability built into h-ai. This needs real code, not just a
  test — see Phase 2 below.

## Phase 1 — prove real code-writing quality (no new code needed)

Just use h-ai as it exists today. Real, bounded test:

1. Launch h-ai for real (`sh button.sh <house_root>`, or via the
   taskbar's h-ai cell if that's confirmed working by the time you
   read this — check `ONBOARDING.md` for current status).
2. Via relay injection (see README.md), send it 3-5 real, concrete
   coding prompts of increasing difficulty. Suggestions (adjust as
   needed, but keep them REAL tasks with a checkable right answer, not
   vague ones):
   - "Write a C function that reverses a null-terminated string in place."
   - "Write a bash one-liner that finds all files over 10MB in the current directory."
   - "Write a Python function that checks if a string is a valid IPv4 address, no regex."
3. For each response: actually try to COMPILE/RUN what it wrote (in a
   scratch dir, not this house's real code) and note whether it's
   correct, close-but-buggy, or wrong. Don't just eyeball it — actually
   test it.
4. Record real results (prompt, full response, whether it worked) in
   a new `&.widgits/open-hai/api-test-results.md` — this is the actual
   deliverable, not a verbal summary.

## Phase 2 — prove (or build) real tool use

This is the harder, more open-ended half. Two real paths, pick based
on what Phase 1 tells you about the models:

**Path A — test Ollama's native tool-calling API directly** (fastest
to try, doesn't require changing h-ai's code):
1. `curl` directly against `llama3-groq-tool-use:latest` on the LAN
   host, using Ollama's real `/api/chat` endpoint with a `tools` array
   in the request (see Ollama's own API docs for the real JSON shape —
   don't guess the format, verify it against a real successful
   response first with a trivial one-tool test).
2. Give it ONE simple tool (e.g. a fake `get_weather(city)` or
   `read_file(path)` schema) and a prompt that should trigger it. Does
   the model actually emit a real tool-call in its response, or does
   it just answer in prose?
3. Document the real result (working request/response pair or the
   real error) in `api-test-results.md`.

**Path B — study how agent-45 itself does tool use** (agent-45-aug3.md
already told you it uses `gemma-lan`/`gemma3:270m`, NOT a model with
a native `tools` flag — meaning agent-45's own tool-calling is almost
certainly PROMPT-BASED: the model is instructed via its system prompt
to emit tool calls in a specific text/JSON shape, and agent-45's own
code parses that out of the plain text response, not via Ollama's
native tools API at all):
1. Find agent-45's own real system prompt / tool-call parsing code
   (search `045.muchi-pal-agent🤖️+1++/` for where it constructs the
   prompt sent to the model and where it parses the response for a
   tool call - likely in `system/` or wherever its own orchestrator
   logic lives).
2. If this IS the real mechanism, it's the one to replicate for h-ai
   too (matches the "learn from what's already proven working" bias
   this whole house follows) - RATHER than inventing tool-calling from
   scratch OR assuming Ollama's native tools API will work with a 3B
   model that has no tools flag.
3. Report which path (A, B, or "neither works well") actually produced
   a real, working tool call - don't guess, test both if time allows.

## If you build real tool-call support into h-ai

- Keep `g_backend_mode`'s existing shape (`BACKEND_OLLAMA_RAW` /
  `BACKEND_AGENT45_LEGACY` enum) in mind — a tool-calling mode is a
  natural third value, not a rewrite of the existing plain-completion
  path (keep that working too, don't replace it).
- Any new tool you actually wire up should be REAL (e.g. a real
  `read_file`/`list_dir` against the house filesystem), not a fake
  demo tool - matches this house's own "don't build hypothetical
  demos, build real capability" bias.
- Test via h-ai's own real UI + relay injection, same as Phase 1 - not
  a separate standalone test script that bypasses the actual app.

## Testing rules (same as every other khtpm/-hq window in this house)

- Relay injection only (`#.desktop/open_hai_agent_relay.txt`), never a
  real mouse/keyboard simulation tool.
- PNG-dump + receipt (`'p'` key or `--dump-and-exit`) for visual
  verification, never `xwd`/PIL screenshots - see README.md for why.
- If curl/Ollama calls seem to hang, give them real time (a 3B model
  on a shared LAN Mac can genuinely take 10-20+ seconds for a longer
  response) before assuming something's broken - confirmed this
  session, not a guess.

## Deliverable

`&.widgits/open-hai/api-test-results.md` — real prompts, real
responses, real pass/fail verdicts (did the code actually compile/run
correctly? did a real tool call actually fire?), for both Phase 1 and
Phase 2. This is what proves (or disproves) "yes, this can do real
agent work" - a summary without the real transcripts doesn't count.

## Escalate to Sonnet instead of guessing if

- Phase 2 requires a genuinely new architectural decision (e.g. "should
  h-ai support MULTIPLE simultaneous tool-enabled backends" or "should
  tool execution be sandboxed") rather than a scoped code addition.
- Neither Path A nor Path B produces a working tool call after a real
  attempt at both - that's a real finding worth a second opinion on
  what to try next, not something to keep guessing at alone.
- Any tool you're about to wire up would let the model execute
  arbitrary shell commands or write to files outside `&.widgits/
  open-hai/` without a human in the loop - that's a real safety
  question, not a unilateral call.
