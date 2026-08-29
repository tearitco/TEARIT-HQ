---
name: taskbar-keyboard-relay-and-terminal-render
description: SUPERSEDED — original 2026-08-18 finding that started the taskbar terminal-mirror + relay-dispatch work; see taskbar-tpmos-parallel-refactor.md and taskbar-history-txt-migration-investigation.md for the actual built/verified result
metadata:
  type: project
---

# Taskbar keyboard relay + terminal-rendered taskbar — SUPERSEDED, kept as historical/prerequisite only

**Compacted 2026-08-24** (doc-compaction pass) — full original content moved to
`archive/taskbar-keyboard-relay-and-terminal-render.ARCHIVE.md` in this same directory.
**That archive file is DELETED as of 2026-08-29** (direct instruction, whole
`archive/` folder removed) — recoverable only via `git log --diff-filter=D`.

**Original finding (2026-08-18):** the real, current taskbar implementation
(`*.monads/*.livedesk-taskbar/ops/khtpm_strip_parser.c` + `khtpm_taskbar_manager.c`/
`khtpm_taskbar_manager_main.c`) is a split-process pair, a real intentional port of the
older `tp_taskbar.c`'s relay/dispatch shape. Two tasks were scoped from that finding:

1. **Task 1 — X11 menu keyboard-input relay bug hunt.** Result: code read in full
   (`poll_agent_relay()`, `dispatch_key_code()`, `dispatch_code()`), no static bug found —
   an honest null result, not a fix. The doc itself concluded this class of bug needs live
   reproduction, not more code reading.
2. **Task 2 — terminal-rendered taskbar, controllable via the same relay file.** This is
   what actually got BUILT — see the two successor docs below, which contain the real,
   live-verified implementation and status. Do not re-read this doc's own Task 2 section
   for current status, it's pre-implementation scoping only.

**Read these instead for current, real status:**
- `taskbar-tpmos-parallel-refactor.md` — the real terminal ASCII mirror
  (`khtpm_strip_render_ascii.+x` + `khtpm_strip_keyboard_ascii.+x`), DONE and
  live-verified, matching TPMOS's renderer/keyboard-input split exactly.
- `taskbar-history-txt-migration-investigation.md` — the real X11 capture→relay
  cutover (Phase 1-3, DONE 2026-08-19), replacing the old inline-dispatch path entirely.

This doc also contains a real, still-relevant architectural finding worth knowing
before touching this area — quoting its own header: **"the taskbar was built BACKWARDS
from the house standard"** (2026-08-18, direct user correction) — see the ARCHIVE for
the full explanation if you need the reasoning, not just the two successor docs' "what
it looks like now."
