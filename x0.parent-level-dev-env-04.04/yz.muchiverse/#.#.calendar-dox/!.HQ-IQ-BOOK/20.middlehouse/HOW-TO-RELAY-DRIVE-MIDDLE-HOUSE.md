# How to relay-drive and test the "middle-house" apps (terminal + GL era)

Written 2026-10-24. Companion of `middle-house-conversion.txt` (the list). Read from `button.sh`, `keyboard_input.c`, `orchestrator.c` and `gl_mirror.c` of `014.wsr-pal` and `@.apps/my-chara-txt`. **Nothing here was run on 2026-10-24**: the only thing observed live was the file layout of `014.wsr-pal` (`pieces/apps/player_app/history.txt` holding bare numbers, `pieces/display/current_frame.txt` holding a drawn menu). Every "expect" below is the documented behaviour; the first real run is a test of this document too.

## 1. What a middle-house app is made of

| Process | File | Job |
|---|---|---|
| orchestrator | `system/orchestrator` (`button.sh run`) | starts and tracks the others, `pieces/os/proc_list.txt` |
| keyboard_input | `system/keyboard_input` | owns the tty; one key = one line appended to history |
| prisc+x | `system/prisc+x pal/main_loop*.pal` | reads history by cursor, runs the game's ops |
| renderer | `system/renderer` | draws the terminal frame from `pieces/display/current_frame.txt` |
| chtpm parser / rgb render | `system/chtpm_parser_pal`, `chtpm_rgb_render` | layout (`.chtpm`) to a frame and a raw RGBA `rgb_frame.raw` |
| gl_mirror (optional) | `system/gl_mirror` | the GL window: blits `rgb_frame.raw`; also forwards keys while it holds `pieces/apps/gl_os/session/input_focus.lock` |

The whole loop is files. That is what makes relay driving possible: input is a file you append to, output is a file you read.

## 2. Where the keys go

Two history files, written by `keyboard_input.c` for every key (`append_key`):

- `pieces/apps/player_app/history.txt` : **bare decimal, one per line** (`27`, `13`, `49`). This is what `prisc+x`'s `read_history` opcode reads, by cursor (`history_cursor` in `player_app/state.txt`).
- `pieces/keyboard/history.txt` : `KEY_PRESSED: N`. Only the chtpm parser reads it (the real TPMOS format).

A relay test writes to the first one for PAL-driven screens, and to **both** for chtpm screens (do what the keyboard process does: bare number to `player_app/history.txt`, `KEY_PRESSED: N` to `keyboard/history.txt`). Never truncate either file while the app runs: both are cursor-based. Comment lines are not understood here (unlike the khtpm per-pid relay): do not write `#` lines.

Key codes: printable ASCII is itself, `13` Enter, `27` Esc, `9` Tab, `8` Backspace. Arrow keys: wsr-pal's `keyboard_input.c` defines `ARROW_LEFT 1000`, `ARROW_RIGHT 1001` (Up and Down follow in the same block; the other apps were not read). **Read the app's own `keyboard_input.c` before sending an arrow; do not assume it matches.**

## 3. Which directory is the live one (session isolation)

(checked 2026-10-24 by grepping each button.sh for `pieces/sessions`)
- **Run in place** (live files under the app folder): wsr-pal, zoo, LPNS+3, LPNS+MAP+4, mutaclsym 18.0G, ttg-tactics, rpg-xyz, rtp-xyz, yahoo-app.
- **Private session folder** `<app>/pieces/sessions/<epoch>-<pid>/` (the live `history.txt` and `current_frame.txt` are there, NOT in the app folder): muchi-pals, pal-chain, pal-forum, pal-chat-irc, muchi-pal-agent, login-signup, avatar-creation, mutaclysm 19.00, agy-txt, editor, aomorai-editor, civ-txt, my-biotech, my-chara-txt, my-lawyer, qtc, piececraft-xyz, tactics-txt, TSC_ELO, START_BUTTON, TSOTS.

