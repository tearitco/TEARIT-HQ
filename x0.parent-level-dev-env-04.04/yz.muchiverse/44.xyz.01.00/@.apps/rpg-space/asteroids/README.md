# asteroids/ - user / generated asteroid models (owner 2026-10-27)

A folder `<name>/model.txt` here is an asteroid shape that a planet-map node can use: set the node's kind to `gen:<name>`
(state/nodes.txt row `region|<wx>|<wy>|gen:<name>|<amount>`). Format = a voxel piece: first line `dim X Y Z`, then one
`x y z r g b` line per voxel (max 16 cells a side, `gen_vox_max` voxels in tuning.pdl). A missing or bad file falls back to the
plain rock nugget; a generated asteroid keeps its own colours and mines as rock. `sample_rock/` is a hand-made 10-voxel test model
(red / blue / green rows, a yellow top voxel) that the gate loads. The generator side (image -> 3D, concept bank) only has to write
`model.txt`: BOOK 16.game/RPG-SPACE-USER-ASTEROIDS-DESIGN.md.
