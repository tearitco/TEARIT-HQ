# Editor and PAL
Date: 2026-06-24

## Purpose

This lane is about reusable authoring:

- `op-ed` as the generic editor core
- project-specific editors as thin wrappers
- PAL as the reusable behavior layer

## Core Rule

If a feature belongs in more than one project, it should probably become an op or a PAL abstraction.

## Why This Matters

Editors tend to grow accidental complexity fast.
If every project gets its own custom editor logic too early, reuse becomes expensive and the managers get bloated.

## What To Build First

- a reusable editor core
- stable save/load roundtrip
- project/session/path resolution that does not rely on hardcoded assumptions
- PAL hooks for repeatable behavior

## Where It Connects

- `fuzz-op`: editor-driven play becomes part of the mechanics loop
- `xo-pets`: controllers and behaviors should be drop-in
- Wraith: the same edited world should eventually open in the visual shell

## Short-Term Goal

Make one editor path that can:

- create a piece or tank
- modify state
- bind behavior
- save it
- reopen it

## Success Signatures

- the editor can author behavior without special parser hacks
- PAL reduces duplication instead of adding a second language of one-off logic
- project-specific editor features remain thin wrappers
