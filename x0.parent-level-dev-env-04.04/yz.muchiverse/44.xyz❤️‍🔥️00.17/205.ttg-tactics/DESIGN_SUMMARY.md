# Design Summary — Community Tabletop Tactics (`205.ttg-tactics`)

**Date:** 2026-07-28 · **Rev:** 2  
**Full doc:** [`DESIGN.md`](./DESIGN.md)  
**Status:** Draft (implementable rules locked)

## What it is

A **chess + poker alternative** tabletop tactics game: 12×12 board, community-member pieces, clocks, Elo, ante/pot, dual ASCII/RGB house runtime.

## Architecture (non-negotiable)

Muta/CHTPM file-mediated path — **not** freeglut product:

```
keys → dual history → chtpm/prisc → validated ops → master_ledger
  → compose_frame (ASCII truth) → renderer_pulse → terminal
  → compose_rgb_frame → rgb_frame_changed → gl_mirror
```

- Pal loop: `sleep 16667` µs (~60 Hz), match muta  
- Launch (TTG): renderer → chtpm → rgb → gl → **keyboard last** (fork muta orchestrator)  
- Product AI emits **history keycodes** (not a second ledger brain)

## Locked MVP rules (see Key Decisions §15)

| Topic | Decision |
|-------|----------|
| Board | 12×12 |
| Frame | **TTG 48×22** (not muta VIEWPORT_H=16) |
| Actions | `moved`/`acted` flags (not free AP) |
| Move | 4-dir orthogonal BFS; 1 unit/tile |
| Combat | `dmg = max(1, atk - def)`; Chebyshev range; no RNG |
| Win | Regicide; clock flag; double-flag material table |
| Army | 1 King + 3 Soldier + 1 Wizard + 1 Farmer / side; fixed spawns |
| Pot | Ante-only TC; settle from pot_ledger; idempotent |
| Elo | K=32, 1v1, no Glicko `rd` |
| Seats | 0-based (`player_N` LPNS → N−1) |

## PR plan (short)

PR0 skeleton → PR1 move → PR2 attack/regicide → PR3 clocks → PR4 pot → PR5 AI keys → PR6 dual render → PR7 Elo → (+ PR8 build). Harnesses `01`–`08` defined.

## Explicitly rejected

Freeglut monolith; real-time RTS; real money in core loop; muta-16 squeeze without taller TTG frame; live direct-ops AI as default.
