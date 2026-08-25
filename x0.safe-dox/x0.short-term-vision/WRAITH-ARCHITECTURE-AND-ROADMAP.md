# Wraith / TPMOS Architecture, Conventions & Roadmap

Consolidated 2026-07-06, replacing all prior files in this directory
except `settings-hub-window-geom-design-j5.md`, which stays as its own
file since it's an actively-maintained, current design doc (last
touched today, across multiple sessions) rather than settled history.
Superseded files: `0x-pet-wraith-architecture-j29.md`, `2fix-j29.txt`,
`chtmgl-wraith-media-contract-j30.md`, `chtpmgl-assumptions-j29.md`,
`chtpmgl-mirror-contract-j29.md`, `fs-looks.txt`, `handoff-j29.txt`,
`ops-audit-table-j29.md`, `ops-surface-j28.md`, `README.md`,
`share-kvp-webcam-poc-j01.md`, `todo-j5.txt`, `todo-jul2.txt`,
`tpmos-wraith-browser.md`, `window-first-enclosure-editor-vision-j29.md`,
`window-geometry-render-plan-j5.md`, `wraith-gl-readiness-retrospective-j29.md`,
`wraith-shell-fs-roadmap-j28.md`, `wr-prompt-j28.txt`, `#.wussup-j28.txt`,
`xo-editor-bridge-roadmap-j28.md`. These accumulated over ~9 days
(2026-06-28 through 2026-07-06) as design docs, roadmaps, retrospectives,
and bug logs; a lot of their content was superseded by later work in the
same set. This doc keeps only what's still true. A zip backup of the
originals exists (made before deletion) if historical/narrative detail
is ever needed.

Also folded in here: two files that were physically sitting in the
sibling `x0.piececrafts/` directory (`j5-prompt.txt`,
`j5-summary&handoff.txt`) but are actually about this same
Wraith-desktop-shell/window-geometry feature, not piececraft-wraith
specifically — moved here topically (see section 4).

**Path migration note:** docs from 2026-06-28/29 refer to hosted Wraith
projects under `projects/wraith/wraith-projects/`. This was renamed to
`projects/wraith-alpha/wraith-projects/` partway through — every fact
below uses the current `wraith-alpha` path.

---

## 1. Core Architecture Principles (standing, durable)

- **Enclosure = sovereign, file-backed artifact.** A "world/tank/pet
  bundle" (map, residents, controller bindings, editor metadata,
  exports) is its own durable thing on disk; Wraith (and any future
  host, e.g. a hypothetical `0x-pet`) is just a *front door* over that
  same contract, never a second format. `wraith-ed` is the existing
  precedent for an "editor-pattern" host. This same principle extends
  to UI/host facts, not just launch identity — e.g. `project.pdl`'s
  `WINDOW` section (section 3) is per-project sovereign data, not
  something Wraith owns.
- **Reuse-order convention** (apply before writing new code): use an
  existing op as-is → widen it → copy-and-modify locally → only then
  write something genuinely new. Safe-modification protocol: copy
  locally, prove the new behavior works, preserve old callers'
  behavior unchanged, upstream/consolidate later — never mutate a
  shared op in place if something else depends on its current behavior.
- **ASCII/GL mirror contract:** ASCII must mirror semantic UI *truth*
  (nav order, focus, actions, row/group structure, state) — never
  approximate GL pixels. Object classes (interactive / informational /
  decorative) determine what must mirror. Standard scroll-strip row
  shape: `^_UP` / `v_DOWN` / `Thumb:[...]`. Media assets degrade to
  audit-token text (e.g. `Audio: track_02 playing`) as the guaranteed
  floor — a richer RGB→ASCII projection is optional upside, never
  required.
