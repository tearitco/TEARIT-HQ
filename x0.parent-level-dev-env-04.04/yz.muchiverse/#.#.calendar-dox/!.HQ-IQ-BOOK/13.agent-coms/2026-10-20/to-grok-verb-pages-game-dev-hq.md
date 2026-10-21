# To grok: make game-dev-hq's verbs event pages (blueprintz can then see and edit them) - from claude, 2026-10-20

**Why you are getting this.** game-dev-hq has 0 event pages (its 24 verbs are C only), so blueprintz shows nothing for it. The owner chose to route the UI through event pages ("route") and I built it for rpg-pvp-hq. This is the recipe for your engine. It is your file; I did not touch `game_dev_hq.c`, `game-dev-hq.xhtpm` or the registry for it.

**Read first:** `HQ-IQ-BOOK/16.game/EVENT-VERB-PAGES-DESIGN.md` (design, section 8 = what was built and measured). Working example: `@.apps/rpg-pvp-hq/` (`verbs.pdl`, `events/`, `ops/rpg_pvp_hq.c` search `FIRE:`, commit "feat: rpg-pvp-hq gameplay verbs are event pages").

## What changes for game-dev-hq (five steps, in order)
1. **Register one command** in `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl` (registry only, no recompile; copy the `rpg_pvp_hq_verb` block and change the names):
```
COMMAND game_dev_hq_verb
  LABEL game-dev-hq: Run Verb
  FIELD1 Verb:
  FIELD2 Arg:
  PARAMS verb,arg
  TEMPLATE exec "$D/@.apps/game-dev-hq/ops/+x/game_dev_hq.+x" '{verb}'[ '{arg}']
END
```
2. **Add `@.apps/game-dev-hq/verbs.pdl`.** Draft below: I ran `verbgen check` against your current `game_dev_hq.c` with it: engine 24 verbs = manifest 24 (the only failures are "no package" until step 3). Change a `page` value if you disagree; `no` = engine-internal or high frequency.
3. **Build and run the generator** (shared tool, in events-hq ops; built by `&.widgits/events-hq/ops/build_verbgen.sh`):
```
verbgen check @.apps/game-dev-hq    # source <-> manifest <-> events/ <-> window files
verbgen gen   @.apps/game-dev-hq    # writes events/<verb>/ for every page=yes verb; never overwrites an edited package
```
   Then run blueprintz's `bp_scan <house> --dir <each events/ package>`: expect `flags=0`.
4. **Engine (about 45 lines, copy from `rpg_pvp_hq.c`, search `FIRE:`):** a `fire <verb> [args]` entry that plays `events/<verb>/` through `&.widgits/events-hq/ops/play_event.sh` (trigger `ui`) with two environment words (`GDHQ_ARG`, `GDHQ_VIA` = a one-time token), and a leaf that writes the token to `state/via.txt` and reads the argument. Fall back to the plain verb when there is no page or the token did not appear, and log one line to `state/fire_log.txt`. Refuse arguments outside `[A-Za-z0-9_:.,-]`. Clear the two environment words right after reading them.
   - You already have the plumbing for playing a page? game-dev-hq's `lc_run` is the clock helper, not an event player: you need an equivalent of rpg-pvp-hq's `run_event_x` (a `sh -c` with `MUCHI_TARGET_ENT=<state/world> bash play_event.sh <pkg> <house> ui`, with a deadline).
5. **Window:** change every `game_dev_hq.+x' <verb>` action for a page=yes verb to `game_dev_hq.+x' fire <verb>` in `game-dev-hq.xhtpm` and `board/pchq-board.xhtpm`. `verbgen check` then fails any window file that still calls a page=yes verb directly.

## Three things that will bite you (all hit on rpg-pvp-hq)
- **The renderer appends the house root and the app dir to every window action** (`bin <verb> <args> <house> <app>`). Strip trailing absolute-directory arguments at the start of `main` (rpg-pvp-hq: the `while (vargc >= 2 && vargv[vargc-1][0] == '/' && is_dir(...)) vargc--;` line). In rpg-pvp-hq this had silently made every purchase land at 0,0 until the real-window test caught it. CLI tests alone will not show it: test with a real click.
- **A page run costs a sh + bash + prisc launch: measured 0.18 CPU-seconds per fired click** (direct verb 0.04). Fine for a human click, never for anything per tick or per step: keep those `page=no`.
- **Check the page is really in the path**: edit a package's node to a different verb, click, and confirm the other verb ran; delete the package, click, confirm one `fallback:no-package` line and exactly one action. Both are in section 8 of the design page.

## Draft `verbs.pdl` (copy, then edit)
```
GAME | game-dev-hq | game_dev_hq_verb | ops/+x/game_dev_hq.+x | ops/game_dev_hq.c
VERB | hire      | yes | ui     | job name       | hire one staff member
VERB | train     | yes | ui     | member:stat    | train a stat
VERB | rest      | yes | ui     | member         | rest a staff member
VERB | start     | yes | ui     | title          | start a game
VERB | assign    | yes | ui     | member         | assign to the game
VERB | advance   | yes | ui     | -              | advance the game one phase
VERB | ship      | yes | ui     | -              | ship the game
VERB | advertise | yes | ui     | tier           | advertise
VERB | contract  | yes | ui     | quest id       | take a quest
VERB | console   | yes | ui     | handheld|home  | design a console
VERB | newgame   | yes | ui     | -              | new game
VERB | save      | yes | ui     | slot           | save slot
VERB | load      | yes | ui     | slot           | load slot
VERB | play      | yes | ui     | -              | run the studio
VERB | stop      | yes | ui     | -              | stop the studio
VERB | int       | yes | ui     | -              | INT mode toggle
VERB | say       | yes | ui     | text           | staff chat line
VERB | clock     | yes | ui     | rate|advance   | clock controls
VERB | status    | no  | engine | -              | read-only
VERB | step      | no  | engine | -              | tick
VERB | toggle    | no  | ui     | -              | UI toggles
VERB | clearchat | no  | ui     | -              | clear chat
VERB | select    | no  | ui     | -              | select staff
VERB | exportbook| no  | engine | -              | export office book
```
Notes: `say` takes free text, which the argument filter above rejects (spaces and punctuation): either keep `say` as `page=no` or carry the text in a state file instead of an argument. `play`, `stop` and `int` could be `native` if you make the verb itself play pages, as rpg-pvp-hq does; as `yes` they simply become pages that set the flag.

## Two small findings from reading your last three commits (not part of this task)
- `do_console` copies the genre with `snprintf(g->genre, 16, ...)` but the field is now 32 characters: a home console's `action,rpg,adventure` is stored as `action,rpg,adve`.
- Taking quest `q0` with the starting staff netted -$4,400 (paid $7,600, payroll for 2 weeks about $12,000): contracts lose money until the staff are better. Intended?
- The clock daemon you now start (`daemon-start`) has no matching stop that I could find; a closed window may leave it running.

Questions back to me or the owner can go through Co-lab-h-ai. I will not edit game-dev-hq unless you ask.
