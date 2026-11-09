#!/bin/bash
# nb_print_test.sh - G3.4: a page prints its projected rows to a PDF
# (requirements: scratch house, no network, no live browser).
#
# The print path is ops/nb_print_page.sh (page.state.txt -> PostScript
# -> ps2pdf), driven by the manager on a `print:` request line; the
# Chrome action script is ops/nb_write_print.sh. This suite pins the
# toolchain end-to-end with zero live state:
#
#   1. PRINTER      --out mode produces a valid PDF (magic, size, pages)
#   2. NAMING       default mode writes into <house>/&.hq-apps/network/
#                   downloads/, named from the sanitized TITLE
#   3. ESCAPING     parens/backslashes/UTF-8 in the page do not corrupt
#                   the PostScript (the failure mode that eats a page)
#   4. ACTION       nb_write_print.sh writes exactly "print:" and nothing
#                   else into the manager's request file
#   5. FAILURES     missing state file -> rc 1 + ERROR; PATH without
#                   ps2pdf -> rc 1 + "ERROR: ps2pdf" (never a fake file)
#
# Usage: sh nb_print_test.sh        exit 0 = all cases pass
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
NB="$(cd "$HERE/.." && pwd)"
PRINT_SH="$NB/ops/nb_print_page.sh"
WRITE_SH="$NB/ops/nb_write_print.sh"
WORK=/tmp/nb_print_test
FAIL=0

note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
command -v ps2pdf >/dev/null  || { echo "FAIL: ps2pdf missing (poppler-utils)"; exit 1; }
[ -x "$PRINT_SH" ] || { echo "FAIL: nb_print_page.sh not executable ($PRINT_SH)"; exit 1; }
[ -x "$WRITE_SH" ] || { echo "FAIL: nb_write_print.sh not executable ($WRITE_SH)"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
H="$WORK/house"           # scratch house root
mkdir -p "$H/#.desktop"
STATE="$H/#.desktop/network_browser_page.state.txt"
cat > "$STATE" <<'EOF'
URL|https://example.com/article
TITLE|Print me (with parens) and \backslashes — UTF-8 ✓
PARA|1
HEAD|1|A Printed Article
PARA|2
TEXT|Body paragraph one, wrapping onto multiple lines when the column is narrow enough that the words must break at the margin rule.
LINK|https://example.com/linked|read more
PARA|3
LIST|1|-|First bullet
LIST|2|-|Nested bullet
PARA|4
TEXT|Second paragraph with (parens) and \backslash and ünïcoder text.
MEDIA|img|https://example.com/x.png
VIDEO|video|https://example.com/x.mp4|1|100
EOF

# ---- 1. PRINTER: --out produces a valid PDF
if sh "$PRINT_SH" --out "$WORK/out.pdf" "$STATE" > "$WORK/out.log" 2>&1; then
    note "print --out: rc 0"
else
    fail "print --out exited nonzero: $(cat "$WORK/out.log")"
fi
[ -s "$WORK/out.pdf" ] || fail "out.pdf missing or empty"
if [ -f "$WORK/out.pdf" ]; then
    head -c 5 "$WORK/out.pdf" | grep -q '%PDF-' && note "magic: %PDF- ok" || fail "not a PDF (magic mismatch)"
fi
command -v pdfinfo >/dev/null && {
    pages="$(pdfinfo "$WORK/out.pdf" 2>/dev/null | sed -n 's/^Pages:[[:space:]]*//p')"
    [ "${pages:-0}" -ge 1 ] 2>/dev/null && note "pdfinfo: $pages page(s)" || fail "pdfinfo found no pages"
}

# ---- 2. NAMING: default mode writes a TITLE-derived PDF into downloads/
DESTDIR="$H/&.hq-apps/network/downloads"
sh "$PRINT_SH" "$H" > "$WORK/def.log" 2>&1 || fail "default print failed: $(cat "$WORK/def.log")"
if ls "$DESTDIR"/*.pdf >/dev/null 2>&1; then
    P="$(ls -t "$DESTDIR"/*.pdf | head -1)"
    note "default naming wrote $(basename "$P")"
    case "$(basename "$P")" in
        *Print_me*|*Print*) note "name derives from TITLE: ok" ;;
        *) fail "name does not derive from TITLE: $(basename "$P")" ;;
    esac
else
    fail "no PDF landed in downloads/"
fi

# ---- 3. ESCAPING: hostile chars did not corrupt the document (if the PS
# broke, gs/ps2pdf exits nonzero or produces an empty/invalid file)
if head -c 5 "$DESTDIR"/*.pdf 2>/dev/null | grep -q '%PDF-'; then
    note "escaping: hostile page produced a valid PDF"
else
    fail "hostile page produced no valid PDF"
fi

# ---- 4. ACTION: nb_write_print writes exactly "print:"
PDIR="$WORK/pkgdir"
mkdir -p "$PDIR"
sh "$WRITE_SH" print "$PDIR" "$H" || fail "nb_write_print rc nonzero"
if [ "$(cat "$H/#.desktop/network_browser_request.txt" 2>/dev/null)" = "print:" ]; then
    note "request file holds exactly 'print:'"
else
    fail "request file holds: [$(cat "$H/#.desktop/network_browser_request.txt" 2>/dev/null)]"
fi

# ---- 5a. FAILURE MODE: missing state file
mv "$STATE" "$STATE.gone"
if sh "$PRINT_SH" --out "$WORK/never.pdf" "$STATE" >/dev/null 2>&1; then
    fail "missing state file should fail"
else
    note "missing state file: rc nonzero as required"
fi
if [ -e "$WORK/never.pdf" ]; then
    fail "missing state file still produced a file"
else
    note "missing state file: no file produced"
fi
mv "$STATE.gone" "$STATE"

# ---- 5b. FAILURE MODE: ps2pdf absent from PATH
if PATH=/tmp/nb_print_test/nowhere "$PRINT_SH" --out "$WORK/never2.pdf" "$STATE" >"$WORK/nops2pdf.log" 2>&1; then
    fail "missing ps2pdf should fail"
else
    grep -q 'ERROR: ps2pdf' "$WORK/nops2pdf.log" && note "missing ps2pdf: rc nonzero + clear error" || fail "missing ps2pdf: unclear error: $(cat "$WORK/nops2pdf.log")"
fi

if [ "$FAIL" -eq 0 ]; then
    echo "nb_print_test.sh PASS"
else
    echo "nb_print_test.sh FAIL"
fi
exit $FAIL
