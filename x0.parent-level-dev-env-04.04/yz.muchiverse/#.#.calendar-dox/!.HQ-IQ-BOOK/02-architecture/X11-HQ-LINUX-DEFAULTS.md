# House viewers on the Linux list

2026-10-21. Document only. Nothing here is installed.

The house windows are better at the files we actually play. Linux will not offer them in "Open with" until two different things exist. A desktop entry puts the name on the application list. A handler that accepts the file path is what makes it the default. We have the windows. We do not have the handler.

## Which window is which

| Job | App | Launch today | Opens a file from the command line |
| --- | --- | --- | --- |
| Image | `44.xyz.01.00/&.hq-apps/img-hai` | `sh button.sh run` | No. It scans its own draft folders. |
| Video | `44.xyz.01.00/&.hq-apps/video-player-hq` | `sh open_video_player_hq.sh` | No. A running window takes `add <path>` on `video_player_action.txt`, or a drop. |
| Audio | `44.xyz.01.00/&.hq-apps/sfx-hai` | `sh button.sh run` | No. It generates sounds. It does not play a wav or an mp3 you hand it. |

Video sound is an `ffplay -nodisp` child inside video-player-hq. There is no house player whose job is "open this audio file." Do not register sfx-hai as the handler for `audio/mpeg`. That would be a lie.

All three are X11 windows on the shared `khtpm_core_render.+x`. A desktop entry has to export `DISPLAY` (this machine uses `:0`). They are not Flatpaks and they are not on `PATH`.

## On the list, not the default

This is the safe half. It adds a name to the menu and to "Open with." It does not steal mp4, png, or mp3 from VLC.

Write one file per app under the user, not under `/usr` and not into the house git tree:

`~/.local/share/applications/lyfedesk-video.desktop`

```
[Desktop Entry]
Type=Application
Name=LyfeDesk Video
Exec=/bin/sh /ABSOLUTE/HOUSE/44.xyz.01.00/&.hq-apps/video-player-hq/open_video_player_hq.sh
Icon=video-x-generic
Terminal=false
Categories=AudioVideo;Video;Player;
StartupNotify=false
```

Same shape for LyfeDesk Image (`img-hai/button.sh run`) and LyfeDesk Audio (`sfx-hai/button.sh run`). `Name=` is what a person sees, so it says LyfeDesk. The path is absolute because a file manager does not start in the house.

Then:

```
update-desktop-database ~/.local/share/applications
```

The names show up in the menu. Double-clicking a file still uses whatever `mimeapps.list` already names, usually VLC or the desktop's image viewer.

Do not copy these files into `/usr/share/applications`. That is the system list and it is not ours to edit from the house.

## Default, after a handler exists

`xdg-open` and a file manager run the desktop entry as `Exec ... %f` or `%F`. The house launchers ignore that word. Setting the default now would open an empty window and drop the file on the floor.

The missing piece is a small wrapper per app, still not written:

1. If that window is not running, start it the same way `button.sh` / `open_video_player_hq.sh` does.
2. Wait until its manager is reading commands.
3. Hand it the path.

For video the hand-off already exists. The manager reads new lines of `video_player_action.txt` after the size it saw at start. A wrapper appends one line:

```
add /absolute/path/to/file.mp4
```

`video_player_op` also accepts `drop` when `DROP_PATH` is set, and turns that into `add`. Image and audio do not have this line yet. img-hai would need a real "open this png" verb before it can be `image/png`. sfx-hai needs a play-this-file verb before it can be `audio/*`. Until those verbs exist, only video is a candidate for a default.

When the wrapper exists, the desktop entry becomes:

```
Exec=/bin/sh /ABSOLUTE/HOUSE/.../open-with.sh %f
MimeType=video/mp4;video/webm;video/x-matroska;
```

Then, for this user only:

```
xdg-mime default lyfedesk-video.desktop video/mp4
xdg-mime query default video/mp4
```

`query` must print `lyfedesk-video.desktop` or it did not stick. The file that changed is `~/.config/mimeapps.list`. Leave `/usr/share/applications/mimeapps.list` alone.

Suggested types, once each wrapper is real:

- Video: `video/mp4`, `video/webm`, `video/x-matroska`, `video/ogg`.
- Image: `image/png`, `image/jpeg`, `image/webp`. Not yet.
- Audio: `audio/mpeg`, `audio/ogg`, `audio/wav`, `audio/flac`. Not yet.

## What not to do

- Do not unregister VLC, mpv, or the desktop image viewer. Add ours beside them. A default is one line in the user's `mimeapps.list`, and it is reversible by setting the old desktop id back.
- Do not point `MimeType` at an app that cannot take a path. Menu entry first, default second.
- Do not commit `~/.local/share/applications` into the house. The absolute house path is this checkout. A desktop file with that path belongs on the machine, written when someone asks to install it.
