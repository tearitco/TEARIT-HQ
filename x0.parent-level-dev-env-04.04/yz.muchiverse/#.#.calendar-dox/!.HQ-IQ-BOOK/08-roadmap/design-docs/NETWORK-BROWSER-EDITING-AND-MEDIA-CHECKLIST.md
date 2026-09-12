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
- [ ] D4: worker `IMG|<url>` rows go through the same
      fetch→`nb_media_to_sprite`→sprite-dir pass as `MEDIA|` rows
      (manager `collect_page_media`, gap proven in design doc §2).
- [ ] D5: placeholder tile + alt text when the fetch/decode fails
      (today a failed IMG row is dropped or blank).
- [ ] Render a JS page with an `<img>`; tile draws with real pixels.
      Verify recipe in B doc.

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
|          | (fill in as tasks complete) | | |

## How to resume
1. Read `NETWORK-BROWSER-EDITING-AND-MEDIA-DESIGN.md` (the plan).
2. Pick the first task with `[~]`/`[ ]` here, read its how-to doc.
3. Build, verify with real evidence, commit scoped, update this sheet.