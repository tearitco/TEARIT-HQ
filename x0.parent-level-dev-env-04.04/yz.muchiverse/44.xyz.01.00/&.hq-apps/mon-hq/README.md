# mon-hq — the session monitor / stray-process reaper

A small HQ window (taskbar → HQ menu → **`mon`**) that lists every
board / engine / hq process this house can spawn, sorts them into
**GOOD** (owned, accounted for) and **BAD** (stray, leaked), and gives
you a one-click **KILL ALL BAD**.

Built 2026-09-10 after a real incident on this (weak) machine — see
below.

---

## Files

| file | role |
|---|---|
| `mon_scan.sh` | the engine. Standalone: `list` \| `publish <ui.txt>` \| `kill-all [--with-pals]` |
| `mon-hq.xhtpm` / `mon-hq.css` | the window, rendered by the shared `khtpm_core_render.+x` |
| `mon_refresh.sh` | the `<module>` backend — re-runs `mon_scan.sh publish` every 2 s |
| `button.sh` | launcher (single-instance guard + seed + `setsid` the renderer) |
| `../../*.monads/*.livedesk-taskbar/ops/open_mon.sh` | glob-safe HQ-menu entry (a leading `&` in an `sh -c` menu row is job-control) |
| `state/ui.txt` | runtime projection (git-ignored) |

Run it by hand any time:

```sh
sh 44.xyz.01.00/&.hq-apps/mon-hq/mon_scan.sh list
sh 44.xyz.01.00/&.hq-apps/mon-hq/mon_scan.sh kill-all
```

---

## How GOOD vs BAD is decided

`mon_scan.sh` takes one `ps` snapshot, keeps every process whose command
line matches a **house pattern** (`prisc+x … .pal`, `/system/orchestrator`,
`chtpm_parser_pal`, `button.sh run`, `bv_render_3d` / `bv_gpu_raymarch` /
`bv_dispatch` / `bv_menu_input`, `pc_clock_daemon`, `pchq_board_projector`,
`khtpm_core_render.+x`, `*_hq_manager.+x`, `swatch_picker_manager`, …) and
is **not** part of the desktop shell (`khtpm_taskbar_manager_main`,
`khtpm_strip_parser`, `run_khtpm_strip.sh`, the strip xhtpms, the splash).

Each kept process is then classified:

| class | meaning | GOOD / BAD |
|---|---|---|
| `shell` | in the taskbar's own process-group | **GOOD** — protected, never killed |
| `pal` | a registered livedesk pal window (`…/home/livedesk/pals/…`, in the proc-ledger) | **GOOD** |
| `singleton` | a house-wide manager (`swatch_picker_manager`, `*_hq_manager`) — legitimately outlives its window, relaunched on demand | **GOOD** |
| `hq-window` | a lone `khtpm_core_render` / manager `setsid`-detached from the taskbar — this is how **every** `-hq` window normally runs | **GOOD** |
| `child` | its parent chain reaches another live house process | **GOOD**, unless that stack's root is itself BAD (resolved in a 2nd pass) |
| `pal-unreg` | a pal window that is **not** in the proc-ledger | **BAD** |
| `orphan` | reparented to `init(1)` — its launcher died, nothing owns it | **BAD** |
| `detached` | an **engine stack** member (`prisc` / `orchestrator` / `chtpm_parser_pal` / `button.sh run` / `bv_*` / `pc_clock_daemon` / `pchq_board_projector`) whose parent is `systemd --user` (cut loose from the taskbar) **or** not a house process, **or** it's unregistered and 3 min+ old | **BAD** |

