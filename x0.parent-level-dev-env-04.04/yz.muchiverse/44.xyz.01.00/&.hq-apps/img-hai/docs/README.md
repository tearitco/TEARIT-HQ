# img-hai - the text-to-image studio window

**img-hai** is an x11-hq window (an "h-ai" app, built like `chat-hai` and `co-lab-hai`) for making game art with image models without anyone sitting at a terminal. You type words (or pick a part from the word bank), press Draft, and a remote machine draws the picture; the result lands in a palette of tiles that you can place on a canvas and mix live, judge (yes/no by two judges), and accept as a candidate for the game's brixel parts. It can also run an unattended batch of drafts across several machines.

Status on 2026-10-13: **built and working, owner has not yet reviewed it**, nothing accepted into the game yet. See `HANDOFF.md` for exactly what is verified and what is not.

## Read this first, by what you want to do
| I want to... | Read |
|---|---|
| use the window (draft, mix, judge, run a batch) | `USER-GUIDE.md` |
| understand how it is built, change the code | `CODE.md` |
| add a button, host, model, word, batch, image format, test | `EXTENDING.md` |
| run or write tests that drive the real window | `TESTING.md` |
| set up or repair the remote machines, fix a failure | `OPERATIONS.md` |
| take over this project (state, decisions, risks, next steps) | `HANDOFF.md` |

## The 60-second picture
```
 you (mouse / keys)                                   remote machine (debil, Mac)
        |                                                     ^
        v                                                     | ssh / scp (keys, low priority)
 [ img-hai window ]  <-- state/ui.txt, canvas.raw --  [ img_hai_manager.+x ] --fork--> [ img_hai_job.+x ] --> img_draft.py (python, torch, diffusers)
   (shared renderer)  --- clicks: state/request.txt -->        |
                                                               +--fork--> [ img_prompt.+x ]  <-- word bank: data/img/words.pdl, parts.pdl (+ candidate overlays)
 results:  data/img/draft/<day>/img/*.png   ledger.txt (one line per draft)   verdicts.txt (JUDGE rows)   accept_candidates.pdl (both judges said yes)
```
- The **window** is only a picture of two files the manager writes. It owns no state.
- The **manager** (one C file) owns everything: tiles, placements, judges, the job queue.
- **Images are made off this machine** (the owner's rule: GPU and big work go to debil / the Mac). Nothing is generated while a game runs.
- **A model drafts, people decide.** A model never sets a word weight, never promotes a bank change, never approves its own picture. Both judges (owner and claude) must say yes before Accept writes a candidate, and a human still promotes it.

## Where everything lives
| What | Path (from the house root `44.xyz.01.00/`) |
|---|---|
| the app | `&.hq-apps/img-hai/` |
| the word bank, ops, drafts, ledger, verdicts | `&.widgits/concept-bank/data/img/`, `&.widgits/concept-bank/ops/` |
| the relay test driver | `&.widgits/_shared-lib/ops/relay_drive.c` (+ `02-architecture/RELAY-DRIVE.md` in the book) |
| design and decisions | book `16.game/IMG-HAI-DESIGN.md`, `16.game/WORD-BANK-TO-IMAGE.md`, `16.game/GAMES-DECISIONS-2026-10-13.md` |
| a reader-friendly copy of these docs | `/home/no/Desktop/github/work/XO/25.games-plan/` (not in git) |

## Run it
```
sh '&.hq-apps/img-hai/button.sh' run     # opens the window (builds first if needed)
sh '&.hq-apps/img-hai/button.sh' stop    # closes it
```
It is not in the HQ/Toys menu yet (see `HANDOFF.md`, open items).
