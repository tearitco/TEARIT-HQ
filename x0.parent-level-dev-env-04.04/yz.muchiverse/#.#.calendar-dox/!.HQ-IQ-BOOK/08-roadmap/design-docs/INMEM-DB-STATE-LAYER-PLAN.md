# In-memory DB state layer (port of wraith-alpha's `tpmos_share_kvp`) — plan

**Status: PLAN, not started. Written 2026-09-20 from the user's direction; source facts verified by reading the code, not from summaries.**

> ## SCOPE CLARIFICATION (user answers, 2026-09-20) - READ THIS FIRST
> - **The DB is only *necessary* for video/streaming** (webcam and similar frame feeds). It is an **OPTION and a DEMO, not a rewrite.**
> - **Files stay the default** ("we like to-file"): the DB should **write through to files** (mirror mode) wherever it holds state, so tools, shell scripts and the audit trail keep working unchanged.
> - So **P2-P4 below (moving UI state, replacing includes with published state) are OPTIONAL/DEFERRED**, not scheduled. The scheduled work is P0 (freeze includes) and a **streaming demo** (P1/P5): DB blob API + mirror-to-file, proven with a frame feed.
> - **Decisions:** client model = **both** (unix-socket protocol by default, optional thin fast-path library for hot loops); capacity = **design for growth** (hashed, growable store), size defaults modestly; audit ledgers **stay file-native**.
> - The transitional-include removal (§5) remains the direction but is not urgent and does not depend on the DB.


## 0. Direction (user, 2026-09-20)

- File read/write is too slow for hot paths (webcam frames, per-keystroke `cli_io_state.txt`). The fast path is **wraith-alpha's in-memory DB**, ported into khtpm/livedesk, **not** hand-rolled per-feature shared-memory blocks.
- **Text-included shared `.c` files are a transitional convenience, not the standard** (`khtpm_ui_common.c`, `khtpm_ui_scale.c`, `kh_proc_registry.h`, …). The end state is processes that share *data* through the DB, not code through includes.
- SQL on top of the DB is a *later* phase (unchanged from the 2026-09-11 note).
- The **dock unfactor is slated** (§6).

## 1. What wraith-alpha actually has (verified in source)

Root: `x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/ops/`

| Piece | File | What it is |
|---|---|---|
| Live frame cache | `lib/tpmos_live_frame_cache.c` (158 lines) | POSIX shm `/tpmos_live_frame_cache_v1`, ONE 8 MB RGBA slot + key (≤256), `pthread` process-shared mutex, `generation` counter readers can poll. Used by the webcam lane (`wraith_rgb_daemon.c`); ~17 fps GPU raymarch claim is board-viewer's, not the cache's. |
| **The "DB"** | `tpmos_share_kvp_db.c` (160, the daemon) + `lib/tpmos_share_kvp_runtime.c` (473, the client API) | POSIX shm `/tpmos_share_kvp_v1` holding a struct with a process-shared mutex, **128 text entries** (key ≤256, value ≤8 KB), **4 blob entries** (≤4 MB, per-blob `generation`), `dump_generation`. Daemon started on demand (`ensure_daemon`), takes **forked snapshot dumps to files** (`snapshot_and_dump`). Per-project **backend mode** `file | shmem | mirror` (`backend_mode()`); `mirror` = memory + async file write. |

**Correction to the working assumption:** the "in-memory DB" is *implemented on* POSIX shared memory. The real choice is **the DB API + mirror/dump policy vs. ad-hoc raw shm blocks**, not DB-vs-shmem as a medium. This plan therefore means "port the kvp DB (API + modes)", and raw shm stays only as the storage under it (and for direct blob/frame maps).

Not present: tables/rows, SQL, change notification for **text** keys (only blob `generation` and `dump_generation`), robust-mutex crash recovery, per-house namespacing (fixed segment name), permission hardening (`0666`).

## 2. Gaps vs. what khtpm needs

