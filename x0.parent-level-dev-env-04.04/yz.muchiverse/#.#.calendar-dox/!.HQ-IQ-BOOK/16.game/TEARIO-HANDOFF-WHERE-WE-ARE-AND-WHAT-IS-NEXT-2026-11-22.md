# Teario handoff: where it stands, what is open, how to continue (2026-11-22)

Written by Claude (branch `claude`) at the owner's request because quota was low ("write up a very detailed new set of handoff docs, 1 for teario, 1 for minecraft"). The Minecraft twin is `MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md` (same folder).

Labels used: **READ** (I opened it), **RAN** (I ran it and saw the result in this project), **SAID** (the owner said it), **OPEN** (known unknown), **PROPOSAL** (my idea, not decided). Anything not labelled is a claim, not a fact: check it with a fresh run (AGENTS.md "Never report a number you did not observe").

## 0. Read before touching anything (in this order)

1. `AGENTS.md` at the repo root, row "A game mechanic, rule, enemy, pickup..." and "Commit discipline".
2. `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` section 2 ("ALL MECHANICS AS EVENTS").
3. `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md` (one rule = pure module + CLI op + registry row + harness case + event page).
4. Decision log `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`, D92 to D96 (D92: no native rules, no exceptions; D96: `default_yaw` key).
5. Skill `pchq-3d-board-driving` and `18.pc-hq/3D-CONTROLS-AND-VIEWS-REVIEW-STANDARD-2026-11-19.md` before any camera, possession or facing work.

## 1. What Teario is

Teario-Make is the Mario-style level editor plus platformer, in its OWN window (owner ruling: Teario has its own window and header, not the rpg-edit one). Files: `44.xyz.01.00/@.apps/teario-make/` (`board/open_board.sh`, `board/pchq-board.xhtpm`, `button.sh`, ...). It draws through the shared projector `pchq_board_projector.+x` and is driven by the shared binary `game_core` (`game_core driver <house> <app> <app>`, registry row `teario-make-driver`). There is NO per-game C (decision D62).

Owner rulings that bind every change (SAID):
- Platform mode is **flat 2D** (key `0`); the 3D view must look normal.
- The player is the **extruded RPG Maker sprite**.
- Research the unknown before building; falsify your own premise with a control.
- Decks and videos are voiced: English voice with Japanese subtitles, or the reverse, never the same language twice.

## 2. State of the work (what I can vouch for)

- **Phase P1 is done** (window, board, play desk, cursor, harness `teario_window.pdl`, 56 checks). RAN earlier this session; re-run it before you trust it (section 8).
- Last commit of mine on the Teario line: `6fe3a0868` (pushed). Since then: only read-only work and the merge of grok (`f5b32097a`, local, not pushed).
- Play desk: `events.txt` player row `People1`, goal `!Other1`. The xelector cursor carries `possessed_id`.
- Engine keys go through `bv_keys.+x` (file based, see the skill). `default_yaw` lives in `arrow_config.txt`.

## 3. Owner requirement not yet built: the stamp bank shows an example picture of each stamp (SAID 2026-11-22)

