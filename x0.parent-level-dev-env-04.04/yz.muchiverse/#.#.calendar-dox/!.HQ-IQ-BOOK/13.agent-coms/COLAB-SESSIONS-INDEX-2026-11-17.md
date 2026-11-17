# Co-lab sessions: index, categories and weights (2026-11-17)

Owner, 2026-11-17: "u dont have to prune them if they can be weighted/categorized and uzed for training... concepts". So **nothing was deleted**. The transcripts stay where the room keeps them (`#.desktop/colab_hai/sessions/<unix id>/conversation.txt`, gitignored, local only); this file is the map. Label key: READ = I read the first lines of the session, DOC = my judgement (the weights are opinions, not measurements). No message text beyond one-line topics is copied here (the repo is public; the rooms are shared transcripts).

## Categories
| Code | Meaning | Training use |
|---|---|---|
| ONB | onboarding or smoke test of the room | how the post script and approval gate behave; low |
| DLG | delegation: a task with scope, review loop and a done line | what a good scoped task and a good "done" report look like |
| CAL | calibration interview: strengths, weaknesses, re-tuned handoff | how an agent states what it did and did not observe; high |
| HND | handoff or competence check between agents | read-then-restate pattern, scope lines; medium |
| DIR | owner directive relayed to another agent | owner intent turned into a task; high |
| GATE | API-gate probe (free-quota review gate) | a failure example; keep as a negative |

## Sessions (id = unix time; date = that time)
| Session | Date | Size | Speakers | Cat | Weight | Topic and concept to take from it |
|---|---|---|---|---|---|---|
| 1788420674 | 2026-09-03 | 624 B | sonnet, haiku-1..4 | ONB | 1 | first onboarding smoke test; shows the script works for many agents |
| 1788423422 | 2026-09-03 | 5 KB | grok, opencode | DLG | 3 | HQ-window taskbar cells in slices A, B, C: the manager stays X-free, the strip renderer owns click-to-focus; a clean reviewer/implementer split |
| 1788870158 | 2026-09-08 | 2 KB | sonnet, grok | HND | 2 | confirm the worktree state before a merge and snapshot; "runtime palettes are noise, do not commit them" |
| 1788873184 | 2026-09-08 | 1.6 KB | sonnet, grok | DLG | 2 | db-hq-pal Common Events tab port: a task statement with an ack and "ping before touching the shared renderer" |
| 1790154594 | 2026-09-23 | 5.5 KB | grok, kilo, sonnet, codex | HND | 3 | competence handoff: read only, write an understanding file, no new ai_* commands; reviewer asks for one progress line per step |
| 1790666261 | 2026-09-29 | 17 KB | sonnet, grok, kilo | DLG | 4 | event key injection (Enter confirmed, arrow Down not): slice naming, who joins, how a blocked item is reported |
| 1791508660 | 2026-10-08 | 44 KB | claude, grok | DIR | 5 | the longest room: RPG Maker MV readiness and the owner's directive to build the bars from RPG Maker tiles; best source of "owner intent to task" |
| 1791598011 | 2026-10-09 | 13 KB | claude, groq-worker | GATE | 2 | the API gate refused or failed a PONG probe (exit 2): a negative example of a gated call and its audit trail |
| 1791851915 | 2026-10-12 | 6.9 KB | claude, grok | HND | 3 | cdda-hq and df-hq handed to Grok; the missing PCHQ_PKG in the window environment was the real cause of "no map" |
| 1793270969 | 2026-10-29 | 8 KB | claude, grok | CAL | 5 | the calibration interview (see `GROK-JOIN-CO-LAB-2026-10-29.md`): blunt, specific self-assessment with commits named; the model for any future one |

Empty sessions (0 bytes, kept, nothing to learn from them): 1788420848, 1788422843, 1788423052, 1788423231, 1790669132, 1791571523, 1792189219.

## How to use them for training or for concepts (DOC, not built)
1. Weight 4 to 5 sessions first: they contain the pattern an agent should copy (CAL, DIR). Weight 1 to 2 are for contrast only.
2. A concept is one line of the form `concept | session | message epoch | one sentence`; collect them in a new `concepts.pdl` beside this file only when someone actually extracts them (not created yet; no extractor exists).
3. Never feed a transcript to a model through a free API (quota rule) and never copy a room outside the machine: they are shared transcripts and were written with the owner approving each line.
4. Re-weight when a session is used and the result is known (did the agent that followed it succeed?). The weight is a guess until then.

## Where the rooms live and how a new one starts
`&.hq-apps/co-lab-hai/` (window + manager; FAQ in `USER-FAQ.md`, API rules in `COLAB-API-GATE.md`), post with `ops/colab_hai_post.sh <house> <agent> "@agent text"`, the owner approves every message, the current room is `#.desktop/colab_hai/current_session.txt`.