- **CPU-safety pitfall (real, already fixed once, watch for recurrence):**
  Wraith's render loop is capped ~60fps and was never the actual CPU
  problem — the real source of runaway CPU was **orphaned host `ffmpeg`
  processes** (webcam/screen-record capture) surviving after a Wraith
  session ended, each consuming 150%+ CPU. `pieces/os/kill_all.sh`
  now also kills `wraith_webcam_capture`, `wraith_screen_record`,
  `ffmpeg .*video4linux2`, `ffmpeg .*x11grab`, and stale pid files;
  the capture ops themselves also need to kill residual ffmpeg by
  session/output path on their own startup/stop, not just clear pid
  files. **If CPU climbs during any media-related testing, suspect host
  `ffmpeg` before the Wraith frame loop.**

---

## 2. State/File Conventions & Gotchas

- **Two different, easy-to-confuse state conventions, for two different
  invocation paths:**
  - `manager/state.txt` — the generic convention, loaded by
    `load_state_file("projects/%s/manager/state.txt")` keyed off
    `project_id` derived from the current layout path. Used by projects
    reached via `href`/standalone (not desktop-embedded).
  - `session/state.txt` + `session/wraith_body.txt` — the
    desktop-embedded `Window`/`g_windows[]` convention.
  These are **not interchangeable** — using the wrong one is a real,
  repeated source of "why doesn't my `${var}` show up" bugs.
- `wraith_body.txt` is plain summary text only — real numbered/interactive
  nav rows must live in `scene.objects.pdl` (both ASCII and GL should
  mirror the same scene-owned rows, not hand-fake nav in the body text).
- Every Wraith-hosted project should ship a minimal session triad from
  day one — `session/wraith_body.txt`, `session/state.txt`,
  `session/scene.objects.pdl` — or the ASCII shell degrades to a
  "Missing project body file" fallback.
- `grid.pdl` payload format (header `GRID id=/rows=/cols=`, then
  `CELL row=/col=/ch=/fg=/bg=/border=/nav=/action=` lines) is the
  adopted format for dense colored-cell content in the ASCII shell —
  confirmed shipped and in use (e.g. `session/webcam_preview.grid.pdl`).
- Marker-file redraw seam is the general mechanism for any
  dynamic/watched surface (not just video) — a project-owned marker
  file (e.g. `session/fs_watch.marker`) that a watcher appends to,
  polled the same way `frame_changed.txt` already is. Never poll on an
  unrelated trigger, and never fake a manual-refresh button when a
  watcher+marker can do it automatically.
- Projects reading `session/history.txt` for commands need their own
  consumption cursor, or redraws will replay stale commands.
- Launcher IDs must be normalized consistently (hyphen vs. underscore)
  between whatever renders `DESKTOP_ACTION:launch_*` commands and
  whatever does the exact-match lookup on the receiving end — a
  mismatch here silently no-ops the launch with no error.
- Nested Wraith project compiles can leave `ops/+x/*.+x` (the actual
  runtime path a manager executes) stale even after `ops/src/+x/*.+x`
  recompiles — any compile step must copy the fresh binary into
  `ops/+x/`.
- After recompiling any long-lived Wraith process (parser, manager,
  daemon), the running instance must be restarted to pick up the new
  binary — recompiling alone does nothing to a process already running.
- **`href` navigation** lives entirely inside the parser's own process
  memory (`current_layout` global) — clicking any element with a
  non-empty `href` directly reassigns which file is "the whole screen."
  The only cross-process way to trigger a layout switch is the
  file-based `request_layout_change()` → `pieces/display/layout_changed.txt`
  → the parser's own poll loop.
- **Two independent nav-index systems exist** — the parser's own
  auto-incrementing counter (ASCII mode) vs. `wraith-alpha_manager.c`'s
  `g_max_index`/`launcher_start` arithmetic (GL mode). They often agree
  by coincidence, not by construction — extending one does not
  automatically extend the other. (See also `x0.piececrafts/PIECECRAFT-WRAITH-ARCHITECTURE.md`
  section 2 for the "re-grep, don't trust any doc's site count" warning
  on this same arithmetic.)
