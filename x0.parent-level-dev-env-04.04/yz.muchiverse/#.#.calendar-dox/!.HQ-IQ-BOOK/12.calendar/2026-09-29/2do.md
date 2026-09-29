# 2026-09-29 — OpenRouter delegation for open-hai (Grok-driven, not colab)

## Why this doc exists

Direct user instruction: "i want to set up the open routers, so grok
will beable to delegate using the free api, in our own open-hai window
it can control, instead of trying to communicate to kilo with colab,
its too slow" — then, given a leads doc of candidate free models: "lets
try these before we continue... wanna doc this 2do?" This is that doc.

## What landed today (real, verified)

- Read the user's leads doc (`XO/7.apis-2-openhai/open-hai-free-apis.md`)
  proposing 4 "free via OpenRouter" models (Nemotron Ultra, Poolside
  Laguna S, Dots-Studio dots-3-note-preview, Nex-AGI n2.5-pro/mini).
  **Did not trust it at face value** — live-checked each slug against
  `GET https://openrouter.ai/api/v1/models` before touching any code:
  - `nvidia/nemotron-3-ultra-550b-a55b:free` — real, `tools` supported.
  - `poolside/laguna-s-2.1:free` — real, `tools` supported.
  - `dots-studio/dots-3-note-preview:free` — real, `tools` supported.
  - `nex-agi/nex-n2.5-pro:free` / `nex-n2.5-mini:free` — **do not
    exist**. The real slugs are `nex-agi/nex-n2.5-pro` / `-mini` with
    no `:free` tier at all. Left out. The leads doc is otherwise real
    but this one entry looks fabricated/guessed — treat any future doc
    like it the same way (verify before adding).
- Confirmed the house already has a **live, working OpenRouter key**
  at `&.widgits/open-hai/state/openrouter_api_key.txt` (from the
  2026-08-16 handoff) — no new key needed. Live-tested all 3 real
  models with a real `tools` request; all 3 returned genuine
  `finish_reason: "tool_calls"`.
- Added all 3 to `g_models[]` in
  `&.widgits/open-hai/ops/khtpm_open_hai_manager.c` under
  `BACKEND_OPENROUTER` (same enum the 3 existing OpenRouter entries
  already use — no new backend needed). Rebuilt clean via
  `build_open_hai_manager.sh` (pre-existing snprintf-truncation
  warnings only, not new).
- Re-confirmed the real, already-documented reason `qwen/qwen3.8-max-
  free` "didn't work" last time: it's wired to `BACKEND_TOKENROUTER`,
  a genuinely different service (different key file, different base
  URL `api.tokenrouter.com`) — not an OpenRouter model at all. Not a
  bug, just a routing fact worth restating since it's what prompted
  this whole thread.

## What's still open — the real ask (Grok controlling open-hai directly)

The user's actual goal isn't "more models in the dropdown" — it's
**Grok delegating small, auditable workloads to a free model through
open-hai's own window, bypassing co-lab-hai's human-approval-gated
round-trip entirely** because that round-trip is "too slow" for this
use case.

This needs a real control surface for open-hai, analogous to the
`entity_menu_history/<pid>.txt` relay every `khtpm_core_render.c`
window already has (see K9 doc) — but Grok is an external CLI agent,
not a process that can write X11 relay files against a live PID it
doesn't know. Options not yet evaluated:

1. **File-drop request surface** — a plain-text request file (like
   `robot-chat`'s `rc_write_send.sh`/`oh_write_request.sh` pattern,
   already real and in use by open-hai itself for `CYCLEMODEL`) that
   Grok appends a prompt to, and open-hai's own manager picks up,
   dispatches to a chosen free model, and writes the reply back to a
   result file. This is the smallest real step — **no colab, no
   approval gate**, since it's Grok driving its OWN sandboxed window,
   not posting into the shared human-visible room.
2. Whether this needs a NEW dedicated small app/manager (a "grok-hai"
   sibling of open-hai) vs. reusing open-hai's own request file
   directly — open-hai's `oh_write_request.sh` already exists and its
   shape (`CYCLEMODEL` etc.) may just need one more verb (`ASK <text>`
   or similar) rather than new infrastructure.
3. Auditability requirement (explicit, "small auditable workloads") —
   whatever the mechanism, every prompt/response pair needs to land
   somewhere the user can review after the fact (likely just
   `open-hai`'s existing `chat_history`-shaped state file, already a
   real, append-only convention in this house).

**Not started.** Next concrete step: read `oh_write_request.sh` +
`khtpm_open_hai_manager.c`'s request-dispatch loop in full to see how
close verb (1) already is, then propose the smallest real addition to
the user before building it.

## Also flagged this session (informational)

Posted to Grok in colab (pending approval): the robot-chat live chat
window (blueprint `ROBOT-CHAT-BLUEPRINT.md` §3.3 / build-order step 4)
already exists — built and reworked to a real `prisc+x` pal script
earlier this same week (`&.widgits/robot-chat/`). Grok's stated "next
build is the robot live chat window" may be redundant; flagged before
Grok/Kilo start new construction on it.

## Real tool-use milestone testing (before writing this up as done)

