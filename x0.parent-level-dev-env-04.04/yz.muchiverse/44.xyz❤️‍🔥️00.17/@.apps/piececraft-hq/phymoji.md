# PHYMOJI — piececraft-hq's own real-object voxel-entity standard

**Status: PLANNING/DESIGN ONLY, nothing built yet.** Written 2026-08-03,
direct instruction: document our real intentions for how "a tree," "a
mountain," "a car" actually get rendered, long-term, before building any
of it. Real precedent this doc builds on, not invents:
`#.ref/plugy3d-ngn-2026_v19.PHYM/#.PYMOJI.md` — a complete, already-
written, language-agnostic spec for extruding 2D emoji into 3D
volumetric voxel data. Read that file in full before this one; this doc
is the *adoption plan* for piececraft-hq specifically, not a
restatement of the standard itself.

---

## 1. What "phymoji" means here

**Phymoji = piececraft-hq's own real objects** (trees, later mountains/
cars/animals/NPCs), rendered as genuine depth-extruded, per-voxel-
destructible volumes — **as opposed to** the flat, single-glyph terrain
blocks (grass/dirt/stone/sand/water/wood/leaves/ore in `terrain_legend.
txt`) that make up the ground itself. Direct instruction: these are
**deliberately kept separate systems** — flat terrain blocks stay exactly
as they are (grass is NOT becoming a phymoji this pass), phymoji is
layered on top for real *objects* in the world.

**The real deviation from vanilla Minecraft, stated directly and kept
here on purpose**: a classic Minecraft block is atomic — mine it, the
whole block vanishes. A phymoji entity is internally an **8x8x8 grid of
sub-voxels** (per the real PyMoji standard's own canonical tile-size ×
extrusion-depth). Mining a phymoji tree doesn't have to remove the whole
tree at once — it can remove individual sub-voxels, exposing a real
cross-section. This is a genuinely new mechanic this house hasn't built
before, not a reuse of an existing pattern - real, later work (§5), but
the *data model* (§3) needs to support it from day one so it isn't a
rewrite later.

---

## 2. Real pipeline, mapped onto piececraft-hq's own actual code

The real PyMoji standard's own §8.1 pipeline, with each stage mapped to
what already exists in this house vs. what's genuinely new:

```
Unicode emoji (e.g. 🌳 U+1F333)
        │
        ▼
Font Rasterizer → RGBA PNG        [REAL, EXISTS: chtpm_rgb_render.c's own
                                    ensure_emoji_asset_generated(), calls
                                    ops/+x/emoji_gen_atlas.+x - FreeType +
                                    NotoColorEmoji.ttf, already generates
                                    real RGBA tiles on demand today]
        │
        ▼
Crop to real bounding box          [PARTIALLY REAL: bv_render_3d.c's own
(remove "dead space" - direct       compute_bbox_and_edge_color() already
instruction: "removed of their      does bbox cropping - but for a FLAT
dead space around the tree          2D texture applied to one quad face,
emoji")                             not a real 3D volume - see §4 below]
        │
        ▼
Deep extrude to 8x8x8 voxels,       [NOT BUILT - real new work. Existing
front-crisp/depth-attenuated        pipeline stops at "flat cropped
sides (PyMoji §4, Rules A/B/C)      texture," never produces a genuine
                                     per-voxel 3D model at all]
        │
        ▼
Normalize to 0..7 coord space       [NOT BUILT - real new work]
(PyMoji §5.4)
        │
        ▼
Store as real 3D voxel CSV          [NOT BUILT - real new work, real new
(x,y,z,r,g,b, PyMoji §6.2)          file format/location, see §3 below]
        │
        ▼
Place in world as a real,           [NOT BUILT - real new work, needs its
per-voxel-destructible entity        own mini-DDA per entity, see §4]
```

**The honest gap, stated plainly**: today's pipeline produces a flat
cropped *texture* used to skin a single quad face of a bounding-box
entity (the existing `g_entities[]`/`ray_aabb_hit_3d` system, one AABB
per entity, one texture sample per hit). That's architecturally
different from a real phymoji volume (a genuine 8x8x8 voxel grid with
its own internal structure, minable at the sub-voxel level). Building
phymoji means extending the pipeline past where it currently stops, not
reusing the existing entity system unchanged.

---

## 3. Real data layout (proposed, needs confirmation)

**Kept separate from civ-txt-style flat emoji blocks**, per direct
instruction. Existing flat-texture assets live at
`pieces/registry/emoji_assets/<hex>/voxels_16.csv` (a 2D texture crop,
NOT a 3D model, despite the "voxels" name — the naming is a real
pre-existing inconsistency in this house's own code, not something
this doc invents). Proposed real phymoji namespace:

```
pieces/registry/phymoji_assets/<entity_id>/
    voxels.csv          # real PyMoji §6.2 format: x,y,z,r,g,b
    meta.json           # real PyMoji §7.1 entity definition (adapted)
```

Where `<entity_id>` is a real, stable name (`tree_small`, `tree_large`,
`chicken`, ...), not a raw emoji hex — multiple size/species variants of
"a tree" get their own real distinct entity_ids, per direct instruction
("we are going to have different size tree emojis"), rather than one
asset scaled at render time for everything (though PyMoji's own real
§5.1 `voxel_expansion` transform is ALSO available for genuine size
variety without a new source asset - both mechanisms are real and
available, see §4c).

