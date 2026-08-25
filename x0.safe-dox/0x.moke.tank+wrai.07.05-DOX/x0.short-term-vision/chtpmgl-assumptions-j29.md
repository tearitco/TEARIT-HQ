# CHTPMGL Assumptions J29

Date: 2026-06-29
Status: Current understanding of `chtpmgl`, grounded in the present code and Wraith validation files.

==================================================
1. PURPOSE OF THIS DOC
==================================================

This file exists to make the `chtpmgl` lane explicit:

- what is actually implemented now
- what is only placeholder/demo state
- how `chtpmgl` differs from plain `chtpm`
- what assumptions I am currently making
- what should be finished before we trust it as the canonical rich UI reference

This is not a final spec. It is a corrective alignment doc.

==================================================
2. CURRENT REFERENCE FILES
==================================================

Primary engine/code references:

- `pieces/apps/gl_os/plugins/gltpm_parser.c`
- `pieces/apps/gl_os/plugins/gl_desktop.c`
- `!.2do/handoff-j1.txt`

Current Wraith-facing validation files:

- `projects/wraith-alpha/wraith-projects/chtmgl-wraith/README.md`
- `projects/wraith-alpha/wraith-projects/chtmgl-wraith/ASSUMPTIONS.md`
- `projects/wraith-alpha/wraith-projects/chtmgl-wraith/layouts/chtmgl-wraith.chtpm`
- `projects/wraith-alpha/wraith-projects/chtmgl-wraith/session/scene.objects.pdl`

Related comparison reference:

- `projects/agy-text-editor`

==================================================
3. MY CURRENT ASSUMPTION
==================================================

`chtpmgl` should become the richer visual/layout reference layer for TPMOS and Wraith, but not the source of behavioral truth.

Behavioral truth should still live in:

- reusable ops
- session/state files
- project-owned receipts / scene outputs

`chtpmgl` should define how rich controls, media surfaces, and structured layouts are expressed and rendered.

It should not force a second copy of navigation logic, filesystem logic, or project lifecycle logic.

Just as important:

- ASCII auditability must remain first-class
- `chtpmgl` should not mirror by trying to convert pixels into ASCII
- instead, ASCII and GL should project from the same semantic UI truth

==================================================
4. HOW I CURRENTLY UNDERSTAND THE DIFFERENCE
==================================================

## `chtpm`

Current role:
- text-oriented markup
- button/link style interaction
- straightforward parser flow
- good for ASCII-first proof surfaces

Strength:
- simple and already integrated into the main Wraith lane

Weakness:
- rich layout/media/widget vocabulary is limited or awkward

## `chtpmgl`

Current role:
- GL-oriented structural UI / scene description
- richer container and widget vocabulary
- intended path toward panels, menus, sliders, checkboxes, image surfaces, canvas-like zones, and later media

Strength:
- can express richer visual composition than plain `chtpm`
- already has hierarchical/container concepts in the GL lane

Weakness:
- not yet finished enough to treat as the one stable reference
- current implementation mixes real capability with placeholder behavior

## `wraith_gl`

Current role:
- Wraith-specific GL presenter/runtime path
- host-side desktop/window rendering
- current RGB/GL mirror for Wraith windows and project surfaces

Important distinction:
- `wraith_gl` is not yet the same thing as a clean `chtpmgl` contract
- today it is a host renderer/presenter with project probing and scene rendering seams
- longer term it should consume the same semantic UI truth that also feeds the ASCII audit surface

==================================================
5. WHAT IS ACTUALLY IMPLEMENTED NOW
==================================================

Based on the current `1.TPMOS` code:

1. Hierarchical/container parsing exists in the GL lane.
   - `gltpm_parser.c` handles container-style tags such as `panel`, `div`, `window`, `header`, `menu`, `ul`, and `canvas`.
   - Parent/child relationships are tracked with a stack.

