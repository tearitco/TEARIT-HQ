# Wraith Shell + Filesystem Roadmap
Date: 2026-06-28
Status: Near-term execution plan

## Purpose
Turn Wraith into a real shell that can discover, browse, and launch nested internal projects, files, directories, and app-like programs without hardcoded menu math or project-specific hacks.

This is the most concrete next lane because it directly matches:
- the TPMOS file-truth model
- the Wraith session model
- the current desire for `text-fs` / `gui-fs`
- the later need for editor, XO, and drag/drop-like discovery flows
- the need for a reusable op surface that PAL/Prisc and agents can call with arguments

## Active Codebase Rule
For the current implementation phase, the active Wraith codebase is:

`x0.parent-level-dev-env-02.01/1.TPMOS_c_+rmmp.0102.0027`

Specifically:
- `projects/wraith-alpha`
- `projects/wraith/wraith-projects/`

The Wraith copy inside `x0.moke-pet-project-04.03` is reference-only until the `1.TPMOS` Wraith lane is polished enough to copy over safely.

Future agents should not split implementation across both trees during this phase.

## What This Push Is
This push is not "make Wraith bigger."

It is:
1. move all Wraith-internal content behind one real nested filesystem contract
2. expose that contract through navigable shell views
3. unify launch/discovery rules for files, dirs, and programs
4. keep customization early enough that the shell does not fossilize
5. express the real work as reusable ops instead of one-off manager branches

## Scope

### In Scope
- nested discovery under `projects/wraith/wraith-projects/`
- Wraith-visible file and directory browsing
- app/program discovery using the same nested scanning model
- `text-fs` and `gui-fs` as two presentations over the same underlying tree
- nav-safe folder traversal
- settings project inside Wraith
- reusable file ops where possible
- explicit op surfaces for scan, browse, launch, refresh, and settings writes
- PAL/Prisc-ready command shape for later scripting and agent reuse
- testing via key injection and frame evidence

### Out of Scope
- full file editing
- drag-and-drop GUI polish
- local LLM control loops
- network / P2P integration
- broad parser invention that bypasses existing button/nav patterns

## Concrete Short-Term Goals

### Goal 1: Make Wraith Internal Discovery Canonical
All Wraith-facing projects and app-like programs should resolve from nested paths under:

`projects/wraith/wraith-projects/`

Deliverables:
- one scanner op that walks nested Wraith project/app directories
- one normalized manifest/output file for Wraith launcher consumption
- no dependence on top-level `projects/<id>` placement for Wraith internal content

Preferred shape:
- `scan_wraith_tree <root> <mode> <out_manifest>`
- manager reads manifest/projection output only
- later PAL/Prisc can call the same op directly

Acceptance:
- Wraith launcher can open internal projects from nested paths
- row order is manifest-driven, not hardcoded
- full nested project path survives launch and restore

### Goal 2: Add Real Filesystem Browsing To Wraith
Wraith should expose filesystem traversal as a real shell feature, not as a one-off editor subflow.

Deliverables:
- `text-fs` view: text-first tree/list navigation
- `gui-fs` view: same tree, different presentation
- common underlying scan output and path state
- separate symbols for directory, file, and program entries
- browse/refresh path changes handled by reusable ops, not inline manager code

Preferred op family:
- `fs_list <path> <mode> <out_file>`
- `fs_enter <current_path> <entry> <out_path>`
- `fs_back <current_path> <out_path>`
- `fs_refresh <path> <out_file>`

Acceptance:
- user can navigate down and back up nested directories entirely through nav controls
- current path is file-backed
- directory listing updates when files/folders are added externally
- both views point at the same underlying directory truth

### Goal 3: Support Nested Nav Cleanly
Folder traversal must preserve TPMOS button/nav discipline.

Deliverables:
- reusable nav state contract for nested menus/tree traversal
- no fake text pretending to be buttons
- support for fold/open behavior where it fits the existing parser model
- manager only maps key/input to op calls and projection state

Acceptance:
- nested directory traversal does not break global numbering or back behavior
- Wraith shell can enter and leave a folder stack repeatedly without focus corruption

### Goal 4: Unify Programs With Filesystem Discovery
App-like Wraith programs should be discoverable through the same nested scan path as files and folders.

