# 0x-Pet Wraith Architecture
Date: 2026-06-29
Status: Future-track architecture note

## Purpose
Record the future `0x-pet-wraith` idea now, while the Wraith shell/editor and XO enclosure/controller patterns are still being shaped.

This is not top priority ahead of Wraith shell/fs work, but it is important enough to define now so later implementation does not fork into incompatible patterns.

## Core Understanding
The important thing is not whether the front door is called `x0-pet`, `0x-pet`, or Wraith.

The important thing is that the **enclosure** becomes the sovereign file-backed artifact, and the surrounding programs become different hosts over the same underlying contract.

That means:
- one flow can drop a controller into an enclosure and let Wraith drive/control it
- another flow can drop one or more enclosures into a `0x-pet/` directory and select one through a filesystem/editor pattern
- both flows should operate on the same enclosure truth, not on separate runtime-specific formats

## The Real Difference
Your observation is right: the likely difference between the current `x0-pet` idea and a future `0x-pet` program is mostly:
- where the enclosure is discovered from
- whether enclosure location is defaulted or explicitly chosen
- what the default front door/host is

It should **not** become:
- a separate enclosure format
- a separate controller format
- a separate editor format
- a separate PAL/agent contract

## Current Code Evidence

### Active Wraith implementation lane
For the current phase, all real Wraith implementation work should stay in:

`x0.parent-level-dev-env-02.01/1.TPMOS_c_+rmmp.0102.0027`

The Wraith copy inside `x0.moke-pet-project-04.03` is a reference context only for now.

Rule:
- do not implement this future `0x-pet-wraith` direction by editing both Wraith trees in parallel
- polish the `1.TPMOS` Wraith lane first
- copy/adapt into `x0.moke-pet-project-04.03` only after the Wraith side is actually proven
### Moke-Pet / enclosure precedent
`x0.moke-pet-project-04.03/README.md` and `HANDOFF.md` already establish the right enclosure direction:
- project root owns the enclosure
- controllers and pets are drop-in occupants
- dynamic scan/perception is file-backed
- controller discovery should be flexible

Key precedent:
- enclosure ownership belongs to the project root
- controllers should be discoverable, not welded into one path

### Legacy `xo-pet-v1` limitation
`projects/xo-pet-v1/pieces/manager/xo-pet_manager.c` still shows the older split-root/default model:
- it probes upward for `pieces/world_tank_01/map_enclosure`
- it hardcodes bundle-level paths like `pieces/world_tank_01/map_enclosure`
- controller and world roots are treated as partly separate runtime concerns

That is useful as a PoC, but it is the wrong long-term shape for the unified editor/Wraith path.

### Wraith editor precedent
`projects/wraith/wraith-projects/wraith-ed` already proves the pattern you want to reuse:
- file-backed project truth
- Wraith-hosted editor surface
- project-owned input op
- saves / PAL events / exports as auditable files

The important leap is:
- do not make `0x-pet-wraith` as a special one-off pet shell
- make enclosure selection/edit/control follow the same editor-pattern architecture as `wraith-ed`

## Proposed Model

### 1. Enclosure Is The Sovereign Artifact
An enclosure should be a file-backed directory bundle with:
- world/tank state
- map/enclosure files
- resident pieces/entities
- controller attachment metadata
- optional editor metadata
- optional export/receipt files

That enclosure should be usable by:
- Wraith-hosted control/view
- Wraith-hosted editing
- `0x-pet` selection and launch
- PAL/Prisc orchestration
- agents calling the same ops

### 2. Host Programs Are Just Front Doors
The future host surfaces should differ mainly in discovery/default behavior:

#### Wraith-hosted flow
- enclosure opened inside Wraith
- enclosure controlled through a Wraith project window
- enclosure modified through editor-pattern controls

#### `0x-pet` program flow
- one or more enclosures live under a chosen directory such as `0x-pet/`
- user selects enclosure through filesystem-style navigation
- selected enclosure opens under the same underlying contract

These should share:
- enclosure format
- controller attach contract
- editor/save/export contract
- PAL/Prisc callable ops

### 3. Editor Pattern Should Own The Future Shape
This should be explored alongside `wraith-ed`, not as a totally separate pet UI rewrite.

Reason:
- `wraith-ed` already proves file-owned editing, PAL event drafts, save artifacts, and project-local ops
- if enclosure control/editing follows that pattern, agents and humans get one uniform surface
- later PAL and agent automation can call the same ops rather than learning a separate pet-specific workflow

## Architectural Direction

### Recommended shape
Treat `0x-pet-wraith` as an **editor-pattern enclosure host**, not just a controller app.

That means the architecture should combine:
- enclosure discovery
- enclosure selection
- controller attachment/swap
- enclosure editing
- save/export/receipt generation

through one op/PAL/editor-friendly contract.

### Not recommended
- keeping a permanent split between “Wraith-controlled enclosure” and “0x-pet-managed enclosure”
- hardcoding one default enclosure path forever
- making pet editing depend on manager-local logic instead of editor-pattern ops

## Proposed Filesystem Shape

