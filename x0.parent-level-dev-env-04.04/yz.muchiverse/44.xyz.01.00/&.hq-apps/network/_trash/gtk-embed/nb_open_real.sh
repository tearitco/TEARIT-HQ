#!/bin/sh
# nb_open_real.sh <url>  - network-browser "open in a real engine" hook
# (NB-JS-ENGINE-ROADMAP.md §8 path C). Prefers our own WebKitGTK window
# (nb_webkit_view.+x, built only when the dev pkgs exist), then
# $NB_REAL_BROWSER, then whatever real browser is installed.
#
# Called by the renderer's dispatch as: nb_open_real.sh <url> <pkg> <house>
URL="${1:-}"
[ -n "$URL" ] || { echo "nb_open_real: need a url" >&2; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"

VIEW="$HERE/+x/nb_webkit_view.+x"
if [ -x "$VIEW" ]; then
    setsid env DISPLAY="${DISPLAY:-:0}" "$VIEW" "$URL" >/dev/null 2>&1 < /dev/null &
    exit 0
fi
for b in "${NB_REAL_BROWSER:-}" epiphany epiphany-browser firefox google-chrome chromium chromium-browser xdg-open; do
    [ -n "$b" ] || continue
    if command -v "$b" >/dev/null 2>&1; then
        setsid env DISPLAY="${DISPLAY:-:0}" "$b" "$URL" >/dev/null 2>&1 < /dev/null &
        exit 0
    fi
done
echo "nb_open_real: no real browser found (set NB_REAL_BROWSER or install one)" >&2
exit 1
