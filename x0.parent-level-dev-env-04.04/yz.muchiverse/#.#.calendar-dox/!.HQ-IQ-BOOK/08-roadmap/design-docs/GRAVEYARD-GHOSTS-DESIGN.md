# Graveyard + Ghosts — the AI work-management toy (design, 2026-10-06)

**Status:** design + first quests written; nothing built yet except the folders and docs.
**Owner of the idea:** the owner (2026-10-06). **Manager of the work:** Claude (appointed by the owner to lead handoffs, appoint
bots, read their history and give them training events as harness runs).
**Working names** (owner: "just for fun, we can think of different name later"): `^.grave` (the quest board, a gravestone),
`^.ghost` (the babysitter, the ghosts it sends out). Both live at the house root next to `^.hai-horn`.

## 0. Owner's words (kept verbatim so the intent survives)

> "it should be an emoji, so it will be a 'grave yard' for the 'quest board written on it' and it should send out 'ghost emojis'
> that it manages that do all the work ... from the grave stone I can see all tasks, all ghosts, and history of all ghosts.
> ghost will appear as entities on desktop / pc-hq to test entities, do things, tend to entities, etc; visually."
> "^.grave = quest, ^.ghost = babysitter." "horn and or students" first. Quests not run by a babysitter go on the board.
> "that's the selling point of this house" — organize it intuitively, accessible to the user too, with dashboards.

## 1. The idea in one paragraph

A **gravestone** 🪦 is an entity (a pal, like every other thing in this house) whose face is a **quest board**: every open, active,
finished and failed task is "written on it". From it the owner can see all quests, all ghosts and every ghost's history. The
gravestone **sends out ghosts** 👻: small entities, each with a brain (a HORN worker or a local student model), a current quest and an
append-only history. Ghosts appear on the desktop and in pc-hq as ordinary entities and visibly do things: test other entities,
tend to them, run a quest. Claude is the **manager**: appoints ghosts, hands them quests, reads their history, and can hand them
training events (FSM / IRL / RL) as harness runs.

## 2. Why this is not a new subsystem (reuse map — read before building)

`08-roadmap/design-docs/robot-chat/ROBOT-CHAT-BLUEPRINT.md` §1: *"A robot is a pal (desktop entity) like any other ... There is no separate
'bot subsystem.'"* The same rule applies here. Everything below already exists:

| Need | Already in the house | Where |
|---|---|---|
| An entity with a glyph, a menu, a desk position | pal folders (`pal.pdl` glyph, `meta.pdl` methods, `desktop_pos.txt`), spawned from `DESK` rows | `xyzfs/users/<uuid>/home/livedesk/pals/`, robot pal `robot_chat_001` |
| A ghost "doing something" | registered event COMMANDs, zero-recompile | `#.ref/menu/event_commands.registry.pdl` (blueprint §2.2) |
| A brain for worker ghosts | HORN: multi-provider, tool calling (`list_dir read_file grep_files edit_file write_file run_script`), approval gate for write/edit/exec | `^.hai-horn/` (`horn_turn`, `horn_tool_exec`, `tools/horn_tools.json`) |
| A brain for student ghosts | local Ollama over the LAN | `&.widgits/open-hai/` (host and model are hardcoded today: quest Q002) |
| A task queue with real pass/fail | FSM plan queue: `plans/queue` → `done` / `failed` by REAL exit code, one at a time | `%.harnesses/harnecient-fsm/run_queue.sh`, `run_plan.sh` |
| Deterministic scoring, "joints" (hand-set weights) | harness pipeline §3.3, §7.4 | `08-roadmap/design-docs/HARNESS-DELEGATION-PIPELINE.md` |
| A dashboard window, nav-numbered, restylable | layout files + `<overlay>` + nav index (built 2026-10-06) | `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md`, `08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md` |
| The "babysitter" idea itself | 👻 pal with a custom user-editable dashboard (feeding / cleaning / fighting) | `RUSSIAN_DOLL_HOUSE_DESIGN/AGENT_ROADMAP_ANSWERS.md` §10 |
| Hierarchy boss > managers > workers > students | owner > Claude/Grok > OpenRouter (HORN) > local models | `RUSSIAN_DOLL_HOUSE_DESIGN/0.my-concerns.md` |

## 3. The cast

- **🪦 Gravestone (`^.grave`)** — the quest board. One entity. Its window has three faces (Quests | Ghosts | History).
- **👻 Ghost (`^.ghost/roster/<ghost-id>/`)** — one per appointed bot. A pal with a glyph, a tier, a brain, a current quest, a history.
- **Manager (Claude)** — appoints and retires ghosts, writes and assigns quests, reads histories, schedules training events.
- **Owner** — sees everything, can post a quest, appoint/retire, replay history, approve gated actions.
- **Tiers** — `worker` (HORN, via OpenRouter and the other providers) and `student` (local model, in training to become a worker).
  Outside agents (Kilo, Grok, ...) keep working in their own branches and receive **quests with onboarding packets**; they do not
  need to be ghosts, but a ghost can wrap one so its history lands on the same board.

Babysitter mapping (the original idea, applied to bots): **feeding** = give a ghost quests, context and training events;
**cleaning** = review, verify, revert what it produced; **fighting** = benchmark ghosts against each other (the IRL arena).

## 4. Data model (plain files, append-only where history matters)

