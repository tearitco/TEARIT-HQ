# Handoff index 2026-11-04: what is proven, how to re-run it, what is not

Tags: **OBSERVED** = I ran it that day and saw the result. **READ** = opened, not run. **CLAIM** = repeated from a note, not reproduced. House root `44.xyz.01.00/`; harness dir `&.widgits/_shared-lib/harness/` (run `prisc <name>.pal` from there; build prisc per `harness/README.md`). Branches `claude` = `grok` = `main` = `c69823588` at time of writing. Rule reminders: NO Python (AGENTS.md), explicit-pathspec commits, heavy work behind `cpu_gate`, do not drive the editor window while the owner is using it.

## Proven (re-run to confirm)
| Claim | Tag | Re-run | Result seen |
|---|---|---|---|
| Pet post guard (kill switch, caps, audit, hold, approve/veto) | OBSERVED | `pet_post.pal` | 36 PASS |
| IRC post propagates across two real machines | OBSERVED | `irc_two_machine.pal` with `NET_REMOTE NET_REMOTE_IP NET_LOCAL_IP [NET_WRAP NET_WRAP_ARG]`; unset = SKIP | 19 PASS vs debil, 19 PASS vs Mac (two rows in `results/irc_two_machine.txt`) |
| Editor live cases (real window; restore after) | OBSERVED | `LIVE_OK=1 prisc live_<name>.pal`, editor window open, tell the owner first | dropdown_thumb 5, lamps_at_dusk 9, placer_verb 7, place_and_move 10, mouse_align 28, mouse_calibrate 52 |
| Clock restore + lamps per skin (scratch house) | OBSERVED for df_hq only today; the other 9 skin tables (10 `lamps_clock_*` files) and cdda_hq p7/p8 passed in the earlier session but were not re-run today (CLAIM) | `ops/+x/playthrough_op.+x run play/phases/lamps_clock_<skin>.phases.pdl` (niced; one at a time) | df_hq: p1_clock_load PASS, p2_lamps PASS |
| No new Python | OBSERVED | `py_lint.pal` | PASS; baseline `py_baseline.txt` (72 grandfathered), only shrinks |
| Narrated deck video, audio stream required | OBSERVED | `DECK=… OUT=… PRESENT_TTS_HOST=user@host prisc deck_video.pal` (run WITHOUT a cpu_gate wrapper: the case gates its own steps) | 8 PASS, 69 s |
| Network pane in the editor (chat dropdown: network) | OBSERVED | `rpg_edit_hq.+x chatmode network`, screenshot via `relay_drive.+x shot` | real lobby rows, amber pets / blue people / green guard line |
| Pane controls: netroom next, netguard kill/approve/veto | OBSERVED | `rpg_edit_hq.+x netroom next`, `netguard kill` twice | kill file created then removed; room cycled |
| Live approve/veto on the real node | OBSERVED | throwaway room, `pet_post.pdl` `hold | 1`, 3 posts, approve x2, veto x1, then restore | 2 posted, 1 dropped; node restored (outbox 312 B, no audit/pending/pdl/room) |
| `cpu_guard` trim function | OBSERVED | include `cpu_guard.c` with `-Dmain=guard_main`, call `trim_tail` on scratch files | 1,500,000 B -> 131,040 B at a line boundary; 100,000 B untouched |
| Chat pane mouse wheel scroll | OBSERVED | relay `MOUSE_EVENT: 4/5 <x> <y> 1` with the pointer over the pane | 6,117 px changed up; back to start (diff 0) |

## Known gaps (do not claim these work)
- **Pets are scripted.** Every pet line in the tests was a posted string, not a pet choosing words. First real pet post is the next task.
- **PageUp/PageDown on the chat pane**: relay `KEY_PRESSED: 204/205` with the pointer over the pane changed nothing (diff 0). Cause unknown (pane ignores it or the relay does not deliver it).
- **`cpu_guard` sweep loop** that finds the chat node and trims its files has not run; the running guard was not restarted (live after the next `guard.sh start`). Only `trim_tail` itself is tested.
- **2D/ASCII parity** of today's work (lamps, placer, network pane) was never checked.
- **Wrapped by hand:** the editor was recovered with `board_ctl recover` and reloaded to `rpg-edit-hq` with `pchq_board_action.sh <session> load-map rpg-edit-hq` (a bare recover can leave another book loaded).
- **Harnesses that pin a path hide real-layout bugs** (`PET_POST_CHAT_OP` hid two bugs). Run once against the real house layout.

## Traps (each cost a failed run)
- `|` inside a case argument is `\p` (also inside an awk regex: `[\p]`); a literal backslash is `\\` (Enter for `bv_keys` is `\\n`).
- `cpu_gate run` around a pal case that gates its own steps refuses ("another heavy job holds the gate"); run the pal directly.
- CSS in khtpm windows is read at window start only; relaunch to see colour changes.
- `kill`/bulk-kill of the editor stack is often denied; `board_ctl recover` was allowed.
- Do not leave a daemon started with bare `setsid nohup`: use `&.widgits/_shared-lib/kh_spawn.sh <house> <owner_pid> <name> -- <cmd>`.
- Live editor cases save to slot 9 and restore house.txt/brush.txt/voxel_log.txt/lamps; a leftover placed item makes the next click say "spot taken".

## Left on disk, not mine (do not delete)
- `@.apps/rpg-edit-hq/events/phase_dawn/event_pkg/pages/page_1/*.hai.bak` (h-ai's own backups of the dawn page).
- Modified before this session: `harness/cases/play_rpg_{dust,space}_planet_map.pdl`, `%.harnesses/harnecient-fsm/stats_summary.txt`.
- Owner-pending: `show_pointer=1` in live `#.desktop/hq_ui.pdl` (backup `/tmp/claude-1000/hq_ui.pdl.pre-pointer`); `~/phase2-net` scratch on debil and the Mac; a stray palnet node on the Mac (pid 13406 when seen).

## Next (owner order, 2026-11-04)
1. Blank oversized page: find the size the board holds. 2. Legend-writer determinism case. 3. One real pet post through the guard. 4. Shop slice, THREE item ids (cdda/df/mcl; explicit id map, conservation per id). Source pages for tear-it co: the two floating islands in the editor's own space and the multi-planet pages in rpg-space / rpg-dust (not yet located). Reports: `XO/31.Nov/nov-4/5`, `6`, `7`.
