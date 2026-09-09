# Open items — real, current, as of 2026-09-08

*See `00-INDEX.md` for the fuller list with file pointers. This is the
short version.*

1. Events/db-hq: registry grew 2026-09-08 (`mr_world` — transfer/shop/battle/fade are **kv only**). db-hq still only Common Events is an editable tab. Catalog: `TILESETS-EVENTS-AND-GAME-CLONES.md`.
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
     + CLI-2 CommonJS + CLI-3 fs/REPL + CLI-4 ESM landed on `opencode`
     (d7797d74, 1588f1fd, cee6e587, cli-1..cli-4 commits)** —
     `make` → `nbjs`; `nbjs file.js [args...]` runs node-style (no
     browser globals, `process.argv/cwd/env/stdout/stderr.write/exit`,
     exit 0/1/2, CPU-guarded by the 2 s eval budget); `require()`/
     `module`/`exports` with JSON require + circular-require handling
     (CLI-2); fs-lite `require('fs')` (readFileSync/writeFileSync/
     appendFileSync/existsSync, recursive mkdirSync, CLI-3) and `duk -i`
     REPL (works piped; the REPL also has require/fs) CLI-3; source-level
     ESM (CLI-4): line-based `import`/`export` → CJS transpile for entry
     files, required modules, and single REPL lines (default/named/
     namespace/side-effect imports; named/default/brace/from/star
     exports; `__esModule` interop);
     `nbjs --browser page.js [fetch.dom]` is the released DOM page runner
     (console.* → stdout, rendered rows → stdout, no khtpm/chtpm/GUI
     dependency); `./install-duk.sh` exposes it as a `duk` command +
     `$duk` env var; bare `duk` on a terminal starts a REPL (non-tty
     stdin stays the framed daemon, manager-safe). `make check` green
     (dom/fetch/events + `cli_test` 18-case suite). Remaining note:
     Duktape 2.7.0 has NO arrow functions (`(() => 1)()` parses "empty
     expression not allowed") and NO `let` (`let x = 1` →
     "unterminated statement") — `const` and `var` work; use
`function(){}` callbacks and `var`/`const` — engine limitations,
      documented for page authors. Multi-line ESM statements, dynamic
      `import()`, and decorators/type annotations are out of scope;
      any further fs/path depth is user-requested
      (`NB-JS-CLI-NODE-LIKE-MODE.md`). Engine roadmap rung 6: file-backed
      `document.cookie` jar LANDED (2026-09-07, `1f943aba`, new `wck`
      make-check suite — C natives with RFC-6265 host/path/expiry-
      max-age scope, jar at `$NB_COOKIES_FILE`, survives LOADs via disk);
      remaining rung-6 piece is real `history`/`location` navigation to
      the manager (before touching `network_browser_manager.c`, the
      khtpm-house-standards lock requires reading INDEX tier-1 docs +
      `CENTROID_GOLD_STD.md` first).
12. Game-clone tiles/events (MC / CDDA / Civ / GTA / RPG Maker): pickers
    + event-guide PDLs + sample maps exist; **not** drop-on-board,
    voxel→events-hq, desk persistence, or battle/shop UI. See
    `TILESETS-EVENTS-AND-GAME-CLONES.md` §6.
13. Tiled + OHRRPGCE palettes, My Palettes library, tile-editor:
    **design only** — `design-docs/MY-PALETTES-TILED-OHR-TILE-EDITOR-DESIGN.md`.
    Local demos already exist under `#.potential-assets/#.hampster-tiles…`.
14. Strip submenus → data-driven (`livedesk_taskbar.pdl`
    `<cell>_menu_N_label/_cmd`): player/ai/db converted; the four
    directory-scanning builders (user/pals/toys/clock) still hardcoded
    — need a prefix+scan+suffix pdl pattern. `TASKBAR-MENUS-DATA-DRIVEN.md`.
15. sql-hq: built through step 6 + db-cell wiring (step 9). Remaining —
    staged Begin/Commit/Rollback polish (7), Export + re-runnable
    History (8), grid keyboard nav + RFC-4180 quoted-comma CSV
    splitter (10). `design-docs/SQL-HQ-DESIGN.md`.
16. pc-hq board Interact/focus (grok): WM-managed board window +
    per-frame focus re-assert + fix "Interact Mode never arms" (reparse
    -on-vars-change not firing for the pchq board). `09-appendix/
    pc-hq-leg-vs-nu-fix.md` §5/§6/§6b, `pc-hq-bugs.md` Bug 2.
17. Process-lifecycle teardown (TPMOS parity): **WIRED + LIVE-VERIFIED
    2026-09-09** — restart→register→quit→reap→truncate confirmed on the
    running desktop; dropdowns unaffected; legacy pidfile still written.
    Left: app-fork funnelling — a window/manager NOT launched by the
    taskbar still doesn't register (§5 steps 4–7).
    `design-docs/PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md`.
18. ~~Shared source compiled by copy-into-`ops/`~~ **DONE 2026-09-09**:
    switch every `build_*.sh` to compile the canonical
    `&.widgits/_shared-lib/*.c` in place with `-I` (binary stays local),
    delete the ~5 stale `ops/` copies (6 already drifted), packaging
    keeps self-contained subtrees via one `vendor-into.sh`.
    `design-docs/SHARED-SOURCE-COMPILE-IN-PLACE.md`.
19. prisc+x.c fork consolidation: ~26 copies / 7 variants across ~20
    pal-using projects. **Phase A (classify) DONE 2026-09-09**
    (`PRISC-X-FORK-CLASSIFICATION.md`): the `_shared-lib/` canonical is
    the newest VM, every fork is *behind* it (10-cluster carries a real
    path-trunc bug) — upgrade-everyone. Phase B blocker: nothing builds
    the canonical yet, so it needs a standalone-build + `.pal`-corpus
    diff before any project switch. `PRISC-X-FORK-CONSOLIDATION.md`.