Direct instruction before handing Grok anything: "make sure the api's
can use the tools we expect it to use etc, b4 we add this to hq-iq-
book, this is important milestone." Tested all 3 new models end to
end through open-hai's real `SEND|` request protocol (not just a raw
curl) with the exact 2-tool (`list_dir`/`read_file`) schema
`send_to_openrouter()` actually sends:

- **nvidia/nemotron-3-ultra-550b-a55b:free** — reliable. Real
  `tool_calls`, correct tool, correct path argument, repeatable.
- **poolside/laguna-s-2.1:free** — hit a real upstream 429 ("shared
  pool" rate-limit, same class the house already documented for
  gemma-4-26b-a4b-it:free back on 2026-08-18) on a retry. Not a code
  or reliability problem — an expected free-tier congestion risk, same
  as the existing entries. Worth knowing before leaning on it for
  time-sensitive delegation.
- **dots-studio/dots-3-note-preview:free** — found and fixed a REAL
  bug (see the commit below): `extract_openrouter_tool_call_raw()`'s
  path extraction required the model's escaped JSON to have no space
  after the colon (`\"path\\\":\\\"`); dots-studio's own serialization
  legally includes one (`\"path\": \"`), which silently defeated the
  match and always resolved to the house root regardless of the real
  requested path. Confirmed via a side-by-side raw curl against
  OpenRouter (model's own argument was byte-for-byte correct - the
  extractor was not). Fixed (`4ab519c49`, `claude` branch), rebuilt,
  and re-verified live: correct tool, correct path.

**Real process note, worth remembering**: most of this debugging chase
happened on the WRONG branch by accident - I'd committed the new
models on `claude`, then `git checkout main` to restore unrelated
runtime-state files, and main's own copy of `khtpm_open_hai_manager.c`
(pre-dating the `claude`-branch commit) silently came back with none
of the new models. Every "dots-studio" test in that window was
actually silently falling back to `g_models[0]` (`stable-code:latest`
over local Ollama) - which is why the replies looked like real model
misbehavior (wrong tool, wrong path, or outright unrelated prose)
instead of a clean pass/fail. Root cause confirmed by `strings <binary>
| grep dots-studio` coming back empty despite a "clean build" message -
a build succeeding is not proof it built the file you think it did.
Recovered with no lost work (`git stash` held the real fix the whole
time) by fully stashing, switching to `claude`, popping, then
re-applying just the real patch (minus a throwaway debug block) and
committing there. **Lesson for next time this house does dual-branch
work in one sitting**: `strings <binary> | grep <something-new>` before
trusting ANY test result across a branch switch, not just a rebuild.

## Desired API fix (documented now, not yet built - user: "we will do it")

The whitespace patch above is real and verified, but it's still a
hand-rolled `strstr` byte-pattern match — the same class of fragility
that produced this exact bug, patched one symptom at a time. The real,
better answer already exists elsewhere in this house's own history:

**`1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev`** (a different, older
project tree, not in this repo) has a real, generic, structurally-
correct dot-notation JSON parser: `ops/src/json_parser.c` (183 lines -
tracks `{`/`[` nesting depth and string escapes properly, not another
strstr hack). It's used as a real compiled op, e.g.:
```
json_parser <response-file> 'candidates[0].content.parts[0].functionCall'
```
This is genuinely whitespace-safe by construction — it would never
have had the dots-studio bug in the first place.

**Caveat found while checking this** (don't copy gem-dev's own usage
wholesale): its own response-parsing call sites are hardcoded to
**Gemini's** native response shape (`candidates[...].content.parts
[...].functionCall`), not Ollama's or OpenRouter's. Its own handoff
doc (`#.dox/groq-api-handoff.txt`) documents an unresolved bug from
exactly this mismatch - pointing that same Gemini-shaped parser at an
Ollama/llama3-groq-tool-use backend got back empty `{}` responses,
because Ollama's real native shape is `message.tool_calls[...].
function.{name,arguments}`, structurally different from Gemini's. That
bug was never actually fixed there - there is no working "here's the
Ollama tool-calls shape" reference to lift as-is.

**The real plan**: port `json_parser.c` itself (the generic dot-
notation engine, not gem-dev's Gemini-specific call sites) into
open-hai's `ops/`, and replace `extract_openrouter_tool_call_raw()`
entirely with calls shaped for OpenRouter's real, actual response
schema: `choices[0].message.tool_calls[0].function.name` and
`choices[0].message.tool_calls[0].function.arguments` (itself a JSON-
string-encoded object - `arguments` come back as a string that needs a
second parse pass, same as gem-dev's own `function_call.tmp` two-step
in its manager). This removes the whole class of whitespace/escaping
bugs this session just chased down, and gives every future OpenRouter
model the same reliability nemotron already has, without model-by-
model patching.

**Not started. Next step when picked up**: read `json_parser.c` in
full, confirm it handles the escaped-string-within-a-string case
`arguments` needs (a value that is itself JSON, not a plain string),
then port it + rewrite `extract_openrouter_tool_call_raw()`'s two call
sites (name lookup + a second pass on the escaped `arguments` string)
against it.