"stamp bank should show example of stamp, like palettes."
- The stamp bank is the list of stamps (multi-cell prefabs) the editor can place. Today a stamp is a name/row only (I did not find a picture per stamp; OPEN: confirm by opening the bank in the window).
- Wanted: next to every stamp name, a small rendering of what the stamp looks like, the same way the palette studio shows a swatch/preview per palette entry.
- PROPOSAL (house shape, no new C): a stamp row gets a `thumb` field (or the projector draws it from the stamp's own cell rows, scaled into one cell of the bank list). The preview must come from the SAME rows that place the stamp, so the picture and the placed result cannot disagree (one authority per fact). Look at how the palette studio draws previews (decisions D33 to D35) and reuse that path; do not write a second renderer.
- Acceptance (can fail): a harness phase that adds a stamp with a known cell pattern and asserts the preview cells equal the placed cells; open the PNG of the bank window and look at it. A passing file check with an empty bank on screen is a failed test (AGENTS.md).

## 4. The open path, in order (from the plan; all unfinished unless marked)

| # | Item | State | Note |
|---|---|---|---|
| P2 | Play mode: the player (`People1`) is possessed and moves with arrows/jump | **OPEN, blocked** | see section 5 |
| P4 | Mechanics as events (stomp, coin, brick, goal, hurt) | OPEN | each rule an op + event page + RULE row + harness case; do not add to `game_core.c` |
| - | Data-driven stamps (stamp = rows, not code) | OPEN | with section 3 |
| - | Brick / coin / teleport blocks | OPEN | rules as ops, tiles as data |
| - | Project tab (save/load a level as a book) | OPEN | |
| - | Custom block library | OPEN | |
| - | Snake leftovers | OPEN | the snake app shares `game_core`; check it still passes |
| - | Audits (D92 stopgap removal) | OPEN | Teario enemies/stomp/fireball are hard-coded in `game_core.c` today; D92 says a stopgap to be removed. Do not add more there. |

## 5. The blocking problem: arrow keys in the Teario window (R14) and the two designs

READ/RAN earlier: arrow keys pressed in the Teario window reach only the engine **cursor**, never the game. Mechanism: the relay `<EDIT>/state/interact_relay.txt`. So a possessed player cannot be moved by arrows.

- **Option C (owner: "go on option C", not yet authorized to build in this stretch; ask first):** while a game-driven possession is held, the engine forwards arrow and jump key codes to the host inbox; `game_core` tails that inbox. Needs: the engine side (`bv_menu_input.c`, heaviest-collision file, touch only for this), a `game_core` tail of the inbox, a harness case that proves a forwarded arrow moves `People1` and a deliberately dropped key does not.
- **Fix 2:** in play mode, `game_core` must possess `People1` itself (a row, not a hidden flag) so there is a controlled hero at all.
- Unknown still open: what raised the cursor to level 30 (R12), whether the camera-mode 3/4 pictures are stale (R15), the draw order of voxel over maker-land, the cause of the relay's length growth. Do not close these by guessing; reproduce with a control.

## 6. How to work here (commands and traps)

- Scratch house under `/tmp` for every test; never the live house for an experiment.
- Heavy work: `cpu_gate <house> run <label> -- <cmd>`, one job at a time, `nice -n 15`. Never compile while an engine runs; never relaunch the window twice back to back.
- No Python, no new `.sh`. Harness = pal + case `.pdl` + C op (`&.widgits/_shared-lib/harness/README.md` first).
- Commit only your paths: `&.widgits/_shared-lib/ops/+x/commit_scoped.+x <repo> --branch claude --message-file F --trailer 'Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>' --push -- <paths>`. Never `git stash`, `reset`, `add -A`. Never touch `xyzfs/users`.
- An engine fix only reaches an open board through a NEW session (stale session copy of `bv_render_3d`).
- Photographs: open the PNG and say what you see; never describe a picture you did not open.
- Esc releases possession first, then leaves In (one-shot `esc_first.txt`).

## 7. Reports and docs

Put learnings in this one doc (or the plan it points to), and add an index row for a new file. XO copies are docs/media only (`xo_export.+x`).

## 8. First hour for the next agent

1. `git log --oneline -5` on `claude`; confirm `f5b32097a` or later.
2. Run the harness case `teario_window` in a scratch house; record the number of checks that pass (I last saw 56; do not quote that number as current).
3. Open the Teario window, open the stamp bank, take a PNG, and write down whether a stamp shows a picture (section 3).
4. Ask the owner whether option C is a go.

## 9. BUILT 2026-11-22 (second session, branch `claude`)

**Step 1, option C (R14 fix): BUILT in a scratch house, commit `4222f9d85`, NOT yet seen in the live window.**
- What it is (RAN): a host `keybinds.pdl` row `GAMEKEY | <name> | <keycode>` makes `board-viewer/ops/bv_menu_input.c` append `"<code> <ms>"` to `<host>/../state/interact_relay.txt` (the file `game_core` already tails with its own byte cursor) INSTEAD of moving the cursor, but only while `xelector_01` `possessed_id` names something with no engine piece (D1: People1 is daemon/game driven). Hero possession, no possession and unlisted keys are unchanged. The Teario-Make launcher `board/open_board.sh` adds five rows (left 1000, right 1001, up 1002, down 1003, jump 32) idempotently (shell fragment tried twice on a scratch file: five rows, no duplicates).
- Design note: the "host inbox" of the brief turned out to be the existing `HOSTKEY` relay path (`<host>/../state/interact_relay.txt` = `<editor>/state/interact_relay.txt`), so no new inbox and no `game_core.c` change was needed for the tail (it already has `keys_poll`). Design-choice answers: one authority per fact (the key list is data in keybinds.pdl; the relay is the one existing channel), modular (no per-game C in the engine, the gate is "no engine piece"), scalable (any host lists its own codes), long term (the same row serves the phone editor), house shapes (pdl row + marker-free append file, which the driver caps at 64 KB). The decision-log row is OWED: `DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` carries another agent's uncommitted hunk (14 lines), so I did not touch it; add this note there as the next free D number.
- Harness `teario_gamekeys` (`harness/teario_gamekeys.pal`, `cases/teario_gamekeys.pdl`): **16 checks, 16 pass (observed)**. Real `bv_menu_input` built from source + real `game_core` in a scratch host. Forward: right + jump reach the relay, the cursor stays at pos_x=5, `game_core keys` reads 2 applies 2, after `advance 6` `body=666,1776,36,-136` (the same pin as `teario_platform` tpl_keys). Negative 1: the same source built with `-DBV_NO_GAMEKEYS` forwards nothing (the relay is empty), `KEYS read=0`, People1 stays at `body=540,`. Negative 2: real op, `possessed_id=none`: nothing forwarded and the cursor moves to pos_x=6 (vacuity: the key was handled). README table row for this case is OWED (README.md carries a foreign uncommitted hunk).
- **KEY FINDING for step 2 (READ in `game_core.c` lines 1270-1273, not yet run live):** the driver applies relay keys only when `KIND == K_REALTIME && MODE == M_PLATFORM`, `mode_play`, running, `S.phase == PH_PLAY` and a human seat. The Teario-Make window's driver is `KIND editor` (game.pdl), so in the live window a forwarded arrow will be read by nobody until Play starts a realtime platform game (teario-play shape) with a human seat. That, plus the possession row (Fix 2), is the real P2 work; the engine half is done.
- To see it live: rebuild `bv_menu_input.+x`, open a NEW Teario-Make session (the stale-session-copy rule), possess People1, press an arrow. Not done.

**Steps 2 to 5: NOT STARTED** (P2 header cells/Play/possession row, P4 stamp placing by event page, data-driven stamps + brick/coin/teleport ops, stamp-bank example pictures). Stopped at the first step that needs the live window and photographs.

## 10. LIVE DRIVE 2026-11-22 (third session; RAN in the real Teario-Make window, photographed and opened)

What I did: rebuilt `&.widgits/board-viewer/ops/+x/bv_menu_input.+x` from the committed source (`gcc -Wall -Wextra -O2 ... -lm` through `cpu_gate`; the old binary had **0** `GAMEKEY` strings, the new one has them; the two older session copies under `board-viewer/pieces/sessions/` still lack it), launched a NEW session with `button.sh run`, and drove it with relay codes (`<session>/pieces/apps/player_app/interact_relay.txt`: arrows are the decimal lines 1000 left, 1001 right, 1002 up, 1003 down, 13 Enter; `bv_keys` cannot send them because it sends one ASCII char).

**Observed (RAN):**
1. Launch is fine: board, stamp bar (ground, erase, start, goal, enemy, power), Make popup (`15 rows, not published`), minimap. 2D view (`0`): ground strip along the bottom, the player sprite on cell (2,10) with the green cursor on it after I walked the cursor there, the goal boulder at the right. 3D view: a one-cell-wide ground strip running away at a diagonal, the cursor as a green wire box, the pale moon cube floating in the sky. The 2D and 3D pictures agree on where the level is; the 3D default view is hard to read (a strip into the sky), a camera-preset question, not a bug I can prove.
2. Arrow codes move the cursor, not the camera: 1000 gives x-1, 1001 x+1, 1002 y-1, 1003 y+1 (single presses read back 3 s apart). My first burst of three presses read back short by two moves, so keys sent faster than the engine wakes can be coalesced or late; I did not chase it (OPEN).
3. **BUG / MISSING LINK (the real P2 blocker, proven live):** with the cursor on `People1`'s cell (2,10), Enter leaves `possessed_id=none`. Cause (READ, `bv_menu_input.c` `entity_at_cell`, line 204): Enter only possesses a piece folder `pieces/<id>/state.txt` that has an `entity_type`; `People1` is only a row of the play desk's `events.txt` (`2 10 220 60 60 People1 ...`), and `pieces/` holds just hero_01, moon_01, sun_01, xelector_01. So option C's engine half is correct in a scratch house (`teario_gamekeys` 16/16) but a human can never reach its gate in the live window: **nothing sets `possessed_id` to a game-driven name**. The `key_possess` release branch already has a comment for "externally driven (a pet / Harold)", so the engine expects some other writer to set it.
4. Stamp bank (owner requirement): the bank shows NAMES only, no example picture. Confirmed in the live window.
5. Stale text: the popup says "a game was in progress; it continues while the Player menu says playing" while the header says `In: off` and no game runs. Misleading, not fixed.

**What this means for P2 (design, MINE):** a `teario-play` realtime platform app ALREADY EXISTS (decision D63, MODE platform, `teario_platform` 29 checks, `verify_clear`), so the open question is not "build a Play driver"; it is "how does the Make window's Play hand over to it". Two coherent shapes: (A) Make's Play button appends a ledger row (`play` verb) and the driver of the PLAY app seats a human on `People1` and WRITES `possessed_id=People1` into the xelector as a projection of that row (one authority: the ledger row; the engine only reads it), so arrows are forwarded by option C and the realtime driver applies them (its gate `KIND realtime + MODE platform + human seat` then holds); (B) let the editor driver run the platform body in place (what I advised against: it puts play rules into an editor-kind driver). I recommend (A). Needed pieces: a `play` verb row (decision log row owed; the log file carries a foreign hunk), a projection writer for `possessed_id` (game_core, generic, not Teario-specific), a harness case that fails when the row exists but the xelector does not name People1, then a live photo: arrow moves the sprite.
