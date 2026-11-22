# A whole world from the city slices (design only, 2026-11-21)

Nothing in this note is built. Centon and the bay slice stay the two worked examples. A later agent copies their pattern. It does not invent a new painter, and it does not start a Minecraft mode.

The planet ladder this note follows is `RPG-DUST-DESIGN.md` sections 3 to 5 (2026-10-23, proposed, not built): a scale row per level, a seed, a child hash, generated pages as cache, and an override ledger for anything a person changed. The ship-and-planets notes in `RPG-DUST-PETSLIKE-BOARD-DESIGN.md` (D12 to D19) are the same family: many planets, each with a seed, detail only where you are. The chemistry rulebook the index calls `DUSTOPIA-HACK.md` was not in the grok tree when this note was written. Do not treat this file as that rulebook.

## What the two cities already proved

Both cities are the same four files plus one op.

1. A CONFIG `.pdl`: one `BOX` (south, west, north, east), one `CELL` (metres, origin), and `FEAT` rows (`kind`, name, `line` or `polygon`, lon, lat). Kinds the painter knows: park, path, road, water, pier, building, beach.
2. `rta_city_op` rasters that file to a grid and a PNG. The grid caps at 220 by 300 cells. North row is file row 0. A grass cell that can reach the map border becomes water, which is how a harbour appears without a bay polygon.
3. `rta_stand_op` writes the editor desk. Building glyph `B` extrudes to height 3. The other glyphs stay flat.
4. `game_new_op` makes an editor book. The desk replaces the template board. The book shows up after the editor board is opened again.

Centon is 168 by 254 at 10 m (Charleston Battery through the College). The bay slice is 163 by 216 at 20 m (Dogpatch through the bridge landing, Salesforce Tower named on the nearest footprint). Real-city names on the data are labels of the source. The game name is Centon for the first slice only. The bay slice has no in-universe name. A later city keeps the real name on the source file until the owner names the place.

Play rules stay ops and ledger rows (`CAR`, `WALK`, `CASH`, `WANTED`, `MOM`). They are not part of the map generator.

## The setup menu is rows, like a Civilization start

The menu is a `.pdl` the editor can list. It is not a new window and not a branch in `game_core`.

```
WORLD | size    | small | standard | large
WORLD | body    | earth | moon | mars | seed
WORLD | seed    | <integer>
WORLD | cell_m  | 1000 | 256 | 20 | 10
WORLD | oceans  | 0.0 to 1.0
WORLD | detail  | centon | bay | <other stamp id>
```

Suggested sizes, in globe cells of about 1000 km, so the whole body fits one desk under the 220 by 300 cap:

| Size | Globe cells | What the player is choosing |
|---|---|---|
| small | about 16 by 8 | one band of a body, a test |
| standard | about 40 by 20 | one Earth-like body |
| large | about 80 by 40 | still one body, more coast |

`body=seed` means no downloaded city. `dust_seed` from the dust plan (a pure hash of the parent seed and the child address) fills land, water, and relief. `body=earth|moon|mars` means the coarse page is a scaled public raster, and cities are stamps on top. The same seed twice must write the same grid. A hand-edited generated file must not be the authority. Edits are `OVR` rows, as in `RPG-DUST-DESIGN.md` section 4.

A later `world_new_op` would read those rows and call the painter. It is not written. The five design-choice questions go in the decision log before that op.

## How detail sits on a coarse world

This is the dust scale ladder, used only at the levels a city needs.

| Level | Cell | Role |
|---|---|---|
| globe | about 1000 km | the body. One desk. Land, water, ice. |
| region | about 16 km | one coarse cell becomes a region page: coast, river, mountain. |
| area | about 256 m | a district. Still generated, or a scaled-down city stamp. |
| play | 10 m or 20 m | Centon and the bay slice, as they are now. |

Zoom is a door to the child page, not a new camera. The child seed is the hash of the parent seed and the address. The child page must agree with the parent cell: if the globe cell is water, the play page may not be a city, and a gate fails the build. `RPG-DUST-PETSLIKE-BOARD-DESIGN.md` calls that the conservation rule (the fine page has to agree with the coarse page you just left).

A downloaded city is an override, not a second planet. The stamp names a box on the globe. Inside that box the play page is the city `.pdl`. Outside it, the seed stands. Removing the stamp deletes nothing but the override. The coarse world remains. Charleston and San Francisco are the first two stamps. More cities are the same stamp with a new box and a coarser `CELL` so the grid still fits 220 by 300.

Do not store the whole Earth at 10 m. That is tens of millions of cells on a side. The dust plan already says a planet is a seed plus a ledger, and a page is generated when a window needs it.

## Cities worth a stamp, and where the file comes from

Use one source per city. Prefer a city footprint service that answers GeoJSON for a box. OpenStreetMap through Overpass is the fallback for roads, water, parks, and piers. Overpass was at capacity on 2026-11-21 (HTTP 502, then a mirror that returned no bytes). When it is busy, use the city portal. Do not download a state or a country extract. The game does not call these servers while it runs. The file is CONFIG, saved once under `#.NNEST_ASSETS`, not in git.

Cell size is chosen so the box fits the painter cap. 10 m is a neighbourhood. 20 m is a few districts. 256 m is a metro as one area page. The list is the source, not a promise that the pull has been run.

