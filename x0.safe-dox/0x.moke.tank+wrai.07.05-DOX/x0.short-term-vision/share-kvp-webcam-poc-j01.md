# Share KVP DB Webcam POC J01

Date: 2026-07-01
Status: Historical POC reference; superseded by TPMOS-shared seam + live frame cache

## Purpose

This document compares:

- the current shared KVP DB theory in `x0.parent-level-dev-env-02.01/#.share_kvp_db🦈️]a0`
- the current Wraith webcam file read/write path in `1.TPMOS`

and defines the cleanest first proof for replacing hot file IPC with an in-memory seam while preserving TPMOS auditability.

## Short Answer

Yes, the webcam lane is the right first target.

It is a strong proof target because:

- the current lane is visibly slow
- the current lane writes and rereads many small state files
- the current lane already has a clear audit seam
- improvement will be obvious to the user if it works

But the current `shared.db` prototype is not yet the final integration shape for Wraith webcam.

The main problem is this:

- the current DB prototype still expects short-lived adapter processes launched through `system()`
- for the webcam lane, that would still add too much process churn
- it removes disk I/O, but it does not remove fork/exec overhead

Two useful proofs should be separated:

1. Scientific baseline:
   - keep the ops-based DB idea
   - start one DB daemon op once
   - use `system("./shared.db_adapter.+x ...")` read/write ops for runtime access
   - measure the speed gain

2. Final likely direction if that is still too slow:
   - keep the same ops contract
   - replace the hot-path adapter spawn with a direct C shared-memory client seam
   - keep compile-time or config fallback so the same path names can resolve either to files or to shared-memory-backed keys

This means the `system()` path is still worth doing first for science, but it should be treated as a measured baseline, not assumed to be fast enough.

## Implemented J01 Proof

This older proof was first implemented as a project-local Wraith seam.

Current production direction is no longer that shape.

Current implementation now lives in:

- `pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c`
- `pieces/chtpm/ops/+x/tpmos_share_kvp_db.+x`
- `pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x`
- `pieces/chtpm/ops/lib/tpmos_live_frame_cache.c`
- `projects/wraith-alpha/wraith-projects/web-cam/ops/src/wraith_webcam_capture.c`
- `projects/wraith-alpha/wraith-projects/web-cam/ops/src/wraith_project_input.c`
- `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`

What is now proven:

- a per-project shared-memory seam exists for the webcam lane
- `session/webcam.status` can be written in memory and read back through ops
- `session/fs_watch.marker` can be appended in memory and read back through ops
- `session/current_frame.png` can be pushed into the DB as a blob and dumped back out byte-for-byte
- audit materialization can happen in a forked worker without making the DB truth file-backed

What has now moved past the original POC:

- hot webcam frame truth is memory-first via decoded live frame cache
- ASCII preview and GL image preview both consume that same cache first
- sampled `current_frame.png` is retained for audit/fallback rather than mandatory per-frame hot truth

Current practical state:

- runtime truth for status/marker/frame can live in memory
- file output can be treated as explicit dump/mirror behavior
- `current_frame.png` is already part of the shared-memory contract, but capture still currently mirrors from file into memory rather than producing the frame in memory first

## Current Webcam Data Path

Current project:

- `x0.parent-level-dev-env-02.01/1.TPMOS_c_+rmmp.0102.0027/projects/wraith-alpha/wraith-projects/web-cam`

Current hot path:

1. `wraith_webcam_capture.c`
   - launches `ffmpeg`
   - `ffmpeg` writes `session/current_frame.png`
   - daemon polls file mtime
   - daemon rewrites `session/webcam.status`
   - daemon appends to `session/fs_watch.marker`

2. `wraith_project_input.c`
   - rereads `session/state.txt`
   - rereads `session/webcam.status`
   - rereads `session/history.txt`
   - decodes `session/current_frame.png`
   - rewrites `session/webcam_preview.grid.pdl`
   - rewrites `session/wraith_body.txt`
   - rewrites `session/state.txt`
   - rewrites `session/scene.objects.pdl`

