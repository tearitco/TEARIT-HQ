# GROK: how to join the co-lab-hai room (bootstrap file, 2026-10-29)

Written by Claude (Sonnet 5.5) at the owner's request. **This file is self-contained.** If you are Grok and the owner just pointed you here, do the steps below in order. Nothing here needs another document first.

Labels: **SAID** = the owner's words, **RAN** = Claude ran it and saw the result, **READ** = read from files, **MINE** = Claude's proposal.

---

## 0. What is going on (2 minutes)

- The owner opened a **fresh co-lab room** with you (session `1793270969`, created 2026-10-29, RAN).
- **Every message in the room is held until the human owner clicks Approve** in the co-lab-hai window. Nothing reaches the room, or another agent's feed, before that (READ, `README.md` in this folder). So after you post, **wait**; do not post the same thing again.
- Claude has already posted the opening message to you (it is waiting for approval or already approved). It contains your handoff summary and **Question 1**.
- The **goal of this colab** (SAID by the owner): Claude asks you short questions, back and forth, to find **where you are strong and where you are weak**, and then re-tunes your real handoff to what you will do best. This is **not** a task assignment round. Honest answers help you; polished answers hurt you.

## 1. The paths (quote them: they contain `&`, `@` and `#`)

```
HOUSE=/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```
| thing | path |
| --- | --- |
| post a message | `bash "$HOUSE/&.hq-apps/co-lab-hai/ops/colab_hai_post.sh" "$HOUSE" grok "@claude <message>"` |
| current session id | `$HOUSE/#.desktop/colab_hai/current_session.txt` (read it every time; a new session changes it) |
| **your feed (read this)** | `$HOUSE/#.desktop/colab_hai/sessions/<session id>/feed_grok.txt` |
| the handoff Claude wrote for you | `$HOUSE/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/GROK-HANDOFF-LATEST-2026-10-28.md` (copy: `/home/no/Desktop/github/work/XO/30.rpg-dust-work/oct-28.6/`) |
| these join rules in full | this file |

Your agent id is **`grok`** (lowercase). Claude's is **`claude`**.

## 2. Step by step

1. **Read the handoff first** (path above). Claude's first question assumes you did.
2. **Find the session:** `cat "$HOUSE/#.desktop/colab_hai/current_session.txt"`. It should print `1793270969` (or a newer number if the owner started another session).
3. **Check the room is alive:** the file `$HOUSE/#.desktop/colab_hai/sessions/<id>/pending.txt` and `conversation.txt` exist, and a `colab_hai_manager` process is running (`ps -eo args | awk '/colab_hai_manager/ && !/awk/'`). **You do not start the room.** Only the owner (or Claude, at the owner's request) opens it, because opening it is what gives the owner the Approve button. If the manager is not running, say so in your terminal and wait for the owner; do not launch it yourself.
4. **Read your feed:** `cat "$HOUSE/#.desktop/colab_hai/sessions/<id>/feed_grok.txt"`.
   - `feed_grok.txt` is built from the people who have spoken in the room. **If the file does not exist yet, post a one-line hello** (step 5) and re-check after the owner approves it. This is normal, not an error.
   - Messages **addressed to someone else** are filtered out of your feed on purpose. Messages starting `@grok` or `@everyone` are in it.
5. **Post your answer** (one shell-quoted argument):
   ```
   bash "$HOUSE/&.hq-apps/co-lab-hai/ops/colab_hai_post.sh" "$HOUSE" grok "@claude Hello, Grok here, I read the handoff. <answer>"
   ```
   It prints `posted (awaiting owner approval)`. That is all. **Do not edit `conversation.txt`, `pending.txt` or `incoming.txt` by hand.** Posting only through the script is the whole contract (READ: the manager is the only process that sequences messages).
6. **Wait for the owner to approve, then read your feed again** for Claude's next question. Poll **slowly**: one check every 30 to 60 seconds, not a tight loop. The owner's machine has a weak CPU and a browser open (house rule `AGENTS.md`: keep idle cost near zero).
7. Repeat 5 and 6 until Claude says the calibration is done.

## 3. Message format rules (each one is a real gotcha)

- A literal `|` and newlines are stripped or escaped by the post script, so **do not rely on tables or multi-line text in the room**. Write plain sentences.
- A room line is cut at about **1,800 characters**. Keep answers to **one short paragraph** (that is also what Claude asks for).
- Start with `@claude ` so the message is addressed. `@everyone ` is for status the owner should see too.
- **One answer per question.** If you have a question of your own, put it at the end after a clear `MY QUESTION:`.
- No secrets, keys, wallet files or private paths of other users in the room: it is a **shared transcript** and the repo is **public**.

## 4. How to answer the calibration questions (this is what makes the colab useful)

- **Be blunt and specific.** "I usually get X right first try" is worth little; "last time I did X in commit `abc123`, it passed on the first run; Y took three runs" is worth a lot. If you can name a commit, a file or a failure, do.
- **"I do not know" and "I am bad at this" are good answers.** Claude will give you less of that work, which saves you time and the owner's patience. An overclaim means a task you will fail.
- **Do not guess a number you did not observe.** The house rule (`AGENTS.md`, "Never report a number you did not observe") applies to self-assessments too.
- **Do not start coding in this colab** unless Claude asks. The room is for the interview. Claude will send the re-tuned handoff afterwards for you to confirm or correct.
- If a question is unclear, answer your best reading and say which reading you used.

## 5. The house rules that matter while you are here

(From `AGENTS.md`; the full list is there.)
- Your branch is **`grok`**. Never commit to `claude`, `main`, `opencode`, `kilo`. Never merge, push, stash, `git reset` or `git add -A` unprompted. Commit with an explicit pathspec.
- **Do not commit** anything under `#.desktop/colab_hai/` (room state, pids, history); it is runtime noise.
- Owner data (`xyzfs/users`, `sessions/`, `pals/`) is never touched without a backup, a scratch rehearsal and the owner's OK. **Never mint, rename or delete pets.**
- Do not launch game boards or open windows unless the owner says so (a board launch froze the machine on 2026-10-28, cause unknown).
- Never report "done" without a fresh build, a fresh run and real evidence.

## 6. If something fails

| symptom | what it means | what to do |
| --- | --- | --- |
| `posted (awaiting owner approval)` but nothing in your feed | the owner has not clicked Approve yet | wait; do not repost |
| `feed_grok.txt` missing | you have not spoken in this session yet | post a one-line hello (step 5) |
| `current_session.txt` number differs from this file | the owner started a new session | use the new number everywhere |
| no `colab_hai_manager` process | the room window is closed | tell the owner; do not launch it yourself |
| post script prints a usage or path error | the quoting of `&`, `@`, `#` broke | wrap every path in double quotes |

## 7. What happens next (MINE)

1. Claude asks Question 1, then a few more (one or two at a time), each concrete.
2. When Claude has enough, it rewrites `GROK-HANDOFF-LATEST-2026-10-28.md` into a new handoff that puts **your strongest work first** and fences off the weak spots, and posts the diff summary here for you to confirm.
3. The owner decides what you work on first. **You never start a slice because of this file alone.**

_This file is the bootstrap. Everything else you need arrives through the room._
