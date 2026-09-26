# hard-vvar-agent-Q0000 — vvarware design (v1 scaffold)

> The "write other bots" dev-agent. v1 = basic working versions of all
> 8 features, deep versions in v2. Scaffold ALL of it now, iterate later.
>
> Direct instruction (wussup-vvarqoo.txt): "let it battle/move around.
> but also let it learn, build itself. charge. buy batteries, chat,
> write other bots. etc" + "use gemma1b if u need to try 2 use
> groq/gemini" + "i also may want a right-sidebar continuous progress
> report".

## Brain
- LAN llama 3.2 3B via ollama: `http://10.0.0.187:11434/api/chat`,
  model `llama3.2:3b`, native `tools[]` schema (the 045
  send_message.c ollama-branch shape — proven working in groq-ollama).
  This is a LAN target, not a local model file.
- Tool dispatch is deterministic (045 execute_tool.c pattern): the
  model emits `{"tool_calls":[{function:{name,arguments}}]}`, the
  monad parses it, runs the matching op, appends the result to the
  context log, and loops the model with the result. No prompt-engineered
  JSON — 3B has a real tool schema.
- Fallback brain: gemma3:270m (same host) for lightweight "what now"
  goal selection when 3B is busy. Never blocked on one model.

## The 8 features (v1 = basic)
| # | feature      | v1 op (ops/ dir)                          | how it works                             |
|---|--------------|--------------------------------------------|------------------------------------------|
| 1 | battle       | `battle.c` / `battle.sh`                   | pick a muchi entity, HP-exchange tick vs its `hp.txt`/`hp_max.txt`, log to ledger |
| 2 | move         | `move.c` / `move.sh`                       | roams a simple grid, records `pos.txt`, bumps into entities |
| 3 | learn        | `learn.sh` (wraps 045 generate_corpus)     | turns new facts into a corpus file, feeds it to itself next round |
| 4 | build-itself | `build_self.sh`                            | edits its own `ops/`/`pieces/` sources within the monad sandbox only |
| 5 | charge       | `charge.sh`                                | spends stored energy to refill its battery |
| 6 | buy-batteries| `buy_batteries.sh` (wraps muchi mr_change_gold) | spends gold on battery packs |
| 7 | chat         | `chat.sh` (wraps send_message)             | real LLM chat turn, appended to master_ledger |
| 8 | write-bots   | `write_other_bots.sh` (wraps opencode_ask) | spawns a new bot: writes a fresh `entities/<name>` dir + meta.pdl + ledger |

## State & ledger
- `pieces/brain/state.txt` — key=value brain state (battery, gold,
  energy, xp, level, pos_x, pos_y, ai_state).
- `pieces/brain/master_ledger.txt` — the ONE progress ledger every
  feature appends to (house convention, `[ts] EventType: details |
  Trigger: <op>`).
- `pieces/brain/goal_queue.txt` — pending goals the brain picks from
  (the "what do I do next" loop).

## Live report widget (right-sidebar continuous progress report)
- New widgit `&.widgits/vvarware-report/` — a small X11/GL window that
  tails `pieces/brain/master_ledger.txt` (last N lines, live refresh)
  so the bot's progress is always visible on the right side of the
  desktop.
- Same file-mediated pattern as every other widgit (own op + binary,
  reads the ledger file, redraws on a timer).

## Self image
- `button.sh window` now seeds `entities/self/assets/robot.png` from the
  supplied `_.qoo+.png` image and points `asset.pal` at it, so the desktop
  window should open as a robot sprite instead of a plain glyph stub.
- `tool_chat.sh` is the tooled chat path; it uses the same LAN Ollama
  brain loop and can emit tool calls.

## Ops copy rule (self-contained, avoid 00.10-style path rot)
- Copy 045's proven ops INTO this monad (sources + `+x/`): 
  `send_message.c`, `execute_tool.c`, `json_parser.c`, `cmd_exec.c`,
  `file_ops.c`, `list_dir.c`, `web_search.c`, `generate_corpus.c`,
  `opencode_ask.c`, `tts_speak.c`, `compose_frame.c`.
- Rebuild them in THIS monad's `ops/+x/`. No absolute paths anywhere;
  every op resolves its own root from the monad dir or PRISC_PROJECT_ROOT.
- Cross-project references only through relative hops that are verified
  (like muchi's `open_event_ez.sh` math).

## Monad layout (mirrors book-stack/muchi-pet)
```
*.monads/*.hard-vvar-agent-Q0000/
├── button.sh                  # house-standard monad entry (run|window|kill|check|help)
├── pieces/brain/              # brain loop: state.txt, master_ledger.txt, goal_queue.txt, run.sh
├── pieces/brain/corpus/       # learned corpus files
├── ops/                       # copied+own op sources
├── ops/+x/                    # compiled binaries
├── entities/                  # bots written by feature 8 (and its own self dir)
└── hard-vvar-q0000.md         # this doc
```

## v2 (deferred)
- Deep battle (real move/attack tiles, actual muchi damage apply)
- RL curriculum training on this monad's own ledger
- Multi-model routing (gemini/groq when available)
- Report widget interactivity (click lines, drill into ops)

## Milestones
1. brain online: llama3.2:3b reachable from monad, tool loop works
2. all 8 v1 ops present + rebuilt, each writes to master_ledger
3. button.sh + scrypts wrapper (`scrypts.sh vvar ...`)
4. report widget tails the ledger
5. verify: check passes, openall idempotent, one window, ledger growing
