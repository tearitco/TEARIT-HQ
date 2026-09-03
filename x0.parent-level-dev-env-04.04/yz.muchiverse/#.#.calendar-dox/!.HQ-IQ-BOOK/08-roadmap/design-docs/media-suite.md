# Media Suite — `103.media-studio` → x11-HQ toys

**Status:** spec / not started · **Owner of build:** Grok (foundations) → OpenCode (per-app logic)
**Date:** 2026-09-03 · **Supersedes the app-by-app notes in:** `FORWARD-ROADMAP-2026-09-02.md` §2b,
`GROK-HANDOFF-2026-09-02.md` §2 item 2, `browser-prompting/platform-passes/13.grok-media-studio-continuation-delegation.md`

---

## 1. What this is

`44.xyz.01.00/103.media-studio/` currently holds four standalone **freeglut / OpenGL**
programs, each with its own `*_main.c` pile and its own `button.sh`:

| dir | app | today |
|---|---|---|
| `103.img-editor/` | Muchi Image — Photoshop-shaped raster editor | `ie_main.c`, freeglut, fixed 800×600 |
| `103.3d=blender-clone/` | Muchi Blend — Blender-shaped 3D | `be_main.c`, freeglut, Assimp import |
| `103.daw/` | Muchi DAW — tracker/DAW | freeglut |
| `103.vid-edit/` | Muchi Video — NLE | freeglut |
| `100.tts-point-2-anything/` | TTS point-and-speak | **ignore for now** (owner) |

They are **not** house-shaped: no manager, no `.chtpm`, no shared `khtpm_core_render`,
no `toy.pdl`, so they don't appear under the HQ **toys** cell and don't obey the
theme / opacity / nav conventions every other window follows.

**Goal:** re-house them as CENTROID/khtpm **toys** — one small manager per app that
publishes a `.chtpm` projection rendered by the shared `khtpm_core_render.+x`, the
same shape `co-lab-hai` / `open-hai` / `network-browser` use. This is a **migration,
not a redesign** — the tool set, keymaps and honest MVP limits in each `HOW2_*.md`
carry over as-is.

### Owner decisions locked in (from the Grok session, 2026-09-02/03)

1. **Image + Blender = ONE app.** They were split only for build convenience. The
   merged app has a **2D map and a 3D map** in one viewport with **piececraft-HQ-style
   camera controls** (orbit / pan / zoom / axis views). See §3.
2. Build order: **img(+3d) → daw → vid.** TTS skipped. Network apps
   (`041.pal-chain`, `041.pal-forum`, `044.pal-chat-irc`) **wait** — they have `.c`
   references but aren't nav-friendly yet.
3. **Nothing hardcoded.** Launcher paths, toy identity, viewport substrate paths —
   all read from `.pdl`, never `snprintf`'d into C. (Owner caught a hardcoded path
   in `khtpm_taskbar_manager.c` mid-session — don't repeat it.)
4. **Foundations first** (this doc's §2 skeleton, by Grok), **then OpenCode** fills
   in the actual editor logic, one sub-app per prompt.

---

## 2. Foundation shape — identical for all three toys

Reference implementation to copy: **`44.xyz.01.00/&.hq-apps/co-lab-hai/`**
(manager IPC + projection loop) and **`&.hq-apps/signup-hq/`** (smallest complete
example, added 2026-09-02).

Per toy dir `103.media-studio/<app>/`:

```
<app>/
  toy.pdl                     # identity: SECTION|title|<Name>  SECTION|launch|button.sh
  button.sh                   # gold-std launcher (see below)
  build.sh                    # gcc -std=c11 -Wall -Wextra -Wno-format-truncation -O2
  <app>.chtpm.bootstrap       # checked-in seed; has the <module> tag
  <app>.chtpm                 # runtime projection (seeded from bootstrap, git-ignored churn)
  <app>.css                   # colors only — NO window{width/height}
  ops/
    <app>_manager.c           # the only new C; ~300 lines
    <app>_action.sh           # writes "verb:value" to #.desktop/<app>/request.txt
    +x/<app>_manager.+x       # build output
```

### 2.1 `toy.pdl`

```
SECTION      | KEY    | VALUE
------------------------------
SECTION      | title  | Muchi Image
SECTION      | launch | button.sh
```

The taskbar scanner (`khtpm_taskbar_manager.c:toys_scan_one_root`, already scans
`house_root`, `@.apps`, **and `103.media-studio/`**) picks this up automatically —
presence of `toy.pdl` is the whole contract. `livedesk:open-toy:<fullpath>` runs
`launch`. **No taskbar-manager edit needed** once `toy.pdl` exists.

### 2.2 `button.sh`

Model on `&.hq-apps/signup-hq/button.sh`:

- resolve `HOUSE_ROOT` by walking up to the dir containing `44.xyz.01.00` (or take
  `$2` from the toy launcher),
- `pkill -x "<app>_manager.+x"` to clear a stale instance,
- restore `<app>.chtpm` from `<app>.chtpm.bootstrap` if the `<module>` tag is gone,
- `setsid nohup "$RENDER_BIN" "$HOUSE_ROOT" "$CHTPM" >/dev/null 2>&1 &`
  where `RENDER_BIN` = the shared `*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x`.

`build.sh` compiles `ops/<app>_manager.c` only; the renderer is already built by the
strip build.

### 2.3 `<app>_manager.c` skeleton

Copy `signup_hq_manager.c` verbatim and rename. Contract that must survive the copy:

- `argv[1]` = `house_root` (nothing else hardcoded); derive
  `g_pkg = <house>/103.media-studio/<app>`, `g_out = <pkg>/<app>.chtpm`,
  `g_state = <house>/#.desktop/<app>`, `g_req = <state>/request.txt`.
- **Loop:** `handle_request()` → update model → `write_projection()` (atomic
  `.tmp`+`rename`, only when content changed vs `g_last`) → `if (!parent_alive()) break;`
  → `nanosleep(150ms)`.
- `parent_alive()` reads `<pkg>/module_parent.pid` (renderer writes its own pid there
  before forking the module); returns 1 if the file is absent.
- **Projection is sidebar+panel** — `<window><page name="main"><sidebar>…</sidebar><panel>…</panel></page></window>`.
  A bare `<panel>` with no `<sidebar>` sibling is the one shape that renders empty
  (learned the hard way on signup-hq). Menus/tool-strip/outliner go in `<sidebar>`,
  the canvas/viewport placeholder in `<panel>`.
- **No `<item action="CLOSE">`** — the window chrome already draws an `x`.
- `<cli_io action="'…/ops/<app>_action.sh' 'verb'"/>` → renderer runs
  `<app>_action.sh verb <pkg> <house> <typed_value>` (argc 4).
  `<item action="'…' 'verb'"/>` → argc 3.

**Grok's deliverable stops here:** all three managers compile, launch from the toys
cell, show a titled sidebar+panel window with the menu labels and a "viewport goes
here" placeholder panel, self-exit when the renderer dies. No editing yet.

---

## 3. The merged Image+3D viewport (the one real design question)

**Recommendation: ONE app, ONE viewport, a 2D⇄3D mode toggle — not two apps, not two windows.**

- **Substrate:** reuse the house's existing software 3D/2D grid renderer used by
  piececraft-HQ's board (`&.widgits/board-viewer/`, `bv_render_3d.c` — the same one
  behind civ-txt / tactics-txt / aomorai-editor / piececraft-hq / piececraft-xyz).
  **Do not** pull freeglut/OpenGL into a khtpm window — the renderer is 2D X11
  blitting; an in-process GL context fights it. board-viewer already solves
  orbit/pan/zoom/ortho-views in software.
