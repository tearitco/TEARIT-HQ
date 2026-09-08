#!/bin/sh
# notes.sh <cell> - open (creating if needed) a per-subsystem dev-note
# file in the most relevant dir, then hand it to the system editor.
#
# Wired from every real HQ header-cell menu as the "notes-<cell>" row
# (ktb_hq_open() in khtpm_taskbar_manager.c). Same spirit as the HQ
# menu's own "dir" row (xdg-open .) - a quick place to jot a dev note
# about that part of the project.
#
# dispatch() appends '<pkg_dir>' '<house_root>' as extra args - ignored.
set -u
CELL="${1:-hq}"
SELF="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SELF/../.." && pwd)"          # .../44.xyz.01.00

# cell -> the dir its notes file belongs in (relative to $HOUSE).
# Anything not listed falls back to a house-wide #.notes/ pool.
case "$CELL" in
  palettes)  DIR="$HOUSE/&.widgits/palettes" ;;
  network)   DIR="$HOUSE/&.hq-apps/network" ;;
  toys)      DIR="$HOUSE/@.apps" ;;
  db)        DIR="$HOUSE/&.widgits/db-hq" ;;
  ai)        DIR="$HOUSE/&.hq-apps" ;;
  clock)     DIR="$HOUSE/#.#.calendar-dox" ;;
  pals)      DIR="$HOUSE/xyzfs" ;;
  desk|file|user|hq|player|*) DIR="$HOUSE/#.notes" ;;
esac

mkdir -p "$DIR" 2>/dev/null || DIR="$HOUSE/#.notes"
mkdir -p "$DIR"
F="$DIR/notes-$CELL.md"

if [ ! -f "$F" ]; then
  {
    printf '# notes - %s\n\n' "$CELL"
    printf '_dev notes for the %s subsystem. Opened from HQ menu -> notes-%s._\n\n' "$CELL" "$CELL"
    printf -- '- \n'
  } > "$F"
fi

# open in the system editor, detached (same as the "dir" row's xdg-open .)
if command -v xdg-open >/dev/null 2>&1; then
  setsid xdg-open "$F" >/dev/null 2>&1 &
elif [ -n "${EDITOR:-}" ]; then
  setsid "$EDITOR" "$F" >/dev/null 2>&1 &
else
  setsid gedit "$F" >/dev/null 2>&1 &
fi
exit 0
