# 🤖➡️🤖 open-hai-api-delegation-guide.md — delegating small tasks to a free API model through open-hai

**From:** Claude/Sonnet (agent_id `sonnet`) **To:** Grok
**Date:** 2026-09-29. **Status:** 🟢 ACTIVE, real, live-verified end to end.

📌 **This doc's own full, exact, canonical path** (copy verbatim):
```
#.#.calendar-dox/!.HQ-IQ-BOOK/13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md
```

---

## 0. Why this exists

Direct owner instruction: delegate small, auditable workloads to a free
OpenRouter model through **open-hai's own window**, instead of routing
everything through co-lab-hai's human-approval-gated room. This is
NOT a replacement for colab - colab is still how you talk to me or the
owner. This is a second, separate, faster channel for you to hand a
small file-level task to a free model and get a real, auditable result
back, without waiting on a human approval round-trip for the read-only
parts.

House root (always quote this path):
```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

---

## 1. The whole real mechanism (file-based, same shape as everything else in this house)

open-hai is a real, already-running khtpm window
(`&.widgits/open-hai/`) with a real manager process
(`khtpm_open_hai_manager.+x`) that polls a plain-text request mailbox.
There is no HTTP endpoint for you to call directly - you drive it the
same way any agent drives any khtpm app in this house: write a file,
poll another file for the result.

**Launch it** (if it isn't already running):
```
bash "<house_root>/&.widgits/open-hai/button.sh" "<house_root>"
```
Safe to call even if already running - it kills any stale instance
first and relaunches exactly one.

**Pick a model** - write the exact model name (see §2) directly to:
```
<house_root>/&.widgits/open-hai/sessions/model.txt
```
No "cycling" needed - `current_model()` does an exact string match
against this file every time it sends a request, and falls back to
`stable-code:latest` (a local Ollama model, NOT what you want) if the
name doesn't match anything real. Double-check your spelling against
§2's exact list before sending anything.

**Send a task**:
```
printf 'SEND|<your task text>\n' > "<house_root>/&.widgits/open-hai/state/request.txt"
```
Wait for `<house_root>/&.widgits/open-hai/state/busy.state.txt` to read
`0` before sending the NEXT request - a request sent while one is
still in flight is silently dropped (transcript gets a real
`[dropped: previous request to this model is still in flight...]`
line so you'll see it happened, but it never reached the model).

**Read the result** - find the current session, then tail its
transcript:
```
cat "<house_root>/&.widgits/open-hai/state/active_session.txt"
tail -5 "<the path that file contains>/transcript.txt"
```
`transcript.txt` is a real, append-only, plain-text log -
`U|<your prompt>` / `A|<the model's reply, or a real tool result>` -
this IS the audit trail the owner asked for. Never truncate or edit it
yourself.

---

## 2. Real, live-verified free models (as of 2026-09-29)

All via the same real OpenRouter key already on disk at
`&.widgits/open-hai/state/openrouter_api_key.txt` - you don't need
your own key.

| Model (write this exact string to `sessions/model.txt`) | Status |
|---|---|
| `nvidia/nemotron-3-ultra-550b-a55b:free` | **Recommended default.** Reliable, real `tool_calls` confirmed repeatedly, correct argument extraction every time tested. |
| `dots-studio/dots-3-note-preview:free` | Real, works (a real JSON-extraction bug that broke its tool calls was found and fixed today - see `12.calendar/2026-09-29/2do.md`). |
| `poolside/laguna-s-2.1:free` | Real, works, but hit a real upstream 429 ("shared pool" congestion) during testing today - expect occasional failures, not a code bug. Retry if you see `[error: no 'content' field in OpenRouter reply...]`. |
| `nvidia/nemotron-3.5-lightning:free`, `google/gemma-4-26b-a4b-it:free`, `cohere/north-mini-code:free` | Older, previously-verified OpenRouter entries - still real, not re-tested today. |
| `qwen/qwen3.8-max-free` | **Avoid.** Routed to a different service (TokenRouter, not OpenRouter) which is paywalled - not a real free option right now. |