1. **Capacity.** 128 keys × 8 KB is far too small (per-window `csv_hq_ui.txt`-style projections, per-PID history, ledgers). Needs a growable/hashed store.
2. **Change notification.** Windows learn about state via ~300 ms file polling / marker-file size growth (house rule: append-only markers, never mtime). The DB needs a per-key sequence and a wake mechanism (futex/eventfd or a changed-keys ring), plus an **append** op returning the new sequence (replaces "marker file grew").
3. **Namespacing.** Segment name must include a house-root hash — the private test houses used for Xephyr testing must not touch the live desktop's DB.
4. **Crash safety.** A holder dying with the mutex locked stalls everyone → `PTHREAD_MUTEX_ROBUST` + owner-died handling.
5. **Shell/tool access.** Many house scripts `cat`/`echo >>` state files. Needs tiny CLI ops (`kvget`/`kvput`/`kvappend`) **and** `mirror` mode so files keep existing until each consumer migrates.
6. **Client code.** Any C client needs client code. To avoid the include problem: define a small wire protocol on a unix socket for control/cold paths (any language, no shared code), and let only hot blob/frame paths map shm directly. **Open question (§8): socket protocol vs. a thin shared client lib.**
7. **One-writer rule** (house) must be preserved as a DB-level convention (owner per key prefix), not by convention alone.

## 3. Phased plan (each phase independently shippable)

- **P0 – Freeze.** Add a check script listing the allowed cross-binary `#include`s (§5 inventory); no new ones. (Small; do first.)
- **P1 – Port + harden (scoped to the streaming demo).** Extract `tpmos_share_kvp` into a new house op set (daemon + CLI ops + client), fix §2 items 1–5. Keep `mirror` mode default so nothing changes for shell users.
- **P2 – (OPTIONAL, deferred) First state consumer: `cli_io_state.txt`.** The per-keystroke full read-modify-write that started this idea (2026-09-11). Dual-write (mirror), verify parity for a week, flip reads to memory. Success = backspace/typing latency measurably lower, files still produced for tools.
- **P3 – Dock unfactor (independent of the DB; slated).** Uses manager + template + existing file IPC. See §6.
- **P4 – (OPTIONAL, deferred) Remove the transitional includes** using published state (§5); files/mirror remain the source.
- **P5 – SCHEDULED DEMO: frames/media on the blob API** (already proven by wraith-alpha), with write-through to files, for a webcam/canvas feed.
- **P6 – SQL layer** over the same store.

## 4. What this replaces (and doesn't)

Replaces: per-key file RMW for UI state, marker-file polling for change detection, cross-binary shared code for *data* (config/scale/proc list/code tables). Does **not** replace: append-only *audit* ledgers on disk (kept via mirror/dump — "if it's not in a file it's a lie" still holds for audit), or the intra-renderer file splits (§5 group 1).

## 5. Include inventory and removal order (measured 2026-09-20)

