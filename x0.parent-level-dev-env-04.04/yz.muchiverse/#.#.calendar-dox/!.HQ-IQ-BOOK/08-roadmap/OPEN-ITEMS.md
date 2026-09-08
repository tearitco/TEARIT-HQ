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
11. NB-JS engine as a node/bun-like CLI runner: **v0 + CLI-1 node runner
     landed on `opencode` (d7797d74, 1588f1fd, cee6e587, cli-1 commit)** —
     `make` → `nbjs`; `nbjs file.js [args...]` runs node-style (no browser
     globals, `process.argv/cwd/env/stdout/stderr.write/exit`, exit 0/1/2,
     CPU-guarded by the 2 s eval budget); `nbjs --browser page.js
     [fetch.dom]` is the released DOM page runner (console.* → stdout,
     rendered rows → stdout, no khtpm/chtpm/GUI dependency);
     `./install-duk.sh` exposes it as a `duk` command + `$duk` env var;
     bare `duk` on a terminal starts a REPL (non-tty stdin stays the framed
     daemon, manager-safe). `make check` green (dom/fetch/events + new
     `cli_test` 7-case suite). Remaining note: Duktape 2.7.0 has NO arrow
     functions (`(() => 1)()` parses "empty expression not allowed") and NO
     `let` (`let x = 1` → "unterminated statement") — `const` and `var`
     work; use `function(){}` callbacks and `var`/`const` — engine
     limitations, documented for page authors. Farther out: require/fs,
     `NB-JS-CLI-NODE-LIKE-MODE.md` ladder.
