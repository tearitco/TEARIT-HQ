#!/bin/bash
# nb_print_page.sh - turn the projected page (page.state.txt) into a PDF.
#
# The manager has no PDF library and adding one is not an option here, so
# the print path is: page.state.txt -> PostScript -> ps2pdf (poppler,
# already installed). The PDF lands in the app's own downloads/ directory,
# like downloads, and the line printed on stdout is what the pane shows.
#
# Only the CONTENT lines of the projection are printed - URL/PARA/LINK/
# RICH/MEDIA/... markers are document structure, not words. HEAD| levels
# become bold subheads in descending size; TEXT| becomes wrapped body
# text; LIST| becomes an indented dash line; TITLE| is the document
# heading. Inline link labels that live in TEXT lines print inline
# exactly as they read on screen.
#
# Usage: nb_print_page.sh <house_root>                 # prints the live page
#        nb_print_page.sh --out <file.pdf> [state]    # test/target override
#
# Exit 0 and prints "PRINTED <abs path> (<bytes> bytes)" on success.
# Exit 1 and prints "ERROR: <why>" otherwise. Never writes a partial PDF.
set -u

if [ "${1:-}" = "--out" ]; then
    OUT="$2"; STATE_FILE="${3:-}"
    [ -n "$OUT" ] || { echo "ERROR: --out needs a path"; exit 1; }
else
    HOUSE_ROOT="$1"
    [ -n "$HOUSE_ROOT" ] || { echo "ERROR: need house_root"; exit 1; }
    HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
    STATE_FILE="$HOUSE_ROOT/#.desktop/network_browser_page.state.txt"
    OUT=""
fi
[ -n "$STATE_FILE" ] || STATE_FILE="$HOUSE_ROOT/#.desktop/network_browser_page.state.txt"
[ -r "$STATE_FILE" ] || { echo "ERROR: cannot read $STATE_FILE"; exit 1; }
command -v ps2pdf >/dev/null || { echo "ERROR: ps2pdf (poppler-utils) not installed"; exit 1; }
command -v python3 >/dev/null || { echo "ERROR: python3 not installed"; exit 1; }

if [ -z "$OUT" ]; then
    DESTDIR="$HOUSE_ROOT/&.hq-apps/network/downloads"
    mkdir -p "$DESTDIR" || { echo "ERROR: cannot create $DESTDIR"; exit 1; }
    # File name from the page title, sanitized, unique - the same shape as
    # the downloads path (name + (n)), never overwriting anything.
    TITLE="$(sed -n 's/^TITLE|//p' "$STATE_FILE" | head -1)"
    SAFE="$(printf '%s' "${TITLE:-page}" | tr -c 'A-Za-z0-9._-' '_' | sed 's/^_*//;s/_*$//' | cut -c1-60)"
    [ -n "$SAFE" ] || SAFE="page"
    OUT="$DESTDIR/$SAFE.pdf"
    n=1
    base="${OUT%.pdf}"
    while [ -e "$OUT" ]; do OUT="$base ($n).pdf"; n=$((n + 1)); done
fi

TMP_PS="$(mktemp /tmp/nb_print_page.XXXXXX.ps)" || { echo "ERROR: mktemp failed"; exit 1; }
trap 'rm -f "$TMP_PS"' EXIT INT TERM

python3 - "$STATE_FILE" "$TMP_PS" <<'PY'
import sys

src, dst = sys.argv[1], sys.argv[2]

def ps_escape(s):
    return s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

def ps_line(font, size, x, y, text):
    return f"/{font} findfont {size} scalefont setfont {x} {y} moveto ({ps_escape(text)}) show\n"

PAGE_W, PAGE_H = 612, 792
MARGIN = 48
TOP = PAGE_H - 60
BOTTOM = 54
LEAD = 13

body = []
title = None
first_content = True
y = TOP
page_no = 1

def showpage():
    global y, first_content, page_no
    body.append("showpage\n")
    page_no += 1
    y = TOP
    first_content = True

def line(font, size, text):
    global y, first_content
    if y < BOTTOM:
        showpage()
    if first_content:
        first_content = False
    body.append(ps_line(font, size, MARGIN, y, text))
    y -= LEAD

def wrap(text, size):
    # Helvetica average width at 10pt ~ 5.2pt/char; stay inside the margin.
    max_chars = int((PAGE_W - 2 * MARGIN) / (size * 0.52))
    out, cur = [], ""
    for word in text.split():
        cand = (cur + " " + word).strip()
        if len(cand) > max_chars and cur:
            out.append(cur); cur = word
        else:
            cur = cand
    if cur:
        out.append(cur)
    return out or [""]

for raw in open(src, "r", encoding="utf-8", errors="replace"):
    line_raw = raw.rstrip("\n")
    if not line_raw:
        continue
    parts = line_raw.split("|")
    kind = parts[0]
    if kind == "TITLE" and title is None:
        title = parts[1] if len(parts) > 1 else "page"
        line("Helvetica-Bold", 17, title)
        y -= 6
    elif kind == "HEAD":
        level = parts[1] if len(parts) > 1 and parts[1].isdigit() else "2"
        text = parts[2] if len(parts) > 2 else ""
        size = {"1": 14, "2": 12, "3": 11}.get(level, 11)
        if y < BOTTOM + LEAD:
            showpage()
        line("Helvetica-Bold", size, text)
        y -= 2
    elif kind == "TEXT":
        text = parts[1] if len(parts) > 1 else ""
        for w in wrap(text, 10):
            line("Helvetica", 10, w)
        y -= 3
    elif kind == "LIST":
        depth = parts[1] if len(parts) > 1 and parts[1].isdigit() else "1"
        marker = parts[2] if len(parts) > 2 else "-"
        text = parts[3] if len(parts) > 3 else (parts[-1] if len(parts) > 1 else "")
        indent = MARGIN + 14 * int(depth)
        for i, w in enumerate(wrap(text, 10)):
            if y < BOTTOM:
                showpage()
            prefix = ("- " if i == 0 else "  ")
            body.append(f"/Helvetica findfont 10 scalefont setfont {indent} {y} moveto ({ps_escape(prefix + w)}) show\n")
            y -= LEAD
        y -= 2
    # everything else (URL|, PARA|, LINK|, RICH*|, MEDIA|, VIDEO|, ...) is
    # document structure, not printable words.

if first_content and title is None:
    line("Helvetica-Bold", 17, "(empty page)")
body.append("showpage\n")

with open(dst, "w", encoding="ascii", errors="replace") as f:
    f.write("%!PS-Adobe-3.0\n")
    f.write(f"%%Pages: {page_no}\n")
    f.write("%%Title: " + ps_escape(title or "page") + "\n")
    f.writelines(body)
PY

ps2pdf "$TMP_PS" "$OUT" || { echo "ERROR: ps2pdf failed"; exit 1; }
[ -s "$OUT" ] || { echo "ERROR: ps2pdf produced an empty file"; exit 1; }
BYTES="$(stat -c %s "$OUT" 2>/dev/null || stat -f %z "$OUT")"
echo "PRINTED $OUT ($BYTES bytes)"