This is illustrative, not final:

```text
0x-pet/
  enclosures/
    enclosure-desert-01/
      enclosure.pdl
      session/
      maps/
      residents/
      controllers/
      exports/
      pal/
  controllers/
  templates/
```

Equivalent Wraith-hosted versions could live:
- as Wraith internal projects
- or as enclosure bundles discovered and opened by a Wraith enclosure host/editor project

The important part is that the enclosure directory remains sovereign.

## Recommended Op Surface

This future track should eventually reuse the same planning rules as the rest of the short-term work:
- prefer existing ops
- copy-and-mod locally first if needed
- only replace shared ops after local proof

Likely future op families:
- `enclosure_scan <root> <out_manifest>`
- `enclosure_select <manifest> <selection> <out_state>`
- `enclosure_attach_controller <enclosure_id> <controller_id>`
- `enclosure_detach_controller <enclosure_id> <controller_id>`
- `enclosure_load <enclosure_path> <out_state>`
- `enclosure_save <enclosure_path> <slot_or_name>`
- `enclosure_export <enclosure_path> <out_manifest>`
- `enclosure_set_mode <enclosure_id> <mode>`

Editor-pattern follow-ons:
- `editor_load_enclosure`
- `editor_write_enclosure_map`
- `editor_write_controller_binding`
- `editor_write_pal_event`

## Relation To Current Priorities
This is not first priority ahead of:
- Wraith nested discovery
- Wraith fs views
- Wraith launch/unified program discovery
- Wraith settings/customization

But it should be explored at roughly the same time as deeper `wraith-ed` work, because:
- the editor pattern is the right place to unify enclosure selection/edit/control
- otherwise a second pet-specific workflow may harden in parallel

## Recommended Sequence
1. Finish Wraith shell/fs/discovery contract
2. Push `wraith-ed` farther into real file-browser/editor behavior
3. Explore enclosure-as-artifact using the same editor pattern
4. Define `0x-pet-wraith` as a host/editor over enclosure bundles
5. Only then decide whether `0x-pet` should be:
   - a dedicated top-level program
   - a Wraith internal project
   - or both, sharing the same enclosure ops

## Open Questions
These do not block recording the idea, but they will matter later:
- should `0x-pet` be a top-level TPMOS project, a Wraith internal project, or both?
- should enclosures live only under a dedicated `0x-pet/` directory, or also be selectable from broader filesystem views?
- should the first enclosure chooser be built directly into `wraith-ed`, or as a sibling Wraith project that reuses the same editor/file ops?
- should controller binding be stored inside the enclosure bundle, or in a separate host/session layer when the same enclosure is opened in different contexts?
- do you want `x0-pet` and `0x-pet` treated as the same eventual product name, or as distinct modes/front doors?

## Rendering Note (added 2026-07-02)
If enclosures later include 3D/voxel maps (e.g. a tank/world rendered in
3D, not just top-down), that rendering must follow the locked decision in
`x0.piececrafts/ARCHITECTURE-RGB-RENDERING.md`: no OpenGL, ever — the
target is direct RGB signal output, with the composed pixel buffer as the
only durable contract between rendering and display. Any future
enclosure/pet 3D rendering work should reuse that renderer's voxel-cube
convention (tiles extrude negatively from a shared `wy=0` ground plane,
above-ground pieces extrude positively) rather than inventing a second one.

## Manager-Loading Note (added 2026-07-02)
Standing, locked rule (project owner directive): every Wraith-hosted
project's layout must load a manager — the same way HTML loads a
`<script>` tag — regardless of whether its actual hot-path logic already
lives entirely in a synchronous `ops/wraith_project_input.c` file. This
was enforced tonight across 9 Wraith-hosted projects (piececraft-wraith,
fs, chtmgl-video-isolate, chtmgl-wraith, screen-record, wraith-3d-cube,
wraith-browser, wraith-ed, web-cam) — see
`x0.piececrafts/wra-mana-checklist.txt` for the reusable pattern
(init-only manager, `project.pdl`'s `manager=` field as the real
auto-launch trigger, `<module>` tag in the layout as the declared
TPMOS-compliant shape). This directly matches the "Host Programs Are Just
Front Doors" principle above: whatever eventually hosts `0x-pet`
enclosures (Wraith-internal project, dedicated `0x-pet` program, or
both) should follow this same manager-per-project contract, not invent a
separate loading convention. Wraith itself does not yet read the
`<module>` tag to trigger the launch (the real mechanism is
`project.pdl`'s `manager=` field, read by `wraith-alpha_manager.c`) —
worth knowing if this ever gets refactored toward the more literal
HTML/`<script>` model the "browser" long-term direction implies.

## Bottom Line
Yes, the idea is coherent.

The clean version is:
- enclosure is the truth
- Wraith and `0x-pet` are hosts/front doors
- editor-pattern ops define selection, control, modification, save, export
- PAL/Prisc and agents call those same ops later

That is the direction most likely to keep enclosure control, editing, and automation uniform instead of fragmenting into multiple pet-specific systems.