- **2D mode** = the `Z=0` slice of the same grid: a flat paint plane, checkerboard
  under transparency, layers 1–6. This *is* Muchi Image's canvas.
- **3D mode** = the full grid with meshes/voxels, outliner, G/R/S transforms. This
  *is* Muchi Blend's viewport.
- **Camera verbs** mirror piececraft-HQ exactly (same key names, same
  `camera.pdl`-style state file) so the muscle memory transfers: MMB orbit,
  Shift+MMB pan, wheel zoom, `1/3/7` front/side/top, `.` frame-selected, `Z`
  wireframe.
- **Paths from `.pdl`:** the viewport substrate binary path and the camera-state
  file path go in a `<app>/viewport.pdl`, resolved by the manager against
  `house_root` — never `snprintf`'d.

Split only if OpenCode hits a hard wall making the 2D paint tools coexist with the
3D transform gizmo in one input router — and even then, split the *input mode*, not
the window.

---

## 4. Per-app scope for OpenCode (after foundations land)

One prompt per row. Each starts from the compiling skeleton and the matching
`HOW2_*.md` as the acceptance checklist.

| # | App | Bring across from `HOW2` | New for the house |
|---|---|---|---|
| 1 | **Muchi Image+3D** | B/E/G/R/I/H tools, `[ ]` size, `X` swap, layers 1–6, undo-per-stroke, Ctrl+S export PNG, drop PNG/JPG → layer; **and** Blend's G/R/S, X/Y/Z constrain, wireframe, `.obj`/`.fbx` import, outliner | 2D⇄3D toggle on one board-viewer viewport; piececraft camera verbs; tools/layers/outliner in `<sidebar>` |
| 2 | **Muchi DAW** | (read `103.daw/HOW2_DAW.md`) tracker grid, transport, per-track vol/mute, export | pattern grid as a `<scrolllist>`/`<row>` projection; transport as `<item>` actions |
| 3 | **Muchi Video** | (read `103.vid-edit/HOW2_VIDEO.md`) timeline, clip trim, ffmpeg export | timeline in the panel; clip list in the sidebar; ffmpeg only on export, never on scrub |

Standing limits from every `HOW2`: main loop always sleeps (no busy-spin);
ffmpeg/Assimp only on import/export, never on the paint/scrub/playback path;
composite + upload only when dirty; idle ≤ 8 fps.

---

## 5. Definition of done

- **Foundations (Grok):** `toy.pdl` + skeleton for all 3; each opens from the HQ
  toys cell as a themed sidebar+panel window, obeys opacity/theme live, self-exits
  with the renderer. `git grep` shows no new hardcoded absolute or house-relative
  path in the managers — all via `.pdl`.
- **Per-app (OpenCode):** the corresponding `HOW2_*.md` "Try" list is reproducible
  inside the khtpm window; the old `*_main.c` freeglut binary can be deleted.

## 6. Sources

- `44.xyz.01.00/103.media-studio/*/HOW2_*.md`, `*/button.sh`
- `44.xyz.01.00/&.hq-apps/co-lab-hai/`, `&.hq-apps/signup-hq/` (manager pattern)
- `44.xyz.01.00/&.widgits/board-viewer/ops/bv_render_3d.c` (viewport substrate)
- `44.xyz.01.00/@.apps/piececraft-hq/` (camera verbs, `toy.pdl`)
- `khtpm_taskbar_manager.c:toys_scan_one_root / livedesk_build_toys_menu` (toy discovery)
- `44.xyz.01.00/#.desktop/livedesk_launchers.pdl` ("read the target from a .pdl" rule)