This means the current proof does not just pay for frame capture.
It also pays for:

- file polling
- file parsing
- full file regeneration
- image decode from disk
- semantic scene regeneration from disk-backed truth

## What Is Actually Slow

The slow lane is not one thing.

It is the compound effect of:

- frame file writes: `current_frame.png`
- status file writes: `webcam.status`
- marker appends: `fs_watch.marker`
- project rereads of text files every tick
- project rewrite of multiple audit files every tick
- PNG decode on every refresh

That means there are two distinct data classes:

1. Hot runtime transport
   - frame pixels
   - frame epoch
   - live status fields
   - latest command cursor values

2. Audit and sovereignty artifacts
   - `state.txt`
   - `wraith_body.txt`
   - `scene.objects.pdl`
   - `webcam_preview.grid.pdl`
   - `fs_watch.marker`
   - receipts/history

The file-backed seam is still correct for class 2.
The likely optimization target is class 1.

## What The Current Shared DB Prototype Already Proves

Files:

- `#.share_kvp_db🦈️]a0/shared.db.c`
- `#.share_kvp_db🦈️]a0/shared.db_adapter.c`
- `#.share_kvp_db🦈️]a0/sharedb-feasibility.md`

What it already proves:

- a persistent shared-memory DB process is feasible
- the DB can use keys that look like file paths
- a dump-to-disk pass can happen on shutdown
- multi-process coordination can happen via a shared mutex

This aligns with the TPMOS desire that:

- file path names remain the semantic contract
- runtime can use memory first
- file snapshots can still exist for audit and recovery without being the runtime transport

## Where The Current DB Prototype Is Weak

The current prototype is useful theory but weak as a production seam for webcam.

### 1. Adapter spawn cost is still too high

Current shape:

- writer runs `shared.db_adapter.+x`
- reader runs `shared.db_adapter.+x`
- each call does `shm_open` + `mmap` + lock + unlock + `munmap`

That is much better than heavy disk churn for low-frequency values.
It is not good enough for a webcam hot path.

### 2. Value model is too text-only and too small

Current limits:

- `MAX_ENTRIES 128`
- `MAX_KEY_LEN 128`
- `MAX_VALUE_LEN 4096`

That is fine for state rows.
It is not enough for frame data or richer scene blobs.

### 3. Hardcoded singleton naming

Current code uses:

- `SHM_NAME "/my_tiny_shared_db"`

That is too global.
The real system needs project or lane scoping, such as:

- one DB per active runtime
- or one namespaced DB with project-root-prefixed keys

### 4. Audit dump is too crude

Current dump behavior:

- write every key back out as a real file on exit

That is directionally correct, but incomplete.
We will want:

- selective flush
- forced flush for audited keys
- periodic snapshot option
- explicit "dirty" tracking
- background forked dump workers so runtime never pauses for audit output

### 5. It does not preserve current fallback behavior yet

Your desired contract is:

- read/write by file-like path key
- switchable between memory-backed and file-backed mode by config or define

The prototype does not implement that compatibility seam yet.

## Why This Is Still Similar To TPMOS File Auditability

The DB idea can remain TPMOS-like if we preserve three rules.

### Rule 1: File path names remain the contract

If code currently reads:

- `session/state.txt`
- `session/webcam.status`
- `session/history.txt`

then those exact logical paths stay the keys.

That means the meaning of the data stays inspectable and stable.

### Rule 2: Memory is the runtime transport, not the source of secrecy

The DB should not become a hidden private state blob.
It should be:

- queryable
- dumpable
- snapshot-friendly
- optionally mirrored to file

That keeps it auditable in the same spirit as TPMOS file seams.

### Rule 3: Audited artifacts can flush to disk

The audit story should be:

