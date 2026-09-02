# 🎵📸🎬 design-docs.md — Creative-App Design Guide (🧱 CHTPM/PAL House Style)

> 🧭 **Purpose**: A future-agent guide for building three 🎨 creative-application clones using this house's 🧱 CHTPM/PAL architecture.
>
> 🎵 **daw** — a GarageBand clone (multi-track audio workstation)
> 📸 **image_editor** — a Photoshop clone (raster pixel editing)
> 🎬 **video_editor** — an iMovie clone (timeline video editing)
>
> ⚠️ **Prerequisite**: Read `!.HOUSE_STDS.md` in full first. Every ⚡ pitfall and 🏗️ scaffolding rule there applies 1:1 here. This doc only adds the **creative-app domain layers** on top.
>
> 📜 **Golden rules from the house** (re-stated for emphasis — don't skip these):
> - 🧱 Every project = **`system/` + `ops/` + `pal/*.pal` + `pieces/chtpm/layouts/*.chtpm`** + **`default_op.txt`** + **`button.sh`** — see HOUSE_STDS §A.1.
> - ⚡ **`(key - '0') - 1`** is CORRECT for `${piece_methods}` numbered dispatch — never "fix" it (§A.3).
> - 🚫 **`'q'` must never quit** anything (§F.3 — verify in `gl_mirror.c` before binding it).
> - ⚡ **`state_changed.txt`** is NEVER grown by an op — it clobbers focus/arrow nav (§A.6-2, §F.14).
> - ⚡ **KV reads must `strcspn(v, "\r\n")`** — missing strip kills relay/Enter dispatch (§A.6-3, §F.15).
> - ⚡ **`default_op.txt`** must use `name type handler {desc}` rows, NOT bare paths (§F.16).
> - 🏗️ **Copy `system/` binaries** from `014.wsr-pal💸️📌️+2/system/` — wsr-pal's `chtpm_rgb_render` has both on-demand emoji generation AND `MAP3D`/overlay support (§A.4, §F.12). Never blindly swap to mutaclysm's fork.
> - 🧪 **Headless testing**: `NO_GL=1 bash button.sh run-app <root>` + inject keycodes into `interact_relay.txt` (§C, HOUSE_STDS §C/§F.17). **No Python** — all pixel/buffer verification is done in pure C (§0).
> - 🐚 **Shell-out from C** uses `system()` / `popen()` (gcc only) — never invoke a non-existent interpreter. `ops/dump_rgb_png.c` already shells out to real tools via `system()` (HOUSE_STDS §B, §E.5).

---

## §0 — 🏗️ Creative App Common Foundation

All three projects below share this 🧱 CHTPM/PAL structural spine AND a **single shared canvas widget**. Build each project as its own directory tree mirroring HOUSE STDS §D.3 (scaffold copy of `board-viewer`), then spawn the same `&.widgits/canvas-widgit/` from all three.

### 📁 Directory Skeleton (per project)

```
<project>/
├── button.sh                  🎮 launcher (run-app / build / kill / check)
├── default_op.txt             📜 op registry (name type handler {desc})
├── scripts/
│   └── build.sh               🔨 compiles ops/, copies system/ binaries
├── ops/                       💪 C binaries → ops/+x/<name>.+x
│   ├── <proj>_compose_frame.c   🖼️  writes view.txt → current_frame.txt chrome
│   ├── <proj>_menu_input.c      ⌨️  dispatches keys, grows screen_changed marker
│   ├── <proj>_render_media.c    🎨  writes canvas.raw RGBA buffer (mode-flagged)
│   ├── <proj>_media_io.c        💾  read/write/import/export media files
│   └── check_rgba.c             🔍 C-based pixel-sampler for headless verification (§0)
├── pal/
│   └── main_module.pal        🔁  proven idle-loop pattern (HOUSE_STDS §A.7)
├── pieces/
│   ├── chtpm/layouts/<proj>.chtpm  📺 XML-ish screen layouts
│   ├── apps/player_app/             📂 real state, relays, frame buffers
│   ├── display/                     📂 frame_changed / screen_changed / receipts
│   ├── registry/                    📂 fonts, emoji_assets
│   └── sessions/<ts>-<pid>/         🗂️ ephemeral session (HOUSE_STDS §A.2)
└── projects/<proj>/pieces/<id>/     🗂️ dynamically-regenerated piece.pdl
```

### 🎨 The ONE Canvas Widget (shared by all three projects)

Per user directive: **one widget, reused everywhere.** No per-project widget copies.

> `&.widgits/canvas-widgit/` — a **generic RGBA viewer** spawned via `setsid` from ANY host app:
>
> ```
> setsid ... button.sh run-widget <host_project_root> &
> ```
>
> The widget reads **only** two files from the host root (path is the same for every app):
> - `pieces/apps/player_app/canvas.raw` — raw RGBA32 pixel buffer (W×H×4 bytes)
> - `pieces/apps/player_app/canvas.receipt.txt` — `width`, `height`, `bytes_per_pixel=4`, `mode=<flag>` (e.g. `mode=waveform`, `mode=preview`, `mode=canvas`, `mode=piano_roll`)
>
> The widget itself **has no domain logic** — it just uploads `canvas.raw` as a GL texture via `gl_mirror` and blits one quad (the only thing that calls raw GL/GLUT, per HOUSE_STDS §H.7). Each host app's `<proj>_render_media` op decides what to draw into that RGBA buffer.
>
> **Spawning from each host** (all identical pattern):
> ```
> setsid bash &.widgits/canvas-widgit/button.sh run-widget <host_root> &
> ```
> - 🎵 **DAW** spawns it with `mode=waveform` or `mode=piano_roll` (written into `canvas.receipt.txt` by `daw_render_media`).
> - 📸 **Image editor** spawns it with `mode=canvas`.
> - 🎬 **Video editor** spawns it with `mode=preview`.

### ⚡ Common Marker Discipline (HOUSE_STDS §A.6, §F.14)

Every op that touches a marker file MUST follow this exact chart. These bugs have killed real apps in this house — don't repeat them.

| Marker file | Who writes it? | Purpose | 🚫 NEVER |
|---|---|---|---|
| `pieces/display/frame_changed.txt` | `chtpm_parser_pal.c` on every key; YOUR compose ops | Trigger `renderer`/`chtpm_rgb_render` to flush a frame | — (let engine own the per-key grow) |
| `pieces/apps/player_app/state_changed.txt` | **NO OP EVER** | Idle-sync focus restore from `active_gui_index.txt` | Any op growing this → arrows get clobbered back to item 1 (🏠 yahoo-app bug, §F.14) |
| `pieces/display/<proj>_screen_changed.txt` | Your `<proj>_menu_input.c` key-tail ONLY when a real action set a message; OR your module loop when the active piece id genuinely changed | Your PAL loop polls this → compose-on-change | Compose ops, idle-compose, frame renders |
| `pieces/display/rgb_frame_changed.txt` | `chtpm_rgb_render` daemon only | Signals `gl_mirror` a new RGBA buffer is ready | Your own media ops (use canvas.raw + canvas widget, §0) |

> 🔄 **Note**: Since all three projects now use the canvas widget (NOT the `0x01` inline overlay path), you do **not** need the SOH skip-line trick for these apps. The widget reads `canvas.raw` directly and independently. The `0x01` path is only for the board-viewer 3D-overlay use-case (HOUSE_STDS §E.1) — don't use it here.

### 🔁 Proven PAL Module Loop (HOUSE_STDS §A.7 — copy verbatim)

Adapt `<op>` to `<proj>` (`daw`, `image_editor`, `video_editor`). The idle-sync early-returns if the active piece id is unchanged; compose-on-change only; `hit_frame` grows `frame_changed.txt`; `read_history x2, x1` consumes each relayed key exactly once.

```
li x1, 0
drain:  read_history pieces/apps/player_app/interact_relay.txt x2, x1
        beq x2, x0, drain_done
        j drain
drain_done:
li x9, 0; <proj>_menu_input x9; <proj>_compose_frame; hit_frame
read_pos x7, "pieces/display/<proj>_screen_changed.txt"
loop:
  li x9, 0; <proj>_menu_input x9            ; idle-sync, early-return if piece unchanged
  read_pos x8, "pieces/display/<proj>_screen_changed.txt"
  beq x7, x8, check_key                     ; marker UNchanged → poll relay for a real key
  addi x7, x8, 0; j render
check_key:
  read_history pieces/apps/player_app/interact_relay.txt x2, x1
  beq x2, x0, no_key
  <proj>_menu_input x2; j render
no_key:  sleep 30000; j loop
render:  <proj>_compose_frame; hit_frame; sleep 30000; j loop
```

### 🧪 C-Based Pixel Verification (NO PYTHON — pure gcc/C only)

Since this house is `gcc`/`system()`/`popen()` and explicitly **no Python**, all headless pixel verification is done by a tiny C op. Use the existing `ops/dump_rgb_png.c` pattern (HOUSE_STDS §B, §E.5) as your template — it shells out via `system()` to real tools, reads buffers in C with `open()`/`fread()`, never invokes an interpreter.

**The pattern — write a `check_rgba.c` op in each project's `ops/`:**

```c
/* check_rgba.c — open canvas.raw, index pixels by (y*W+x)*4, assert */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    /* read canvas.receipt.txt for width/height */
    FILE *r = fopen("pieces/apps/player_app/canvas.receipt.txt", "r");
    int W = 0, H = 0;
    char line[256];
    while (fgets(line, sizeof(line), r)) {
        line[strcspn(line, "\r\n")] = '\0';        /* ⚡ always strip — §F.15 */
        if (strncmp(line, "width=", 6) == 0)  W = atoi(line + 6);
        if (strncmp(line, "height=", 7) == 0) H = atoi(line + 7);
    }
    fclose(r);

    /* read canvas.raw into buffer */
    FILE *f = fopen("pieces/apps/player_app/canvas.raw", "rb");
    unsigned char *buf = malloc(W * H * 4);
    if (!buf || fread(buf, 1, W * H * 4, f) != (size_t)(W * H * 4)) {
        fprintf(stderr, "check_rgba: buffer read failed\n");
        return 1;
    }
    fclose(f);

    /* sample known coordinates: top-left, center, bottom-right */
    unsigned char *tl = &buf[(0 * W + 0) * 4];
    unsigned char *ct = &buf[((H/2) * W + (W/2)) * 4];
    unsigned char *br = &buf[((H-1) * W + (W-1)) * 4];

    /* write results to a receipt file the .pal loop can read */
    FILE *out = fopen("pieces/display/check_rgba_result.txt", "w");
    fprintf(out, "tl_r=%d tl_g=%d tl_b=%d\n", tl[0], tl[1], tl[2]);
    fprintf(out, "ct_r=%d ct_g=%d ct_b=%d\n", ct[0], ct[1], ct[2]);
    fprintf(out, "br_r=%d br_g=%d br_b=%d\n", br[0], br[1], br[2]);
    fclose(out);
    free(buf);
    return 0;
}
```

Register it in `default_op.txt`:
```
check_rgba int check_rgba.x {index canvas.raw pixels for headless assertion}
```

**Usage in PAL** — append a one-shot verification pass after the main loop's `drain_done`:
```
li x9, 0; check_rgba x9          ; write check_rgba_result.txt
<proj>_compose_frame; hit_frame  ; re-compose so the assertion text shows
```

Then assert by reading `check_rgba_result.txt` in C (another tiny op, or via `system("grep ...")`). This is how every bug fix in HOUSE_STDS §C was verified without an interpreter — direct pixel-buffer indexing in C (§G).

### 🧪 Headless Testing Baseline (HOUSE STDS §C, §F.17)

| Layer | How to inject | What it exercises | ✅ When to use |
|---|---|---|---|
| Parser→Module (full chain) | `[timestamp] KEY_PRESSED: <code>` into `pieces/keyboard/history.txt` | Entire engine + your module loop | Frame/timing tests, full-path regression |
| Module relay only | bare keycodes into `pieces/apps/player_app/interact_relay.txt` | Just your `<proj>_menu_input` + compose | Fast unit-style nav/input tests, no display needed |
| RGBA buffer asserts | `check_rgba` op samples `canvas.raw` by `(y*W+x)*4` | Pixel-level media output correctness | Headless visual regression on waveform / preview / canvas |

> 📌 **Symptom-split**: If a keypress works via one path but not the other, the bug is at their boundary — not "the engine is broken." Identify the layer before debugging (§F.17).
> 📌 **Two test layers don't confuse** (§F.17): keyboard-history injection exercises the full parser→module chain; `interact_relay.txt` injection bypasses the parser and tests only module relay-polling. Symptoms differ — identify the layer first.

---

## 🎵 §1 — daw (GarageBand Clone)

A multi-track 🧱 CHTPM/PAL audio workstation. Tracks list as a numbered menu in CHTPM; the waveform + piano-roll renders into the **shared canvas widget**.

### 📋 Core Domain Concepts

| House concept | DAW mapping |
|---|---|
| `<proj>_compose_frame` | Renders track list (numbered rows), transport state, tempo/time signature, timecode, active clip `[>]` |
| `<proj>_menu_input` | `1`-`9` select track; `q` never quits; `w`/`s` navigate; `[Space]` play/pause; `p` record/play; `t` tap tempo; `n` new track; `d` delete track; `e` edit clip (switches canvas widget to `mode=piano_roll`); `b` back to waveform view |
| `<proj>_render_media` | Writes `canvas.raw` RGBA buffer — waveform strip OR piano-roll grid (depending on `mode` in `canvas.receipt.txt`). Writes playhead cursor position as a colored vertical line |
| `<proj>_media_io` | Reads/writes `.wav` (import via `system("sox ...")` or `system("ffmpeg ...")`), `.ppj` (project: track list, tempo map, clip refs), `.midi` (note data) |
| `${piece_methods}` rows | Track rows: `METHOD | Track 1: Drums (muted) | KEY:2` etc. |
| `<proj>_screen_changed.txt` | Grown ONLY by `daw_menu_input` key-tail on mode/tempo/playback state change |
| Shared canvas-widgit `mode=` | `mode=waveform` (default) or `mode=piano_roll` — written to `canvas.receipt.txt` by `daw_render_media` |

### 🏗️ Scaffolding Steps

1. **Copy** `&.widgits/board-viewer` → `daw/` as your seed.
2. **Edit `default_op.txt`**: register `daw_menu_input`, `daw_compose_frame`, `daw_render_media`, `daw_media_io`, `check_rgba`.
3. **State file**: `pieces/apps/player_app/daw_state.txt` — KV store: `tempo=<n>`, `time_sig=<n>/<n>`, `playhead=<ms>`, `recording=<0|1>`, `track_count=<n>`, `edit_mode=<waveform|piano_roll>`. **Must be symlinked in `button.sh`** (§A.2, §F.7).
4. **Canvas buffer**: `pieces/apps/player_app/canvas.raw` + `canvas.receipt.txt` (`width`, `height`, `bytes_per_pixel=4`, `mode=<flag>`). **Symlinked in `button.sh`** (§F.7).
5. **Layout**: `pieces/chtpm/layouts/daw.chtpm` —
   ```xml
   <panel>
     <text label="🎵 DAW  tempo=${tempo}  ${timecode}  REC=${recording}  mode=${edit_mode}" /><br/>
     <text label="Transport: [Space]=play/pause  p=record  t=tap  e=edit  b=waveform" /><br/>
     ${piece_methods}
   </panel>
   ```
6. **Audio**: Route raw audio samples through `daw_render_media` into the `canvas.raw` RGBA buffer as a waveform (one pixel column per audio sample-bucket, height = amplitude). For actual `.wav` playback verification in headless mode, `daw_media_io` shells out via `system("sox input.wav -r 44100 -c 1 -b 16 -t raw - | head -c ...")` — then index the resulting raw PCM in C to assert sample timing. 🧪 Verify playhead position by sampling `canvas.raw` pixel columns at the expected x-coordinate via `check_rgba`.
7. **Spawn the canvas widget** at session start (in `button.sh` or a dedicated op):
   ```
   setsid bash &.widgits/canvas-widgit/button.sh run-widget "$PROJECT_ROOT" &
   ```
8. **Piano-roll** (mode=piano_roll): `daw_render_media` draws a grid (time × notes) into `canvas.raw` — note-on events as colored rectangles, grid lines, current playhead. Mouse clicks in the widget relay back via gl_mirror → host writes note data → re-renders.

### ⚡ DAW-Specific Pitfalls

| # | Pitfall | Fix reference |
|---|---|---|
| P1 | Writing `rgb_frame.raw` directly from an audio waveform renderer → silent revert to stale 2D within 30ms | Use the **shared canvas widget's** `canvas.raw`, NOT `rgb_frame.raw` (§0) |
| P2 | Tempo changes during playback not visible on the waveform → phantom "wrong beat" reports | Grow `<proj>_screen_changed.txt` from `daw_menu_input` key-tail on tempo change; compose-on-change only (§A.6-3) |
| P3 | `daw_state.txt` not symlinked → session-local writes vanish → tempo/playhead resets every restart | Add it to `button.sh`'s explicit symlink list (§A.2, §F.7) |
| P4 | `1`-`9` track selection off-by-one because `${piece_methods}` starts at `KEY:2` internally | `(key-'0')-1` is CORRECT — trust it (§A.3, §F.2) |
| P5 | Record-enable state flickering because `state_changed.txt` pinged on every idle compose | NEVER grow `state_changed.txt` from `daw_compose_frame` (§A.6-2, §F.14) |
| P6 | Piano-roll mode switch doesn't re-render because canvas widget polls `canvas.receipt.txt` but `mode=` field not updated | `daw_render_media` MUST rewrite `canvas.receipt.txt` with the new `mode=<flag>` every render (§F... the receipt+checksum pattern, §B) |

---

## 📸 §2 — image_editor (Photoshop Clone)

A raster pixel editor where the **canvas** renders in the **shared canvas widget**, and the host app provides the tools/layers/undo UI chrome.

### 📋 Core Domain Concepts

| House concept | Image Editor mapping |
|---|---|
| Host `image_editor/` app | Layers panel, tools palette, status bar, color picker, history/brush-size controls — pure ASCII CHTPM UI |
| `<proj>_compose_frame` | Renders layer list as numbered rows; active layer `[>]`; tool name; zoom %; brush size |
| `<proj>_menu_input` | `1`-`9` select layer; `b`=brush, `e`=eraser, `t`=text, `r`=rect, `l`=lasso, `g`=gradient, `c`=crop, `m`=fill; `Ctrl+Z`/`Ctrl+Y` undo/redo; `[`/`]` brush size; `+`/`-` zoom; `Delete`=clear layer; `x`=swap fg/bg |
| Shared canvas-widgit | Generic RGBA viewer — spawned via `setsid`, reads `canvas.raw` + `canvas.receipt.txt` from host root. **This is the same widget the DAW and video editor use** |
| `<proj>_render_media` | Writes `canvas.raw` RGBA buffer (the actual pixel content); writes `mode=canvas` to `canvas.receipt.txt` |
| `<proj>_media_io` | Reads/writes `.png` (via `ops/dump_rgb_png.c`, HOUSE_STDS §B, §E.5), `.ipj` (project: layer list + raw canvas bytes) |
| `<proj>_screen_changed.txt` | Grown ONLY by `image_editor_menu_input` on tool/mode/zoom/layer changes |
| Canvas buffer path | `pieces/apps/player_app/canvas.raw` + `canvas.receipt.txt` (`width`, `height`, `bytes_per_pixel=4`, `mode=canvas`) — **same shared path** (§0) |

### 🏗️ Scaffolding Steps

1. **Seed from** `&.widgits/board-viewer` → `image_editor/`.
2. **Edit `default_op.txt`**: register `image_editor_compose_frame`, `image_editor_menu_input`, `image_editor_render_canvas`, `image_editor_media_io`, `check_rgba`.
3. **Canvas buffer**: `pieces/apps/player_app/canvas.raw` + `canvas.receipt.txt`. **Symlinked in `button.sh`** (§F.7).
4. **Layout**: `pieces/chtpm/layouts/image_editor.chtpm` —
   ```xml
   <panel>
     <text label="📸 Editor  ${tool}  zoom=${zoom}%  brush=${brush}px  fg=${fg} bg=${bg}" /><br/>
     <text label="Layers (active [>]):" /><br/>
     ${piece_methods}
   </panel>
   ```
5. **Input forwarding**: `onClick="INTERACT"` on the canvas area (HOUSE_STDS §D.2) — engages nav mode, relays raw keys into `interact_relay.txt`; the canvas widget reads them directly for pan/zoom while not actively drawing a shape. `active_gui_is_typing.txt` (§D.2) tells you "is the user mid-stroke?" so you can suppress keyboard menu nav during a drag.
6. **Shell-out for PNG I/O**: `image_editor_media_io` uses `system("ffmpeg -i input.png canvas.raw")` (or `system("stb_image ...")` compiled in-C) — always via `system()` in C, never Python. `ops/dump_rgb_png.c` is your reference for the C+system() pattern (§B, §E.5).
7. **Emoji/voxel textures**: The on-demand emoji pipeline (§E.5) gives you a free brush-tip palette — emit 🎨 emoji per brush type and point the texture sampler at `pieces/registry/emoji_assets/<hex>/voxels_16.csv`. Reuse `t_grass`, `t_tree` etc. for texture-fill brushes (§H Index #9). Since the canvas widget just renders `canvas.raw`, texture brushes are pre-rasterized into the canvas RGBA buffer by `image_editor_render_canvas`.
8. **Spawn the canvas widget**: `setsid bash &.widgits/canvas-widgit/button.sh run-widget "$PROJECT_ROOT" &`

### 🖌️ Brush/Stroke Pipeline (host↔widget IPC)

```
User draws mouse (in canvas widget's GL window):
   ↓
widget relays mouse via gl_mirror → writes stroke points to
   pieces/apps/player_app/stroke_points.txt  (x,y,r,g,b,a per line)
   ↓
host's image_editor_menu_input (idle-sync) polls stroke_points.txt,
   applies them to canvas.raw via image_editor_render_canvas,
   grows <proj>_screen_changed.txt → host re-composes →
   canvas widget re-reads canvas.raw on next frame
```
This is file-mediated IPC (HOUSE_STDS §H.2) — no custom shared memory needed.

### ⚡ Image-Editor-Specific Pitfalls

| # | Pitfall | Fix reference |
|---|---|---|
| P1 | Canvas widget's GL window sizing hardcoded → blank window when canvas resized | `canvas.receipt.txt` (not hardcoded dims) is the source of truth (§B) — follow the receipt+checksum pattern |
| P2 | Mouse drag in canvas widget also triggers host menu nav → jumping between layers mid-stroke | Read `active_gui_is_typing.txt` before processing key nav (§D.2) |
| P3 | Canvas pixel writes race a headless `NO_GL=1` test that reads `canvas.receipt.txt` simultaneously | Wrap headless tests in `timeout` + verify no leftover (§C) |
| P4 | Layer deletion reindexes numbered rows but `${piece_methods}` still shows `[N]` stale | Re-generate `piece.pdl` from `image_editor_compose_frame` after every layer add/delete |
| P5 | Undo history (`history.txt`) written as bare paths in `default_op.txt` → undo no-ops silently | `name type handler {desc}` rows ONLY (§F.16) |
| P6 | Brush strokes look "upside-down" vs. expected | The camera basis math has two branches — verify empirically with a known asymmetric marker (§E.3, §C); do NOT trust pure algebra. For 2D canvas this is usually just `y = H-1-y` flip — verify by sampling via `check_rgba` |
| P7 | PNG read/write shells out to a non-existent tool | Use `system("ffmpeg ...")` or compile `stb_image` directly into the C op — check what's installed first (§G: check available libs) |

---

## 🎬 §3 — video_editor (iMovie Clone)

A timeline video editor. Timeline rows in CHTPM; **preview frame** renders in the **shared canvas widget**; export = a long-lived C worker op.

### 📋 Core Domain Concepts

| House concept | Video Editor mapping |
|---|---|
| `<proj>_compose_frame` | Renders timeline track rows as numbered items; project name; timecode; playhead position; selected clip `[>]` |
| `<proj>_menu_input` | `1`-`9` select clip; `Space`=play/pause; `<`,>`=step frame; `J`/`K`/`L`=rewind/play/ffwd; `I`/`O`=trim in/out; `C`=cut/split; `V`=paste; `[`/`]`=blade tool; `+`/`-` track height; `R`=ripple delete; `X`=export |
| Shared canvas-widgit | Generic RGBA viewer — shows preview frame from `canvas.raw`. Same widget as DΘW and image_editor (§0). `mode=preview` in receipt. |
| `<proj>_render_media` | Writes preview frame into `canvas.raw` + `canvas.receipt.txt` (`mode=preview`); decodes playhead frame from `media_cache/` via `system("ffmpeg ...")` |
| `<proj>_media_io` | Reads/writes `.mcpj` (project: clip list + edits + effects chain), transcodes via `system("ffmpeg ...")` into `pieces/apps/player_app/media_cache/` |
| `<proj>_screen_changed.txt` | Grown ONLY by `video_editor_menu_input` on clip selection / trim / cut / mode change |
| `video_editor_export_worker` | Long-lived C op: decodes + composites ALL frames, muxes via `system("ffmpeg ...")` → output `.mp4` |

### 🏗️ Scaffolding Steps

1. **Seed from** `&.widgits/board-viewer` → `video_editor/`.
2. **Edit `default_op.txt`**: register `video_editor_compose_frame`, `video_editor_menu_input`, `video_editor_render_preview`, `video_editor_media_io`, `video_editor_export_worker`, `check_rgba`.
3. **Canvas buffer**: `pieces/apps/player_app/canvas.raw` + `canvas.receipt.txt` (`width`, `height`, `bytes_per_pixel=4`, `mode=preview`). **Symlinked in `button.sh`** (§F.7).
4. **Layout**: `pieces/chtpm/layouts/video_editor.chtpm` —
   ```xml
   <panel>
     <text label="🎬 Editor  ${project_name}  tc=${timecode}  mode=${mode}  playhead=${playhead_ms}ms" /><br/>
     <text label="Keys: Space=play  JKL=rewind/play/ffwd  I/O=trim  C=cut  R=ripple  X=export" /><br/>
     ${piece_methods}
   </panel>
   ```
5. **Preview rendering**: While `preview_mode==1`, `video_editor_render_preview` decodes the current playhead frame from `media_cache/<clip>.m4v` via `system("ffmpeg -ss <t> -i <clip> -vframes 1 -f rawvideo -pix_fmt rgba canvas.raw")` into `canvas.raw`, updates `canvas.receipt.txt` with `mode=preview`. The compose op does NOT need the `0x01` skip-line trick since the canvas widget reads `canvas.raw` directly (§0).
6. **Spawn the canvas widget**: `setsid bash &.widgits/canvas-widgit/button.sh run-widget "$PROJECT_ROOT" &`
7. **Export**: `X` key triggers `video_editor_export_worker` (via `default_op.txt` registration). This runs `system("ffmpeg -f concat ... -c:v libx264 output.mp4")` through the PAL module. Since it's long-lived, spawn it with `setsid` so `timeout`-wrapped tests don't cascade-kill it (§F.11).

### 🧬 Three-Layer Media Model (🧠 key architecture)

Video editing has **three distinct layers** — don't collapse them:

```
Source clips  (read-only .mp4/.mov on disk)
     ↓  ffmpeg transcode via system() (degrades-to-raw only for in/out-trim)
Edited sequence  (timeline.clips: id, src, in, out, track, effects)
     ↓  video_editor_render_preview (decodes + composites ONE frame → canvas.raw)
Preview frame  (canvas.raw — what the canvas widget shows)
     ↓  video_editor_export_worker (decodes + composites ALL frames, muxes final)
Export  (system("ffmpeg ...") → final .mp4)
```
The preview and export paths share the **same decode+composite logic** — factor it into a single C function both ops link against. Don't hand-roll two versions (HOUSE_STDS §G: read real code before simplifying).

### 🧪 C-Based Headless Export Verification (NO PYTHON)

Since you can't eyeball a live GL preview headless:

1. Inject `X` key via `interact_relay.txt` → kicks off `video_editor_export_worker` as a **backgrounded** PAL op (spawn with `setsid` — §F.11).
2. `timeout`-wrap the whole `button.sh run-app` test (§C, §F.17).
3. After completion, run `check_rgba` against the final exported `.raw` buffer (or re-decode the export's first frame into `canvas.raw` and sample it):
   - `check_rgba` indices `(y*W+x)*4`, samples top-left / center / bottom-right corner pixels.
   - Writes results to `check_rgba_result.txt`.
   - Assert via `system("grep -q 'expected_value' check_rgba_result.txt")` in C.
4. **Cleanup**: `ps aux | grep video_editor` + `kill <pid>` — never `pkill` (§F.9). Verify no leftover processes.
5. **Pixel-level verification technique**: index the raw RGBA buffer directly in C (`buf[(y*W+x)*4]` for R, `+1` for G, `+2` for B), same as the `check_rgba.c` pattern in §0. This is how HOUSE_STDS §C verified a horizontal-mirror bug and an upside-down-camera bug — pure C pixel sampling, no interpreter (§G).

### ⚡ Video-Editor-Specific Pitfalls

| # | Pitfall | Fix reference |
|---|---|---|
| P1 | Preview frame write races `chtpm_rgb_render`'s unconditional daemon re-render → stale 2D chrome over live preview | Use the **shared canvas widget's** `canvas.raw`, NOT `rgb_frame.raw` (§0) |
| P2 | Canvas widget doesn't refresh when preview frame changes | `video_editor_render_preview` MUST update `canvas.receipt.txt`'s `mode=preview` line every render (§B receipt pattern) — this is what signals the widget |
| P3 | `ffmpeg` transcoding inside `video_editor_render_preview` on every idle tick → 60fps decode thrash | Cache decoded GOP to `media_cache/` — only re-decode on playhead seek (grown via `<proj>_screen_changed.txt`) |
| P4 | Export worker spawns as plain `system(cmd) + &` → `timeout`-wrapped test kills the widget's children too | Use `setsid` when spawning export (§F.11) |
| P5 | `J`/`K`/`L` playback keys collide with house's `'q'`-never-quit rule when `K` is also bound to "kill" | Audit every key against `keyboard_input.c` + `chtpm_parser_pal.c` before binding (§F.3) |
| P6 | Ripple-delete shifts clip indices but `${piece_methods}` rows still show old `[N]` → selection off-by-one | Re-generate `piece.pdl` from `video_editor_compose_frame` after every structural edit |
| P7 | Export mux silently produces black frames (verified wrong via pure algebra on the decode math) | Verify frame sampling empirically via `check_rgba` C-op pixel-read of exported frame corners (§E.3, §C) — index `(y*W+x)*4`, assert non-zero RGB |
| P8 | Cross-test contamination: two headless runs fight over `timeline.clips` | Kill stale sessions before every test (§F.8, §C) |
| P9 | `system("ffmpeg ...")` call has wrong working directory → export writes to wrong session path | All `system()` calls in ops use the session's `pieces/` root — verify with `getcwd()` + debug print, never assume CWD (§G: verify by direct read) |

---

## 📑 §4 — Quick Cross-Reference Index (into `!.HOUSE_STDS.md`)

| You need this | Go re-read |
|---|---|
| Why numbered items dispatch with `(key-'0')-1` | §A.3 — DO NOT "fix" it for `${piece_methods}` rows |
| The idle-sync loop shape (copy-paste ready) | §A.7 |
| Marker discipline (which op grows which file) | §A.6 |
| Camera basis math (two-branch) | §E.3 |
| The `0x01` SOH overlay skip-line trick | §E.1 (only for board-viewer 3D overlay, NOT for canvas widget) |
| Camera key scheme (q/e/r/t/w/a/s/d/c/v/f, modes 1-4) | §E.2 |
| Real raymarch DDA loop (port from mutaclysm) | §E.4 |
| On-demand emoji → voxel texture pipeline | §E.5 |
| Where shared `system/` binaries live (copy from) | §H Index #9 → `014.wsr-pal💸️📌️+2/system/` |
| Why NOT to use mutaclysm's `chtpm_rgb_render` fork | §A.4, §F.12 |
| Headless test harness (`NO_GL=1` + relay inject) | §C, §F.17 |
| Process-group / `setsid` spawning rule | §D.1, §F.11 |
| KV-read trailing-newline strip | §A.6-3, §F.15 |
| `default_op.txt` row format | §F.16 |
| **C-based pixel verification (no Python)** | §B (dump_rgb_png.c), §C (pixel sampling), §G (verify empirically) |
| Widget spawn / focus / real-time sync | §D.1–D.3, §H.2 |
| `onClick="INTERACT"` reserved-string special-casing | §D.2 |
| Widget convention doc (two launch profiles) | §H Index #6 → `&.widgits/WIDGIT_BIBLE.md` |

---

## 🚨 §5 — Abort Conditions (STOP before you build)

If ANY of these is true, do not start implementation — resolve first:

1. 🚫 You cannot `grep -n "blit_overlay\|MAP3D" system/chtpm_rgb_render.c` and see BOTH strings in `014.wsr-pal💸️📌️+2/system/` — wsr-pal's copy is mandatory (§F.12).
2. 🚫 `default_op.txt` is bare paths — rewrite as `name type handler {desc}` rows (§F.16).
3. 🚫 Any op touches `state_changed.txt` — remove it (§F.14).
4. 🚫 KV reads don't `strcspn(v, "\r\n")` — add the strip (§F.15).
5. 🚫 The media buffer (`canvas.raw`, `timeline.clips`, `daw_state.txt`) is not in `button.sh`'s symlink list — add it (§F.7).
6. 🚫 `'q'` is bound to quit anywhere — rebind it (§F.3).
7. 🐍 Any `python`/`python3` invocation in a C `system()` call or ops build script — remove it. This house is gcc + `system()` only (§0, §G).
8. 🚫 You create a SEPARATE widget directory per project instead of reusing `&.widgits/canvas-widgit/` — consolidate (§0 user directive).

> 🎵📸🎬 **Now go build. One project at a time. Headless-verify in C before you trust the math.** 🔧✅