- **Fork-duplication fact, and which fork is actually live:**
  `wraith_parser_alpha.c` is Wraith's own ~2700-line separate
  reimplementation of the classic `pieces/chtpm/plugins/chtpm_parser.c`
  parser — not a wrapper around it. As of 2026-07-06 (`2fix-july6.txt`,
  parent dev-env directory, bug 4), **`chtpm_parser.c` was confirmed via
  `ps aux` to be the actual running parser for wraith-alpha's live
  sessions** — despite the naming, `wraith_parser_alpha.c` was not
  executing at all. Any parser-side fix should be verified against
  (and generally ported to both, to keep the forks in sync) — but
  always confirm which one is *actually running* before assuming a
  code change has any live effect.

---

## 3. Desktop Window System

- `Window` struct originally had no `x`/`y`/`width`/`height` at all —
  these were added reading from a `project.pdl` `WINDOW` section via
  the existing generic `read_pdl_value()` (no new parsing mechanism).
  Backward compatible: a project with no `WINDOW` section just gets
  today's old default behavior.
- Existing primitives to build on, not reinvent: `g_windows[]`,
  `g_active_window_slot`, `WSTATE_OPEN`, `recompute_active_window_slot()`,
  `desktop_active_window_body_visible` (already-existing focus-based
  visibility primitive).
- **Two parallel rendering pipelines describe the same screen and must
  move together:**
  1. The **visual** stream (`current_frame.txt`) — built by the
     parser's `render_element()` via pure sequential `strcat()`, with
     no cursor/coordinate concept at all.
  2. The **semantic/click** system (`current_frame.objects.pdl` /
     `desktop_state.pdl`) — explicit `x=/y=/w=/h=/nav=/action=` per
     element, written separately by `wraith-alpha_manager.c`, used for
     mouse hit-testing.
  Any positioning/rendering change must update both consistently — this
  has caused multiple real, separate bugs when a fix only touched one
  side.
- The whole desktop chrome is a **fixed 128-column × 40-row monospace
  character-cell grid, always** — there is no raw-pixel x/y anywhere in
  the rendering pipeline; "position" always means "which character
  cell."
- `resolve_window_content_origin()` / `append_with_origin_offset()` are
  the shared helpers both pipelines must call for window-content
  positioning, specifically to prevent "two independently hand-written
  copies of the same offset math" drifting apart.
- Width precedence (3-tier): `project.pdl` `WINDOW.width` (always wins)
  > layout-declared width (not yet wired as of last check — the
  parser's `parse_attributes()` has no `width`/`height` in its
  allow-list yet) > hardcoded historical default (96).
- Per-frame GL "receipts" exist for headless testing:
  `current_frame.objects.pdl`, `.desktop_state.pdl`, `.focus_state.pdl`,
  `.cells.pdl`, `.hitmap.pdl`, `.audit.txt`/`.meta.pdl`. **Gotcha:**
  href-reached layouts (like `window-geom`) got no GL/semantic receipt
  of their own content until href-in-GL was wired (`objects.pdl` always
  described the desktop shell, never the href-navigated layout, before
  that fix) — verify this is still true if debugging a similar gap.
- **Lesson (real, once caused a false alarm):** do a live/orchestrated
  run before trusting headless artifact checks — an empty receipt-history
  file can mean "no live session has run here," not "the writer is
  dead," and a stale cached root path in a first headless test can mask
  the real bug entirely.

---

## 4. Settings Hub / Window-Geometry Editor

This is the **current, active feature** — full detail lives in
`settings-hub-window-geom-design-j5.md` (this directory) for the design,
and `2fix-july6.txt` (parent dev-env directory) for exact
session-by-session diffs and bug fixes. This section only records
mechanism facts and open items not fully captured there.

**Generalizable mechanism, not settings-specific:** an *embedded*
window (opened from the desktop's launcher row, appearing in the
taskbar) must never let a menu click inside it mutate the global
`current_layout` — that would blow away the whole desktop. Instead it
writes to a per-project marker file (e.g. `state_changed.txt`, polled
like `frame_changed.txt`), which that project's own manager watches to
re-render its own `wraith_body.txt`, keeping the desktop and taskbar
visible. A *standalone* window (opened via a chrome button or direct
navigation) instead pre-writes a target file (e.g. `wg_target.txt`)
before opening as its own new window. Any future project with an
embedded-vs-standalone duality should follow this same split.

