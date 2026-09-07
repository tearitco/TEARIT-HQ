# Open items — real, current, as of 2026-09-02

*See `00-INDEX.md` for the fuller list with file pointers. This is the
short version.*

1. Events/db-hq: low-risk next steps identified, not started.
2. Cross-platform (Windows/Mac) work: pending.
3. Generic khtpm dispatch table (replace `g_is_<mode>` flags):
   designed, not built.
4. ASCII/headless khtpm renderer: DONE (2026-09-06). `cli` strip
   mirror + per-window text frames + `khtpm_core_render.+x --headless`
   (no X at all). See `design-docs/TERMINAL-MIRROR-PARITY-all-
   windows.md`. Remaining: entity manipulation from the text view
   (separate design).
5. LayDoc → Elem/CSS taskbar retarget: not started.
6. Audit `khtpm_core_render.c` for sibling inline data-loaders like
   `dbhq_load_actors()`: not done.
7. Toys-launch PID tracking (kill-all doesn't reach toys-launched
   apps): open bug, see `04-bugs/BUG-LOG.md`.
8. chat-hai migration onto the khtpm/Harnecient standard: NOT done —
   don't assume otherwise from an older doc.
9. Joystick/controller support: not started.
10. `ktb_pid_alive()` zombie-PID false-positive: structural fix not
    done (workaround documented in `04-bugs/BUG-LOG.md`).
11. NB-JS engine as a node/bun-like CLI runner (require/process/fs,
    exit codes, stderr): **not started — scoped, not too late.** The
    `install_host()`/prelude seam makes it additive; a primitive CLI
    (`ops/nb_js_eval <file> <out>`) already exists. Brief:
    `design-docs/NB-JS-CLI-NODE-LIKE-MODE.md`; ladder queued behind the
    rung-4 XHR/fetch delivery (in flight on `opencode`).
