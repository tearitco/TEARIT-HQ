# media-studio → khtpm toys (img+3D, DAW, video)

**Status:** design + skeleton kickoff — **next agent starts at §9**  
**Date:** 2026-09-03  
**Branch:** `chtpm-var-substitution`  
**For:** a reviewing/implementing agent (OpenCode or otherwise)

This replaces the unfinished “foundations” pass in `grok-frozen-s3.md`.
It is **not** a glut rewrite of `ie_main.c`. It is a **house-spec
migration**: static xhtpm + projector + `khtpm_core_render.+x`, same
class as db-hq-pal / the 2026-09-03 strip retarget.

---

## 0. Owner decisions (locked)

| decision | answer |
|---|---|
| Order | **img+3D first**, then DAW, then video. **Ignore TTS.** |
| 2D vs 3D | **One toy, two views** — not two apps. They were split only for glut ease. |
| Network apps | **Wait.** They already have `.c` but are nav-friendly later. |
| Paths | **No hardcoded house paths in C.** Launch, scan roots, keybinds, export dirs come from `.pdl` / `${HOUSE}` / `${PKG}`. |
| Layout | **Static `.xhtpm`.** Manager writes `state/ui.txt` only. Never regenerate the template (strip `publish_live_chtpm` was that bug). |
| Renderer | Shared `khtpm_core_render.+x`. **No `g_is_media_*`.** |
| Projector | `.pal` via `prisc+x` or compiled `.+x`. **Never bash** as the projector. |
| Old glut | Keep `103.media-studio/103.*` as **reference + rollback** until the khtpm window is signed off. Do not grow `ie_main.c` / `be_main.c` / `daw_main.c` / `ve_main.c`. |

---

## 1. What exists (code-checked)

Live tree: `44.xyz.01.00/103.media-studio/`

| dir | today | HOW2 |
|---|---|---|
| `103.img-editor` | `ops/ie_main.c` + freeglut, `button.sh r` | Photoshop-shaped: tools, 800×600 canvas, 6 layers |
| `103.3d=blender-clone` | `ops/be_main.c` + glut, orbit MMB | G/R/S, outliner, .obj drop |
| `103.daw` | `ops/daw_main.c` | arrangement + piano roll, no WAV |
| `103.vid-edit` | `ops/ve_main.c` | iMovie-ish timeline, demo mp4s |
| `100.tts-point-2-anything` | scripts | **out of scope** |
| `shared/chtpm_nav_mock.c` | visual-only `[>]` bar on glut | throwaway once xhtpm has real nav |

Toys cell (`livedesk_build_toys_menu`) scans **only**:

- `house_root/<child>/toy.pdl`
- `house_root/@.apps/<child>/toy.pdl`

Nested `103.media-studio/103.img-editor` **will not appear**. New toys
live under **`@.apps/<name>/`** with `toy.pdl`. Launch is
`sh "<path>/button.sh" run` (`livedesk:open-toy:`).

`103.media-studio/` stays as the glut originals.

---

## 2. Target: three toys

| toy dir | title | source of truth for behavior |
|---|---|---|
| `@.apps/media-canvas/` | Media Canvas (2D+3D) | HOW2_IMAGE.md + HOW2_BLEND.md |
| `@.apps/media-daw/` | Media DAW | HOW2_DAW.md |
| `@.apps/media-vid/` | Media Video | HOW2_VIDEO.md |

Each:

```
button.sh          # `run` (toys) OR `$HOUSE` (hq-apps style)
<name>.xhtpm       # STATIC, class="media-canvas" etc. (not a g_is_ gate)
<name>.css
toy.pdl            # title + launch=button.sh  (paths relative)
state/ui.txt       # projector output, key=value
state/paths.pdl    # export dir, media dir, keybinds file — NOT in C
pal/<name>.pal  OR ops/<name>_projector.c
ops/<name>_action.sh
```

Gold-std launch (db-hq-pal / co-lab-hai):