| Place | First source | Fallback | Scale that fits one desk |
|---|---|---|---|
| Charleston (done, Centon) | `gis.charleston-sc.gov` EAM buildings, MapServer layer 18 | Overpass | 10 m, the Battery box |
| San Francisco (done, bay slice) | `data.sf.gov` footprints `ynuv-fyni`, streets `3psu-pn9h` | Overpass | 20 m, Dogpatch to the tower |
| New York | NYC Open Data building footprints `5zhs-2jue` on `data.cityofnewyork.us` (`nqwf-w8eh` is 404) | Overpass | 20 m for lower Manhattan, or 256 m for the five boroughs. A FiDi box is already cached |
| Chicago | Chicago building footprints `syp8-uezg` on `data.cityofchicago.org` (the map view is `hz9b-7nh8`) | Overpass | 20 m for the Loop, 256 m for the city. A Loop box is already cached |
| Los Angeles | LA GeoHub (`geohub.lacity.org`) building outlines | Overpass | 256 m for the basin, 20 m for one neighbourhood |
| London | OS Open Zoomstack (`osdatahub.os.uk`, open download) | Overpass | 256 m for Greater London, 20 m for the City |
| Paris, Rome, Cairo, Lagos, Mumbai, Shanghai, Mexico City, São Paulo, Sydney, Tokyo | Overpass for a box the owner names | a national portal only after a person checks the licence | 20 m for a centre, 256 m for the metro |

Tokyo's national maps are at the Geospatial Information Authority of Japan (`gsi.go.jp`). Use them only after the licence for a game cache is read. Until then the stamp is Overpass.

Street names in the `.pdl` are source labels. They are not in-universe names.

## Moon and Mars

These are coarse bodies, not street grids. A city stamp does not apply until a person names a site.

| Body | What to download | What becomes the grid |
|---|---|---|
| Moon | USGS Astrogeology lunar maps (`astrogeology.usgs.gov`) and LRO (`lunar.gsfc.nasa.gov`) | elevation and a mare or highland class, cells of 10 to 50 km so the near side fits one desk |
| Mars | USGS Mars maps on the same Astrogeology site, MOLA elevation | elevation, a dust or ice class, cells of about 50 km |

HiRISE and LRO camera frames are pictures. They are not the walk grid. The same rule as Google Maps on the Battery note: a picture is not CONFIG. The raster that becomes `CELL` values is the elevation product, sampled down to the desk cap.

In-universe names for the Moon and Mars stay in `22.lore-ip`. This note does not assign them.

## The puller (2026-11-21)

There was no script. The one-off curls that built Centon and the bay slice are not a tool. The tool is `rta_pull_op` (`@.apps/rta/ops/rta_pull_op.c`), catalog `@.apps/rta/sources.pdl`, harness `rta_pull` (16 checks, no network: a local file copies, the same id twice matches, an unknown id and an oversize body write nothing, a `later` row is refused).

`list` prints each id. `pull <catalog> <id> <outdir>` saves one row. `http` uses curl with a 32 MiB cap. `file` copies a path beside the catalog. `later` is a named source that is not a desk-sized file, and the op exits 5 with nothing written.

The on-disk list, with sizes and sha256, is `#.NNEST_ASSETS/rta-sources/INVENTORY.pdl`. It is not in git. Any later game may read those files as CONFIG. The handoff that says what to build next is `XO/31.Nov/nov-22/0.rta-cities-handoff.md`.

Saved, outside git, under `#.NNEST_ASSETS`:

| Cache | What it is | Observed |
|---|---|---|
| `rta-battery/` | Charleston Battery through the College, already on disk | 16 MB |
| `rta-bay/` | Dogpatch through the tower, already on disk | 24 MB |
| `rta-sources/nyc-fidi.geojson` | NYC footprints `5zhs-2jue`, box 40.704 to 40.714, -74.018 to -74.008 | 281572 bytes, 270 features |
| `rta-sources/chicago-loop.geojson` | Chicago footprints `syp8-uezg`, box 41.878 to 41.890, -87.640 to -87.625 | 537079 bytes, 406 features |

The old NYC id `nqwf-w8eh` returns 404. The Chicago map id `hz9b-7nh8` is the map view. The table id above is the one that returned polygons. Both boxes came back under the 1500-row page cap, so the file is the whole box. Overpass still returned no bytes on 2026-11-21, so Paris and the other centre rows stay `later`. London Zoomstack, the LA hub, the lunar LOLA DEM, and the Mars MOLA grid stay `later` because each file is a whole country or a whole body. A Moon or Mars pull waits on a bounded elevation tile.

## What a later agent does, in order

1. Read this file and `RPG-DUST-DESIGN.md` sections 3 to 5. Read `RTA-BATTERY-CONVERSION-2026-11-21.md` for the painter.
2. Add the setup rows and one pure op that writes a tiny seeded globe. Harness: the same seed twice matches, a different seed does not, a water parent cell cannot host a city stamp.
3. One Earth coarse page from a public land-ocean raster, scaled into the desk cap. No cities yet.
4. Stamp Centon onto one cell of that page. The play page is the existing Centon desk. The conservation check compares the stamp's water fraction to the parent cell.
5. Repeat for the bay slice, then one city from the table above, at 20 m or 256 m, in a scratch house.
6. Moon or Mars is the same op with a different `body` row and no street layer.

Do not turn on the world clock. Do not put the generator in `game_core.c`. Do not download a new city until the owner names the box.
