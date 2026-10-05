# AI backends — who is who

Written 2026-09-29, direct request ("i wanna document these
distinctions in a specific document so i can stop getting confuse").
Plain, concrete companion to `AI.md`'s terser voice — this doc names
files and paths, `AI.md` states law.

## The three separate model backends in this house

These are three genuinely different systems. None of them contain or
wrap each other.

### 1. `gemma3:270m` (LAN Ollama)

- Config: `#.desktop/ai_backend.pdl` (`gemma_lan_url`/`gemma_lan_model`)
  — currently `http://10.0.0.144:11434`, a Mac on the LAN running
  Ollama.
- Callers: `&.widgits/entity-cli/ops/ai_describe.c` (constrained
  DESCRIBE), `ai_chat.c` (free-text chat), `my-lawyer`'s
  `mylawyer_case_worker.+x`/`mylawyer_judge_worker.+x`, `my-biotech`'s
  equivalents.
- **This is what `robot_chat_001`'s "Chat-gemma" context-menu button
  calls.** A raw, unstructured conversation round trip — no scoring,
  no validation, no ledger, nothing else in this list is involved.
- Stock model, not house-trained, not related to tomom.

### 2. tomom@qroq

- Location: `#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/`
- A genuinely separate, from-scratch, hand-built neural net in C: its
  own vocab model, attention mechanism, MLP layers, Mixture-of-Experts
  chatbot (`chatbot_moe_v1.c`), meta-RL curriculum orchestrator, own
  weight files (`mlp_model.v.txt`, `attention_model.v.txt`, etc).
- The `@qroq` in its name refers to Groq's API, used somewhere in its
  own pipeline (its own docs, not this one) — unrelated to Gemma.
- `AI.md`'s own line: "Tomom is the learner in the design... Gemma is
  not maintaining the tree." Gemma does not run inside tomom, and
  tomom does not run inside Gemma.
- Dormant, not wired into robot-chat, co-lab-hai, or open-hai today.

### 3. OpenRouter models (via open-hai)

- Config/key: `&.widgits/open-hai/state/openrouter_api_key.txt`.
- Manager: `&.widgits/open-hai/ops/khtpm_open_hai_manager.c`
  (`g_models[]`, `BACKEND_OPENROUTER`).
- Real, verified-live free-tier models as of 2026-09-29:
  `nvidia/nemotron-3-ultra-550b-a55b:free`,
  `poolside/laguna-s-2.1:free`, `dots-studio/dots-3-note-preview:free`
  (see `12.calendar/2026-09-29/2do.md`).
- Genuinely capable of tool calls (`list_dir`/`read_file` schema
  proven live) — the only one of the three with real tool-use today.

## Where "Gemma trains tomom" actually comes from

There is no separate pipeline by that name. What you're thinking of is
the **TEARIT / Concept Bank Pipeline**
(`08-roadmap/design-docs/PIPELINE-EMOJI-DIAGRAM-REVISED.md`):

```
WATCH LAYER  → raw observation
GEMMA (constrained, gemma3:270m)  → pick from real node list, fixed format
   (2026-09-29 addendum: OpenRouter can also participate here, "through
   open-hai it helps bootstrap and accomplish the work: train a bank
   layer, hand-tune a tomom weight, place an event-command brick, and
   make tool calls" — PIPELINE-EMOJI-DIAGRAM-REVISED.md §2)
SCORER (C code)      → fixed-format line -> EDIT record, no NLP
VALIDATOR (C code)   → schema + bounds check
PROMOTION LEDGER     → Laplace-smoothed evidence (A-TEARIT-IS-ALL-YOU-NEED.md §2.5)
LIVE STATE           → Concept Bank + FSM/GOAP/Events (*may* include a tomom weight)
```

A tomom weight is one *possible promotion target* of this pipeline —
not the pipeline's name, and not something Gemma does directly or
exclusively. **This pipeline is real design, not running code**: the
promotion ledger (the middle gate) has zero implementation and zero
logged observations for any entity as of 2026-09-29 (see
`&.widgits/concept-bank/AUTO-PROMOTION-RULE.md`).

## The one thing to remember

Robot-chat's "Chat-gemma" button is backend #1 with **none** of the
pipeline above attached. If a future "Chat" button is meant to
actually feed the TEARIT/Concept Bank pipeline (i.e. actually
contribute toward training/promoting something, tomom or otherwise),
that is new, unbuilt wiring — not something the existing chat buttons
already do.