The key rule: **only real game/board engine stacks earn "detached →
BAD".** A `khtpm_core_render` on its own is just a window; being
`setsid`-detached is normal for it. A shell whose argv merely *mentions*
a binary name (an agent's own shell, say) is ignored unless it's a real
`button.sh run` host.

`kill-all` does `TERM` → 2 s → `KILL` on every BAD pid, plus
`kill -<pgid>` on each BAD process-group. The taskbar process-group and
`mon_scan.sh`'s own ancestry are excluded every time, so it can never
take down the desktop or the window you're looking at.

### Does mon-hq itself get checked? Yes.

The `mon` window is a `khtpm_core_render` on `mon-hq.xhtpm`, `setsid`-ed
by `button.sh` exactly like `db-hq` / `stats-hq` / `events-hq`. It shows
up in its own scan as class **`hq-window` → GOOD**. It is never BAD (it
isn't an engine stack), and `mon_scan.sh` also drops its own process
ancestry from the kill set — so pressing **KILL ALL BAD** cannot close
`mon` itself. Its `mon_refresh.sh` loop is a `<module>` child of the
renderer, so it's `child → GOOD` and the renderer `SIGTERM`s it on
window close.

---

## The incident this was built for (2026-09-10)

**Symptom.** The machine felt sluggish; `mon`-style manual `ps` showed
**4 `prisc+x` VMs** with no visible windows:

- 2 × `piececraft-xyz` engine stacks — **up ~23 h**
- `piececraft-hq` engine + its `board-viewer` widget — **up ~10 h**,
  window already closed

Each stack: `sh -c button.sh run` → `button.sh run` → `system/orchestrator`
→ `chtpm_parser_pal` → `prisc+x pal/main_module.pal` (+ `pc_clock_daemon`,
`pchq_board_projector` on the hq one).

**Why it was bad on this box.** Not a runaway spike — a *persistent*
drain. The board-viewer diamond loop (`exec bv_dispatch; sleep 16667`)
keeps ticking ~60 ×/s with nothing to render. Measured: the abandoned
pc-hq stack held ~**10 % of one core** steady, ~16 min of accumulated
CPU per prisc VM. Two of them for a day. On a weak CPU that's the
difference between "the game runs" and "the game stutters".

**Root cause.** `button.sh` launches an engine session but nothing tears
it down when the window's `[X]` closes it — the classic *engine-split
leak* (`piececraft-launch-and-interact` memory; `04-bugs`). The two
oldest stacks were also launched from a `prisc+x` binary built *before*
the 2026-09-09 14:30 canonical rebuild that carries the proc-lifecycle
teardown, and were never written to `#.desktop/livedesk_proc_list.txt`
at all — so `kill_hq_windows.sh` (which reaps by ledger pgid) couldn't
see them either.

**How they were killed.**

```sh
# per stack, from the top wrapper down:
kill -TERM <wrapper_pid>        # 848246, 851123, 1198750
sleep 2
# TERM did NOT cascade - no supervisor - so an explicit sweep:
kill -KILL <every pid in pstree -p $wrapper>
```

`SIGTERM` on the wrapper reaching *none* of the children is itself the
proof they were orphaned stacks with no signal-propagating parent.
14 processes across the 3 stacks; all gone; `.gpu_render.pid` absent
(the GPU daemon had already idle-timed out); the live desktop
(taskbar + 7 pal renderers + `cursword`) untouched.

**What mon-hq changes.** That whole diagnosis is now one click. The
scan classifies exactly those stacks as `detached → BAD` (engine-stack
member, parent = `systemd --user`, unregistered, minutes+ old) and
`kill-all` runs the same `TERM`→`KILL`+pgid sweep, with the desktop and
the monitor window structurally excluded.

---

## Related

- `03-pitfalls/OPERATIONAL-LANDMINES.md` #9 — kill child processes, not
  just the window.
- `03-pitfalls/00-INDEX.md` → **CPU safety**.
- `04-bugs` — the engine-split leak entry.
- `#.desktop/livedesk_proc_list.txt` — the proc-ledger `kill-all` cross-checks.
- `../../*.monads/*.livedesk-taskbar/ops/kill_hq_windows.sh` — the
  ledger-based emergency reaper (`!kill hq` row); mon-hq is the
  observable, engine-aware companion to it.
