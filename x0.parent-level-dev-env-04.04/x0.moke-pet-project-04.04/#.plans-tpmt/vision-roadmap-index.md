# TPMOS Vision Roadmap Index
Date: 2026-06-24

This directory is the structured version of the long chat we just had.
The goal is to keep the strategy readable by later agents without forcing them to untangle one giant memo.

## Why This Exists

The vision has grown into several distinct lanes:

- Wraith / `chtmgl` / GL-shell work
- `fuzz-op` / `xo-pets` / simulation mechanics
- `op-ed` / PAL / editor authoring
- local FSM / mini-LLM / curriculum distillation
- `p2p-net` / LSR / multiplayer / identity / economy

These lanes are related, but they should not all be implemented the same way or in the same order.

## Lane Files

- [Wraith and CHTMGL](vision-roadmap-wraith.md)
- [Simulation and Pets](vision-roadmap-sim.md)
- [Editor and PAL](vision-roadmap-editor.md)
- [Local LLM and Curriculum](vision-roadmap-local-llm.md)
- [Network, LSR, and Multiplayer](vision-roadmap-network.md)
- [Project Progress Matrix](project-progress-matrix.md)

## Shared Rules

1. Reuse belongs in ops or PAL.
2. Renderers should reflect state, not own game logic.
3. The manager should stay thin whenever possible.
4. Project-specific behavior belongs in the project.
5. If a feature naturally crosses multiple projects, it probably needs a shared contract.

## Recommended Order

1. Keep the Wraith / `chtmgl` shell contract stable enough to host future GL projects.
2. Keep `fuzz-op` as the mechanics benchmark while `xo-pets` becomes the cleaner reusable version.
3. Finish a reusable editor/PAL path before multiplying bespoke editors.
4. Stand up the local FSM / mini-LLM interface as a separate intelligence lane.
5. Add networked multiplayer, login, trading, and mining after the core state contract settles.

## What Later Agents Should Ask

- Which lane is the user talking about?
- Is this a new behavior, a new surface, or a reusable contract?
- Should the change go into an op, PAL, manager, parser, or project?
- Is the current request about proving the architecture, or shipping a visible demo?