```
khtpm_core_render.+x  "$HOUSE"  "$HERE/<name>.xhtpm"
```

`<module>` starts the projector **or** `button.sh` starts `prisc+x` like
db-hq-pal. Closing the window must stop the projector (module child
preferred).

---

## 3. Media Canvas — 2D and 3D in one window

### 3.1 Why one app

Same document: layers, selection, undo, File import/export, drop.
2D is an orthographic view of a stack of bitmaps. 3D is the same
objects (or extra meshes) through a camera. Splitting them duplicates
File/layers/nav and fights the owner’s “they were only separate for
ease.”

### 3.2 Chrome (xhtpm)

```
<tabbar>  2D | 3D          view=  in ui.txt, show="${is_2d}" / show="${is_3d}"
<tabbar> or item strip     tools (from paths.pdl / ui.txt, not hardcoded enums in the renderer)
<panel id="canvas">        sprite="${canvas_sprite}"   ← dirty PNG the projector writes
<panel id="outliner">      <repeat> layers or objects
<footer>                   tool, size, zoom, fps
```

Nav: generic `nav_index`, `[>]` / `[^]`. Tool strip is numbered items.
Canvas itself is **not** a pile of nav cells; it is one focusable
region (digit-jump lands on it; pointer events go to the manager via
history / MOUSE_EVENT like other HQ windows).

### 3.3 Canvas pixels (do not invent `g_is_img`)

khtpm `draw_elem` already blits `sprite=`. Projector/manager composites
**into `state/canvas.png`** (or ppm) when dirty, sets
`canvas_sprite=state/canvas.png` in ui.txt, content-hash reparse.

- **2D:** same composite as today’s img editor (layers, brush) — logic
  in the **projector/manager C**, not in `khtpm_core_render.c`.
- **3D:** first cut may software-rasterize a simple mesh+grid to that
  **same PNG** (orbit camera). If that is too weak, spawn the existing
  **board-viewer** widget (`OPEN_BOARD_WIDGET` / piececraft) as a
  sibling for the viewport only — chrome stays khtpm. **Do not** add
  `g_is_pchq_board` clones.

### 3.4 Camera (piececraft-shaped, data-driven)

Do **not** hardcode WASD in the renderer. `state/keybinds.pdl` (copy
shape from piececraft `pieces/system/keybinds.txt`):

```
# code=verb
119=CAM_FWD          # w
97=CAM_BACK         # a  (example — fill from a real table)
...
```

Verbs the canvas manager understands: `CAM_ORBIT`, `CAM_PAN`,
`CAM_DOLLY`, `CAM_YAW`, `VIEW_FRONT`, `VIEW_SIDE`, `VIEW_TOP`, plus
img tools `TOOL_BRUSH`, … Blender HOW2: MMB orbit, Shift+MMB pan,
wheel dolly; piececraft: q/e yaw, 1/3/7 views. **Map in the pdl**,
not in `khtpm_core_render.c`.

### 3.5 Paths.pdl (required)

```
SECTION | KEY        | VALUE
SECTION | export_dir | pieces/apps/player_app
SECTION | media_dir  | media
SECTION | keybinds   | state/keybinds.pdl
SECTION | canvas_w   | 800
SECTION | canvas_h   | 600
SECTION | max_layers | 6
```

All relative to `${PKG}` (the toy dir) unless they start with `/`.
`${HOUSE}` is the renderer built-in. **No `/home/no/...` in source.**

---

## 4. DAW then video (later toys)

Do not start these until Media Canvas File + 2D canvas + 3D orbit
stubs are clickable from **HQ → toys**.

**DAW xhtpm regions** (from HOW2 + nav mock slots): File, Transport,
Channel strip, Arrangement `<repeat>` of tracks, Piano roll,
Mixer `show="${mixer_open}"`. Audio engine stays in a manager `.+x`
(today’s `daw_main.c` logic extracted, glut UI deleted).

