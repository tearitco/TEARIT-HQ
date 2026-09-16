#!/bin/sh
# nb_video_cmd.sh - oneshot video control bridge for the network-browser
# V4 nav row. Called with the action first, then the session dir:
#   toggle <sess>   read <sess>/video.state, write "pause"/"resume"
#                   to <sess>/video.control (playing -> pause, else
#                   resume) so the button always shows the ACTION.
#   seek <sess> F   write "seek:F" (F = 0.0..1.0 fraction) to control;
#                   the player clears the file once the seek lands.
set -u
action="$1"
sess="$2"
ctrl="$sess/video.control"
case "$action" in
    toggle)
        state="paused"
        [ -f "$sess/video.state" ] && state="$(cat "$sess/video.state" 2>/dev/null)"
        if [ "$state" = "playing" ]; then
            printf 'pause\n' > "$ctrl"
        else
            printf 'resume\n' > "$ctrl"
        fi
        ;;
    seek)
        frac="$3"
        printf 'seek:%s\n' "$frac" > "$ctrl"
        ;;
esac
exit 0