Visual Scripting for Events (Scratch / Blueprints) — Vision + PAL Policy
=========================================================================
Reference doc, 2026-08-26. VISION PHASE — nothing in this doc is built
yet. Read this before deciding TEMPLATE vs PAL for any new event
command, and before anyone starts building the events editor's own
visual-scripting tabs.

## The actual goal (direct instruction, 2026-08-26)

The events editor will eventually open a Scratch-like/Blueprint-like
window with **three tabs, all views onto the SAME underlying compiled
program**, not three separate editors that happen to produce similar
output:

1. **Scripting** — the current RPG-Maker-style command list (what
   exists today: pick a command type, fill fields, get a numbered list
   of nodes). This tab is DONE, real, and stays as the default/simplest
   entry point.
2. **Scratch-style** — draggable, snap-together visual blocks (the
   Scratch/MIT visual language look — jigsaw-piece blocks, C-shaped
   loop/branch wrappers). **Built as of 2026-08-29** (doc-audit pass:
   real Scratch-view rendering now exists in `khtpm_entity_menu_render.c`,
   see commits `2a1205b`/`029c232`) — this line previously said "not
   built yet."
3. **Blueprints-style** — a node graph with connector wires (the Unreal
   Engine Blueprints look — boxes with input/output pins, wired
   together spatially). Not built yet.

Whichever tab you're in, editing in one should be reflected in the
others (same shape as Scratch's own "see the blocks, see the
underlying code" duality, or a decompiler/recompiler pair) — because
all three are just different RENDERINGS of the same compiled `.pal`
program. This is why the compile TARGET matters so much: **the more of
event-command compilation lands as real, inspectable `.pal` VM
instructions (not opaque shell scripts), the more of it can eventually
be rendered as a visual block/node without hand-writing a
per-command-type visual mapping.**

## Why this changes today's TEMPLATE vs PAL decision

Before this vision was stated explicitly, the working rule (see
`EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`) was just "TEMPLATE vs PAL is
whichever matches what the command actually is — most things are a
plain `exec` call, so TEMPLATE is usually right." That's still true as
a MECHANICAL description, but it's the wrong DEFAULT given this goal:

- **PAL-mode commands** compile to real `prisc+x` instructions
  (`li`/`ecall`/`beq`/`j`/...) directly in `event.pal`. A future visual
  layer can pattern-match a known instruction sequence (e.g. the exact
  `li x15,7 / li x12,V / ecall "path" "key"` shape Control Switch
  compiles to) back into a labeled visual block ("Set Switch X to ON")
  with zero per-command special-casing — the mapping is mechanical,
  instruction-shape to block-shape.
- **TEMPLATE-mode commands** compile to `exec cmd_N.sh` — a single
  opaque line pointing at a generated shell script. A future visual
  layer has no way to know what that script DOES without either (a)
  parsing arbitrary shell (fragile, unbounded), or (b) a hand-written,
  per-command-type special case that says "if the target looks like
  THIS wrapper, render it as THIS block" — which defeats the entire
  point of a data-driven registry (right back to the tier-3 hardcoding
  problem `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md` fixed).

**New default policy:** prefer PAL over TEMPLATE for any new event
command wherever the command can genuinely be expressed as real VM
instructions (an `ecall`/`li`/`beq`/`j` sequence with substituted
literal args) — not just "whichever is more convenient to write today."
TEMPLATE remains legitimate ONLY when there's a real, documentable
technical reason PAL can't express the command (see Task 2's
`call_common_event` for a real example — `OP_EXEC` has no env-var-
setting capability, and `MUCHI_CALLER_PKG` genuinely needs one). When
that happens, say so explicitly as a comment on that COMMAND block in
`event_commands.registry.pdl`, the same way `compile_page()`'s own
Conditional Branch comment documents its tier-3 exception — never leave
a TEMPLATE choice looking like an unexamined default.

This is the same three-tier discipline as before, just with an
additional, more specific reason (visual-editor mappability, not just
"avoid C bloat") for why PAL should be tried FIRST, not reached for only
when TEMPLATE obviously doesn't fit.

## What is explicitly NOT decided yet (don't guess past these)

- The actual visual-block/node RENDERING code (parsing compiled `.pal`
  back into a block/node layout, and compiling edited blocks/nodes back
  to `.pal`) is completely unbuilt. This doc is scope-setting for
  compiler-output SHAPE only, not a spec for the editor UI itself.
- Whether Scratch-style and Blueprints-style are two independently
  built renderers or one shared graph model with two skins is an open
  design question — don't assume either shape when this work actually
  starts; ask first (per this house's standing rule on real design
  forks).
- Whether every existing TEMPLATE-mode command (`show_text`,
  `show_choices`, `change_gold`) gets migrated to PAL later, or stays
  TEMPLATE forever because their existing `mr_*.c` ops are fine and a
  visual "black box: run this op" block is an acceptable permanent
  fallback for SOME commands, is also undecided. Not every command
  needs to be visually transparent — a "Show Choices" block that's
  visually just "Show Choices (configure via panel)" with the same
  fields as today, backed by a TEMPLATE `exec`, may be a perfectly fine
  permanent design. Don't treat "convert everything to PAL" as an
  automatic mandate — see `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`'s own
  "what NOT to do" section on premature universal-refactoring.
- Extending `OP_EXEC` to support an `env KEY=VALUE` token (which would
  let Task 2's Call Common Event be real PAL too) is a real, scoped,
  future option — noted in `COMMON-EVENTS-MANAGER-HANDOFF.md`'s Task 2
  guidance — but is a shared-VM change and needs its own sign-off,
  not bundled into Task 2 without asking.

## Extended direction — harnesses themselves, moved to its own doc

Direct instruction (2026-08-27): harnesses should eventually be
PAL/event-authored, not just bash calling ops - and hand-written C
generally is a long-term candidate for replacement by writing events,
wherever the VM can express the thing. Full reasoning, a real
feasibility check against `prisc+x.c`'s actual syscalls, and a priority
list of "harness-friendly" event commands worth building next now live
in `HARNESS-AUTHORING-GUIDE.md` §3 (indexed in `INDEX.md`) - read that
before designing any new harness or deciding a new event command's
priority.

## Where this fits with everything else already documented

- `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md` (this directory) — the
  three-tier test and the registry engine itself. Still the correct
  starting point for "does this belong in C at all." This doc adds a
  refinement on TOP of that: once something's confirmed tier-2
  (registry-worthy), prefer PAL over TEMPLATE within tier 2, for the
  reason above.
- `COMMON-EVENTS-MANAGER-HANDOFF.md` (this directory) — Task 2's
  guidance section has been corrected to point here; read that
  correction before implementing Call Common Event.
- Documentation-consolidation note (2026-08-26): all real documentation
  for this house now lives in this directory (`1.^V-hq/`), not scattered
  across feature directories. `#.ref/` and similar per-feature
  directories hold basics/reference data (registries, raw command
  lists, etc.) — not documentation. If you're about to write a new
  `.md` design/architecture doc anywhere else in the tree, put it here
  instead, to avoid the drift this consolidation pass just cleaned up.
