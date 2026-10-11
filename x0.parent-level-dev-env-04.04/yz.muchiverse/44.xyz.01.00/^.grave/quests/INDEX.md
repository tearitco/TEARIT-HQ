# The board (what is written on the gravestone)

Manager: claude. Status flow: open → claimed → active → review → done | failed | abandoned. Only the manager sets `done`.

| id | quest | tier | size | status | assignee | blocked on |
|---|---|---|---|---|---|---|
| [Q001](Q001-halo-chat-v01/QUEST.md) | land HALO_CHAT v0.1 — it already exists on origin/opencode (`4b72a4bd7`); review, verify, decide auto-promotion, cherry-pick | manager / worker | M | review | claude | ported in worktree (branch `claude-halo-pull`, `9028e0706`); needs live run, HORN e2e, owner OK on auto-promotion |
| [Q002](Q002-open-hai-provider-config/QUEST.md) | open-hai provider config (reuse HORN provider table) | worker / outside-agent | M | open | - | which providers in the UI |
| [Q003](Q003-build-gate-include-list/QUEST.md) | build gate misses #included files (stale-binary bug) | student / worker | S | DONE 2026-10-06 (both gates PASS; `verify.sh`, `gate_test.sh`) | - | - |
| [Q004](Q004-proc-shell-loop-audit/QUEST.md) | replace per-pid shell loops over /proc | student / worker | S–M | open | - | - |
| [Q005](Q005-phone-in-every-inventory/QUEST.md) | a phone (+ entity uid, number, wallet id) in every entity's inventory, new and retroactive | manager / outside-agent | M | mostly done: 55 phones + uids live; HUD sprite and start-time measurement left | claude | - |
| [Q006](Q006-one-board-layout/QUEST.md) | one generic board layout for grave, roster, server, phone windows | manager / layout-capable agent | M | built + scored headless (13/13); not shown live, no menu link | claude | owner to look at it |
| [Q007](Q007-user-data-branches/QUEST.md) | per-user data branches: take `xyzfs/users` out of code history (`user/jb`), `jb` integration branch | manager / careful outside-agent | L | steps 1-6 and 8 done on `claude`; other branches + `jb` code branch left | claude | data branches are LOCAL ONLY |
| [Q008](Q008-retire-old-horn-transport/QUEST.md) | retire the old HORN transport `horn_chat_openrouter` (4 scripts still call it; port them, then delete) | worker / outside-agent | S–M | open | - | opencode vs claude: who does it |
| [Q009](Q009-first-events-phone-send-and-route/QUEST.md) | first events: `phone.send` + `server.route` with tunables and a ledger, proven by a verifier | worker / outside-agent | M | open | - | - |
| Q010 | HORN error reporting: every provider failure logged + exit classes 0-6, 429 marks exhausted (`horn_chat_backend.c`; harness `quest_q010_horn_errors` 44/0; live check: bad key -> exit 4, `http=401`) | W | S | DONE 2026-10-07 (merged `1a5326372`) | free-model agent (Claude Sonnet worker) | - |
| Q011 | `quest_check`: gate that decides whether a delegated attempt counts (lock, scope, base, budget, attempts); harness `quest_q011_quest_check` 127/0 | W/M | M | DONE 2026-10-07 (merged `9f35cd917`) | agent | - |
| Q012 | `ghost_run`: one attempt of one quest end to end (precheck -> fresh worktree from tip -> backend -> extract code -> quest_check -> harness -> attempts/NNN) | M/W | L | DONE 2026-10-07 (merged; harness `ghost_run` 197/0 from a clean build; fake backend only, no real provider call yet) | agent | - |
| Q013 | `quartermaster`: check/record free quota, cpu slots, review backlog, spawn limits before/after each attempt | W | S-M | DONE 2026-10-07 (merged; harness `quartermaster` 63/0) | agent | - |
| Q014 | measure Groq free-tier limits from headers (`quest-pilot/q014-groq-limits/observed.md`: 1000 req/day, 8000 tok/min per model) | D | S | DONE 2026-10-07 | manager | - |
| Q015 | Tester ghost v0: open one named entity via the relay/state files and report pass/fail on the stone (screenshots are blocked on this machine: use state files) | D->S | M | open | - | Q012 |
| Q016 | `desk_restart`: safe remote restart (preflight refuses before killing, login shell, explicit display, UP/DOWN/TIMEOUT); harness `desk_restart` 47/0 | W/M | M | DONE 2026-10-07 (merged) | agent | - |
| Q017 | `hq-ftp` transfer app (spec `HQ-FTP-LAN-SYNC-SPEC.md`) | W/M | L | open | - | - |
| Q018 | store install op: `git clone` + unpack against a local test repo (design `XYZFS-DISTRIBUTION-VIA-STORE-DESIGN.md`) | W | M | open | - | - |
| Q019 | pilot 3: Eden talk phrases by a free Groq worker behind a deterministic judge (`quest-pilot/q019-phrases`; harness `quest_q019_phrases` 35/0) | W | S | DONE 2026-10-07 (iteration 1) | Groq gpt-oss-120b | - |
| Q020 | pilot 4: `var_cmp` (compare numbers/variables so event pages can branch) by a free Groq worker; harness `quest_q020_var_cmp` 161/0; 4 iterations, 17,054 tokens (see `quest-pilot/q020-var-cmp/quest_ledger.txt`) | W | S | DONE 2026-10-07 as a process pilot; **the op duplicates `variable_math` (do not register)** | Groq gpt-oss-120b | - |
| Q021 | patch-style repairs: send only the failing lines + error, accept a patch/function replacement instead of re-emitting the whole file (measured waste in q020) | W | M | open | - | - |
| Q022 | lessons bank + `prompt_compose` (learned preamble lines with hit counts, token-budgeted) | M/W | M | open | - | see `LEARNING-LOOP-BANKS-WEIGHTS-NO-REPROMPT-DESIGN.md` |
| Q023 | answer bank: `bank_get/bank_put` keyed by sha256(spec+harness+model), harness-verified entries only | W | M | open | - | - |
| Q024 | `weights_update`: attempts ledger -> `W|` rows per (ghost, skill) and per prompt component | M/W | M | open | - | Q012 (done) |

Next ids start at Q025. Copy `_TEMPLATE/` to add one. Design: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md`.
