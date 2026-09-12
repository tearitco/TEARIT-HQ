# Network browser: editing + images + video — RUNNING CHECKLIST

Live status sheet for the plan in
`NETWORK-BROWSER-EDITING-AND-MEDIA-DESIGN.md`. One task at a time; each
task gets its own how-to/arch doc written FIRST, then built, then
verified with real evidence, then checked off here. Hand-off safe: any
agent can resume by reading the doc for the next unchecked task.

Rule from AGENTS.md: never claim done on compile alone — every task needs
fresh make + live run + real evidence (state file, screenshot, receipt).

## Legend
- [ ] = pending · [~] = doc written, build not started · [x] = built +
  live-verified with evidence

## Task list

### 0. Pre-flight (after 2026-09-11 merge of Sonnet's refactor)
- [x] Merge `origin/main` → `opencode` (commit `2cbf0019`, pushed).
      No conflicts; both history lines preserved.
- [x] Renderer + manager both compile clean from merged source. Window
      opens on relaunch: PID 2486116 (renderer), PID 2486162 (manager),
      frame shows default Network Browser UI. `module_parent.pid` staleness
      was a pre-existing race, not a merge regression — note: pid file
      should be cleaned on manual relaunch.
      → build: `…/network/ops/build_core_render.sh` + `…/network/build.sh`
      → launch: `…/network/button.sh $HOUSE_ROOT` (after removing stale
      module_parent.pid in the pkg dir).

### A. Cli-io editing (vault: hand-off to Sonnet)
- [x] Root-caused + live-proven (2026-09-10): BackSpace works when the
      address cli_io is ARMED; 1 click no longer arms (two-step rule),
      and reparse disarmed mid-edit.
- [x] DESIGN doc + ownership note committed (`8d1820f0`, `e9732b41`).
- [x] Sonnet refactor merged (incremental reparse engine +
      cli_io/text_area survive reparse); OWNED BY SONNET, not opencode.
- [ ] Verify on the live browser: type in address bar, BackSpace edits,
      page/console reparse doesn't kill the edit. (Post-merge sanity,
      part of task 0.)

### B. In-page images via existing sprite path (D4–D5)   ← IN PROGRESS
Docs:
- [x] B doc: `NETWORK-BROWSER-IMAGES-HOWTO.md` (arch + exact gap +
      fix + verify steps). Committed 2026-09-11.

Build:
- [x] D4: worker `IMG|<url>` rows emit `MEDIA|I|<resolved>|<alt>` via
      `sb_put` (bypasses `rw_row`'s pipe-stripping); manager
      `merge_render_rows` replaces `MEDIA|` rows just like other RENDER
      rows. Live-verified static: `file:///…/tests/fixtures/img_test.html`
      → `IMG|…#.desktop/nb_sprites/m0|red box` in page.state.
- [x] D5: `write_placeholder_sprite()` emits a grey bordered tile when
      `fetch_to_sprite` fails (no more dropped rows). Live-proven twice:
      malformed fixture → grey placeholder tile in window AND in
      sprite.csv (`90,90,90`); well-formed PNG → real red tile
      (`253,0,0`). Border+diagonal marks it as placeholder.
- [x] Render a page with an `<img>`; tile draws with real pixels.
      PROBE B2 (static) PASSED 2026-09-11 live in window 0x1a00002:
      ffmpeg-made `redbox.png` shows as a RED SQUARE. Stale fixture was
      an stb-incompatible PNG ("invalid filter") — regenerate fixtures
      with ffmpeg, never hand-made PNGs.
- [x] JS-page `<img>` route PASSED: `img_js_test.html` (script sets
      title v2 → worker RENDER runs → `merge_render_rows` replaced rows)
      → `IMG|…#.desktop/nb_sprites/m0|red via worker` → sprite red
      (253,0,0). Proves D4's worker emit path end-to-end.
- [ ] PROBE B1 (manifest covers), PROBE B3 (placeholder on 404)
      still to run.

### C. Video via wraith-alpha player subprocess (V2)
Docs:
- [ ] C doc: `NETWORK-BROWSER-VIDEO-HOWTO.md` (wraith contract
      `video.control`/`video.playback`/`current_frame.png` + how we
      spawn it under the manager).

Build:
- [ ] V2 p.o.c.: one short mp4 + a local fixture page with `<video>`;
      player subprocess steered by `video.control`, frames land on the
      canvas via `source_ref`/sprite path.
- [ ] Probe C1: watch `frame_index` advance; screenshot a mid-frame.
  (V1 keep ffplay for real playback; V3 rawvideo pipe is later if ever
  needed.)

### D. HTML subset spec (D8–D9)
Docs:
- [ ] D doc: `NETWORK-BROWSER-HTML-SUBSET.md` (supported/unsupported
      tags+attrs, entity table, corners).

Build:
- [ ] Subset contract written into `nb_dom.h` (comments + flags).
- [ ] Parser fixture corpus: small .html cases covering every
      supported/unsupported construct; test harness asserting nb_dom
      output rows.
- [ ] Close enumerated holes from the audit (worker `<video>` RENDER
      branch missing, worker IMG row, entity gaps) as the subset doc
      specifies.

### Z. Wrap-up
- [ ] Update this checklist's statuses throughout.
- [ ] Every touched source change is committed scoped per AGENTS.md
      (never `git add -A`; commit at end of session; own branch only).
- [ ] Any user-facing change recorded via PRESENTATION-VIDEO-PIPELINE.

## Per-task evidence log
| Date | Task | Build | Evidence |
|------|------|-------|----------|
| 2026-09-11 | Merge Sonnet refactor | — | merge `2cbf0019`, pushed, tree clean |
| 2026-09-11 | B/D4+D5 | `build.sh` OK ×4 | Probe B2 live: img_test.html → IMG|…/nb_sprites/m0|red box (page.state) + red sprite.csv (253,0,0); placeholder path proven via malformed fixture (grey 90,90,90) |
| 2026-09-11 | B/D4 (worker) | — (same build) | Probe JS: img_js_test.html → title "v2" (worker RENDER replaced rows) → IMG|…/m0|red via worker → sprite red (253,0,0) |
|          | (fill in as tasks complete) | | |

## How to resume
1. Read `NETWORK-BROWSER-EDITING-AND-MEDIA-DESIGN.md` (the plan).
2. Pick the first task with `[~]`/`[ ]` here, read its how-to doc.
3. Build, verify with real evidence, commit scoped, update this sheet.

## Two-checkout gotcha (2026-09-11, cost an hour)
- The LIVE desktop runs from `…/NNEST-12.00/…` (no `-opencode`); the AGENT
  edits+commits in `…/NNEST-12.00-opencode/…` (its own git checkout).
- Editing+reproducing BOTH in `-opencode` while pointing `button.sh` at
  `NNEST-12.00` proves nothing — the live manager binary won't have the
  new code.
- Deploy path (user decides): agent commits in `-opencode`, user syncs the
  changed files / checkout into `NNEST-12.00`. When self-testing, launch
  the browser FROM the `-opencode` tree (its own taskbar ops renderer +
  fresh manager binary + own `#.desktop` state all exist there), then kill
  only the browser's exact PIDs afterwards — never `pkill
  network_browser_manager` (it kills the whole house desktop incl. pals).
- Kill tip: `kill <renderer_pid> <manager_pid>` by exact PID; then
  `rm -f …/network/module_parent.pid` before re-launching.