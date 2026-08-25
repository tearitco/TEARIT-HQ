# Wraith Screen Record

Purpose:
- record the live GL session to mp4 from inside Wraith
- keep start/stop/export control local first
- expose whether system-audio capture was available during the run

Current scope:
- start/stop full-display X11 capture
- best-effort Pulse monitor capture for mp3/system audio
- export mp4 into `session/recordings/`
- preserve ASCII and GL status plus last poster frame

Follow-up:
- move recorder daemon into shared reusable ops after this lane is proven
- add region/window-select args later instead of full-display default
