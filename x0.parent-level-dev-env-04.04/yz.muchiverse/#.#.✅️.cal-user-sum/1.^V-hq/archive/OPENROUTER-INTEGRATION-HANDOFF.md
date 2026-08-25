# OpenRouter/TokenRouter integration — handoff

**Status: DONE for OpenRouter, fully live-verified end to end including the
real UI. TokenRouter is a real, confirmed PAYWALLED NON-STARTER for now
(see below) — don't spend more time on it until the account has credit.**

Direct instruction that closed this out: *"ok, lets just go with openrouter
and mark token router as a 'paywalled' non starter. this is good. then if
open router works, lets get it going in open-hai, using inject and
'un-harnessed' and we can call it a day for now."*

## The real, complete result

OpenRouter is fully wired into `open-hai`, works through the real UI (not
just the manager in isolation), and its native tool-calling REPLACES the
local Harnecient-hack keyword detector for this backend — confirmed with
a real tool actually executing and showing real results in the GUI.

**Real, live-verified chain, in the actual running app, via real relay
injection through the actual UI action (not just direct file writes):**
1. Cycled the real MODEL nav row (`cycle_model()`) via the same relay
   mechanism a real click/digit-jump would use — landed on
   `google/gemma-4-26b-a4b-it:free`, confirmed both in `model.txt` AND
   visually in a real dumped frame (`[>]24. google/gemma...` in the
   sidebar, green/focused, exactly as a real user would see it).