`desktop_focused_window_project_id` (published every frame by
`write_projection()`) was originally added just so `window-geom` could
know which project's `project.pdl` to read. It was later reused
(2026-07-06, `2fix-july6.txt` sections 6-7) as the fix for a *different*
bug: `chtpm_parser.c`'s generic `project_id` variable always stays
`"wraith-alpha"` for embedded sub-project content (since `current_layout`
never actually changes for body-passthrough pages), which broke cli_io
save/restore scoping for embedded fields. Worth knowing this variable
now serves two purposes.

**Known open items — flagged as unresolved/unverified, not settled facts:**
- A prior investigation (now-deleted `j5-prompt.txt`) found and proposed
  a fix for a **project_id-clobber bug** inside `wraith_parser_alpha.c`'s
  `load_vars()`: `set_project_id_from_layout()` sets `project_id`
  correctly for a nested/href-reached project, but a *later*
  `load_state_file()` call (loading `alpha_state.txt`, which itself
  starts with `project_id=wraith-alpha`) clobbers it back, so the nested
  project's own `manager/state.txt` never loads and its `${vars}` render
  blank. **This is a different mechanism from** the "`project_id` always
  stays generic for embedded content" bug fixed in `chtpm_parser.c` this
  session (previous paragraph) — they're easy to conflate since both
  produce blank/wrong `${var}` rendering for embedded/nested content, but
  they're in different files with different root causes. Given
  `chtpm_parser.c` (not `wraith_parser_alpha.c`) is the one actually
  running live (section 2), **re-verify whether this specific clobber is
  still live in `chtpm_parser.c` before assuming it's fixed or still
  broken** — it was never confirmed either way as of the last write-up.
- GL mode was seen rendering extra stray `|` bar characters in embedded
  body-line padding — traced to a padding/column-count misalignment
  against the real outer-frame right edge (via `resolve_frame_width()`),
  not a structural double-render, but not fully fixed as of last check —
  needs a live screenshot to finish diagnosing.
- A universal per-window chrome "[i]" button that opens `window-geom`
  targeted at the active window: the design (a 4th chrome button on
  both the ASCII and GL emission paths, dispatched via a
  `DESKTOP_ACTION:open_window_geom` route) is sound, but wiring status
  should be re-checked against the current code rather than assumed
  from any prior write-up.

---

## 5. Media & Shared-Memory Architecture

- Media tag contract (`<img>`/`<audio>`/`<video>`): minimal required
  fields are `src`, `x`/`y`/`width`/`height`, `id`, plus
  `autoplay`/`loop`/`controls`/`poster` where applicable. Standing rule:
  parse → semantic object → decoded/runtime state → GL renders the rich
  form, ASCII renders the semantic-state row (`IMG:`/`AUDIO:`/`VIDEO:`
  tokens) — both must originate from the same decoded truth, never
  diverge independently. Implementation difficulty order: image
  (easiest, best precedent) → audio → video (most expensive). As of the
  last confirmed status, image/audio/video were each individually
  proven working in the active GL parser lane (an earlier doc claiming
  audio/video were unimplemented was superseded one day later).
- **Live-frame-cache / shared-KVP architecture** — proven, in
  production for webcam. Key files:
  `pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c`,
  `pieces/chtpm/ops/+x/tpmos_share_kvp_db.+x`,
  `tpmos_share_kvp_adapter.+x`, `pieces/chtpm/ops/lib/tpmos_live_frame_cache.c`,
  `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`. What's proven:
  per-project shared-memory seam for webcam state/marker/frame;
  `current_frame.png` can round-trip through the DB as a blob; audit
  materialization happens in a forked worker without making the DB
  file-backed.
