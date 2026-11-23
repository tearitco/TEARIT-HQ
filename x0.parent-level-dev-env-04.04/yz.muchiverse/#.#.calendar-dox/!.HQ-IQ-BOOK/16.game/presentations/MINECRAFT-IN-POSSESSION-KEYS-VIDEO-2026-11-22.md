# Minecraft (Voxel window) played by KEYS in possession: video write-up (2026-11-22, D115)

Replaces the proof value of the Step ev / popup-button video (D114): that proved the ops, this proves the game is played the way a person plays it (BOOK presentations standard section 11).

## What was recorded (screen recording of the live window, 81.6 s, 7 scenes)
Live window `@.apps/vox/board/open_board.sh` (launched through `cpu_gate`), frames by `dump_frame_png_op` (~4 per second, epoch log, cut by marks), English voice (`pet_tts_local.py --voice en-US-AnaNeural`), Japanese subtitles, Actor1 face left of the words. Per-scene volumedetect: mean -22.6 to -24.0 dB, max -3.0 to -6.8 dB (all audible). Output: XO `17.minecraft-in-possession-keys-video-2026-11-22/vox_in_possession_keys.mp4` (plus `snap.txt` ledger tails and `marks.txt`).

## Setup versus keys (stated in the narration)
- SETUP (not keys): launcher opened the window; Joe was already a `joe` actor in the live ledger (bag: pick, cobble, bread); an earlier test left him possessed.
- KEYS (engine session key relay, the same file a keyboard feeds; decimal codes): `57` (key 9) release, `1001` (right arrow) cursor onto Joe's token, `13` Enter (HUD `poss People1`; possession by `desk_token_at`, D101), then `1001` east, `1003` south, `108` (l) mine x2, `1000` west, `112` (p) place, `117` (u) up, `112` place, `107` (k) eat, `48` (0) 2D and back.
- Ledger after each key is in `snap.txt`: MOVE rows for the arrows, DAMAGE then MINE+DROP for two `l`, PLACE for each `p`, MOVE up for `u`, EAT (hunger 12 -> 1) for `k`. `vox_op verify` said `verified` afterwards.

## Design (D115)
GAMEKEY rows -> `<app>/state/interact_relay.txt` (existing input op, only while a game token is possessed, cursor does not move) -> `vox_keys_op tail` (registered via `kh_spawn.sh`, owner = the board window pid, exits with it, 4 stat() wakeups per second idle, one `vox_op act` fork per key) -> `vox_op act`. Key table is data: `@.apps/vox/keys.pdl` (arrows, l mine, p place, u up, j down, k eat, b wait, n sleep; m and g are the board menu's). Harness `vox_keys` (27) with sabotages.

## Findings
- The two risks named in the brief did not occur: Enter possessed the `People1` token on vox's `events.txt`, and letter keys reached the GAMEKEY forward (rows in the ledger prove both). No engine change was needed.
- The window's `In:` label stayed `off` during the run: a click on it and a relay `key 49` did not toggle it, and the session relay keys worked regardless. The video does not claim In: ON.
- NOT done: 2D cue for a mined pit (the 2D view is a uniform brown tile map; the placed cobble shows as grey, the pit looks like its neighbours); a Blueprintz scene (no window slot used for it).

## State restore
Backups in the session scratchpad (tar + sha256). `#.desktop/vox_ledger.txt` restored byte for byte (sha 80c32c00...), `vox_board.pdl` and `pchq_board_view.txt` unchanged (808666bc..., dfaff441...). Windows closed by their own `board_window.pid` only.