**Video:** File, Transport, Preview sprite, Inspector, Timeline
`<repeat>` of clips. ffmpeg **only** on import/export, never on the
play loop (HOW2 CPU budget).

Shared `103.media-studio/shared/media_drop_path.c` can move into
`&.widgits/_shared-lib` if both canvas and video need drops — not a
copy-paste into the renderer.

---

## 5. What you must NOT do

1. New `g_is_img` / `g_is_daw` / `g_is_blend` in `khtpm_core_render.c`.
2. Rewrite `*.xhtpm` every tick.
3. Hardcode `/home/no/...` or `103.media-studio/...` in C.
4. `git add -A`. No `#.desktop`, pids, glut receipts, export.png.
5. Grow `ie_main.c` / `be_main.c` as the product.
6. Put the new toy only under `103.media-studio/` (toys scan will miss it).
7. Bash projector.
8. Compact `g_nav[]` for tool panels — `HANDOFF-scope-nav-and-chtpm-port.md` §9.
9. Network pal-chain/forum/irc in this workstream.

---

## 6. Kickoff skeleton (this commit)

`@.apps/media-canvas/` :

- `toy.pdl` — title Media Canvas, launch `button.sh`
- `button.sh` — `run` derives HOUSE as `../..`; launches renderer on
  `media-canvas.xhtpm`
- `media-canvas.xhtpm` — 2D/3D tabs, stub labels from `state/ui.txt`
- `state/ui.txt` — checked-in defaults so the window opens without a
  projector on day one
- `state/paths.pdl` — export/media/keybinds relative paths
- `state/keybinds.pdl` — camera + tool verbs (fill as features land)

DAW/video dirs: `@.apps/media-daw/` and `@.apps/media-vid/` (starters,
HOW2 chrome in xhtpm, stub ui.txt). Glut `103.*` is reference only —
not launched from toys.

Old glut apps: untouched, not registered.

---

## 7. Pass criteria (Media Canvas v0)

- HQ → toys lists **Media Canvas** (toy.pdl under `@.apps/`).
- Window is `khtpm_core_render.+x` + static xhtpm (class not
  `events-hq-window` / `db-hq`).
- Switching 2D/3D tabs changes `show=` content via ui.txt, **not** a
  new chtpm file.
- `media-canvas.xhtpm` mtime stable while using it.
- No house absolute paths in the toy’s C/pal (none in v0).
- Digit nav on tools/tabs; Esc does not immediately need a glut kill.

v1 (next): canvas.png updates on a brush stroke; 3D tab orbits a demo
mesh into the same sprite; File export uses `paths.pdl` `export_dir`.

---

## 8. Review checklist

- [ ] One canvas toy, not img + blender as two toys
- [ ] Toys live in `@.apps/` with `toy.pdl`
- [ ] Static xhtpm + ui.txt
- [ ] No new renderer mode flag
- [ ] Paths in pdl
- [ ] Glut originals not deleted yet
- [ ] TTS / network apps not in this PR

---

## 9. Next agent — read this before writing code

This section is the **how**. §§0–8 are the locked product decisions.
Do not start from `ie_main.c`. Do not invent a second renderer.

### 9.1 Mandatory reading (house gold std — files, not summaries)

Read **in this order**, in full, then come back:

1. `.claude/skills/khtpm-house-standards/SKILL.md`  
   Incident: an agent rewrote a dead `*_render.c` while `button.sh`
   already launched `khtpm_core_render.+x`. **Always grep `button.sh`
   first.** Adopted answer: no `g_is_<app>`, no plugin `.so`, no
   linking to share UI; manager process + generic renderer;
   `reparse_chtpm_if_changed()` + generic `<cli_io>` + `XGrabKeyboard`
   while armed.

