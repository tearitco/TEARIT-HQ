# XO + Editor Bridge Roadmap
Date: 2026-06-28
Status: Follow-on plan after Wraith shell/fs push

## Purpose
Sequence the next layer of work so Wraith shell progress feeds directly into XO-pets and editor/PAL work instead of competing with it.

## Core Position
Do not split effort across three half-finished systems.

The correct near-term chain is:
1. stabilize Wraith shell/discovery/nav
2. prove one reusable filesystem-backed editor bridge
3. use XO-pets as the controller/PAL proving sandbox

At every step:
- extract reusable ops first
- let PAL/Prisc scripts orchestrate those ops later
- avoid baking user or agent workflows into manager-specific code
- prefer existing ops over new ops whenever the old seam can be generalized cleanly
- if generalizing an old op is risky, fork/copy it locally first, prove the modified version, then upstream the generalization later

## Why XO-Pets Still Matters
`xo-pets` is still the best proving ground for:
- controller swapping
- AI/FSM experimentation
- simpler tank/world setup
- PAL-driven loops
- cleaner contracts than `fuzz-op`
- op-with-args reuse for both humans and agents

But it should inherit lessons from the Wraith filesystem work first.

## Concrete Follow-On Goals

### Goal 1: Define A Reusable World/Controller Contract
Before deeper XO work, make the callable surface explicit.

Minimum contract surface:
- spawn/load tank or world
- attach controller
- set mode
- tick/update
- render/export state

Preferred shape:
- standalone ops with stable args
- PAL/Prisc scripts call those ops
- managers only route/project state
- if an existing op already covers 70-90% of the need, widen it instead of cloning behavior into a sibling op

Acceptance:
- a controller can be swapped without parser changes
- project-specific behavior stays in project files/ops
- user and agent flows can invoke the same ops without rewriting logic

### Goal 2: Decide The Editor Host Shape
Editor work should not fork into two incompatible systems.

Near-term preferred shape:
- Wraith hosts the shell/window/filesystem side
- a narrow editor project handles project-specific editing behavior
- reusable save/load/file manipulation lives in ops
- PAL/Prisc should be able to drive the same save/load/file ops later
- existing `op-ed` and related file/save ops should be audited first before inventing editor-local replacements

Acceptance:
- editor can create or mutate project state without special parser hacks
- file save/load works in at least one narrow end-to-end flow

### Goal 3: Use Fuzz-Op As Benchmark, Not As Final Contract
Current evidence suggests `fuzz-op` is mixed:
- entity rendering and discovery already scan piece folders under world/map paths
- some parser/manager behavior still has project-specific hardcoded branches

Implication:
- use `fuzz-op` as the mechanics benchmark
- do not copy its hardcoded edges into XO or Wraith
- preserve its reusable ops and scanning lessons where they are already good
- when a legacy op is already broadly useful, prefer adapting it over drafting a new “clean” duplicate just for naming aesthetics

### Goal 4: Make Drag/Drop Mean Auto-Discovery First
"Drag and drop" should first mean:
- external file/folder addition
- filesystem refresh
- automatic entity/program visibility without hardcoded registration

Reusable interpretation:
- add file or folder
- run scan/refresh op
- consume updated manifest/state
- no new bespoke C path required

The GUI gesture can come later.

Acceptance:
- adding a valid file-backed entity or Wraith project is enough for discovery
- no code edit is needed just to make a new entity visible

## Suggested Milestones

### Milestone A: Wraith FS Complete
Blocked on nothing except current Wraith shell work.

### Milestone B: Wraith-Hosted Editor Slice
One narrow editor path that can:
- browse files
- load a target project/game folder
- save a changed artifact
- emit a reportable state diff

Rule:
- each action above should map to a reusable op callable from manager, PAL/Prisc, or agent flow
- implementation review should explicitly say whether the action reused an existing op, widened an existing op, or truly required a new one
- if the action began as a copied local op variant, the review should say when it is safe to replace or merge back into the shared op

### Milestone C: XO-Pets Inversion Or Embedding Decision
After B, choose one:
- embed an XO-style world as a Wraith project
- or clone/invert into a Wraith-native `wraith-pet` project

Decision rule:
- choose whichever preserves file-backed discovery and controller modularity with less manager logic

### Milestone D: PAL Event / Controller Proof
Prove one PAL-driven controller or event flow in XO-pets or the Wraith-hosted equivalent.

Acceptance:
- PAL chooses behavior
- ops execute it
- manager stays thin
- args are sufficient that the same op can be reused by user flow, script flow, and agent flow

## Testing Expectations
Use the same testing discipline as the shell work:
- key injection
- frame review
- file diff evidence
- root frame report before risky edits

Required later proof:
1. create/load world or tank
2. attach/swap controller
3. trigger one PAL-driven behavior
4. save/load roundtrip
5. confirm no cross-project corruption
6. confirm PAL/Prisc is calling reusable ops rather than a parallel custom path

## Open Questions Worth Resolving Later
- whether XO should live as a nested Wraith project first or remain separate while sharing ops
- whether the first editor proof should happen in `wraith-ed` or a smaller dedicated slice
- which existing op-ed file ops are reusable immediately versus needing cleanup

## Bottom Line
The short-term future should not fragment.

Use Wraith shell/filesystem work to create:
- one discovery contract
- one nav contract
- one save/load-ready filesystem surface
- one reusable op surface with args

Then let PAL/Prisc, editors, XO controllers, users, and agents all call into that same surface instead of growing duplicate logic.
