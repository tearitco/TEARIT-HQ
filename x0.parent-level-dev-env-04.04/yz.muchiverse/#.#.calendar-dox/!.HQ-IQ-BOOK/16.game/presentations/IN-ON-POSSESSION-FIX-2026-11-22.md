# In: ON, possession and the cursor that follows (2026-11-22, D117 and D118)

## Root cause
The top-bar label `In: off` never turned `In: ON` in the Voxel (`@.apps/vox`), Robot Trap Auto (`@.apps/rta-board`) and Teario-Play (`@.apps/teario-play`) windows. All three copy the Teario board template whose tab `tb-in` calls `@.apps/piececraft-hq/ops/pchq_board_action.sh <bv_session> interact` and whose label comes from the shared projector `@.apps/piececraft-hq/ops/pchq_board_projector.c`. The action is fine (run by hand: key 13 appended, label `In: ON`). The window was bound to the wrong session: the projector starts before its own engine registers in the ledger, `ledger_peers widget` still named the latest earlier live session of the same project (a leaked one from an earlier take), and the projector cached it until its directory vanished. Measured first launch: `board/state/ui.txt` named session 1795376278-1247342, the new one was 1795406962-2885662; clicks wrote to the old one. Fix: for the first 240 s the cache is re-checked every 10 s (`PCHQ_BV_SETTLE_S`, `PCHQ_BV_RECHECK_S`). Modelled on rpg-space and the petslike boards, which never look the session up through the ledger (their own op resolves it from the board).

## Also fixed (found while proving it)
1. Esc did not release a possession on these boards: nobody published `<board>/state/esc_first.txt`. The projector now writes `57` once per possession while In is ON (what `petslike_core.c` does for its boards). Rule kept: Esc releases possession first, then leaves In.
2. The xelector (outline cube, HUD `pos`, 2D cursor) stayed behind when a possessed desk token moved (D118): `bv_render_3d.c` `load_xelector` now follows the possessed token row of the shown desk `events.txt` (helper `desk_token_cell`, knob `-DBV_NO_TOKEN_FOLLOW` for the harness).

## Files
`@.apps/piececraft-hq/ops/pchq_board_projector.c`; `&.widgits/board-viewer/ops/bv_render_3d.c` (my hunks only: `desk_token_cell`, `g_tf_*`, the call in `load_xelector`, the selector line in the frame setup); `&.widgits/board-viewer/ops/bv_token_follow_test.c`; harness `in_session_follow` (cases/in_session_follow.pdl, 19) and `token_follow` (13). Decision rows D117 and D118.

## Observed live (human path, one window at a time, new launch each)
Click `In:` once: Teario-Play (after tapping, because the first click on an unfocused tab only focuses it there), Voxel and RTA each showed `In: ON` in the OPENED png. Then: Esc (possession released, `In: ON`), arrows to the token, Enter (`poss People1` in the HUD), then keys only.
* Teario-Play: token 6,10 to 1,10 by left keys, xelector state and HUD `pos 1,10,1` followed; outline stays on the player.
* Voxel: Joe 4,3 to 6,4 by arrows, `l` mine and `p` place wrote DAMAGE / PLACE rows (`#.desktop/vox_ledger.txt`), HUD `pos 6,4,4` followed.
* RTA: Joe 0,10 to 0,11 by `i` (enter car), HUD `pos 0,11,1` followed. The car did not drive (fuel 0, `turn 1`); no drive claim.
* Esc twice on all three: first `possessed_id=none` with `In: ON`, second `In: off`.
Steps that were setup (not keys): the launch, the New game / you play p1 / START GAME clicks in Teario-Play, the click on `In:`.

## Click behaviour (not changed)
In Voxel and RTA one click on `In:` activates it, `relay_drive tap` (two clicks) toggles twice. In Teario-Play an unfocused tab needs the focus click first. Use `click` where the tab is already focused or the board is Voxel/RTA, and read `pieces/keyboard/history.txt` of the session to see what was sent.

## Not done
The videos (XO `19.in-on-possession-proof-videos-2026-11-22/`) were not re-recorded in this job; the live runs above are photographed (frames in the job scratchpad, not committed). No per-window session record (a later change would remove the ledger lookup). Live state was restored by hash (`pchq_board_view.txt`, `vox_ledger.txt`, `vox_board.pdl`; the rta files did not exist before and were removed).