- **Three standing rules for this architecture:**
  1. File path names remain the semantic key contract even when the
     actual data is memory-backed — the path is the identity, not
     necessarily the storage.
  2. Memory is the runtime transport, not a hidden/secret store — it
     must stay queryable/dumpable like a file would be.
  3. Audited/sovereign artifacts (state.txt, wraith_body.txt,
     scene.objects.pdl, receipts) flush to disk on demand/shutdown; that
     dump work happens in a forked worker so it never stalls the hot
     path.
- Two data classes, only one of which belongs off files: **hot runtime
  transport** (frame pixels, epoch counters, status — fine to be
  memory-only) vs. **audit/sovereignty artifacts** (state, body,
  scene, receipts — must stay file-backed per the enclosure principle
  in section 1).

---

## 6. Editor / Multi-Window Family Vision (future track, partially started)

**Governing recommendation (still current):** build the eventual game/enclosure
editor as a **family of cooperating narrow Wraith windows** (map view,
picker, event editor, exporter) sharing one enclosure/project truth,
rather than one big integrated editor built upfront — unify later once
the pieces exist and prove themselves independently. All windows in the
family must read/write a common shared-truth set: map files,
entity/piece files, event/script files, controller bindings,
session/editor metadata, export/build metadata.

**Explicit non-goals for the current pass:** no full unified `wraith-ed`
editor yet; no saved window-layout presets; no meta-layout/embedded-micro-layout
composition; no second windowing/focus-tracking system parallel to
`g_windows[]`.

**XO-pets bridge sequencing** (superseded in architectural detail by
section 1's enclosure principle, but the sequencing/proving-ground idea
is still useful): use `fuzz-op` as a mechanics benchmark, not a final
contract; drag/drop should first mean auto-discovery via filesystem
refresh, not a GUI gesture, before building anything fancier.

---

## 7. Browser Vision (future track, scaffolded only)

Recommended direction: a **TPMOS-native hybrid browser** — sovereign
HTML/CSS/DOM/layout/paint model first, a small embeddable JS runtime
later, an optional WebKit-style compatibility backend only much later
(never make WebKit the first architecture anchor). Everything should be
ops-based (`browser_fetch`, `browser_parse_html/css`, `browser_build_dom`,
`browser_layout`, `browser_exec_js`, etc.) rather than one monolith.
State must stay file/session-inspectable (`dom_tree.pdl`,
`style_tree.pdl`, `layout_tree.pdl`, `display_list.pdl`) even though the
hot path can be memory-first, per section 5's rules.

Current status: page fixtures + session truth + a `browser_exec_js` op
are scaffolded (`projects/wraith-alpha/wraith-projects/wraith-browser`).
Remote fetch, real CSS, a general JS runtime, standards-compliant HTML
parsing, and tabs are **not yet implemented**.

HTML-alignment target vocabulary (durable reference, not yet fully
built): containers (`window`, `div`, `panel`, `header`, `menu`,
`canvas`), media (`img`, `audio`, `video`), controls (`button`,
input-like, `checkbox`, `slider`), attributes (`id`, `src`, `width`,
`height`, `label`, `value`, `checked`, `autoplay`, `loop`) — explicitly
not chasing full CSS/DOM/JS completeness yet. Any future browser or
widget work should inherit the ASCII/GL mirror contract (section 1),
not bypass it.

---

## 8. Long-Term Blue-Sky Ideas (explicitly NOT near-term — preserved so they aren't lost, not a roadmap)

From an early brainstorm, kept only so a future agent knows these ideas
exist and were intentionally deferred, not forgotten: p2p AI users;
RL/diffusion-learning LLMs driving gameplay; full 3D graphics (see
`x0.piececrafts/PIECECRAFT-WRAITH-ARCHITECTURE.md` for what's actually
been built toward this); an HTML-like markup standard for Wraith GL
(section 7 is the concrete start of this); easy game editors with
filesystem manipulation (section 6); a stock-market/"humans on Earth"
civ-sim with R&D; PvP mini-games with collectible pieces; crypto
mining/trading; an explicitly-deferred "soulpen franchise" idea. None of
these should be started without a dedicated planning doc first.