2. `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/CENTROID_GOLD_STD.md`
   (adopted 2026-08-31). Core rule: **one `Elem` tree** (`x/y/w/h` +
   `CssStyle`) is the only UI truth. X11/ASCII are thin walks of that
   tree. **Business logic does not live in `khtpm_core_render.c`.**
   Also: read Tier 1 (`khtpm-generic-dispatch-design.md` **the file
   itself**, `TPMOS-COMPLIANCE-DEBT.md`) before touching renderer C.

3. `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CHTPM-ARCHITECTURE-FIX.md`
   Layout vs data. Human authors `.xhtpm`/`.chtpm`. Manager writes
   **key=value** (`state/ui.txt`). Parser `${var}` + `<repeat>` +
   `show=`. **Never regenerate the template every tick.**  
   Note: that doc’s “khtpm has no substitute_vars” is **stale**. On
   this branch `kh_substitute_vars` / `kh_load_vars_multi` /
   `kh_expand_repeats` **already exist** in
   `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (~L877–1247).
   `reparse_chtpm_if_changed()` content-hashes the vars file so
   identical rewrites do not rebuild the tree.

4. Working copy of that architecture:  
   `&.hq-apps/db-hq-pal/` — `dashboard.xhtpm` `vars="state/ui.txt"`,
   `<module>` / `button.sh` starts `khtpm_core_render.+x` + `prisc+x`
   on `pal/dbhq_projector.pal`. **class="db-hq-pal"`** so `g_is_db_hq`
   stays off. Copy this launch shape.

5. Strip retarget (same bug you must not repeat):  
   `09-appendix/PROGRESS-strip-static-layout.md` +
   `09-appendix/HANDOFF-scope-nav-and-chtpm-port.md` **§9**.  
   Taskbar used to `write_small_file` entire
   `#.desktop/strip_header.chtpm` (`publish_live_chtpm`). Now static
   `khtpm_strip_*.xhtpm` + `#.desktop/strip_ui.txt`. Media toys must
   stay on the **ui.txt** side.

6. Nav badges: same HANDOFF §9. Do not compact `g_nav[]`. Draw copies
   have no `parent`. Edit `_shared-lib/khtpm_draw_core.c` (ops copy is
   overwritten by `build_core_render.sh`). Rebuild ≠ live
   (`/proc/pid/exe` → `(deleted)`).

7. `&.widgits/_shared-lib/system/string-ops.md` if you write `.pal`.

### 9.2 Gold std restated for these three toys

| layer | owns | does not own |
|---|---|---|
| `*.xhtpm` | regions, `${var}`, `<repeat>`, `show=`, `action=` | pixel paint, audio, ffmpeg |
| `state/ui.txt` | key=value the template binds | XML tags |
| `state/paths.pdl` | export/media/keybinds **relative** to `${PKG}` | `/home/no/...` |
| `state/keybinds.pdl` | keycode → verb | C `if (key=='w')` in the renderer |
| `ops/*_action.sh` | click → rewrite ui.txt / tell manager | compiling IR, glut |
| projector `.pal` or `*_projector.c` | dirty canvas.png, track list, playhead | `Elem` construction |
| `khtpm_core_render.+x` | parse, layout_sidebar_panel, nav, sprite blit, frame dump | `g_is_daw` |

Launch (toys): `livedesk:open-toy:` → `sh "<abs>/button.sh" run`.  
`button.sh` derives `HOUSE` as `$(cd "$HERE/../.." && pwd)` from
`@.apps/<toy>/`. Then:

```
khtpm_core_render.+x  "$HOUSE_ROOT"  "$HERE/<name>.xhtpm"
```

`vars="state/ui.txt"` resolves against **`g_package_dir`** = directory
of the xhtpm (see `kh_find_vars_attr`). That is correct for these toys.
(`#.desktop/...` vars paths resolve against `g_house_root` — strip
only.)

Window **class** is `media-canvas` / `media-daw` / `media-vid`. Those
strings must **never** be added to the `g_is_*` detection loop in
`main()` (`khtpm_core_render.c` ~17860). Generic sidebar+panel path
already handles `<tabbar>` + `<sidebar>` + `<panel>` + `<scrolllist>`
+ `<repeat>` (`layout_sidebar_panel`).

