#!/bin/sh
# scene_shot.sh - ONE trustworthy picture of what a board's 3D engine draws, without touching the live window or engine (owner 2026-10-28: "if ur not confident, can u dump frame/png? we need to have confident testing tools. make what u need").
# It copies the app's live engine SESSION folder AND the game's pc-hq folder (the engine writes pick / HUD / sun / moon files into the pc-hq folder while it renders, so a render must never run on the live one) to a scratch folder, applies the camera overrides to the COPY's bv_state.txt, renders ONE frame with the engine binary the live window uses (the CPU one-shot, niced), turns the raw frame into a PNG (raw_to_png)
# and measures it (frame_stats on the middle of the picture, so the engine's debug text and minimap do not count). The live session is only READ.
# Usage:  scene_shot.sh <app_dir> <out.png> [key=value ...]
#   <app_dir>    <house>/@.apps/<app> (its board/state/ui.txt names the session: bv_session=)
#   key=value    camera / state lines written into the COPY's pieces/system/bv_state.txt (focused_project_root is pointed at the pc-hq COPY)
#   cfg:key=value   a line written into the pc-hq COPY's pieces/system/arrow_config.txt (e.g. cfg:tp_distance=1 cfg:tp_height=6), same replace-or-append rule
#   env SHOT_XEL="x y z" puts the xelector of the COPY there (pieces/xelector_01/state.txt pos_x pos_y pos_z) (e.g. camera_mode=3 cam_yaw=180 cam_pitch=-60 cam_base_x=4 cam_base_z=8 cam_z_level=0 selector_x=4 selector_y=8); the file is shown first as `before`
#   env SHOT_BIN = another engine binary to render with (a patched build under test); SHOT_INSET (default 25) = percent cut from each side before measuring; SHOT_KEEP=1 keeps the scratch copy; SHOT_TIMEOUT (default 150 s)
# Writes <out.png>, <out.png>.engine.txt (what the engine printed) and <out.png>.receipt.txt (session id, engine binary, every override, the camera lines AS RENDERED, the frame_stats numbers). Prints the verdict line.
# Exit: 0 the frame is real (frame_stats ok or sparse) | 1 the frame is blank / flat / dark (do NOT trust it as a picture of the scene) | 2 usage or no session | 3 the render failed or produced no frame.
# Verified by: harness frame_tools.pal (the two tools); this script was run on the live rpg-space session (see BOOK 16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md section 12).
set -u
[ $# -ge 2 ] || { echo "Usage: scene_shot.sh <app_dir> <out.png> [key=value ...]" >&2; exit 2; }
APP=$1; OUT=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd); HOUSE=$(cd "$HERE/../../.." && pwd)
RAW2PNG="$HOUSE/&.widgits/_shared-lib/ops/+x/raw_to_png.+x"; STATS="$HOUSE/&.widgits/_shared-lib/ops/+x/frame_stats.+x"
for t in "$RAW2PNG" "$STATS"; do [ -x "$t" ] || { echo "scene_shot: missing tool $t (build &.widgits/_shared-lib/ops/build_raw_to_png.sh and build_frame_stats.sh)" >&2; exit 2; }; done
S=$(sed -n 's/^bv_session=//p' "$APP/board/state/ui.txt" 2>/dev/null | head -1)
[ -n "$S" ] && [ -d "$S" ] || { echo "scene_shot: no live engine session for $APP (bv_session= empty or the folder is gone)" >&2; exit 2; }
W=$(mktemp -d /tmp/scene_shot.XXXXXX) || exit 2
[ "${SHOT_KEEP:-0}" = 1 ] || trap 'rm -rf "$W"' EXIT
cp -r "$S" "$W/s" || { echo "scene_shot: cannot copy the session" >&2; exit 2; }
cd "$W/s" || exit 2
ST=pieces/system/bv_state.txt
PQ=$(sed -n 's/^focused_project_root=//p' "$ST" | head -1)
[ -d "$PQ" ] || { echo "scene_shot: the session's focused_project_root ($PQ) is not a folder" >&2; exit 2; }
cp -r "$PQ" "$W/pchq" || { echo "scene_shot: cannot copy the pc-hq folder" >&2; exit 2; }
sed -i "s#^focused_project_root=.*#focused_project_root=$W/pchq#" "$ST"
printf '%s\n' "$W/pchq" > pieces/system/real_project_root.txt
CFG="$W/pchq/pieces/system/arrow_config.txt"
setkv() { f=$1; k=$2; v=$3; if grep -q "^$k=" "$f" 2>/dev/null; then sed -i "s#^$k=.*#$k=$v#" "$f"; else echo "$k=$v" >> "$f"; fi; }
if [ -n "${SHOT_XEL:-}" ]; then read -r XX XY XZ <<EOF_XEL
$SHOT_XEL
EOF_XEL
  XS="$W/pchq/pieces/xelector_01/state.txt"; setkv "$XS" pos_x "$XX"; setkv "$XS" pos_y "$XY"; setkv "$XS" pos_z "$XZ"; fi
echo "before: $(grep -E '^(camera_mode|cam_yaw|cam_pitch|cam_base_x|cam_base_z|cam_pan_[xyz]|cam_z_level|current_z|selector_[xy])=' "$ST" 2>/dev/null | tr '\n' ' ')"
for kv in "$@"; do
  case "$kv" in *=*) ;; *) echo "scene_shot: bad override $kv (need key=value)" >&2; exit 2;; esac
  case "$kv" in cfg:*) kv=${kv#cfg:}; setkv "$CFG" "${kv%%=*}" "${kv#*=}";; *) setkv "$ST" "${kv%%=*}" "${kv#*=}";; esac
done
rm -f pieces/system/cam_home.txt
BIN=${SHOT_BIN:-ops/+x/bv_render_3d.+x}; [ -x "$BIN" ] || BIN="$HOUSE/&.widgits/board-viewer/ops/+x/bv_render_3d.+x"
rm -f pieces/display/rgb_frame_3d_overlay.raw pieces/display/rgb_frame_3d_overlay.receipt.txt
PRISC_PROJECT_ROOT=$PWD nice -n 15 timeout "${SHOT_TIMEOUT:-150}" "$BIN" >"$W/engine.out" 2>&1
cp "$W/engine.out" "$OUT.engine.txt" 2>/dev/null
RAW=pieces/display/rgb_frame_3d_overlay.raw; REC=pieces/display/rgb_frame_3d_overlay.receipt.txt
[ -s "$RAW" ] && [ -s "$REC" ] || { echo "scene_shot: the engine wrote no frame (see $W/engine.out; SHOT_KEEP=1 keeps it)" >&2; exit 3; }
"$RAW2PNG" "$RAW" "$OUT" --receipt "$REC" >/dev/null || { echo "scene_shot: raw_to_png failed" >&2; exit 3; }
STATTXT=$("$STATS" "$OUT" --inset "${SHOT_INSET:-25}"); RC=$?
{ echo "session=$(basename "$S")"; echo "pchq=copy of $PQ"; echo "engine=$BIN"; echo "overrides=$*"; echo "camera_as_rendered=$(grep -E '^(camera_mode|cam_yaw|cam_pitch|cam_base_x|cam_base_z|cam_pan_[xyz]|cam_z_level|current_z|selector_[xy])=' "$ST" | tr '\n' ' ')"; echo "$STATTXT"; } > "$OUT.receipt.txt"
echo "$(echo "$STATTXT" | tr '\n' ' ')"
exit $RC
