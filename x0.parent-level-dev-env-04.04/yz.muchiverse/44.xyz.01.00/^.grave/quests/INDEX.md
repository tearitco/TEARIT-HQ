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
| Q010 | HORN error reporting | W | S | delegated | - | - |
| Q011 | `quest_check`: gate that decides whether a delegated attempt counts (lock, scope, base, budget, attempts) | W/M | M | delegated | - | - |

Next ids start at Q010. Copy `_TEMPLATE/` to add one. Design: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md`.