```
^.grave/
  README.md                     how the board works, rules every quest shares
  quests/
    INDEX.md                    the board as a table (the dashboard reads the same data)
    _TEMPLATE/QUEST.md          copy this to post a quest
    Q001-<slug>/QUEST.md        one folder per quest; QUEST.md = status header + prompt + onboarding + log
^.ghost/
  README.md
  roster/
    _TEMPLATE/                  copy this to appoint a ghost
    <ghost-id>/ghost.pdl        tier, brain, glyph, state (idle|questing|resting), current quest
    <ghost-id>/history.log      append-only: one line per event (assigned, started, tool call summary, passed/failed, verdict)
```

Quest status values: `open` → `claimed` → `active` → `review` → `done` | `failed` | `abandoned`. Only the manager moves a quest to `done`
(after verification: fresh build + fresh run + evidence, per AGENTS.md "Verification"). `failed` and `abandoned` are never deleted:
they stay on the stone, that is the "graveyard".

## 5. The gravestone window

A livedesk window built from layout files (the same machinery as the in-game menus), every interactive element nav-numbered:
- **Quests** — the board: id, title, tier needed, status, assignee. Actions: post a quest, open one, assign, change status.
- **Ghosts** — the roster: glyph, tier, state, current quest, last event. Actions: appoint, retire, wake, send out onto the desktop.
- **History** — pick a ghost or a quest, read its append-only log; replay is reading in order.

## 6. Ghosts on the desktop / in pc-hq

A sent-out ghost is a normal entity (a `DESK` row + pal folder). It does visible work through COMMANDs it is allowed to run:
test an entity (open it, drive it by the relay, take a frame dump, write the verdict to its history), tend to an entity, run its
quest. First visible behavior to build: **a ghost that opens one named entity, drives it through the relay, and reports pass/fail on
the stone.** That is also the first real use of ghosts to test the house itself.

### 6b. Many graves, many ghosts — placed from h-ai like the palette placer (owner, 2026-10-06)

There can be **multiple graves** (several quest boards, e.g. one per project) and **multiple ghosts**. They are *placeable*: a palette
in the **h-ai menu** (taskbar cell "ai") lists the available graves and ghosts; clicking one makes it the brush and clicking a
cell places it on the desktop or in pc-hq — exactly how the palette placer places tiles/emojis today.

Real mechanism to reuse (do not reinvent): `&.widgits/tile-picker/ops/tp_place_desktop.c` —
`tp_place_desktop.+x <widget_state_dir> <desktop_root> [glyph] [name]` turns a glyph into a **real persistent pal** (own package dir
under the session's `pals/`) plus a **real `DESK` row**, the same shape `livedesk_place_pal()` produces for every other entity, so the
existing reload path (`livedesk_spawn_desk()`) brings placed graves/ghosts back after a reset with no new code. The palette side is
the palettes widget (`&.widgits/palettes/`, driven by `*_options.txt` files like `emojis_options.txt`): add a `graveyard_options.txt`
(🪦 grave variants, 👻 ghost variants) and the h-ai menu entry that opens it.

Consequences for the data model: a grave has its own board (`quests/` scoped to it, so each placed 🪦 points at one board folder and a
ghost points at its roster folder via its pal's `pal.pdl`); the default board is `^.grave/quests/`. A ghost may be placed (visible,
questing) or unplaced (resting in the roster). Placing a ghost is what "sends it out".

## 7. Training events (RL / IRL / FSM) — deliberately later, but designed in

A **training event** is a quest whose scoring is deterministic: an FSM plan dropped in `plans/queue`, run by the existing runner,
filed to `done`/`failed` by its REAL exit code, with the result appended to the ghost's history. This is the harness pipeline's
"Stage 0" (§7.2): collect real data and hand-set "joints" before any ML. RL/IRL weights come only after enough verified history
exists; the arena ("fighting") is two ghosts on the same plan, scored by the same deterministic check.

## 8. Phases

0. **Now (done with this doc):** folders, templates, first quests (HALO_CHAT, open-hai provider config, build-gate fix, /proc audit).
1. A static gravestone + ghost pals; the board window reads `quests/` and `roster/` (read-only dashboard).
2. A ghost runner: one worker ghost takes one quest through HORN (`horn_turn`), logs to `history.log`, writes the status header.
3. History view and quest posting from the window (cli_io, nav-numbered).
4. Student ghosts (local model) and the first training events through the FSM queue.
5. Visible ghosts on desktop/pc-hq: the entity-testing ghost from §6.

## 9. Safety rules (non-negotiable, from AGENTS.md and the house)

- A ghost writes only inside its quest's scope, on its own branch or worktree; never `main`, never another tool's branch.
- HORN's approval gate for write / edit / exec stays ON for ghosts; the manager (or owner) approves.
- No API keys in git, ever (two were committed once; fixed 2026-10-06, keys still in history: rotate).
- CPU: heavy ghost work runs `nice -n 15 ionice -c3` (weak-CPU machine).
- Spend limit per ghost for paid providers (open question 4).

## 10. Open questions for the owner

1. Do ghosts get a **separate git worktree each** automatically (safest, per BRANCH-STRATEGY.md), or share one?
2. Who approves a ghost's gated actions: the manager, the owner, or the manager with owner-visible log?
3. First ghost and first quest to send out: HALO_CHAT (Q001) or the build-gate fix (Q003, a small safe warm-up)?
4. A per-ghost spend cap for paid providers, and where it is configured.
5. Glyph per tier (e.g. 👻 worker, 🫥 student) or one ghost glyph for all?
6. Should the gravestone accept quests typed by the owner directly in its window (cli_io), or only via files at first?
7. Final names (this doc says `^.grave` / `^.ghost` "for now").