1. **Intra-binary splits — leave for now:** `khtpm_core_render.c` includes `khtpm_draw_core.c`, `khtpm_render_core.c`, `khtpm_reparse_diff.c`, `khtpm_grid_jump.c` (one program cut into files, no cross-binary sharing). Only revisit if the engine itself is split across processes (render server over the DB).
2. **Cross-binary `.c` shares — remove first:** `khtpm_ui_common.c` (renderer + `khtpm_entity.c`), `khtpm_ui_scale.c` (ui_common, both placers, `khtpm_show_choices.c`).
   - **Allowed-list addition (2026-09-20):** `khtpm_grid_jump.c` is now also text-included by `tp_arm_placer_rmmv.c` (the Place overlay's typed cell jump, same behaviour as csv-hq). Documented TRANSITIONAL, user-approved; it is a pure state machine with no globals, so replacing it later means a small shared op or published cell-jump service. No other includes were added.
3. **Shared headers — remove next:** `kh_proc_registry.h` (4 users), `khtpm_css_parser.h` (5), `khtpm_taskbar_manager.h`, `khtpm_strip_codes.h`, `khtpm_plat.h`, `khtpm_core.h`.

| Include | Becomes |
|---|---|
| `khtpm_ui_scale.c` | one published value (`#.desktop/ui_scale.txt` now → DB key `ui.scale`/`ui.ref`) computed once by the desktop process |
| `kh_proc_registry.h` | DB key family `proc.*` (mirror keeps `livedesk_proc_list.txt`) |
| `khtpm_strip_codes.h` | generated data table / DB keys (it is a numeric contract between binaries — see pitfall #22) |
| `khtpm_ui_common.c` | split: config loaders → published state; METHOD reader → manager op; entity-menu launcher → small op |
| `khtpm_css_parser.h` | last; needs a render server (or stays as the one engine library) |

## 6. Dock unfactor (IN PROGRESS - stages 1-2 done 2026-09-20; audit + stage table: `DOCK-UNFACTOR-AUDIT.md`)

**Status:** stage 1 (dead code, -378 lines) and stage 2 (always-on-top respawn -> standalone op `ktb_zorder_op.+x`, -201 lines, fixes pals no longer being respawned) are committed and verified in a private Xephyr; renderer 12,550 -> 11,971 lines. Layout/paint/pager/peer-window code (~880 lines) is **blocked on generic engine features** (flex shrink-to-fit + pager element, secondary surfaces), not on the DB.

`khtpm_core_render.c` is 12,550 lines (5,010 comment, 7,196 code). The dock/strip mode is ~1,350 function lines (`layout_dock_bar`, `dock_paint_menu`, `dock_paint_peer`, `dock_*`, `ktb_*`, `ktb_toggle_zorder_respawn`, `write_theme_opacity`, plus dock globals). The dock's *logic* already lives in `khtpm_taskbar_manager*` (talking over `strip_history.txt`); the violation is dock **layout/paint/behaviour code inside the shared renderer**. Approach: move dock behaviour to manager + template data (CENTROID style), not to another binary that `#include`s the engine. Pitfalls to respect: dock windows are WM-managed (`dock_managed`), dock keyboard grab/focus rules (pitfall #24, `X11-AND-SESSION-PITFALLS.md`), nav numbering across header/bottom, the `assign_nav_and_layout` idempotency rule. Verify with the golden geometry (`2082x45+200+50` top, `2096x45+200+1619` bottom at the 2496x1664 reference) and a real-hardware keyboard check.

## 7. Non-goals

Per-feature raw shm blocks; SQL in P1–P5; changing audit-ledger-on-disk guarantees; renaming markup (`.xhtpm` stays — the `.xhtm` rename is cancelled).

## 8. Decisions (user, 2026-09-20)

1. **Client model: both.** Unix-socket protocol for state/control (no shared code), plus an optional thin fast-path client library for hot loops.
2. **Capacity: design for growth**, size later (hashed, growable store; modest default).
3. **First scope: streaming/video only** - not a rewrite of state files. Files remain default; DB writes through to file.
4. **Audit ledgers stay file-native** (`master_ledger`, `entity_menu_history`, ...).

## 9. Grounding

`1.TPMOS_c_+rmmp.0103.0001/pieces/chtpm/ops/tpmos_share_kvp_db.c`, `.../lib/tpmos_share_kvp_runtime.c`, `.../lib/tpmos_live_frame_cache.c`; `x0.parent-level-dev-env-04.04/wraith-architecture-j25.md` (webcam lane uses the shared frame cache); `board-viewer-3d-perf-ceiling` (GPU daemon numbers); `CHTPM-INCREMENTAL-REPARSE-DESIGN.md` (dock-first rollout); `XHTPM-RE.md`, `UNFACTOR-PAL-X.md` (entity split); `03-pitfalls/HOUSE_CODE_PITFALLS.md` #22, #24.
