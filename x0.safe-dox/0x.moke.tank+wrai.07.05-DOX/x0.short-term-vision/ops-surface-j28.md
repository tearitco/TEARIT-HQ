# Ops Surface J28
Date: 2026-06-28
Status: Candidate reusable op surface for Wraith FS, editor, and XO follow-on work

## Purpose
Define the first concrete reusable op surface for the current short-term push.

This document exists to prevent three failure modes:
1. writing new ops when an existing one already covers the need
2. rewriting existing behavior inside managers
3. inventing PAL/Prisc flows that bypass the real reusable ops

## Reuse Order
This order is mandatory:
1. use an existing op as-is if it already fits
2. widen an existing op if the extension is small and makes it more reusable
3. write a new op only if widening an old one would create argument rot, confusion, or project pollution

Every implementation note should explicitly say which of the three happened.

## Safe Modification Rule
When an existing op is the right base but changing it directly is risky:
1. copy it into the local/project lane first
2. modify the copied version there
3. prove the new behavior locally
4. keep the old/shared op behavior intact while proving the new path
5. only replace or upstream the shared op after the local proof is solid

The goal is:
- no premature breakage for old callers
- no speculative shared-op rewrite before the new contract is proven
- eventual convergence back to one reusable op when the new behavior is validated

## Current Candidate Ops To Audit First

### 1. `pieces/apps/gl_os/plugins/+x/gl_os_project_scan.+x`
Current behavior:
- resolves `project_root`
- scans project sources
- writes a cache/manifest-style file

Why it matters:
- closest existing candidate for Wraith nested project/app discovery
- already thinks in terms of scan -> cache/projection output

Current limitation:
- flat/project-list oriented
- not yet a generic nested crawler
- tied to GL-OS naming/output assumptions

Best likely path:
- widen into a more general scan op if the output contract can be cleaned up without GL-OS pollution

### 2. `pieces/apps/gl_os/plugins/+x/gl_os_loader.+x`
Current behavior:
- switches context into GL-OS
- writes manager/layout state
- triggers frame refresh marker

Why it matters:
- closest existing launch/context-switch seam
- useful reference for Wraith entry-launch routing

Current limitation:
- specific to GL-OS desktop activation
- not a general entry launcher yet

Best likely path:
- keep as reference unless the launch contract can be generalized cleanly

### 3. `pieces/apps/playrm/ops/+x/project_loader.+x`
Current behavior:
- scans `projects/`
- builds a temporary menu/source artifact
- writes chosen `project_id` into state

Why it matters:
- real existing loader flow
- already scan/select/load shaped

Current limitation:
- uses temp menu generation and direct state mutation for a specific runtime
- still flat-project oriented
- contains some older non-TPMOS patterns like `system()`

Best likely path:
- reuse the flow ideas, but only widen the op if it can be cleaned without dragging old coupling into Wraith

### 4. `pieces/apps/playrm/ops/+x/scan_op.+x`
Current behavior:
- scans current world/tile/entity context
- resolves project paths from env/state
- recursively discovers entities from file-backed directories

Why it matters:
- proves recursive file-backed discovery already exists in the codebase
- useful as a reference for "auto-discovery first" entity logic

Current limitation:
- domain-specific to gameplay/tile inspection
- not a shell/browser op

Best likely path:
- preserve as a reference seam for recursive scanning discipline, not as the actual filesystem browser op

### 5. `projects/wraith/wraith-projects/*/ops/+x/wraith_project_input.+x`
Current behavior:
- project-owned input bridge for Wraith validation projects
- consumes project `session/history.txt`
- mutates project-owned state/body/scene/output files

Why it matters:
- current canonical proof that project-specific behavior should stay in project ops
- the right pattern for editor/XO follow-on work inside Wraith

Current limitation:
- project-local behavior surface, not shared shell/file op surface

Best likely path:
- keep project-specific logic here
- let these project ops call shared file/save/export ops instead of owning all file logic directly

## Candidate Existing Areas To Inspect Before New Ops
- `op-ed` file browser and save/load/export flows
- launcher/file-browser related code under `pieces/apps/*`
- existing state/config utilities
- existing project scan/cache generation flows

Rule:
- before adding a new Wraith FS op, inspect these areas and record why reuse or widening was or was not viable
- if the answer is "copy and modify first", record the intended local proving path and the later upstream/replacement condition

## Proposed Short-Term Shared Op Families

These are target surfaces, not guaranteed new binaries.
Some should be existing ops widened into cleaner contracts.

### A. Discovery / Manifest
Goal:
- crawl Wraith-internal nested content
- classify entries
- emit auditable manifest/projection files