`onclick` and `action` are the **same** `Elem` field. Emit one.

`KH_VAR_VALUE` is 2048. Lists = `<repeat>` + `track_0_text=...`, not
one giant `${arrangement_html}`.

Frame dump for tests:
`44.xyz.01.00/#.desktop/entity_menu_frame_<pid>.txt`
(tag\|id\|class\|label\|…\|nav_index\|…). Inject keys:
`#.desktop/entity_menu_history/<pid>.txt` (`KEY_PRESSED: 201` Down,
`13` Return, `27` Esc). First `stat` of history seeks to EOF — truncate
then wait ~1.5s before append (HANDOFF-scope-nav §4).

Git: **never** `git add -A`. Stage `@.apps/media-*` and this doc.
No `#.desktop`, pids, `103.media-studio` glut receipts, export png/mp4.

### 9.3 What is already on disk (do not recreate)

| path | role |
|---|---|
| `@.apps/media-canvas/` | toy.pdl **Media Canvas**, xhtpm 2D/3D tabs + tools sidebar + outliner `<repeat>`, stub action.sh |
| `@.apps/media-daw/` | toy.pdl **Media DAW**, xhtpm File+transport+channel+arrangement+piano+mixer `show=` |
| `@.apps/media-vid/` | toy.pdl **Media Video**, xhtpm File+transport+inspector+preview+V1/V2/A1/A2 |
| `103.media-studio/103.*` | glut **reference only**. HOW2_*.md is the behavior bible. **Not** toys. Do not register `toy.pdl` there (scan would miss nested dirs anyway). |

Toys scan (`khtpm_taskbar_manager.c` `livedesk_build_toys_menu` /
`toys_scan_one_root`): house_root children **and** `@.apps/` children
that contain `toy.pdl`. **Live scan on HQ→toys open** — no extra
registry. Titles: Media Canvas, Media DAW, Media Video.

### 9.4 Remaining work (ordered)

**A. Verify the three windows open (do this first)**

```
HOUSE=…/44.xyz.01.00
sh "$HOUSE/@.apps/media-canvas/button.sh" run
sh "$HOUSE/@.apps/media-daw/button.sh" run
sh "$HOUSE/@.apps/media-vid/button.sh" run
```

Confirm `/proc/<pid>/cmdline` is `khtpm_core_render.+x … <name>.xhtpm`.
Read `entity_menu_frame_<pid>.txt`. xhtpm **mtime must not move**.
Clicks on 2D/3D, Play, Mixer should only change `state/ui.txt`.
If parse fails: `vars=` path, empty `${}` collapsing attributes,
`show=""` dropping nodes (`show=` drops on `""`/`0`/`false`).

**B. Media Canvas v1 — pixels without a new renderer mode**

1. Compiled manager `ops/media_canvas_manager.c` (or `.pal` if you can
   write a PNG from PAL — you almost certainly want C here).
2. Wire as `<module src="ops/+x/media_canvas_manager.+x"/>` so window
   close kills it (`launch_module` in `khtpm_core_render.c`). If
   generic launch only takes the **first** `<module>`, start the
   manager from `button.sh` like db-hq-pal starts `prisc+x`, and keep
   **zero** `g_is_` flags.
3. Composite layers → `state/canvas.png`. Set `canvas_sprite=state/canvas.png`
   in ui.txt. Template already has a panel; add
   `<item sprite="${canvas_sprite}">` or `sprite=` on a `text`/`item`
   (`draw_elem` already blits `e->sprite`).
4. Pointer: `MOUSE_EVENT` in entity_menu_history (same as HQ windows).
   Map to canvas coords in the **manager**. Brush/eraser/fill from
   `state/keybinds.pdl` + current `tool=` in ui.txt.
