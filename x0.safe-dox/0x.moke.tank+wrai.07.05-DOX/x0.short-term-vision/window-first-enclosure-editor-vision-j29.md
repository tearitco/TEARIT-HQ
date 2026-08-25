# Window-First Enclosure Editor Vision J29

Date: 2026-06-29
Status: Vision/architecture note before the next editor-oriented push.

==================================================
1. CORE IDEA
==================================================

The first real version does not need to be one giant editor window.

It can be a set of cooperating Wraith programs/windows operating on the same enclosure/project truth.

That means:

- one window can be map 2D view
- one window can be map 3D / canvas view
- one window can be picker/entity chooser
- one window can be event editor / script editor
- one window can be exporter / packager

All of them can share the same enclosure/project files.

Later, a unified editor can bring these together into one program window without changing the underlying truth or ops surface.

==================================================
2. WHY THIS IS GOOD
==================================================

This is probably the fastest path to your real dream.

Reason:

- it matches how Wraith already works well: multiple hosted windows
- it avoids waiting for one giant editor abstraction
- it lets each tool stay narrow at first
- it still converges cleanly into a later integrated editor

This is not a detour from the editor pattern.

It is a staged version of it.

==================================================
3. THE SHARED TRUTH
==================================================

The important rule is that these windows/programs are not separate systems.

They should all operate on the same file-backed artifact:

- enclosure bundle
- or project bundle

Depending on host mode.

That shared truth should include things like:

- map files
- entity/piece files
- event/script files
- controller bindings
- session/editor metadata where needed
- export/build metadata

==================================================
4. THE FIRST PRACTICAL PROGRAM SET
==================================================

The initial split could reasonably look like:

1. Map View
   - 2D and/or 3D view
   - move around
   - inspect / xelect tiles or entities

2. Picker
   - choose entities/assets/tiles/controllers/etc.
   - writes selection state back to enclosure/project

3. Event Editor
   - edits events/behavior for the selected entity
   - saves those files into the same enclosure/project

4. Exporter / Builder
   - recognizes a valid project/enclosure
   - exports or packages it into a runnable/playable form

These can be separate windows first.

Later, the unified editor can host them as subtools or panels.

==================================================
5. HOW THIS RELATES TO WRAITH-ED
==================================================

This suggests a useful distinction:

`wraith-ed` does not need to do everything immediately.

It can evolve in one of two ways:

## Path A: one growing integrated editor

- `wraith-ed` gradually absorbs map view, picker, event editor, exporter

## Path B: editor family first, integration later

- several narrow Wraith projects/windows prove the tool slices
- `wraith-ed` later becomes the integrated host over those same ops/state patterns

Current recommendation:

Start closer to Path B.

That seems better aligned with:

- Wraith’s current multiwindow strengths
- the desire for speed
- lower implementation pressure

==================================================
6. HOW THIS RELATES TO X0 / 0X PET
==================================================

This actually fits the enclosure discussion very well.

Two host styles can exist over the same underlying truth:

## x0 style

- enclosure contains the Wraith controller/tooling directly
- classic "inside the enclosure" feel

## 0x style

- top-level program/front door
- enclosure selected from a broader directory
- more like current `op-ed` style host behavior

These do not need different artifact formats.

They are just different host/discovery/default modes.

==================================================
7. COMPARISON TO EXISTING CODE
==================================================

This vision is not coming from nowhere.

It already matches important existing precedents:

## Wraith today

- already multiwindow
- already project-hosting
- already supports project-owned input ops and scene/body outputs

## `fs`

- already proving a flexible launcher/filesystem hybrid
- can grow toward enclosure/project selection later

## `wraith-ed`

- already exists as the editor-pattern seed
- already proves file-backed editor-style ownership

## `0x-pet-wraith` architecture note

- already frames enclosure as the sovereign artifact
- already argues that host differences should be about discovery/defaulting, not file format splits

==================================================
8. WHAT THIS IMPLIES FOR NEAR-TERM WORK
==================================================

The next push does not need to answer:

- "what is the final perfect editor?"

It only needs to answer:

- what is the first useful cooperating tool set?

Likely first useful answers:

1. stronger `wraith-ed`
2. map/program that can do 2D and later 3D/canvas view
3. picker/event editor as sibling windows/programs
4. exporter as a separate project-aware tool

==================================================
9. OP SURFACE DIRECTION
==================================================

This window-first plan still wants a shared reusable op surface.

Likely families:

- map load/save ops
- entity selection ops
- event/script read/write ops
- export/build ops
- enclosure/project scan/select ops

The important part:

- separate windows first does not mean separate logic paths
- it should still converge on reusable ops with args

==================================================
10. RECOMMENDED INTERPRETATION
==================================================

This is not "avoid building the editor."

It is:

- use separate windows/programs first where that is faster
- keep them inside one shared enclosure/project contract
- later collapse them into a more integrated editor if and when that becomes useful

That seems like the most realistic and least wasteful route from current Wraith to the fuller dream.