2. Sent a real chat message ("List the files in &.widgits/open-hai using
   your list_dir tool.") that ALSO happens to match the local harness's
   own keyword patterns — confirmed it now bypasses that harness entirely
   for this backend (`handle_submit()`'s real branch, see below) and goes
   straight to the API.
3. The model returned a real, correctly-structured native `tool_calls`
   response (`list_dir({"path":"&.widgits/open-hai"})`).
4. That real tool call was ACTUALLY EXECUTED (not just detected/reported)
   via the same real `start_tool_job()`/`execute_pending_tool_into()`
   engine the local harness already used — real directory listing showed
   up in the transcript, visible in the real GUI frame dump.

## Real bug caught and fixed mid-session (direct live report: "i didn't
see model change in model changer. are u sure u did it right?")

`khtpm_open_hai_render.c` (the shell) has its OWN, SEPARATE copy of
`BackendMode`/`g_models[]` from the manager's — real, pre-existing
drift this house's shell/manager split doesn't prevent automatically
(each half owns whatever copies it needs). Adding OpenRouter/TokenRouter
to the MANAGER's list alone made real API dispatch work (confirmed via
curl-equivalent testing), but the SHELL's own list — the one
`cycle_model()` and the sidebar's model-name display actually read —
didn't recognize either new model name, so it silently fell back to
`g_models[0]` (`"stable-code:latest"`) whenever `model.txt` held a name
it didn't know. No crash, no error — just looked like nothing happened.
**Fixed**: mirrored the same 2 real entries into the shell's own list too
(`khtpm_open_hai_render.c`). **Both lists must be kept in sync by hand**
until/unless a real shared-source-of-truth refactor happens (not
attempted here — separate, larger change, noted as an open item below).

## Real, confirmed protocol facts

**OpenRouter** — standard OpenAI-compatible shape:
- Request: `{"model":"...","messages":[{"role":"user","content":"..."}],"tools":[...]}`
  (`content` is a plain string; `tools` is the real, standard OpenAI
  function-calling array).
- Response: `choices[0].message.content` (plain string, or `null` when a
  tool was picked) / `choices[0].message.tool_calls[0].function.name`+
  `.arguments` (arguments as a JSON string). Compact JSON, no space after
  colons.
- Real tool-calling CONFIRMED working, twice: once via direct curl
  (`nvidia/nemotron-3.5-lightning:free`), and again via the exact model
  actually wired into open-hai (`google/gemma-4-26b-a4b-it:free`) — clean,
  correct `tool_calls` both times, naked (no local harness).
- Free-tier model slugs rotate — re-check
  `GET https://openrouter.ai/api/v1/models` for a current `:free` entry
  with `"tools"` in `supported_parameters` if a wired model ever 404s.

**TokenRouter — REAL, CONFIRMED PAYWALLED NON-STARTER, not a protocol
problem:**
- Plain chat completions on `:free` models DO work at $0 balance
  (confirmed: `qwen/qwen3.8-max-free` → real "pong").
- ANY tool-calling request — tried on 3 different models, including
  `:free`-suffixed ones (`qwen/qwen3.8-max-free`, `nvidia/nemotron-3-
  nano-omni-30b-a3b-reasoning:free`) and a non-free one
  (`google/gemma-4-26b-a4b-it`, confirmed present on TokenRouter too,
  but typed `"gemini"` endpoint there, not `"openai"`) — ALL failed with
  a real, consistent `insufficient_user_quota` / `$0.00 credit limit`
  error (403), or the earlier `cache_only_cold` 503 on the same model
  (almost certainly the same underlying quota restriction surfacing
  differently). **This account has $0 credit and tool-calling requires
  a nonzero balance even on free-labeled models.** Not a code problem,
  not a model problem — needs a real top-up before revisiting.
- Real, distinct protocol shape confirmed anyway (useful if/when this
  gets revisited): request `content` is an ARRAY of `{"type":"text",
  "text":"..."}` objects, not a string; response JSON has a SPACE after
  colons (`"content": "..."` vs OpenRouter's compact `"content":"..."`);
  a real separate `reasoning_content` field carries chain-of-thought;
  answers can have leading `\n\n`. All of this is implemented and ready
  in `khtpm_open_hai_manager.c` (`send_to_tokenrouter()`/
  `extract_tokenrouter_content()`) — genuinely working for plain chat,
  just untested for tools due to the credit block.

## What's actually done (code)

`&.widgits/open-hai/ops/khtpm_open_hai_manager.c`:
- `BackendMode` gained `BACKEND_OPENROUTER = 3` / `BACKEND_TOKENROUTER = 4`.
- `g_models[]` has both real entries (both keys are live and working).
- Real key loading for both services (`state/openrouter_api_key.txt` /
  `state/tokenrouter_api_key.txt`, chmod 600, sourced from the user's own
  `&.FREE-AI-KEY/&.Secret-Keys.txt`, kept out of any zip/archive).
- `send_to_openrouter()` / `send_to_tokenrouter()` — real async curl
  dispatch, same proven pattern as `send_to_ollama()`.
- `send_to_openrouter()` now sends a real `tools` array (`list_dir`,
  `read_file` — matching open-hai's own real local tool names exactly).
- `extract_openrouter_tool_call_raw()` — real tool_calls detection
  AND name/path extraction (fixed a real bug caught before shipping:
  the `arguments` field is JSON-string-encoded, so its own quotes are
  BACKSLASH-escaped in the raw response bytes — an early version
  searched for the unescaped form and would never have matched a real
  response).
- `extract_tokenrouter_content()` — real TokenRouter response parsing.
- `dispatch_send()` — routes to the right backend based on
  `current_model()`'s real result; `handle_submit()` calls this instead
  of unconditionally calling `send_to_ollama()`.
- **`handle_submit()` now skips the local `detect_tool()` harness
  entirely when the current model is `BACKEND_OPENROUTER`** — the real
  "un-harnessed" wiring — going straight to `dispatch_send()` so the
  API's own native tool-calling gets a real chance to fire, instead of
  the local keyword detector silently intercepting the message first
  (confirmed this was happening before the fix — a "list_dir" message
  was caught locally and never reached the API at all).
- When OpenRouter returns a real tool call, it's now **actually
  executed** (not just detected) via the same real `start_tool_job()`/
  `execute_pending_tool_into()` engine the local harness already used —
  same real approval gate (`tool_requires_approval()`) applies, so a
  future tools array that adds `write_file`/`cmd_exec` won't silently
  auto-run without approval.
- `PendingTool` typedef + a few globals/forward-declarations moved
  earlier in the file so `check_pending()` could reach them (real,
  mechanical reordering, not a design change).

`&.widgits/open-hai/ops/khtpm_open_hai_render.c`:
- Same `BackendMode` additions and same 2 real `g_models[]` entries,
  mirrored by hand (see "real bug caught" section above for why this
  was necessary).

Both binaries build clean (`build_open_hai_manager.sh` / `build_open_hai.sh`).

## What's NOT done — real next steps, in order

1. **TokenRouter needs real account credit** before tool-calling can be
   tested there at all. Everything else about it is implemented and
   ready.
2. **No shared source of truth for `g_models[]`.** It's duplicated
   between shell and manager, kept in sync by hand — a real, live
   footgun (exactly what caused the "model changer" bug this session).
   A real fix would have the shell read the model list from the same
   place the manager does, or vice versa.
3. **No conversation history sent** — every request is a single
   `{role: user, content: prompt}` message. Real next step: build a
   real messages array from the session's own `transcript.txt`
   (`load_transcript_if_changed()` in the render file already parses
   this format).
4. **No model-name validation / no `"error"` surfacing** — a bad model
   name just shows the generic "no content field" message instead of
   the API's own real error text.
5. **No cost/usage tracking** for either service.
6. **Only `list_dir`/`read_file` are offered as real tools to the API.**
   open-hai's local harness also supports `write_file`/`edit_file`/
   `search_in_files`/`cmd_exec` — those aren't in `send_to_openrouter()`'s
   `tools` array yet. Real next step if wanted: add them (the approval
   gate already handles the safety side correctly for write/exec-class
   tools, confirmed by inspection).

## Real files touched this pass

- `&.widgits/open-hai/ops/khtpm_open_hai_manager.c` (backup:
  `khtpm_open_hai_manager.c.bak-2026-08-16-router-start` — pre-dates
  everything except the very first draft)
- `&.widgits/open-hai/ops/khtpm_open_hai_render.c` (no backup taken for
  this one — small, mechanical, well-isolated change; diff against git
  history if a revert is ever needed)
- `&.widgits/open-hai/state/openrouter_api_key.txt` (real key, chmod 600)
- `&.widgits/open-hai/state/tokenrouter_api_key.txt` (real key, chmod 600)
