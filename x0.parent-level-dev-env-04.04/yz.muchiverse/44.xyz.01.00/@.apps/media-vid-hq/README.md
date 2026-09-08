# media-vid-hq — iMovie-shaped video editor (house spec)

Source: `103.media-studio/103.vid-edit/` (`HOW2_VIDEO.md`).

## Works now
Demo loads `x0.parent-level-dev-env-04.04/#.media-library/sample-10s-vp9.mp4`
onto V1 (10s). Preview is an ffmpeg poster frame (320×180 RGBA),
throttled ~2/sec while playing (HOW2: no per-frame decode). Chrome,
tabbars, timeline, split/del.

Not yet: ffmpeg PCM audio, MP4 export, drop import.

## Test
```
bash 44.xyz.01.00/@.apps/media-vid-hq/button.sh run
```
