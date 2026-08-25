# net/TOOLING-MAP.md — which piece needs which in-house tool

House rule: no jq, no python — pure gcc/cc + sh + .pdl. So every "ask a LAN
model" / "parse its reply" need is served by in-house tooling. This file is
the definitive map. Reference implementation to check first when in doubt:
`@.apps/my-lawyer/` (live-verified; the origin of the chain).

## The ask chain (the only way we call a model)

```
1. build request JSON -> file        (C: each app's own json_escaped();
                                      sh: json_str() escaper, no house tool)
2. POST it            -> connect_op.+x <url> <req_file> <resp_file>
                                      (thin C wrapper around curl;
                                       Content-Type: application/json;
                                       --max-time 600; blocks; rc = curl rc)
3. parse the reply    -> json_parser.+x <resp_file> <dot.path>
                                      (tolerant: strips ```json fences,
                                       walks dot-notation incl. [i],
                                       unescapes \n \t \" \\)
4. fallback           -> empty/NULL -> app-side default, never hang
```

## Need -> tool map

| Need | House tool | Where canonical |
|---|---|---|
| HTTP POST (model calls) | `connect_op.+x` | agent45, my-lawyer, qwen.sh |
| HTTP GET (tags / health) | `curl` (direct) | qwen.sh status — no house GET tool exists |
| JSON escape (C) | inline `json_escaped()` | my-lawyer_case_worker.c:74-94 |
| JSON escape (sh) | `json_str()` (bash `${//}`) | qwen.sh — no house sh tool |
| JSON parse | `json_parser.+x` | agent45 (canonical), my-lawyer, 046, my-lawyer/my-biotech copies |
| Model registry | `net/ollama-lan.pdl` (TIER/HOST rows) | qwen.sh reads it |
| Multi-step delegation | `%.harnesses/harnecient-fsm/` (run_plan.sh, run_queue.sh, INCLUDE/BRANCH/LOOP_UNTIL, SET_MODEL/CHOOSE_MODEL) | harnecient-fsm |
| Window testing | relay injection only (`nav.sh`, ASCII-keycode relay files) | #.desktop/harnesses/, %harnesses/* |
| Verification | C assert ops / receipts / real file effects — never trust model output | Phase-4 design, harnecient-fsm |
| FIM / in-file completion | `codeqwen:7b-code` (ollama) | net/ollama-lan.pdl fim tier |

## Component -> needs

| Component | Needs |
|---|---|
| `net/qwen.sh` | connect_op (POST) + json_parser (parse) + curl (GET status) + ollama-lan.pdl (registry). No python/jq. |
| my-lawyer workers | connect_op + json_parser (`message.content`) + personas (`pieces/registry/personas/`) |
| agent45 (send_message/verify_cell) | connect_op + json_parser (dot-paths incl. `candidates[0]....`) |
| h-ai (khtpm_open_hai_render.c) | detached curl child + json_parser (check_pending) + detect_tool() deterministic dispatch |
| harnesses (nav.sh / run_plan.sh) | relay inject + json_parser + assert ops |
| taskbar (khtpm runtime) | no model calls — pure C + sh + .pdl state files |

## Ladder -> model -> host (single source: net/ollama-lan.pdl)

router=0.5b quick=1.5b coder=3b manager=7b fim=codeqwen:7b-code
hosts: mac=10.0.0.144 linux=10.0.0.187 local=127.0.0.1

## Policy reminder (HARNECIENT-HACK.md / H-AI-RELAY rule 6)

- Model generates plain text; the app decides deterministically and verifies
  the real result. Never let the model route tools or classify directly
  (DESCRIBE, don't CLASSIFY).
- Every model step has a non-LLM fallback.