**Open question, not decided here**: should a phymoji entity's own real
per-voxel *existence* state (which of the 8x8x8 slots survive after
partial mining, §5) live in the SAME `pieces/registry/phymoji_assets/`
location (shared, template data) or per-WORLD-INSTANCE (each placed
tree in `pieces/world_01/chunks/chunk_X_Y/` gets its own real mutable
copy, since two trees of the same species need independently mineable
states)? This is a real, load-bearing decision - registry data should
stay a read-only TEMPLATE (matches this house's own "global/registry
data is explicitly NOT a world piece" convention, `PORTABLE_ENTITY_
ARCHITECTURE.md` §1) - meaning each WORLD INSTANCE of a tree needs its
own real per-voxel existence bitmap, separate from the shared template
CSV. Flagged for confirmation before Phase C (§5) builds real placement.

---

## 4. Real rendering architecture questions (not decided, need real answers)

### 4a. Per-entity mini-voxel-walk vs. single AABB

Today's `g_entities[]` system tests ONE bounding box per entity per
pixel (cheap - at most a handful of entities, one `ray_aabb_hit_3d`
call each). A real phymoji entity with per-voxel destructibility needs
its OWN internal voxel occupancy grid, walked with its own real DDA
(same technique `mc-speed-algos.md` already documents for terrain) -
NOT a single flat box test. This is a real, nontrivial rendering
extension: `bv_render_3d.c`'s own per-pixel loop needs a new phymoji-
entity test that, on a coarse bounding-box hit, walks the entity's OWN
8x8x8 (transformed by `voxel_density`/`voxel_expansion`/scale, PyMoji
§5) sub-grid to find the real closest surviving sub-voxel - genuinely
more expensive per phymoji entity than the current flat-box entities,
though still cheap per-entity given `mc-speed-algos.md`'s own real
empty-space-skip technique applies here too (skip past a phymoji
entity's own real internal air gaps the same way terrain columns do).

### 4b. Trees: terrain glyph vs. real positioned entity

**Real, load-bearing decision, not assumed**: today's debug flat map
(civ-vs-piece.md's own real build) embeds trees as terrain GLYPHS
(`T`/`%`) directly inside chunk Z-files, contiguous with the ground -
this is what keeps them fast under the empty-space-skip optimization
(`mc-speed-algos.md` §3). Converting trees to real phymoji ENTITIES
means REMOVING them from chunk terrain data entirely and making them
real positioned objects (`entities.txt`-style, own `pos_x/y/z`, own
voxel data) - a genuine architecture change, not a drop-in replacement
of the glyph rendering. **Needs your confirmation before Phase C (§5)
touches `pc_generate_chunk.c`'s own tree placement** - this doc doesn't
decide it silently.

### 4c. Size variety mechanism

Direct instruction: "different size tree emojis." Two real, available
mechanisms, not mutually exclusive:
- **Different source assets** (`tree_small`/`tree_medium`/`tree_large`,
  each its own real emoji glyph + own extruded voxel model) - real
  visual variety, not just scaling the same shape.
- **`voxel_expansion` transform** (PyMoji §5.1) - the SAME source asset
  rendered at different world-block sizes without regenerating data.
Recommend using BOTH: a few real distinct tree assets (sapling/young/
mature, or different species) for genuine shape variety, each further
scalable via `voxel_expansion` for continuous size variation within a
species. Not decided which specific emoji glyphs yet - real future
work, not blocking this doc.

---

## 5. Recommended real build order (proposed, not started)

1. **Real phymoji generator op** - extend the existing on-demand emoji
   pipeline (or a new dedicated `pc_phymoji_gen` op) to produce a real
   PyMoji-compliant 8x8x8 voxel CSV (§6.2 format) from a source emoji
   codepoint - real depth attenuation (§4, Rules A/B/C), real bounding-
   box crop (dead-space removal), real 0..7 normalization (§5.4). This
   is the genuinely new piece of work nothing in this house does yet.
2. **Real per-entity mini-DDA rendering** in `bv_render_3d.c` (§4a) -
   walk a phymoji entity's own real voxel grid instead of a flat box.
   Needed before ANY phymoji entity looks like more than a solid-color
   placeholder cube.
3. **One real tree asset** (e.g. `tree_small`), placed as a real
   positioned entity on the debug flat map (§4b's own decision applied
   concretely) - proves the whole pipeline end-to-end for one real
   object before adding variety.
4. **Chicken demo entity** - direct instruction ("i think we should
   render a chicken emoji as well, that will later be an npc") - same
   real phymoji pipeline, a second real asset, placed on the debug map
   now as a static decoration (real NPC movement/AI is `phase2-plan.md`
   §5's own separate, later scope - this is just proving a second
   real entity type works, not building NPC logic).
5. **Multiple tree sizes/species** (§4c) - once the pipeline is proven
   on one real asset, add real variety.
6. **Per-voxel destruction** (§1's own real deviation from vanilla
   Minecraft) - needs the real per-world-instance voxel-existence data
   model (§3's own open question resolved first) and real integration
   with `pc_break_block`/`pc_place_block` (still not built at all,
   civ-vs-piece.md §6 item 6) - genuinely later work, not this pass.

Nothing in this doc has been built yet - written per direct instruction
to record real intentions before any of it gets coded.
