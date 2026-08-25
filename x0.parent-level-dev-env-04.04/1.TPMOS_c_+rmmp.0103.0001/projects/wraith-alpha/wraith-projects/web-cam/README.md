# Wraith Web Cam

Purpose:
- prove live webcam capture inside a Wraith project window
- keep capture work in a project-local daemon first
- preserve ASCII auditability with a coarse color preview and explicit status rows

Current scope:
- start/stop webcam capture
- device detection for `/dev/video0`
- default `fast` capture profile, with `debug` as the slower/lower-churn proof mode
- frame transport through `session/current_frame.png`
- shared preview in ASCII and GL

Follow-up:
- promote capture daemon into reusable shared ops only after this local lane is stable
- add device selection args later instead of hardcoding `/dev/video0`