5. 3D tab: same PNG, camera from keybinds (orbit/pan/dolly). Optional
   later: board-viewer sibling (`OPEN_BOARD_WIDGET`) — chrome stays
   khtpm. **Forbidden:** `g_is_pchq_board` copy, glut `be_main.c` as
   the product.
6. Export: read `state/paths.pdl` `export_dir` relative to `${PKG}`.
   ffmpeg on export only (HOW2_IMAGE CPU budget).

Reference behavior: `103.media-studio/103.img-editor/HOW2_IMAGE.md`,
`103.3d=blender-clone/HOW2_BLEND.md`. Port **behavior**, not the glut
main loop.

**C. Media DAW v1 — engine behind the existing xhtpm**

Layout is already in `media-daw.xhtpm`. Missing: sound.

1. Extract synth/sequencer from `103.daw/ops/daw_main.c` into
   `media-daw/ops/media_daw_manager.c` (**no glut**, no nav mock).
2. Projector/manager writes `n_tracks`, `track_N_text`, `lcd`, `bpm`,
   `n_notes` when the sequence changes.
3. Transport actions already call `media_daw_action.sh` — upgrade that
   to talk to the manager (file IPC: `state/action.txt` then ui.txt),
   not to rewrite stub lists forever.
4. Mixer `show="${is_mixer}"` already in the template.
5. Paths from `state/paths.pdl`. No VST in v1 (HOW2 honest MVP).

**D. Media Video v1**

Layout is in `media-vid.xhtpm`. Missing: decode/play.

1. HOW2_VIDEO CPU: **do not ffmpeg every frame**. Poster on scrub;
   play = last frame + audio pipe.
2. Preview = `sprite=` of a poster PNG the manager writes (same as
   canvas), not a GL widget.
3. Timeline `<repeat>` already per lane; manager owns
   `timeline.clips` under `paths.pdl` `project`.
4. `media_drop_path.c` — move to `_shared-lib` if you need drops;
   do not paste into `khtpm_core_render.c`.
5. Export V1 concat only (HOW2). ffmpeg on import/export only.

**E. Do not do in this workstream**

TTS; pal-chain/forum/irc; deleting glut trees; deleting `evhq_*`;
LayDoc taskbar retarget; new renderer modes.

### 9.5 Copy-paste launch pattern (db-hq-pal)

File: `&.hq-apps/db-hq-pal/button.sh`

- HOUSE as argv[1] **or** (toys) derived `../..`
- `BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"`
- Build via `build_core_render.sh` if missing
- `setsid nohup "$BIN" "$HOUSE" "$XHTPM"`
- Kill only PIDs whose cmdline contains **this** xhtpm name
- Projector: either `<module>` or a second process; **window close
  must reap it**

CSS: optional `<name>.css` next to the xhtpm if `parse_chtpm` loads
sibling css the way entity-menu does — **grep before inventing**.
db-hq-pal uses `dashboard.xhtpm` + app css; follow that if present.

### 9.6 Pass criteria (v1, each toy)

- Listed under HQ → toys via `toy.pdl`
- `khtpm_core_render.+x` + static xhtpm; **xhtpm mtime stable**
- `state/ui.txt` hash changes on real edits
- No `g_is_media*` / no `g_is_daw` in `khtpm_core_render.c`
- No absolute `/home/no` in toy C/pal/pdl
- Frame dump shows numbered File/transport/tools
- Canvas/DAW/Vid: one real behavior from HOW2 (brush stroke **or**
  playhead **or** poster+play) — not all of HOW2 in one PR
- Explicit git paths; update `09-appendix/PROGRESS-media-studio.md`
  and push `chtpm-var-substitution`

### 9.7 If cut off

Progress log: `09-appendix/PROGRESS-media-studio.md`  
This file is the spec. Do not resurrect `media-suite.md` (missing).
Do not treat `CHTPM-ARCHITECTURE-FIX.md` “not started” as true for
`${var}` — the renderer already substitutes; these toys must **use**
it.