Find your session by identity: `ps -eo pid,args | grep '<app>/pieces/sessions'` shows the session id of the process you started, and `ls -d pieces/sessions/*-<pid>`. Never take "the newest folder": another window (or another agent's clone) may have made it. Throwaway sessions are deleted on exit; `data/` is copied back on exit (two sessions at once clobber each other).

## 4. Driving, in order of preference

1. **Relay** (what this document is about): append keys, read the frame file. No X, no focus, no GL needed.
2. **`button.sh sim-key <code> [seconds]`** where the app has one (only wsr-pal today): appends the key, runs `prisc+x pal/main_loop.pal` for a second in the background, kills it. It processes one key against the persistent state; it is the same code path as a human key, not a shortcut.
3. **Real keys** (`xdotool windowfocus <win>; xdotool key ...`) only to prove the GL window or terminal receives input, because the relay bypasses `keyboard_input` and `gl_mirror` entirely. A relay pass says nothing about the GL window (see the memory note "relay testing may mask real focus bugs").

## 5. A test, step by step (works for any app of the list)

1. **Baseline.** Note the app folder, the session folder (section 3), the history file sizes (`wc -c`), `pieces/os/proc_list.txt`, and CPU of the app's processes (`ps -o pid,pcpu,args -C prisc+x,renderer,gl_mirror`). Log CPU to a file, not the terminal (these loops can spin).
2. **Start** the app once: `sh <app>/button.sh run` in a terminal you control (it owns the tty), or `NO_GL=1` where the app honours it (TSOTS does) to run without the GL window. Wait for `pieces/display/current_frame.txt` to be non-empty.
3. **Read the screen**: `cat <live>/pieces/display/current_frame.txt`. It is plain text: the menu rows and their numbers are right there. Pick the number from the frame, never from memory (menu rows get renumbered; wsr-pal's `new-game` action had hard-coded a row twice and broke twice).
4. **Send one key**: `echo 51 >> <live>/pieces/apps/player_app/history.txt` (and `echo 'KEY_PRESSED: 51' >> <live>/pieces/keyboard/history.txt` for chtpm screens). Wait ~1 s.
5. **Read the result**: the frame again (`diff` against step 3's copy), and the app's own state file (wsr-pal: `projects/wsr-pal/pieces/corp_ORB/state.txt`; my-chara-txt: `pieces/system/config.txt`). The state file is the evidence; the frame is the picture.
6. **Repeat** one key at a time, saving each frame. Keep a table: key, time, what changed.
7. **GL check** (only if gl_mirror runs): `pieces/display/gl_display.receipt.txt` and `rgb_frame.receipt.txt` hold checksums of what the GL window drew; `rgb_frame_debug.png` is a dump of the frame. A picture of the GL window needs an X screenshot of its window, found by size.
8. **Stop**: `sh <app>/button.sh kill` (wsr-pal) or the tty Esc path; confirm `ps` shows none of `keyboard_input`, `renderer`, `prisc+x`, `gl_mirror`, `chtpm_*` for that folder. A stray `prisc+x` eats CPU.
9. **Negative check**: send a key the screen does not offer (e.g. a digit above the last row) and see that nothing changes; and change one expected value in your check to prove the check can fail.

## 6. Pitfalls known from the code and the house docs

- A relaunch must clear the old input: `keyboard_input` truncates `history.txt` on start; a half-killed earlier run leaves a stale cursor.
- The terminal and the GL window both write keys; the lock file `input_focus.lock` decides who may. If a relay test works and a real key does not (or the reverse), look at that lock first.
- `chtpm` apps without their glyph bitmaps (`pieces/registry/`) draw blank text in the GL window: checksums look fine, the picture is empty. Session copies must include `pieces/registry` (the buttons already do `cp -r`).
- `orchestrator` kills its process group on exit; do not run it from a shell you need.
- Do **not** drive a GL window with `xdotool` and the relay at once.
- Build before running: `button.sh compile` (wsr-pal) / `scripts/build.sh`. A missing binary prints `MISSING` in `button.sh check` where the app has it.

## 7. Testing the CONVERSION (the point of the list)

For each app converted to an x11-hq window (one `khtpm_core_render.+x`, `.xhtpm`, manager), the old relay test must keep passing against the new window, with the per-pid relay file instead of `history.txt` (`#.desktop/entity_menu_history/<pid>.txt`, `KEY_PRESSED: N`, see `relay-drive-human-testing`):

1. Record a **golden run** on the old app: key list plus the state file after each key (section 5).
2. Replay the same key list into the new window; compare the **state files** (not the pictures: the look changes on purpose).
3. Screenshot the new window for the look; keep the old GL window running in parallel until the owner signs the window off ("we can keep the GL for now as we phase out").
4. Negative: a deliberately wrong key must differ from the golden run, so the comparison can fail.
5. Only then remove the `gl_mirror` and terminal pair from that app's `button.sh run`.