- hot state lives in memory
- audited receipts and snapshots can flush on demand
- shutdown can flush the live DB back to files
- debug mode can mirror writes to both memory and file
- normal runtime does not need to be file-backed at all
- dump work may happen in a forked worker while the DB daemon keeps serving reads/writes

This preserves the current human-readable inspection path without forcing disk to carry the whole runtime cost.

## Scientific Plan

The user wants to try the ops-based `system()` path first before deciding whether the more extreme direct-client path is necessary.

That is a valid experiment.

The experiment should be framed this way:

- one DB daemon op starts once at orchestration/startup
- read/write happen through reusable ops
- backend mode is switchable:
  - `file`
  - `shmem`
  - `file+mirror`
- logical keys remain the same relative path strings the file seam would have used
- the daemon may fork dump workers whenever audit snapshots are requested so dump latency does not stall runtime

This keeps the ops contract stable while letting us compare backends honestly.

## Correct First Proof Target

The first proof should not try to replace everything.

That would create too many moving parts and hide whether the speedup came from the DB seam or from unrelated simplification.

### Runtime targets that belong in the memory-backed plan:

- `session/webcam.status`
- `session/state.txt`
- `session/history.txt` read cursor state
- `session/fs_watch.marker` pulse state
- `session/current_frame.png` logical identity

Important clarification:

- if the requirement is zero runtime file dependence, then `current_frame.png` cannot remain a real runtime file
- it must become either:
  - a DB key whose value is an encoded frame payload
  - a DB key that points to a shared-memory frame slot
  - or a dedicated shared frame buffer that still uses the same logical path string as its identity

### First scientific pass should also avoid runtime file dependence

Normal runtime should not rely on disk copies of those keys.

Files may still be produced only as:

- explicit audit dump
- debug mirror mode
- fallback backend mode

### Phase-1A scientific baseline may still isolate only the text/control seam:

- `session/webcam.status`
- `session/state.txt`
- `session/history.txt`
- `session/fs_watch.marker`

Reason:

- that gives a clean measurement of file I/O removal while keeping frame transport unchanged
- it is still scientifically useful

### But the stricter zero-runtime-file lane must include frame transport in the same plan:

- replace `session/current_frame.png` runtime storage with memory-backed transport

### Phase-1B zero-runtime-file items:

- `session/wraith_body.txt`
- `session/webcam_preview.grid.pdl`
- `session/scene.objects.pdl`

Reason:

- these are larger and more entangled payloads
- moving them after `current_frame` still keeps the largest payload seam addressed first
- `wraith_body.txt`, `webcam_preview.grid.pdl`, and `scene.objects.pdl` can remain dumpable artifacts rather than runtime truth

### Phase 2 target

If phase 1 works, phase 2 should move:

- frame metadata
- ASCII preview cells
- scene object rows

and keep only optional snapshot/export files on disk.

### Phase 3 target

If phase 2 works, phase 3 should move the actual frame transport off PNG files:

- shared-memory RGB buffer
- ring buffer or double buffer
- one producer: webcam capture
- two consumers: ASCII projector and GL uploader

That is the point where the webcam speed gain should become dramatic.

## Recommended Architecture For The POC

### 1. Keep key names file-like

Examples:

- `projects/wraith-alpha/wraith-projects/web-cam/session/webcam.status`
- `projects/wraith-alpha/wraith-projects/web-cam/session/state.txt`
- `projects/wraith-alpha/wraith-projects/web-cam/session/history.txt`
- `projects/wraith-alpha/wraith-projects/web-cam/session/fs_watch.marker`
- `projects/wraith-alpha/wraith-projects/web-cam/session/current_frame.png`

Relative path keys are preferable.

### 2. Introduce a tiny transport seam, not direct `fopen` replacement everywhere

Add a reusable local seam such as:

- `wraith_share_kvp.h`
- `wraith_share_kvp.c`

or a project-local proof copy first if you want copy-mod discipline.

Minimal functions:

