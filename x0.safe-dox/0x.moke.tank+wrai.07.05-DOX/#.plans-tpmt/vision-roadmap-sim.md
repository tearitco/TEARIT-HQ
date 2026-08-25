# Simulation and Pets
Date: 2026-06-24

## Purpose

This lane is about the mechanics stack:

- `fuzz-op` as the proven benchmark
- `xo-pets` as the reusable next-generation sandbox
- eventual LSM-style spawning of pet pieces internally

## What `fuzz-op` Proved

`fuzz-op` already pushed into real mechanics:

- entity selection
- z-level handling
- map switching
- turn / clock coupling
- per-project session state
- mirrored UI state
- ops for scan, collect, place, inventory, and progression
- AI-like movement and reaction loops
- frame synchronization

That makes `fuzz-op` the reference for "how far mechanics can go."

## What `xo-pets` Should Become

`xo-pets` should be the cleaner reusable version of the same idea:

- piece-based and controller-friendly
- easier to reset and reconstruct
- designed for drop-in controllers and editors
- suitable for PAL-driven bot behavior
- easier to host inside bigger systems later

## The Key Design Tension

Do not copy every old mechanic into the new shape just because it exists.
Use `fuzz-op` to define the upper bound, then rebuild the reusable contract cleanly in `xo-pets`.

## Short-Term Goal

Prove a small, stable loop where:

- a pet/world is created
- a controller is attached
- an action changes state
- the change is visible
- the state can be saved and reloaded

## Success Signatures

- the sim can be edited without editing engine internals
- controllers feel replaceable
- PAL can drive repeated behavior
- the same contract can later be rendered in Wraith