If a model 404s later, free-tier slugs rotate - re-check
`GET https://openrouter.ai/api/v1/models` for a current `:free` entry
with `"tools"` in `supported_parameters` before assuming it's broken.

---

## 3. Real tools the model can call, and what happens when it does

Every request offers the model a real, OpenAI-style `tools` array. A
tool call is ACTUALLY EXECUTED (not just detected) via the same engine
the rest of this house's local AI tooling already uses.

| Tool | Runs automatically? | What it does |
|---|---|---|
| `list_dir` | Yes, read-only | Lists a real directory. |
| `read_file` | Yes, read-only | Reads a real file's contents. |
| `write_file` | **No - stops for human approval** | Creates or overwrites a file with new content. |
| `edit_file` | **No - stops for human approval** | Replaces a `search` string with `content` in a file, or appends `content` if `search` is omitted. |

`cmd_exec` (arbitrary shell execution) is **not offered** to the model
right now - deliberately left out (a separate decision for the owner,
not something either of us should silently add).

**When a write/edit tool fires**, the transcript shows:
```
A|[tool request from model] write_file /some/path - approve/deny in the sidebar
```
and `state/pending_tool.state.txt` is populated
(`name|path|content` format). Nothing happens to the file until you
(or the owner) write `APPROVE` or `DENY` to `state/request.txt`:
```
printf 'APPROVE\n' > "<house_root>/&.widgits/open-hai/state/request.txt"
```
Only after that does the transcript show the real result
(`A|[write_file] wrote N bytes to <path>`). **Never assume a
write/edit happened until you see that second transcript line** - the
request line alone means nothing ran yet.

---

## 4. The one real limitation that shapes what "appropriately sized" means

**Every `SEND|` is a single, stateless request - the model has ZERO
memory of any prior turn.** `send_to_openrouter()` builds a
`messages` array with exactly one `{"role":"user","content":"..."}`
entry, nothing else - no system prompt, no prior Q&A, no running
context. This is real and confirmed by reading the request-building
code directly, not assumed.

This means:
- **Good task shape**: one self-contained ask that includes everything
  the model needs to answer it, e.g. "read `<path>` and tell me if
  function X handles a null input" or "list the files in `<path>` and
  tell me which one looks like the manager". Anything where the
  ENTIRE task fits in one prompt + the tool results from that one
  request.
- **Bad task shape**: anything assuming the model "remembers" an
  earlier SEND, or a multi-step plan where step 2 depends on you
  having told it about step 1 in a previous message. It didn't see
  that message. If a task genuinely needs multiple steps, YOU
  (Grok) are the one holding the plan across steps - issue each SEND
  as a fully self-contained request, and stitch the results together
  yourself by reading each transcript reply.
- Keep individual asks scoped to a single file or a small, named
  directory - the free-tier models tested today are small/fast
  models, not built for reasoning over a large multi-file context
  dropped into one prompt.
- A `write_file`/`edit_file` request should describe the exact
  change in the prompt itself (since there's no back-and-forth) -
  the model gets one shot to produce the right `content`/`search`
  arguments from your one message.

---

## 5. Auditability (the owner's actual stated requirement)

Every prompt and every reply is already in `transcript.txt`, in order,
plain text, append-only - point the owner there for a full record of
anything you delegate. You don't need to build any new logging for
this; it's already the real, existing convention this house uses
everywhere else (same shape as `history.txt` per pal, `chat_history`
per robot).

---

## 6. Hard rules, same as every prior handoff

- This is a NEW, separate channel from colab - don't stop posting in
  colab; use this for the small file-level work you'd otherwise wait
  on a human round-trip for.
- Still ask me (@sonnet) or the owner in colab if anything here is
  unclear, or if you hit a model/tool behavior that doesn't match what
  this doc says - the doc could be wrong or stale, the code is the
  real source of truth (`&.widgits/open-hai/ops/khtpm_open_hai_manager.c`).
- Never approve your own `write_file`/`edit_file` request without the
  owner's actual sign-off on the task, same standing rule as every
  other approval gate in this house.
- Commit any code changes that come out of a delegated task on your
  own branch (`grok`), never `main`, same as always.
