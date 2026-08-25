# TPMOS Share KVP Guide J02

Date: 2026-07-02
Status: Active reference
Audience: users, developers, and agents working on TPMOS runtime IPC

## Purpose

This document explains the current TPMOS shared-memory database and live-frame cache model.

It separates two classes of runtime data:

1. basic key/value runtime state
2. streaming / live-frame runtime state

These should not be treated as the same performance lane.

## Short Version

Use `tpmos_share_kvp_*` for:

- text status
- command/state rows
- marker signals
- small auditable values keyed by file-like paths

Use `tpmos_live_frame_cache` for:

- always-live webcam frames
- future video/screen/live-media frames
- any lane where decoded pixels change continuously and need low-latency consumption

Keep file paths as the semantic contract for audit and fallback, but do not force the hot path to be file-backed.

## Core Rule

TPMOS should preserve:

- stable file-like path keys
- auditable dump/mirror behavior
- switchable backends where practical

But it should not force every runtime lane to pay file I/O or process-spawn cost when that lane is inherently live and continuous.

## Part 1: Basic Shared KVP

Current TPMOS-shared seam:

- `pieces/chtpm/ops/+x/tpmos_share_kvp_db.+x`
- `pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x`
- runtime helpers currently live in:
  - `pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c`

What this lane is for:

- `session/state.txt`
- `session/webcam.status`
- `session/fs_watch.marker`
- similar small text or blob artifacts

What it gives:

- shared-memory first write/read
- same file-like path keys preserved
- async mirror/dump back to normal file paths for audit

Good fit:

- low-frequency or medium-frequency updates
- values humans/agents still want to inspect as ordinary text files
- state that benefits from memory-first writes without requiring raw media throughput

Not a good fit by itself:

- true live decoded media transport
- high-frequency frame pipelines where every frame would still pay encode/decode or adapter overhead

## Part 2: Streaming / Live Frame Cache

Current live-frame seam:

- runtime helper in:
  - `pieces/chtpm/ops/lib/tpmos_live_frame_cache.c`

What this lane is for:

- live webcam frames
- future screen-record live monitor
- future high-frequency video/media surfaces

What it stores:

- active flag
- width / height / channels
- byte count
- frame generation
- frame epoch ms
- logical key
- RGBA pixel buffer

What it gives:

- decoded frame truth in shared memory
- no per-frame PNG decode requirement for consumers
- generation-based latest-frame access
- one producer, multiple consumers

Good fit:

- continuously updating sources
- decoded pixel consumers
- low-latency ASCII and GL presentation

## Current Webcam Design

The current webcam lane now uses both seams:

### Hot path

- webcam producer writes RGBA frames into `tpmos_live_frame_cache`
- `web-cam` ASCII preview reads that live cache first
- `wraith_rgb_daemon` webcam image panel now also reads that live cache first

### Audit / fallback path

- `session/current_frame.png` is now a sampled snapshot, not the hot transport truth
- `session/webcam.status` remains auditable
- `session/state.txt`, `session/wraith_body.txt`, `session/scene.objects.pdl`, and `session/webcam_preview.grid.pdl` still exist as project truth artifacts

## Basic vs Streaming Rule Of Thumb

Use basic shared KVP when:

- the value is mostly text
- the value is small
- you still want normal file inspection to feel primary
- update rate is not true media-rate

Use live frame cache when:

- the source is continuous
- the payload is decoded pixels
- latency matters more than file immediacy
- multiple surfaces should consume the same latest frame truth

## Auditability Contract

Memory-first does not mean hidden.

TPMOS auditability is preserved by:

- stable file-like keys
- dump/mirror to ordinary file paths
- sampled snapshots where full-rate dumps are too expensive
- receipts that can state which source was actually consumed

Examples:

- basic KVP audit:
  - `session/webcam.status`
  - `session/state.txt`
- streaming audit:
  - sampled `session/current_frame.png`
  - receipts that say whether rendering used:
    - `live_frame_cache`
    - or `textured_image_rect` from file fallback

## Current Limitation

The current Wraith manager wake path is still largely file-marker oriented.

That means:

- live frame transport is now much better
- but full continuous-media scheduling is not yet completely generation-native end to end

Future improvement:

- let active continuous projects wake from cache generation changes instead of relying on file-marker growth

## Recommended Future Direction

1. Keep `tpmos_share_kvp_*` for ordinary runtime state
2. Keep `tpmos_live_frame_cache` for continuous decoded media
3. Preserve file-path identities as keys and dump targets
4. Prefer receipts that explicitly say what source was consumed
5. Do not collapse live media back into mandatory per-frame PNG truth

## Current Files To Know

Architecture:

- `wraith-architecture-j25.md`
- `2fix.txt`
- `handoff-j26.txt`

Runtime:

- `pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c`
- `pieces/chtpm/ops/+x/tpmos_share_kvp_db.+x`
- `pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x`
- `pieces/chtpm/ops/lib/tpmos_live_frame_cache.c`

Webcam integration:

- `projects/wraith-alpha/wraith-projects/web-cam/ops/src/wraith_webcam_capture.c`
- `projects/wraith-alpha/wraith-projects/web-cam/ops/src/wraith_project_input.c`
- `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`
