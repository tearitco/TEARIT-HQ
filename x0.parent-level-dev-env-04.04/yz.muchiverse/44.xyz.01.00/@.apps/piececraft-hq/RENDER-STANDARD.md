# pc-hq render standard: 3D is real, 2D is a render

Written 2026-10-05, from a direct instruction. Read this before adding any
overlay, range, marker or selector to the board viewer.

## The rule

**The 3D scene is the real thing. The 2D view is only a rendering of it.**

Anything that exists in the game world (the range finder, the selector, an
entity, a marker) is defined in 3D coordinates `(x, y, z)` and drawn by
`bv_render_3d.c` in every camera mode. When the viewer is in a 2D mode
(`render_mode` 0 or 2) the 3D scene is conceptually still there, just not on
screen. The user cannot see it, but it is the source of truth. `bv_render_2d.c`
then projects that same data onto the flat grid.

Put simply: 2D never owns state and never decides anything. It only answers
"how would the real 3D thing look on a flat grid?"

## What this means when writing code

1. **State lives in files, in world coordinates.** Not in a renderer's
   locals, not in screen coordinates. The files are the contract; both
   renderers read them.
2. **One origin, one test, shared.** If both renderers and a confirm/input
   step need the same rule (which cell is the centre, is this cell in range),
   write it once and text-include it. The Move range does this in
   `&.widgits/board-viewer/ops/bv_move_range.c`. Never re-derive the shape or
   the origin separately in 2D and 3D.
3. **Input acts on the real thing.** Esc, Enter and arrow keys change the
   world-coordinate files (`placer.txt`, entity `state.txt`, the range
   matrix). They never touch pixels or per-view state. Whatever the view,
   the result is identical.
4. **A feature is not done until 3D has it.** A 2D-only overlay is a drift
   bug waiting to happen: it will look fine in 2D, vanish in 3D, and the
   next person will wonder which one is right. The 3D one is right.
5. **2D may omit what has no flat meaning, honestly.** (The xelector wire is
   skipped in side-scroll view because it has no height there.) Say so in a
   comment. Do not invent a 2D-only substitute.

## Worked example: the Move range finder

- `pieces/display/move_range_matrix.txt` present = range finder open.
  Absent = nothing drawn, in either view.
- `move_range_entity.txt` names the piece Move was chosen for.
- Origin = the xelector (the range finder's cursor) while it exists, else the
  entity. The same rule in 3D, 2D and the confirm step.
- 3D draws one yellow wire cell per `#`. 2D draws one yellow tile per `#`.
- Esc deletes both files, closing it everywhere. Enter accepts the green
  selector cell if it lands on a `#`, moves the entity, then closes.

Written 2026-10-05 after the 3D range was hardcoded and always on (it ignored
the matrix and Move entirely) while 2D followed a file nothing ever deleted.
That split is exactly the drift this standard is meant to prevent.