Target shape:
- `scan_wraith_tree <root> <mode> <out_manifest>`
- `refresh_wraith_manifest <root> <out_manifest>`

Responsibilities:
- recurse nested directories
- identify `program`, `project`, `dir`, `file`
- preserve full nested paths
- emit data for launcher or fs views

Likely first audit target:
- `gl_os_project_scan.+x`

### B. Filesystem Navigation
Goal:
- keep `text-fs` and `gui-fs` on the same path/navigation truth

Target shape:
- `fs_list <path> <view_mode> <out_file>`
- `fs_enter <current_path> <entry_name> <out_path>`
- `fs_back <current_path> <out_path>`
- `fs_stat <path> <out_file>`

Responsibilities:
- path normalization
- current directory listing
- typed entry labeling
- parent/back traversal

Likely first audit target:
- existing file-browser flows in `op-ed`

### C. Launch / Open
Goal:
- launch a discovered Wraith entry through reusable data, not hardcoded manager branches

Target shape:
- `launch_wraith_entry <entry_path> <entry_type> <session_ctx>`
- `open_wraith_project <project_path> <session_ctx>`
- `open_wraith_program <program_path> <session_ctx>`

Responsibilities:
- normalize entry metadata
- update launch/session bookkeeping
- leave project-specific behavior to project-owned ops/files

Likely first audit targets:
- `gl_os_loader.+x`
- `project_loader.+x`

### D. Settings / Config
Goal:
- make settings early and scriptable

Target shape:
- `settings_get <scope> <out_file>`
- `settings_set <scope> <key> <value>`
- `settings_list <scope> <out_file>`

Responsibilities:
- file-backed theme/config reads
- file-backed theme/config writes
- support concrete Wraith appearance settings such as:
  - desktop colors
  - window/chrome colors
  - fonts
  - background images / wallpaper
- stable keys for PAL/Prisc and agents

Likely first audit targets:
- existing state/config write patterns in project managers and utilities

### E. Editor File Actions
Goal:
- keep `wraith-ed` from becoming a manager-local file tool dump

Target shape:
- `editor_load_game <game_dir> <out_file>`
- `editor_save_snapshot <game_dir> <slot_or_name>`
- `editor_write_event <game_dir> <event_id> <source_file>`
- `editor_export_manifest <game_dir> <out_manifest>`

Responsibilities:
- game folder loading
- save snapshot writing
- PAL event artifact writing
- export receipt generation

Likely first audit target:
- existing `op-ed` save/load/export behavior

### F. XO / Controller Surface
Goal:
- define a callable contract that users, agents, and PAL/Prisc can all reuse

Target shape:
- `xo_spawn_world <world_id> <preset>`
- `xo_attach_controller <world_id> <controller_id>`
- `xo_set_mode <world_id> <mode>`
- `xo_tick <world_id>`
- `xo_render <world_id> <out_file>`

Responsibilities:
- controller attachment
- world/tank lifecycle
- tick/render boundary

Likely first audit targets:
- `fuzz-op` mechanics ops
- project-owned `wraith_project_input` follow-on calls

## PAL / Prisc Position
PAL/Prisc should not invent a second behavior layer for shell/fs/editor/XO work.

Correct pattern:
1. ops do the real reusable work
2. PAL/Prisc sequences those ops with args
3. managers route input and publish projections

Bad pattern:
- manager-local behavior for user flow
- separate PAL-only implementation for automation
- third agent-only path for tooling

There should be one real op surface.

## First Audit Pass To Perform Before Implementation
For each desired feature, answer:
1. Is there already an op that does this?
2. If not fully, is there one that covers most of it?
3. Can widening that op keep the args and output cleaner than a new sibling op?
4. If not, what exact pollution or confusion forces a new op?

Minimum first audit targets:
- `gl_os_project_scan.+x`
- `gl_os_loader.+x`
- `project_loader.+x`
- `scan_op.+x`
- `op-ed` file/save/load/export flows

## Review Gate For Future Work
Before merging a shell/fs/editor/XO feature, the implementation note should include:
- reused existing op: yes/no
- widened existing op: yes/no
- new op added: yes/no
- local copied variant used first: yes/no
- reason a new op was necessary, if applicable
- reason direct shared-op modification was or was not safe
- upstream/replacement condition if a local copied variant was used
- PAL/Prisc compatibility of the resulting args

## Definition Of Good
This ops surface is working if:
- Wraith shell features call reusable ops with arguments
- project-specific Wraith behavior stays in project-owned ops
- PAL/Prisc can call the same ops users rely on
- agent automation does not need a parallel code path
- new features tend to widen existing useful ops instead of multiplying near-duplicates
