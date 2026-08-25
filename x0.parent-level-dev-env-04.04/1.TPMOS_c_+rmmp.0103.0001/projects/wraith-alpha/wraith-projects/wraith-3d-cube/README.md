# Wraith 3D Cube

This is a nested Wraith project.

**CORRECTION (2026-07-02, from project owner):** this project is intended
to use a tile-based map, the same as piececraft-wraith — NOT a single
standalone rotating cube probe. Prior versions of this README and
IMPLEMENTATION.md described it as a single-piece cube probe
(`pieces/cube_probe/artifact.txt`, no `maps/` directory); that was wrong
and should not be treated as the design intent going forward. The current
code (as of this correction) still only renders the single cube_probe
piece — that's a real implementation gap against the corrected intent,
not something already fixed. See `x0.piececrafts/wra-mana-checklist.txt`
for migration status; this project needs its own follow-up pass to add a
`maps/` directory and switch its `scene.objects.pdl`/op to the same
`role=tile_zmap` + entity-marker pattern piececraft-wraith uses, instead
of `role=zslice_piece`.

Purpose:
- Prove that a Wraith-owned 3D map project lives under `projects/wraith-alpha/wraith-projects/`.
- Keep it discoverable by the Wraith launcher/terminal flow, not the global top-level project loader.
- Validate `${game_map}`, `INTERACT`, `is_map_control`, and RGB/ASCII parity from inside the Wraith session, using an actual map (like piececraft-wraith), not a single object.

Notes:
- Currently still only has the single-piece z-slice source
  (`pieces/cube_probe/artifact.txt`, `pieces/cube_probe/state.txt`) — this
  predates the map-based correction above and needs migrating.
- See `IMPLEMENTATION.md` for the (also corrected) plan.
