# The board (what is written on the gravestone)

Manager: claude. Status flow: open → claimed → active → review → done | failed | abandoned. Only the manager sets `done`.

| id | quest | tier | size | status | assignee | blocked on |
|---|---|---|---|---|---|---|
| [Q001](Q001-halo-chat-v01/QUEST.md) | land HALO_CHAT v0.1 — it already exists on origin/opencode (`4b72a4bd7`); review, verify, decide auto-promotion, cherry-pick | manager / worker | M | review | claude | ported in worktree (branch `claude-halo-pull`, `9028e0706`); needs live run, HORN e2e, owner OK on auto-promotion |
| [Q002](Q002-open-hai-provider-config/QUEST.md) | open-hai provider config (reuse HORN provider table) | worker / outside-agent | M | open | - | which providers in the UI |
| [Q003](Q003-build-gate-include-list/QUEST.md) | build gate misses #included files (stale-binary bug) | student / worker | S | open | - | - |
| [Q004](Q004-proc-shell-loop-audit/QUEST.md) | replace per-pid shell loops over /proc | student / worker | S–M | open | - | - |
| [Q005](Q005-phone-in-every-inventory/QUEST.md) | a phone (+ entity uid, number, wallet id) in every entity's inventory, new and retroactive | manager / outside-agent | M | open | - | owner OK before touching live entities |
| [Q006](Q006-one-board-layout/QUEST.md) | one generic board layout for grave, roster, server, phone windows | manager / layout-capable agent | M | open | - | - |

| [Q007](Q007-user-data-branches/QUEST.md) | per-user data branches: take `xyzfs/users` out of code history (`user/jb`), `jb` integration branch | manager / careful outside-agent | L | open | - | owner OK before untracking |

Next ids start at Q008. Copy `_TEMPLATE/` to add one. Design: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md`.
