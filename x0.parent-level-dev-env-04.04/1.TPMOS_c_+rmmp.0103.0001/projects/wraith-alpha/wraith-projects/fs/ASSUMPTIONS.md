# Wraith FS Assumptions

Assumptions being tested:
- `fs` should be one Wraith project with multiple render surfaces, not separate `ascii-fs` and `gl-fs` projects.
- filesystem truth should come from reusable ops/state, not renderer-specific logic.
- the first browser interaction pattern should be copy-mod from `projects/agy-text-editor`.
- later CHTPMGL should render the same directory/search/selection state instead of inventing a second filesystem model.

Current non-goals:
- deep editor features
- media playback
- manager-level hardcoding of filesystem rows

Immediate next step:
- copy-mod the AGY file browser behavior into project-owned `fs` logic, locally first.
