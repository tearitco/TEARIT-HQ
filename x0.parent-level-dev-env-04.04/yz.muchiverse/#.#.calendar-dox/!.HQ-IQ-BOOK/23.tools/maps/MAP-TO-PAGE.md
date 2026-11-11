# Map to a page you can open

`map_json_op` reads one page map and writes three files.

```
map_json_op <map.txt> <out.json>
```

The program is `44.xyz.01.00/&.widgits/_shared-lib/ops/+x/map_json_op.+x`. The source is the `.c` file beside that folder.

What you get, next to each other:

- `out.json` is the copy. It holds every row, the width, the height, the largest side, the file size in bytes, how many cells are filled, how many are empty, and the box the land sits in.
- `out.js` is that same copy under the name `PAGE_MAP`, so the page can read it when you open the folder.
- `out.html` reads `out.js` and shows the size, the counts, any notes, and a picture. Empty is dark. `a` is low land, `b` is a step up, `c` is the high band. Any other mark is orange.

A map bigger than 8192 on a side is refused.

If the map's own folder has them, their text is stored in `notes`:

- `land.seed.pdl` (the seed and the scale)
- `extrusion.pdl` (how tall `a`, `b`, and `c` are)
- `view.pdl` (the fog)
- the header of `cells.txt`, up to the `cells` line (width, height, tile size). The cell grid itself is not copied.

The live centroid page was written on 2026-11-11 to `XO/31.Nov/nov-11/grok/0.live-map.json`, with `0.live-map.js` and `0.live-map.html` beside it. Open the HTML from that folder.
