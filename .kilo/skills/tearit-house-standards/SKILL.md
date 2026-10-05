---
name: tearit-house-standards
description: Core house conventions for TEARIT-HQ. Read before editing any source, building, or testing. Covers file-based state, project layout, session isolation, marker discipline, PDL conventions, digit-dispatch, and relay input.
---

# TEARIT-HQ House Standards

## The one belief everything follows from

**Real, file-based state only — never in-memory — for compliance and audit.** Every mode's state lives in a real file on disk (`.pdl`, `.chtpm`, `*_state.txt`, `action.txt`) a human or another process can open and read at any moment and see the truth. This explains almost every other house convention.

## Project shape (PIECE/MODULE/OS)

Most projects share:
- `system/` — shared engine binaries, **copied** (never symlinked) from canonical source
- `ops/` — project-specific compiled binaries doing real work
- `pal/*.pal` — tiny PAL scripts gluing ops together in a loop
- `pieces/chtpm/layouts/*.chtpm` — layout files defining screens
- `default_op.txt` — registers every op name a `.pal` script can call
- `button.sh` — the launcher (`run`/`build`/`kill`/`check` verbs)

**HOUSE RULE: never use symlinks.** They don't survive Windows checkouts/zip transfers. Copy instead.

## Session isolation

A session is `pieces/sessions/<timestamp>-<pid>/`. Model: **copy-in / persist-out**. A writable file missing from BOTH lists silently writes into the ephemeral session dir and vanishes on cleanup. Always audit `button.sh`'s copy-in + persist lists against everything ops actually write. Binary rebuilds do not apply live — restart the session. Mid-session, code must read the SESSION copy of state, not the project root.

## The digit-dispatch convention

Numbered screen rows (`[>] 1. Label`) come from `.pdl` `METHOD | Label | COMMAND` rows via `chtpm_parser_pal.c`'s `${piece_methods}` generator, whose internal `method_idx` starts at **2**. So `resolved_item = (key - '0') - 1` is the CORRECT compensation. The `-1` is only wrong for code paths reading a genuinely raw keyboard digit that never passed through `KEY:N` regeneration.

## Marker discipline — three files, never conflated

1. `pieces/display/frame_changed.txt` — the render trigger. Only a compose op or menu-input key-tail should grow it.
2. `pieces/apps/player_app/state_changed.txt` — **never grow from any op.** Growing it on every idle compose silently clobbers user navigation within ~1.5s.
3. `pieces/display/<project>_screen_changed.txt` — your project's own "something changed, re-compose" marker.

## PDL for runtime values

Positions, colors, sizes, labels, toggles — anything a user might want to tweak without recompiling — goes in a `.pdl` file (`SECTION | KEY | VALUE`), with safe fallback defaults. Extend the nearest existing PDL rather than inventing a new config file.

## Focus glyphs

`[>]` = focused (navigable), `[^]` = active/engaged, `[ ]` = neither. Drawn entirely inside the shared `chtpm_parser_pal.c` engine — don't reimplement it per-project.

## Relay input

Every window polls a per-PID relay file; real X11 input and agent-written events share one path. Per-PID keying (`<mode>_history/<pid>.txt`) prevents cross-window bleed. This is the house's standing alternative to synthetic X11 events.

## Two rendering families

- **chtpm_parser_pal family** — original ASCII/text-grid engine, PAL-VM driven, no box model. Still running for several apps.
- **khtpm family** (`khtpm_core_render.c`) — newer Elem/CSS engine with real box model. This is the GOLD STANDARD target for all new UI.

Neither is "correct" and the other "wrong" — chtpm_parser_pal apps are not deprecated. The house's posture is "migrate opportunistically" whenever a real feature/bug already has a reason to touch an app's display layer.

## Manager processes

A manager is a real, separate compiled binary (`<name>_manager.c`) owning a feature's logic/state, publishing a plain-text state file the shared renderer reads generically. Business logic NEVER lives inline in the shared renderer.

## CPU safety

Leaked engine stacks peg a core forever. After testing, `ps aux | grep` for stray manager/engine names and confirm zero. `proc-mon` (HQ menu → `mon`) classifies processes GOOD vs BAD and reaps BAD ones.

## Verification discipline

Never say something is fixed without a fresh build + fresh live/headless run + real evidence. A clean compile is not evidence of correctness.