Deliverables:
- launcher/program entries authored as discoverable filesystem-backed records
- clear visual distinction between file, dir, and program
- launch actions routed through reusable ops/manifest data

Preferred shape:
- `scan_wraith_tree` labels entry type
- `launch_wraith_entry <entry_path> <entry_type> <session_ctx>`
- manager does not carry per-program launch trees beyond orchestration

Acceptance:
- `terminal`, `blank-project`, `settings`, and future Wraith-internal programs can all be discovered through the same crawl model
- Wraith shell does not need custom per-program index math

### Goal 5: Add Settings Early
Customization should exist before UI/state assumptions spread everywhere.

Deliverables:
- `settings` Wraith project
- file-backed theme/config state
- early controls for shell-facing appearance values:
  - desktop colors
  - window/chrome colors
  - fonts
  - background images / wallpaper
  - related appearance settings that affect the Wraith desktop and window presentation
- settings writes routed through reusable config ops

Preferred shape:
- `settings_get <scope> <out_file>`
- `settings_set <scope> <key> <value>`
- later PAL/Prisc scripts can call the same config surface

Acceptance:
- theme/config changes are reflected through project/session files
- visual customization specifically covers Wraith desktop/window appearance, not just abstract config toggles
- no Wraith-only hardcoded theme branch is required in the parser

## Implementation Order
1. Discovery op for nested Wraith content
2. Manifest/projection file for launcher rows
3. `text-fs` shell view
4. `gui-fs` shell view on same tree
5. program/file/dir symbol split
6. `settings` project
7. external add/remove refresh verification
8. PAL/Prisc smoke path over the same ops

## Architecture Rules
- Wraith Alpha hosts and projects state; it does not own per-project file logic.
- Discovery/scanning belongs in ops.
- File/path truth stays file-backed.
- If both `text-fs` and `gui-fs` need it, it should not live in a one-off manager branch.
- Use existing op-ed file-manipulation patterns where they fit.
- Avoid custom parser rules unless the current button/fold/activate patterns cannot express the interaction.
- Any action likely to be reused by agents, PAL, or a second project should become an op with arguments before it becomes a manager helper.
- PAL/Prisc is the forward-looking reuse layer: scripts should call these ops, not re-encode filesystem semantics in bespoke code.
- Prefer small ops with auditable receipts over large manager-local control blobs.
- Reuse order is mandatory:
  1. check for an existing op that already does the job
  2. if close, widen that op carefully so it becomes more generally reusable
  3. only add a new op when extending an old one would create confusion, bad args, or project-specific pollution
- If widening an existing op is nontrivial or could break old callers, use the migration path:
  1. copy the op into the local/project lane
  2. modify it there first
  3. prove it works locally with the new flow
  4. preserve old behavior compatibility
  5. only then replace or upstream the shared/original op
- Before writing a new Wraith filesystem op, inspect `op-ed`, launcher/file-browser, and existing file/state utilities for reusable seams.

## Testing Gate
Use the `_.0.aigent-testing-k3.txt` workflow.

Required test slices:
1. Baseline Wraith launch and frame health
2. Nested project discovery smoke test
3. Directory enter/back traversal smoke test
4. Program launch from filesystem-derived entry
5. External file/folder addition reflected after refresh/reload
6. Settings state roundtrip
7. One PAL/Prisc script invoking the same scan/list/launch or settings ops
8. Evidence that the chosen implementation reused or widened an existing op where practical, instead of silently adding a parallel duplicate
9. If a copied local variant was used first, evidence that it preserved old behavior expectations before any shared replacement was attempted

Evidence:
- key injections
- frame snippets
- state file diffs
- root `FRAME_REPORT_<timestamp>_wraith_fs.txt`

## Practical Risks
- hardcoded launcher math still exists in Wraith manager
- parser/project special-cases may still assume flat project ids
- nested path preservation may break save/load or taskbar/window titles
- `gui-fs` can drift into fake markup injection if projection boundaries are ignored

## Definition Of Done For This Push
This push is done when Wraith can:
- discover nested internal projects under `wraith-projects`
- browse directories as a shell feature
- distinguish files, dirs, and programs
- launch programs discovered from that filesystem model
- preserve nav sanity
- expose the core actions through reusable ops with args
- leave a clean surface for PAL/Prisc and agents to reuse
- produce test evidence proving the flow