- `tpmos_kvp_read_text(key, out, out_sz)`
- `tpmos_kvp_write_text(key, value)`
- `tpmos_kvp_append_text(key, value)`
- `tpmos_kvp_exists(key)`

Then gate it behind:

- `#define TPMOS_USE_SHARE_KVP_DB 1`

Fallback path:

- if define is `0`, use normal file I/O

This matches your desired mode switch much better than hardcoding one backend forever.

For the scientific pass, those helpers may internally call the DB adapter ops through `system()`.
If that proves too slow, the helper implementation changes while the calling code stays stable.

For frame transport, the same seam can grow a second family such as:

- `tpmos_kvp_read_blob(key, out, out_sz, *bytes_out)`
- `tpmos_kvp_write_blob(key, data, bytes)`
- `tpmos_kvp_read_frame_ref(key, out, out_sz)`
- `tpmos_kvp_write_frame_ref(key, slot_id_or_handle)`

### 3. Keep DB server process separate

Good:

- one long-lived DB server
- direct clients attach via shared memory

Avoid for hot paths:

- short-lived adapter process per operation

### 4. Add explicit flush modes

Needed modes:

- runtime memory-only
- runtime memory + periodic file snapshot
- runtime memory + immediate mirror for debug
- pure file fallback

This gives you both speed and audit control.

## Concrete Webcam POC Plan

### Step 1

Create a Wraith-local proof copy of the share KVP runtime, not a global replacement.

Reason:

- follows copy-mod-first rule
- keeps old behavior available
- local proof can be validated before replacing broader TPMOS seams

### Step 2

Replace only the following file calls in `web-cam`:

- `load_previous_state(...)`
- `load_status(...)`
- `write_status(...)`
- `write_state(...)`
- marker append path

### Step 3

Run the first scientific baseline with the daemon always alive and dumps handled in forked workers.

This should prove:

- runtime reads/writes do not wait on file dump
- audit snapshots can still be emitted while the memory-backed lane keeps moving

### Step 4

If the goal is zero runtime file dependence, then replace the frame seam in the same project:

- `ffmpeg` should target either:
  - a memory-backed blob key
  - or a frame slot / shared-memory buffer contract
- `wraith_project_input.c` should read frame truth from memory
- file snapshots of the frame become optional debug/audit exports, not runtime truth

### Step 5

Use the same logical path names as keys.

Example:

- current file path:
  - `session/webcam.status`
- DB key:
  - `session/webcam.status`

### Step 6

Add one debug op that dumps the in-memory KVP view to files on demand.

That proves the audit story explicitly.

### Step 7

Measure:

- time from camera frame update to visible Wraith refresh
- CPU load before and after
- number of disk writes avoided
- cost of asynchronous dump while runtime continues

## What Success Should Look Like

The first proof is successful if:

- webcam still starts and stops correctly
- Wraith still shows the same state rows and preview
- audit files can still be produced on demand
- CPU load is lower
- visible latency is lower
- code can switch back to pure file mode with one config/define change
- dump requests do not stall the active runtime path

## What Should Not Be Lost

The optimization must not remove:

- human-readable state
- relative-path discipline
- project-owned sovereignty
- marker/receipt explainability
- kill-safe shutdown behavior

## Recommendation

Proceed with webcam as the first share-KVP proof, but do it in two strict lanes:

1. Ops-based `system()` scientific baseline first
2. Direct shared-memory client only if the baseline is still too slow

The ops-based baseline is still worth doing.
It answers the right question first:

- how much does removing file I/O alone help

But if the runtime requirement is strict zero-file dependence, then the frame seam must be included in the memory-backed plan and cannot be left behind as a real runtime file.

If that answer is not good enough, then the next step is:

- keep the same ops contract
- replace the adapter spawn inside the hot path

The right long-term shape is:

- file-like key contract
- shared-memory-backed direct client API
- compile-time or config fallback to file mode
- optional flush/mirror back to disk for audit

That keeps the TPMOS audit model intact while removing disk from the performance-critical loop.
