# Emoji Studio Wraith

Internal Wraith validation project for 2D RGBA-to-3D extrusion inside `${game_map}`.

Reference source:
- `#.emoji-studio-501.02.05t/&.emoji-studio-solo.02.01`

This is not named `emoji-studio` because it is a Wraith validation project, not the original project identity.

## Scope

This project validates the special `${game_map}` conversion path for image-like data:

```txt
2D RGBA pixels / voxels_8.csv -> extruded colored columns -> RGB frame -> Wraith GL
```

Normal Wraith desktop UI must not use this path. It applies only to `${game_map}` surfaces.

## Existing Standard To Preserve

Emoji Studio stores extracted image/emoji data as CSV:

```txt
pieces/<emoji_name>/voxels_8.csv
pieces/<emoji_name>/voxels_16.csv
pieces/<emoji_name>/voxels_32.csv
pieces/<emoji_name>/voxels_64.csv
```

Each row is one RGBA pixel. Alpha controls occupancy. RGB controls material/color.

## Future Implementation

The manager/RGB bridge should emit a semantic object like:

```txt
IMAGE_EXTRUSION | id=emoji_sample | role=rgba_extrusion | source=pieces/sample_emoji/voxels_8.csv | resolution=8 | surface=game_map
CAMERA | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
```

`wraith_rgb_daemon.c` should:
- parse the CSV.
- create one extruded column/cell for each alpha-visible pixel.
- use RGB as material.
- use alpha as occupancy/height policy.
- project through the Piececraft camera contract.
- rasterize into `current_frame.rgba32`.
- write receipt data for CSV path, resolution, visible pixel count, projected bounds, camera, draw order, and checksum.

## Relationship To Other Wraith Projects

- `wraith-3d-cube`: uses `pieces/<id>/artifact.txt` z-slice piece standard.
- `piececraft-wraith`: uses `maps/map_01_z*.txt` plus tile registry/extrude standard.
- `emoji-studio-wraith`: uses RGBA CSV extrusion standard.

See `ASSUMPTIONS.md` for the current design bets that need user correction.
