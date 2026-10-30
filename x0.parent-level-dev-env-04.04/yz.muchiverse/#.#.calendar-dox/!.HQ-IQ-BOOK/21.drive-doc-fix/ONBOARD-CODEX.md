# Codex re-onboarding: editor drive

Last updated: 2026-10-30. Read this after `00-INDEX.md` and
`HOW2-DRIVE-DOC-FIX.md`; those remain the shared procedure and source of
truth. This note records Codex's verified learnings and mistakes so a new
Codex session can continue without guessing at the screen contract.

## What the screen represents

The edit board combines the selected book/page's 3D canvas with RPG-Pet
party controls. These are separate pieces of the screen:

| Area | Meaning | Expected contents |
|---|---|---|
| Center canvas | Current selected book and page | Page terrain/voxels and placed page entities. A new page starts as an empty floor; it is not a water biome or a render failure. |
| Left sidebar | RPG-Pet party and selected member status | The current party, names/stats, and coin count. |
| Right sidebar | Chat for the selected party member | That member's chat history and input. |
| Bottom entity bar | Party shortcuts plus entities/events on the page | Keep user-facing party rows such as Harold, Ember/Rex, Glacine, and others. Page event rows may appear alongside them. |

`hero_01` and `xelector_01` are implementation actors on this editor
surface. They should not occupy bottom-bar slots. Do not apply that rule to
party members: they are user-facing controls, even when the bottom bar looks
duplicative with the sidebars. The bottom bar and sidebars serve different
navigation/actions, and removing party rows was an incorrect overreach in
Codex's first new-page change.

## New-page behavior

`piececraft-hq/ops/pchq_board_action.sh <bv_session> newpage` creates a blank
page in the selected book and switches to it. The source page's `bar.txt`
can mix party `on-click` rows with page-specific `player-touch` rows. When
creating a new page:

- preserve the party `on-click` rows;
- clear page-specific touch rows and the matching `events.pdl`;
- add a new event only with both its tab-separated bar row and matching
  event package under `ev/<id>`.

Deleting all of `bar.txt` removes the party from the bottom bar. Leaving
`events.pdl` after removing its bar row creates an event the touch runner
cannot discover. Both failure modes were observed during this drive.

Verified on 2026-10-30 in live session `1793345895-441598`:

- `rpg-space/page_2`: event 10 moved the possessed hero from `(8,8)` to
  `(8,9)` and appended the expected play row; ledger size grew 387 to 774.
- `mcl-hq/page_2`: event 20 moved the possessed hero from `(8,8)` to
  `(8,9)` and appended the expected play row; ledger size grew 576 to 963.
- After correcting the party-row regression, fresh
  `/tmp/mcl-page2-restored-party.png` showed Harold, Rex, Gus, the other party
  entries, and `step-south`; `hero` and `xelector` were absent.

The current page's live files may be ignored/runtime data and untracked.
Restore or edit them only as needed for the active test; do not force-add
runtime map state to a code commit.

## Projector and screen evidence

The intended editor-specific hero/xelector footer filter already existed in
`piececraft-hq/ops/pchq_board_projector.c`; the visible rows persisted because
the running `pchq_board_projector.+x` was stale. Rebuild with
`sh @.apps/piececraft-hq/ops/build_pchq_board_projector.sh`, then restart only
the projector processes verified to belong to this editor. Pass absolute
house and host paths when launching it by hand; relative paths fail session
discovery. Verify `board/state/ui.txt` (`n_ent`, `ent_*_label`, and
`no_session`) and capture a fresh window PNG. Source or compile success alone
does not prove the visible board changed.

The blue empty page observed on MCL was an empty voxel floor surrounded by
the scene background. It was not an ocean map. It can nevertheless be
confusing; describe the page as empty until terrain is placed, and record a
separate UX issue if the owner wants a clearer empty-page cue.

## Continue the drive

1. Read `OPEN.md`, `DONE.md`, and `PLAY-SENSE.md`; use
   `harness/editor-skip.sh` to avoid re-driving closed rows.
2. Check `sh '&.hq-apps/proc-mon/guard.sh' status` before adding load. Follow
   the CPU history and stop only processes started for this drive.
3. Drive one user action, then compare state files and a fresh frame. Add
   observations before changing behavior. Fix only owner-named issues.
4. Continue the edit/event/play cycle on cdda-hq and df-hq after the proven
   rpg-space and mcl-hq cycles.
5. Work on `codex`. Commit explicit paths only, push `origin/codex`, then
   cherry-pick to `claude` in a scratch worktree. Leave the live checkout on
   `codex`; do not switch branches in its heavily dirty runtime worktree.

## Codex mistakes to carry forward

- Do not infer that a visually empty page is broken or water. Check the
  selected book/page data and a fresh frame first.
- Do not remove all bottom-bar rows when removing `hero`/`xelector`; party
  rows are real user-facing controls.
- Do not claim a projector source fix is live until its running binary is
  rebuilt/restarted and a fresh screenshot confirms the result.
- Do not treat the editor-loading sandbox failure as a host teardown without
  a host-level run. The corrected-scope host launch was verified live; its
  evidence is in `PLAY-SENSE.md` and `/home/no/Desktop/claude-handoff.md`.
