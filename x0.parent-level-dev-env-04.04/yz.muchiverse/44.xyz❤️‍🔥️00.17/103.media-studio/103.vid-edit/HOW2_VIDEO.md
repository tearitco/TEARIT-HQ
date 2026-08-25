# Muchi Video Editor — Phase 1 try card

## Run
```bash
cd 103.media-studio/103.vid-edit
sh button.sh r
```
Stop: **Esc** or `sh button.sh kill`

## Layout (iMovie-ish)
- **File** menu top  
- **Transport** + timecode LCD  
- **Preview** (center) + **Inspector** (right)  
- **Timeline** below: V1, V2, A1, A2 lanes  

## Try
1. Demo should auto-load 3 colored clips on V1/V2.  
2. **Space** — play; watch preview + red playhead.  
3. Click a clip on the timeline — inspector shows in/out/dur.  
4. **J / L** scrub · **,/.** frame step · **p** return to zero.  
5. **C** split at playhead · **R** ripple delete · **Del** remove.  
6. **[ / ]** nudge clip on timeline.  
7. **File → Save** · **File → Export MP4 (V1)** → `pieces/apps/player_app/export.mp4`  
8. **File → Import Demo Media** reloads demos.

## Keys
| Key | Action |
|---|---|
| Space / K | play/pause |
| J L | scrub −/+ 200ms |
| , . | frame step |
| I O | set in / out (selected clip) |
| C | split |
| R | ripple delete |
| X | export V1 |
| s | save project |
| d | load demo |
| Esc | quit |

## Files
- `pieces/apps/player_app/timeline.clips` — project  
- `pieces/apps/player_app/export.mp4` — export  
- `pieces/apps/player_app/canvas.raw` — preview for future canvas-widgit  
- `media/demo_*.mp4` — sample sources  

## Drag-and-drop (house path aware)
Drop **video / audio / images / webm** from Nautilus onto the window (best: onto the **timeline** so track + time are used).

Supported extensions:
- video: mp4, webm, mkv, mov, avi, m4v, …
- audio: wav, mp3, aac, ogg, flac, m4a, …
- image: png, jpg, webp, gif, bmp, … (3s still)

**House paths** (emoji, ZWJ, `&`, long `file://` percent-encoding) go through
`103.media-studio/shared/media_drop_path.c`. If a drop fails, check:
`pieces/display/last_drop_debug.txt` (or `/tmp/media_drop_debug.txt`).

Fallback: copy files in Nautilus (Ctrl+C) → **File → Paste files (clipboard)** (needs `xclip`).

Also: drag **clips already on the timeline** to move them between tracks/time.

## Audio
Playback is **ffmpeg PCM pipe → PulseAudio** (s16le stereo 44.1k), synced to the playhead.
Demo clips (`media/demo_*.mp4`) include sine tones — you should hear them on **Space**.
Status bar shows `Playing (video + audio)` when Pulse is up, or `Playing (no Pulse — silent)` if not.

## CPU / FPS budget (strict)
- **Play does NOT re-decode video** — holds last poster frame + plays audio only  
  (spawning `ffmpeg` every frame was melting the CPU)
- Video frame via ffmpeg only on **scrub / pause / first load**, max ~**2/sec**, half-res (`nice -n 15 -threads 1`)
- UI ≤**20fps** while playing, ≤**8fps** idle; main loop always sleeps
- Status: `decode=scrub-only`

## Honest MVP limits
No multi-track composite export (V1 concat only), no effects chain, freeglut not CHTPM shell. Preview uses ffmpeg seek-decode (can hitch). Audio is single active clip under the playhead (prefer V1→V2→A*), not mixed multi-track.