2. Button-like interactive parsing exists.
   - `button`, `checkbox`, `slider`, `menuitem`, and `li` are parsed into a shared button/menu-style structure.
   - In current code, these are not yet deeply different semantic widgets; many are treated through the same interactive path.

3. Text nodes exist.
   - `text` labels are parsed and laid out relative to their parent.

4. Image loading exists in the GL lane.
   - `<img>` is explicitly parsed.
   - texture loading goes through `stb_image`
   - `gl_desktop.c` renders a textured quad when texture load succeeds
   - failed image loads fall back to a magenta placeholder block

5. Clipping / scoped rendering exists.
   - the GL lane uses a scissor stack for container clipping

6. Menu synchronization exists at the host window level.
   - `sync_gltpm_menu_from_scene()` copies parsed button labels into the host menu options

7. Canvas-like zones exist conceptually.
   - parser recognizes `canvas`
   - Wraith-side validation currently maps the canvas-like concept onto `${game_map}` / `target_surface=game_map`

==================================================
6. WHAT IS NOT REALLY FINISHED
==================================================

This is the important part.

## Audio / video are not finished in the active code lane.

The old `J1` handoff says audio/video placeholder logic was added at some point, but in the current `1.TPMOS` GL parser and desktop code I checked, there is no active `<audio>` or `<video>` parsing path.

So my current assumption is:

- image support is real enough to count
- audio/video support is still not operational in the active lane
- if we want `chtpmgl` to be a trustworthy reference, audio and video need to be brought to the same level of explicitness as `<img>`

## The older prototype does contain usable prior art.

The older reference lane under:

- `x0.parent-level-dev-env-02.01/#.CHTMGL.E23=cordsclean]💯️`

contains real prior experiments for:

- image tags/loading
- audio plumbing
- video decode/controller work

Additional working reference for ASCII-side image projection:

- `x0.parent-level-dev-env-02.01/#.x0.ref/#.img2term.c`

Current assumption:

- those files are reference material, not a drop-in source of truth
- the best path is to copy-mod the ideas into `chtmgl-wraith`
- media should first appear in ASCII as readable tag/state truth
- image should also get a basic coarse ASCII projection early, to prove the back-and-forth media pipeline
- later, decoded RGB media may also be reprojected back into ASCII instead of staying GL-only

## Widget semantics are still shallow.

Current parser behavior groups several widget tags into the same button-like structure.

That means:
- `checkbox`
- `slider`
- `menuitem`
- `button`

are not yet fully separated by state model, rendering contract, and interaction contract.

## Wraith-side `chtmgl-wraith` is still mostly a probe.

Current `chtmgl-wraith` uses:
- a `.chtpm` file describing the intent textually
- a static `scene.objects.pdl` probe

That means the Wraith project is useful as a validation target, but it is not yet proving a complete end-to-end `chtpmgl` pipeline from markup -> parser -> project-owned scene output -> host render.

## Navigation/control state is still fragile.

The old handoff explicitly calls out instability around hierarchical menu navigation and control delegation.

I currently assume this area is still not mature enough to declare done.

==================================================
7. WHAT I THINK `chtpmgl` SHOULD BECOME
==================================================

If aligned with your vision, `chtpmgl` should become the canonical rich layout/render reference for:

- image surfaces
- audio surfaces
- video surfaces
- panels
- menus
- checkboxes
- sliders
- richer list/grid layout
- canvas-like interactive zones
- later file browser surfaces in GL

But it should do that over shared ops/state, not over duplicated behavior.

So the architectural rule I would use is:

- `chtpm` / ASCII can prove behavior first
- `chtpmgl` should prove the rich rendered form of the same behavior
- shared ops/state stay below both

==================================================
8. ASCII MIRROR / AUDIT CONTRACT
==================================================

This is the rule I currently think we should enforce before pushing further.

ASCII should mirror semantic truth, not GL pixels.

That means `chtpmgl`-style surfaces should resolve into a shared intermediate truth such as:

- object identity
- role
- label
- value/state
- nav index
- action
- grouping / row ownership
- focus / selected state
- asset references where behaviorally relevant

Then:

- GL renders the rich surface
- ASCII renders the audit surface from the same truth

ASCII mirror must preserve:

- nav order
- selected/focused control
- action labels
- row/group structure
- scroll/thumb position
- important state values

ASCII mirror does not need to preserve:

- exact colors
- gradients
- pixel geometry
- animation visuals
- shader effects

Instead, GL-only richness should degrade into readable audit tokens, for example:

- `Theme: amber-dark`
- `BG: image wallpaper_01`
- `Preview: image selected`
- `Audio: track_02 playing`
- `GL-state: rotating model active`

Proposed object classes:

1. interactive
   - must mirror fully in ASCII
2. informational
   - must mirror as readable text/state
3. decorative
   - may mirror only as a short token or be omitted

Important rule:
- if a GL object affects user choice, agent control, or state understanding, it needs an ASCII/audit counterpart

==================================================
9. HOW THIS AFFECTS THE FS PROJECT
==================================================

My current recommendation is still:

- one `fs` project
- first surface: ASCII / `chtpm`
- later surface: `chtpmgl`

Reason:
- we should not split filesystem behavior into `ascii-fs` and `gl-fs`
- we should finish `chtpmgl` enough that the GL filesystem surface is obviously the same system, not a second implementation

So `fs` should remain one project identity, while `chtpmgl` becomes the richer renderer contract we can trust later.

==================================================
10. WHAT SHOULD BE FINISHED BEFORE WE TRUST CHTPMGL
==================================================

I would consider `chtpmgl` directionally proven only after these are explicit:

1. `<img>` is confirmed and documented as stable in the active lane.
2. `<audio>` has a real parser + host/daemon integration contract.
3. `<video>` has a real parser + host/daemon integration contract.
4. `checkbox` has a clear state + render + action model.
5. `slider` has a clear state + render + action model.
6. `menu` / nested menu behavior is stable enough to navigate without special-case hacks.
7. container layout rules are documented:
   - parent/child positioning
   - clipping
   - sizing defaults
   - auto layout assumptions
8. Wraith validation should be project-owned end-to-end, not mostly static scene mock data.
9. ASCII mirror contract is explicit and proven against real `chtpmgl` surfaces, not only static probes.

==================================================
11. CURRENT LAYOUT ASSUMPTIONS
==================================================

What I currently infer from the code:

- container tags create parent nodes
- child text/buttons are auto-laid out if explicit coordinates are absent
- some layout is absolute-ish, some is parent-relative
- clipping is container-scoped through scissor rectangles
- this is not yet a full declarative layout system with a finished box model

So I do not currently assume `chtpmgl` has a complete layout model.
I assume it has a usable but incomplete structural model.

==================================================
12. PRACTICAL NEXT STEPS
==================================================

Near-term:

1. Keep `fs` scaffold light.
2. Pivot attention to `chtpmgl`.
3. Audit active code for:
   - real image path
   - missing audio path
   - missing video path
   - current widget/layout semantics
4. Decide the exact contract for:
   - media tags
   - canvas/game_map zones
   - widget state
   - semantic mirror / ASCII audit projection
5. Update `chtmgl-wraith` so it proves more real pipeline behavior and less static placeholder state.

==================================================
13. OPEN ASSUMPTIONS FOR USER CORRECTION
==================================================

These are the assumptions I want corrected if they are off:

1. `chtpmgl` is meant to become the canonical rich UI/media reference, not just an experimental branch.
2. We should finish image/audio/video explicitly so future agents are not guessing whether the direction is right.
3. One project can have multiple render surfaces; we should not fork project identity by renderer unless the behavior truly diverges.
4. Wraith should use `chtpmgl` as a renderer/layout reference later, but not let it own shared business logic.
