# 2026-09-24 — grok+opencode merged into claude, pals dropdown fully closed

## What landed today

- **`grok` merged into `claude`** (real merge, both branches' distinct
  commits kept - not a reset). Real conflicts resolved by hand: kept
  claude's own pals-dropdown fixes where grok's copy was stale,
  reconciled `bug_bounty.md`'s parallel edits, took grok's real
  additions (game-content menu.chtpm cli-io wiring, concept-bank/
  terumon doc corrections, `tp_arm_placer_rmmv.c`'s `PLACE_RANGE` NxN
  fill feature, `mr_read_receipt.c`). One real near-miss: a first-pass
  automated conflict-marker resolution on `khtpm_core_render.c`
  accidentally dropped an unrelated common-ancestor declaration
  (`g_z_priority`), breaking the build - recovered by taking grok's
  whole file as a known-good base and manually reapplying just
  claude's own small fix on top. Rebuilt clean, live-verified.
- **`opencode` merged into `claude`, scoped to network-browser work
  only** (direct instruction - not the branch's older, unrelated
  taskbar-sizing hacks). Real addition: `stb_image`-based PNG decode
  wired into `draw_elem()`'s sprite path for `<img>` support (Rung 7).
  Excluded two hardcoded window-size changes from a different,
  smaller dev machine (1120x720→500x350, 960→500) - left `TODO`s at
  both sites instead of silently taking them; **a real per-machine
  auto-sizing fix is still needed, not done today**.
- **Pals dropdown bug fully closed** (bug_bounty.md has the full
  investigation trail): missing Cancel row (earlier session),
  duplicate/collided rows (`layout_dock_bar()`'s `stack_n` keyed by
  adjacency instead of `target_id` - fixed), `tax_robot`'s missing nav
  badge (a sprite-tile badge-position branch that already excluded
  `dock-cell` rows never covered the newer `dropdown-child` class -
  fixed, one-line exclusion). All three pixel-verified via a new,
  documented-working test recipe: an isolated Xephyr rig + real
  keyboard-shaped relay (`KEY_PRESSED: 203` x N + `13`, NOT
  `MOUSE_EVENT` or the manager-only `nav`/`hqcell` relay forms - see
  `06-testing/AIGENT-TESTING-K9.txt`'s 2026-09-23 update for the full
  recipe and the wrong "relay is broken" conclusion it retracts).
- **Pal dropdown rows no longer double-render their icon** (direct
  live report, confirmed preexisting, not from today's merges): the
  label text used to embed the raw glyph character on top of the real
  per-pal sprite image draw_elem() already does from `hi_N_sprite` -
  redundant, and the raw-glyph text render looked broken/tofu for
  less common emoji. Label now carries only name+hash.
- **`terumon-dev` merge regression caught and fixed**: the grok merge
  silently revived `x0.parent-level-dev-env-04.04/terumon-dev/` at its
  old, pre-move location (real history shows it was moved into the
  house's own `design-docs/` twice already, `9bcc70a6`/`cbf26560` -
  grok's branch never had that move, and the 3-way merge brought the
  old path back while dropping the moved-to path entirely). Re-did the
  move, re-fixed the same 3 path references. Also caught and fixed a
  self-inflicted bug along the way: a bad pathspec in a multi-file
  `git add` silently aborted the whole staging call, leaving 3 real
  edits uncommitted for a couple commits until caught by checking
  `git status`.
- Everything pushed to `origin/claude`.

## Real, open next steps (ranked, from the still-valid grok handoff -
`XO/1.TERUMON_HANDOFF/^.grok-to-claude-2026-09-23.txt` - plus one new
item from today)

1. **Per-machine window auto-sizing** (new today, not in the grok
   handoff): a hardcoded `1120x720`/`960` default window size can't be
   right for every machine - opencode's own dev box needed `500x350`
   for a 1360-wide display. Should read real screen size (`sw`/`sh`,
   already computed right where both hardcoded sites live in
   `khtpm_core_render.c`) and scale a real default from that. Two
   `TODO` comments mark the exact spots.
2. Open the pals-dropdown-style placing grid once with `PLACE_RANGE`
   set and save a real frame of the filled square - the draw code is
   compiled, never actually shown live.
3. Move/Use/Attack (`skills.pdl` rows) only record a word today - they
   don't aim the grid yet.
4. Arm the Cli-io field on a real entity and confirm the `^` lock mark
   shows on a captured frame - never actually screenshotted.
5. `prisc+x` still prints a missing `default_op.txt` warning even
   though commands run fine anyway - cosmetic, not yet silenced.
6. Battle screen, and RPG Maker project load/save, remain named,
   undesigned parity gaps in `16.game/GAME.md`.
7. The "one weight, two representations" (page vs. numeric/3D) switch
   from `DUSTOPIA-HACK.md`'s addendum - not built.
8. `ai_describe`/score/store loop before a local model may append one
   real event command - not built, and per the handoff's own "do not"
   list, don't add `ai_fsm_transition` before a person can read a
   one-command diff.

The scrollbar/12-row-cap issue in the SAME pals dropdown (a separate,
still-open `bug_bounty.md` entry above the now-closed one) is real
polish work, not urgent-priority, but worth folding into whichever of
the above touches `layout_dock_bar()` next.
