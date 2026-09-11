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
      real `history`/`location` navigation to the manager LANDED
      (2026-09-08, see roadmap — `NAV\n` frame + `consume_pending_nav()`,
      new `wcn` make-check suite; location.assign/replace/reload + href=,
      history.back/forward/go, pushState/replaceState address-bar ADDR).
      Remaining rung 6 scraps: none blocking — `matchMedia`/getComputedStyle
      stubs shipped in `b079f0c9`; rung 7 (layout awareness) deferred.
      Rungs 3-4 also LANDED in the resident worker (timer/event drain,
      XHR/fetch) — the only true Phase-2 remainder is document-order script
      runs (manager still concatenates `<script>` bodies into
      `tmp/page.js`). Real `localStorage`/`sessionStorage` LANDED
      (2026-09-09, new `wst` make-check suite): localStorage = per-house
      disk jar at `<house>/#.desktop/nb_localstorage.txt` via
      `NB_LOCALSTORAGE_FILE` (pct-encoded key/value lines, atomic writes);
      sessionStorage = in-memory, reset per LOAD. Reconciled 2026-09-09.
      Phase-2 **document-order script runs** LANDED (2026-09-09, new `wps`
      make-check suite): manager emits one `<script>` (inline or src-fetched)
      per page.js slice, split on `/*nbjs-script-boundary*/`; the worker
      compiles/runs each slice as a separate program — document order +
      per-script syntax isolation + shared top-level `var` + external src at
      DOM position; failures print `WERR| script N: ...` into the worker
      stderr log (surfaced as `[worker]` lines, boot-hygiene slice). Real
      `http://` server fetch breadth LANDED (2026-09-10): live `python3 -m
      http.server` E2E proved the manager's curl ladder end-to-end — inline +
      relative `<script src>` page renders `seq=a,b,c`, localStorage persists
      across http navs, and JS-side relative `fetch()`/XHR resolve against
      the page URL and hit real servers (fixed the prelude `splitParts`
      double-port bug on base URLs with explicit ports; worker
`resolve_doc_url()` fallback + resolved URL in fetch errors). Only rung
       7 CSS/layout awareness remains as an honest gap. Hardening LANDED
      (2026-09-10): shared per-house cookie jar for page/script/worker
      curls (`nb_curl_cookies.txt` — server Set-Cookie persists + is
      retransmitted, worker fetch same-origin; separate from the
      `document.cookie` jar), browser-correct `script_type_skip` (template/
      module/non-JS types never run) and noscript script suppression, and
      a real remote-site smoke test (example.com redirect + httpbin cookie
      round-trip + iana full page through the ladder). Manager is now
      single-instance per house (`flock` on
      `#.desktop/network_browser_manager.lock`, exits rc=2 on a second
      launch) — the racing-manager pileup that corrupted live E2E runs can
      no longer happen. **Rung 7 CSS/layout awareness LANDED (2026-09-10,
      slice 1 CSS-cascade subset, new `wcs` make-check suite + worker-side
      `nb_css`)**: inline `<style>` + up to 4 linked stylesheets (resolved
      + curled via the shared cookie jar) shipped as a 5th LOAD line;
      rule cache + selector matching (tag/`#id`/`.class`/`*`, descendant,
      comma lists; `:pseudo`/`[attr]` stripped), specificity + source
      order + `!important`, inline wins; computed surface display/
      visibility/opacity/px-width+height; `getComputedStyle` +
      `getPropertyValue`, `el.style` snapshot, offset/client width-height,
      `getBoundingClientRect` (x/y 0), `offsetParent`; `display:none`/
      `visibility:hidden` (self or ancestor) → metrics 0 + null parent.
      Honest slice-2 gaps: no text-flow/layout (sizes = CSS px or 0),
      `@media` never fires (no viewport), no inheritance, `el.style`
      writes don't reflow. Full gate now 55 PASS + 4 binaries; live E2E on
      a 8126 styled fixture → `ghost=none,rect=140x80,offH=80,op=null`,
      zero WERR.
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
    master-ledger column + kh_spawn funnel + self-register landed
    (§5 step 4, tested + live). renderer now registers + reaps its own <module>s (§5 step 5a,
    live-verified). Left: mpg123/music-player-hq, the prisc VM (that's
    chtpm_parser_pal.c's <module> launch), fold livedesk_hq_windows_
    <pid>.txt, drop livedesk_launched_pids.txt (§5 5b–7).
    `design-docs/PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md`.
18. ~~Shared source compiled by copy-into-`ops/`~~ **DONE 2026-09-09**:
    switch every `build_*.sh` to compile the canonical
    `&.widgits/_shared-lib/*.c` in place with `-I` (binary stays local),
    delete the ~5 stale `ops/` copies (6 already drifted), packaging
    keeps self-contained subtrees via one `vendor-into.sh`.
    `design-docs/SHARED-SOURCE-COMPILE-IN-PLACE.md`.
19. prisc+x.c fork consolidation: **Phase B DONE 2026-09-09** — 17
    pal-VM projects (incl. wsr-pal + muchi-pals-egg, whose _WIN32/MinGW
    compile shims were folded into the canonical) now build the ONE
    `&.widgits/_shared-lib/system/prisc+x.c`; ~11 others were already on
    `$_SS`. Left (small): ledger-player + lpns+map (orchestrator-built,
    no scripts/build.sh compiles prisc — need a real line added);
    egg's CreateProcessA cmd.exe-correct custom-op dispatch (needs a
    MinGW build to verify). `PRISC-X-FORK-CONSOLIDATION.md`.
20. HARNECIENT NIGHT-track horizon items: 6 real, concrete proposals
    surfaced while writing the NIGHT_01-10 onboarding/export lesson
    track (`1-1.HARNECIENT.SMOL/`), each grounded in real code found
    live — pc-hq's own trigger layer, wager-chess on the real
    `pal-chain`, finishing `cursword_say()`'s real stub, extending
    `corp_decide.c`'s real `decision_mode` with GOAP/policy modes,
    a Gemma-authored `weights.txt` tool, and swapping the real
    from-scratch attention chatbot's (`#.Z.HUMAN_LLM/.../attention.c`)
    random cold-start weights for Gemma-authored ones. Not started —
    full list + file pointers: `design-docs/HARNECIENT-NIGHT-TRACK-
    HORIZON-ITEMS.md`.
