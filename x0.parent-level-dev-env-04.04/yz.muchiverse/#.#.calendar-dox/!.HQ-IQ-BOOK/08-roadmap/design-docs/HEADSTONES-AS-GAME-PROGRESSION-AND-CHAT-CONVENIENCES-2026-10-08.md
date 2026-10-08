# Headstones as game progression, and chat-window conveniences (fork chats and more)

Status: **NOTES + PROPOSALS, nothing built** (section 1 updated with the owner's answers, same day). Written 2026-10-08 by Claude from the owner's brief. Sections marked **OWNER** are the owner's words or decisions; sections marked **PROPOSAL** are mine and are there to be accepted, changed or struck. Section 5 is empty on purpose: it is where the owner's nuances go.

Owner brief (2026-10-08): "in future games, 'headstones' will be how the user progresses through games. We have it as the quest board in h-ai. I want to document some nuances I will want to add to chat windows, like 'fork' chats; and common/advanced other conveniences."

## 1. Headstones as progression (OWNER direction, grounded in what exists)

**What exists (checked 2026-10-08):** `^.grave/` is the quest board. Each quest is a folder `quests/Q<nnn>-<slug>/` with a `QUEST.md` (the whole spec and onboarding packet), a `## Log` and a `## Result`; `quests/INDEX.md` is the table; failed and abandoned quests are **never deleted**. The h-ai menu row 5 "Quest board" shows it. Design: `GRAVEYARD-GHOSTS-DESIGN.md`, `ROBOT-WORKFORCE-GAMEPLAN-AND-PLAYBOOK.md`.

**The direction:** the same object becomes the player's progress in games. A headstone is a record of something attempted, with its spec, its check, its log and its outcome, kept whether it succeeded or failed.

**PROPOSAL, why that fits:** the shape already has what a progression system needs: an id, a deterministic check (`quest_check`), an owner, a status, and an append-only history. A game quest ("harvest 10 grain") and a robot quest ("write this op") differ only in who the owner is and what the check reads. The goals board proposed for entities (`GOAL | entity | text | check | status`) is the same row, so **one format serves robots, entities and players.**

**OWNER answers (2026-10-08):**
- **A stone can hold many quests.** A stone is a container, not a single task. The current board's one-folder-per-quest becomes: a stone folder with many quest entries inside it.
- **A stone can become a communication board.** It carries a comments section between the quest's creator and its takers, kept with the quest (so the conversation that solved it is stored with it).
- **Many people can take the same quest.** Taking a quest is a claim row, not an exclusive lock.
- **The quest's pal script decides if the stone lives or dies when the quest is done.** The completion rule is code the quest brings (a `.pal` plus its deterministic check), not a global rule: the same stone may persist (a standing board), be archived, or be consumed, depending on its script.
- **Where they live depends on where they apply:**
  - quests on the **taskbar quest board** (the h-ai Quest board row) are in **user data** (the user's desk folder, never in git);
  - quests on **levels and desks** (inside a game) are in the **book/pages** (the session's book:page structure, `EDEN-PLAYABLE-LOOP-AND-BOOK-PAGE-HARNESS-PLAN.md` section 1), so they travel with the level.

**PROPOSAL: data model that fits those answers** (append-only, house style; nothing is edited in place, so many takers cannot conflict):
```
<stone>/
  STONE.pdl        META | id | title | creator | scope=user|book:page | created
                   SCRIPT | quest.pal | <path>          # decides lives/dies; see below
  quests.pdl       QUEST | <qid> | <text> | <check op + args> | <reward> | <state>
  claims.txt       CLAIM | <qid> | <taker> | <ts>        # append-only; many rows per qid allowed
  comments.txt     COMMENT | <ts> | <author> | <qid or *> | <text>   # append-only board
  results.txt      RESULT | <qid> | <taker> | <ts> | <check verdict> | <evidence path>
  log.txt          every state change, one line (the existing quest ## Log, as data)
```
- **Taking:** append a `CLAIM` row. No lock. Whether the first finisher closes the quest for everyone, or each taker is judged separately, is the script's choice.
- **Done:** the check op runs per `RESULT`; then `quest.pal` is executed with the stone's rows and returns one verdict line: `STONE | live`, `STONE | archive`, or `STONE | consume`. **Consume/archive never delete files**: they write a `CLOSED` row and hide the stone; this follows the house rule that finished and failed things are kept.
- **Script safety:** the pal runs through the same fork/exec/waitpid-with-watchdog pattern as other ops (a hung script must not freeze the VM), with a read-only view of the stone and write access only through the verdict line.
- **Comments board:** reuses the chat-window machinery in section 3 (append-only file, cursor reads), so a stone's comments can be forked, searched and pinned like any chat.
- **Two homes, one format:** the user-data board and the book/page board use the identical folder shape; only the root differs. The Quest board window shows the user-data root, and a level shows the stones in its own pages. **Unverified:** the exact on-disk book:page paths; I read the plan doc, not the code.
- **Migration:** existing `^.grave/quests/Q*` folders are stones with one quest. A converter wraps them with no data loss; the INDEX.md table becomes a projection.

**Open (for the owner):** do takers see each other's comments by default or only the creator's? Can a taker abandon a claim (an `ABANDON` row), and does that count against their grade? Can the script ask for the stone to be re-posted (a standing quest, like a daily chore)?

**Harness cases (write first):** two takers claim the same quest and both appear; `quest.pal` returning live/archive/consume writes the right `CLOSED` or no row and removes no file; a hung script is killed at the watchdog and the stone stays live; comments append in order and the creator's and taker's lines both survive; a converted legacy quest reads identically before and after. Mutant: make consume delete the folder, and the case must fail.

## 2. What the chat windows have today (checked)

| Window | What it does | Evidence |
|---|---|---|
| `chat-hai` (h-ai chat) | personas, memory, audit trail, per-session state; the HORN sessions list is `&.widgits/open-hai/state/sessions.state.txt` | files in `&.hq-apps/chat-hai/` |
| `co-lab-hai` | a live room for several agents and the owner: approve/reject gate, `@agent` / `@everyone` addressing with real per-agent visibility, word-wrap, **"+ New session"** which starts an empty room while every old session stays clickable by date | `&.hq-apps/co-lab-hai/USER-FAQ.md` |
| `robot-chat` / entity chat | chat with an entity; history in the entity's `chat_history.txt` | `pals/*/chat_history.txt` |
| `irc-chat-hq` | IRC client window (network HQ effort) | `&.hq-apps/irc-chat-hq` |
| `h-ai-lab` | the HORN/HALO lab | taskbar h-ai row 4 |

Known gaps already recorded: co-lab-hai has no sidebar scroll region (fine for about 6 agents) and no dropdown menu; no relay-driven human session has been run from h-ai to a delegated worker and back.

## 3. Fork chats (OWNER named it; the design below is a PROPOSAL)

**What a fork is:** from any message in a conversation, start a new conversation that shares everything up to that message and then diverges. The original is untouched. It is how you try "what if I had asked it this way" without losing the thread.

**Why this house can do it cheaply:** every chat is already an **append-only file** and sessions are separate folders that are never deleted. So a fork is a new session folder plus a pointer, not a copy:
- `fork.pdl` in the new session: `PARENT | <session id> | <message index or byte offset> | <timestamp>`.
- The reader shows parent lines up to the fork point, then the fork's own lines. Reading is by cursor/size, never `mtime`.
- No parent line is ever rewritten, so there is nothing to merge or lock.

**Behaviours to decide (each is a small, testable rule):**
1. *Fork point:* any message, or only the owner's. Default proposal: any message.
2. *Persona and memory:* the fork inherits the parent's persona and its memory **as of the fork point**, and writes new memory only to itself.
3. *Lineage view:* the sidebar groups forks under the parent (a tree, collapsed by default), shows the fork point, and lets you jump between siblings.
4. *Delete or archive:* never delete; archive hides it. (House rule: failed things are kept.)
5. *Cost:* a fork replays context to the model. With free-tier limits (200,000 tokens/day per model on Groq) the window should show the token estimate before sending, and let the owner trim old lines from the *fork's* context without touching the parent.
6. *Merge back:* none in v1. Copying a message from a fork into another session is the only crossing.

**Harness:** a pal case that creates a session, forks it at message 3, appends to both, and asserts the parent bytes are unchanged, the fork reads parent lines 1-3 plus its own, and an append to the parent after the fork does not leak into it. Mutant: make the fork copy-by-reference read the parent's *current* end.

## 4. Other conveniences (PROPOSALS, grouped; the owner picks)

**Common (cheap, high value):**
- Edit and resend a message (implemented as a fork from the line before it).
- Regenerate a reply (also a fork-sibling, not an overwrite).
- Copy a message or a code block; copy the whole thread.
- Pin or star a message; jump list of pinned lines.
- Search inside a conversation and across sessions (by cursor-read, no index needed at this size).
- Rename and tag a session; sort by date, tag or persona.
- Draft that survives a restart (per-viewer convenience, not shared state).
- Visible token/cost estimate and which provider and model answered (the backend already logs this per reply).
- Keyboard-first: every control reachable by nav number or key (also what makes it relay-testable).

**Advanced:**
- Compare two forks side by side (same prompt, two models or two personas); a score row per reply feeds the delegation bank's feedback ledger (reward/punish, Laplace grade).
- Attach a chat to a headstone or an entity goal, so the conversation that solved a quest is stored with its stone.
- Slash commands (`/fork`, `/pin`, `/summarize`, `/quest`) that turn a message into an op call through the existing `cli_io` path.
- Summarize-and-continue: collapse old lines into a summary row in a new fork when context is too long, keeping a link back.
- Per-session tool permissions (read-only, can post a quest, can run an op) matching the autonomy-0 gate used elsewhere.
- Shared rooms with `@` addressing (co-lab-hai already has this) available for any chat.

## 5. OWNER nuances (to be filled in)

_Empty on purpose. Add each item as: **what you want**, **when it should feel like** (one sentence), and **what must never happen**. I will turn each into a row in section 3 or 4 with a harness case._

## 6. How it would be built (PROPOSAL, in order)

1. A shared chat-session reader/writer as a text-included `_shared-lib` core (one place that knows session folders, cursors and `fork.pdl`), with the fork harness above. Tier M.
2. Fork action in `chat-hai` and `co-lab-hai` (menu row + `cli_io` command), lineage in the sidebar. Tier M with review, because both touch the shared renderer vocabulary only through existing tags.
3. Edit/resend and regenerate on top of fork. Tier M.
4. Search, pin, rename/tag. Tier W (data + parser behind a harness).
5. Headstone attachment, once the goals board exists. Needs the owner's answers in section 1.

Visibility test (the house standard): each step ends with a window the owner can click, driven once through the relay, with a frame dump and a text state that agree.
