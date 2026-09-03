# media-studio → khtpm toys (img+3D, DAW, video)

**Status:** design + skeleton kickoff  
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

DAW/video dirs: **not** created until canvas is signed as a window.

Old glut apps: untouched.

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
