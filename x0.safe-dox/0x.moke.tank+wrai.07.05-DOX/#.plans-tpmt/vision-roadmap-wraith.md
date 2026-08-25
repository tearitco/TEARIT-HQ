# Wraith and CHTMGL
Date: 2026-06-24

## Purpose

Wraith is the visual shell and host runtime.
`chtmgl` is the current GL-facing project family that should eventually run under that shell.

## What This Lane Is For

- host TPMOS projects in a visual runtime
- keep file-backed state auditable
- render state through an RGB/GL bridge
- make future GUI work easier for agents to inspect and debug
- support 2D and 3D surfaces without turning the renderer into a gameplay engine

## What This Lane Is Not For

- it is not where project-specific game logic should live
- it is not a second copy of managers
- it is not the place to hardcode one-off project assumptions

## Current Evidence

- `wraith-alpha` already uses parser -> RGB daemon -> GL presentation
- `chtmgl-alpha` is the current GL milestone family
- GL-OS-style frame work already exists as a bridge path

## Short-Term Goal

Make at least one `chtmgl`-style project open consistently under Wraith with:

- stable state files
- stable frame trails
- stable launch path

That is more important than finishing every visual feature.

## Success Signatures

- a project opens under Wraith without ad hoc fixes
- the frame can be audited after the fact
- the shell can host future GUI experiments without reworking the contract
