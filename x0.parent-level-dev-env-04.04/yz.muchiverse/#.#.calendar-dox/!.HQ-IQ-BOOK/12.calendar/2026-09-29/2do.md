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
