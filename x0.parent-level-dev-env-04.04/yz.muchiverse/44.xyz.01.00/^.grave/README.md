# ^.grave — the quest board (🪦)

The gravestone. Every task for an AI worker is **written on it** as a quest folder. Design:
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md` (read it first). Working name; may be renamed.

- Board (table of all quests): `quests/INDEX.md`
- Post a quest: copy `quests/_TEMPLATE/` to `quests/Q<nnn>-<slug>/` and fill it in.
- A **handoff to any outside agent is a quest**: its `QUEST.md` carries the whole onboarding packet, so the prompt you paste is
  "do quest Q<nnn> in ^.grave/quests/..." plus nothing else. The manager (Claude) writes, assigns and verifies quests.
- Quests that no ghost (babysitter, `^.ghost`) manages live here and stay here; failed and abandoned quests are never deleted.

## Rules every quest shares (the agent MUST follow these)

These come from `AGENTS.md` at the repo root; they are repeated here so a quest is self-contained.

1. **Your own branch, your own paths.** Commit only on your tool's named branch (`claude`, `opencode`, `kilo`, `grok`, `hai`, `codex`).
   Never commit to `main` or another tool's branch. Never merge, cherry-pick across branches, push, force-delete or `git stash`
   unless the quest says so.
2. **Stage by path, then read the list.** `git add <path>` per file (never `git add -A`); run `git diff --cached --name-only` and make
   sure it is exactly your files. Runtime state (`*.pid`, `*.pdl` state, logs, `cli_io_state.txt`) is noise: never commit it.
3. **Never report "done" without evidence:** a fresh build, a fresh run, and the real output (diff, receipt, screenshot, state file).
   A clean compile is not evidence.
4. **No secrets in git.** API keys live in ignored `state/*_api_key.txt` files only.
5. **Weak CPU machine:** wrap multi-minute work in `nice -n 15 ionice -c3`. Never loop over `/proc` in shell with a fork per pid.
6. **Read the docs the quest lists before writing code**, and the house standards skill/docs it points to. If a compliant pattern
   already exists for a sibling, use it; if unsure, ask the manager instead of improvising.
7. **Log as you go:** append a line to the quest's `## Log` for every meaningful step; finish with a `## Result` section.

## Status values

`open` → `claimed` → `active` → `review` → `done` | `failed` | `abandoned`. Only the manager sets `done`.
