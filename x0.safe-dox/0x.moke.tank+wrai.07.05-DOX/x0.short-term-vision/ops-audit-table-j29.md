# Ops Audit Table J29
Date: 2026-06-29
Status: First-pass audit table for Wraith FS, editor, and XO follow-on work

## Purpose
Turn the ops strategy into concrete per-feature decisions:
- reuse existing op
- widen existing op
- copy-and-mod locally first
- truly add a new op

This is a planning audit, not a code-change report.

## Decision Legend
- `reuse`: use the existing op as-is
- `widen`: modify an existing shared op directly because the delta looks small and generally useful
- `copy-mod`: copy an existing op locally first, prove it, then upstream/replace later
- `new`: create a genuinely new op because existing seams are too mismatched

## Audit Table

| Feature | First candidate | Current fit | Decision | Why | Local proving path / next action |
| --- | --- | --- | --- | --- | --- |
| Wraith nested project/app discovery manifest | `pieces/apps/gl_os/plugins/+x/gl_os_project_scan.+x` | Partial | `copy-mod` | Already scans and writes cache-like output, but is flat and GL-OS-shaped. Direct rewrite risks breaking GL-OS callers. | Copy into a Wraith-local scan op, add nested crawl + typed manifest output, prove on `projects/wraith/wraith-projects/`, then decide whether the generalized contract should replace the original. |
| Wraith launch/open from discovered entry | `pieces/apps/gl_os/plugins/+x/gl_os_loader.+x` and `pieces/apps/playrm/ops/+x/project_loader.+x` | Low/Partial | `copy-mod` | Both are useful references, but each is tied to a specific runtime/state mutation model. Direct generalization is risky before Wraith’s launch args are proven. | Create a Wraith-local launcher op variant that consumes typed manifest entries and session context, prove `terminal` / `blank-project` / `settings`, then upstream shared launch semantics later if they stabilize. |
| Wraith text-fs listing and refresh | `op-ed` file-browser flows | Partial | `copy-mod` | Strong behavior precedent exists, but the actual reusable op surface is not yet clearly isolated. Safer to extract locally first. | Build Wraith-local `fs_list` / `fs_refresh` around proven file-browser behavior, keep args generic, then fold back into a broader shared fs op after validation. |
| Wraith enter/back path navigation | `op-ed` file-browser flows | Partial | `copy-mod` | Same reason as listing: there is precedent, but not yet a clean standalone shared op contract. | Prove Wraith-local `fs_enter` / `fs_back` over the same path state used by both `text-fs` and `gui-fs`. |
| Wraith typed entry classification (`file` / `dir` / `program` / `project`) | `gl_os_project_scan.+x` | Low | `copy-mod` | Existing scanner identifies projects but not the full Wraith shell entry model. | Add typed manifest emission in the copied Wraith-local scan op; upstream later only if the type model remains generic. |
| Wraith settings get/set | existing state/config write patterns | Low | `new` | No clear existing shared config op surfaced in the audit. Ad hoc manager writes exist, but not a reusable shell-safe settings contract for desktop/window colors, fonts, wallpaper, and related appearance state. | Define small `settings_get` / `settings_set` ops with stable key/value semantics and receipts for Wraith appearance settings. |
| Wraith editor game load | `op-ed` load/file flows | Partial | `copy-mod` | `op-ed` is the right precedent, but editor-in-Wraith pathing and outputs need local proof first. | Copy the relevant flow into Wraith-local editor ops, prove loading a real game folder, then decide what can be promoted to shared editor/file ops. |
| Wraith editor save snapshot | `op-ed` save flows | Partial | `copy-mod` | Same as above. Correct base, but local Wraith proof should come first. | Prove snapshot save in `wraith-ed`, preserve old `op-ed` behavior, then upstream if the save contract generalizes cleanly. |
| Wraith editor PAL event draft writing | `wraith_project_input.+x` plus `op-ed` PAL direction | Partial | `copy-mod` | Existing Wraith editor already has local behavior. Better to extract deeper file-writing into local sub-ops first, then generalize. | Keep behavior in project-local ops now, but carve out reusable event-writer ops once the artifact format stabilizes. |
| Wraith editor export manifest | `wraith_project_input.+x` plus `op-ed` export direction | Partial | `copy-mod` | Similar to PAL event writing: current local proof exists, but shared export semantics should be proven first. | Move toward a reusable export-manifest op after local receipt/output format is stable. |
| XO entity/world auto-discovery | `pieces/apps/playrm/ops/+x/scan_op.+x` | Partial | `widen` | This already proves recursive file-backed discovery and looks conceptually aligned with auto-discovery. | Audit whether the recursive scan internals can be generalized without pulling in gameplay-only assumptions. If yes, widen or factor shared scan helpers from it. |
| XO world/controller callable surface | `fuzz-op` mechanics ops and project-owned ops | Partial | `copy-mod` | Strong mechanics precedent exists, but the target contract for XO still needs proof. | Define XO-local ops with stable args first, preferably borrowing from existing mechanics ops, then upstream only the pieces that prove broadly reusable. |
| PAL/Prisc orchestration over shell/fs ops | none directly | N/A | `reuse later` | PAL/Prisc should call the proven op surface, not lead its design. | Do not start here. First prove the ops locally; then add PAL/Prisc smoke scripts over the same args. |

## Immediate Recommendations
1. Start with a Wraith-local copy of `gl_os_project_scan.+x` for nested manifest discovery.
2. In parallel, inspect `op-ed` enough to define the first local `fs_list` / `fs_enter` / `fs_back` contract.
3. Do not generalize `gl_os_loader.+x` or `project_loader.+x` yet; use them as launch references until Wraith entry-launch args are proven.
4. Treat settings as the cleanest likely `new` op family.
5. Keep `wraith_project_input.+x` project-local, but have it call reusable file/export/event ops once those contracts are stable.

## Upstream Conditions
Local copied variants should only replace or merge back into shared ops when:
- the new args are stable
- old callers can still be supported cleanly
- receipts/output files are auditable
- at least one real local Wraith flow has passed testing
- PAL/Prisc could call the same op without needing special-case wrappers

## Next Concrete Build Step
Implement the first `copy-mod` target:
- Wraith-local nested discovery op based on `gl_os_project_scan.+x`

That is the best first move because it unlocks:
- launcher manifest cleanup
- filesystem browsing
- program/file/dir classification
- later PAL/Prisc reuse over one real scan surface
