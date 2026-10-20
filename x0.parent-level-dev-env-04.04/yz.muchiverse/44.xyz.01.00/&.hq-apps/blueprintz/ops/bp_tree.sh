#!/bin/sh
# B1 read-only tree. Rewrites the marker block in blueprintz.xhtpm from
# each game's events/*/event_pkg. Does not launch the window.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(dirname "$HERE")"
HOUSE="$(cd "$APP/../.." && pwd)"
X="$APP/blueprintz.xhtpm"
TMP="$(mktemp)"
: > "$TMP"
for game in dsr-rpg cdda-hq df-hq gta-hq mcl-hq tcg-rpgce civ-rpgce game-dev-hq; do
    root="$HOUSE/@.apps/$game/events"
    [ -d "$root" ] || continue
    n=$(find "$root" -name event.ir.pdl | wc -l)
    printf '        <text label="%s (%s events)" class="quiet"/>\n' "$game" "$n" >> "$TMP"
    find "$root" -name event.ir.pdl | sed "s|$root/||; s|/event_pkg.*||" | sort -u | head -12 | while read -r ev; do
        printf '        <text label="  %s" class="quiet"/>\n' "$ev" >> "$TMP"
    done
done
python3 - "$X" "$TMP" <<'PY'
import pathlib, sys
x, block = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]).read_text()
text = x.read_text()
a, b = "<!-- BP-TREE -->", "<!-- /BP-TREE -->"
if a not in text or b not in text:
    sys.exit("blueprintz.xhtpm is missing the BP-TREE markers")
text = text[:text.index(a) + len(a)] + "\n" + block + "        " + text[text.index(b):]
x.write_text(text)
print("bp_tree wrote", x)
PY
rm -f "$TMP"
