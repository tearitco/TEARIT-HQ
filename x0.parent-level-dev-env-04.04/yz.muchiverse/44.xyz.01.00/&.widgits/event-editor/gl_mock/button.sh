#!/bin/sh
# Pure freeglut Event Editor mock — A/B vs product (chtpm->rgb->gl_mirror)
# Not the product path. For look/feel comparison only.
#
#   sh button.sh run          # this GLUT UI
#   sh ../button.sh r         # product widget
#
ACTION="${1:-run}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

case "$ACTION" in
    compile|c|build)
        gcc -Wall -O2 -o "$SCRIPT_DIR/ee_gl_mock" "$SCRIPT_DIR/ee_gl_mock.c" \
            -lGL -lGLU -lglut && echo "OK ee_gl_mock"
        ;;
    run|r|open|*)
        if [ -z "${DISPLAY:-}" ]; then
            echo "No DISPLAY"
            exit 1
        fi
        gcc -Wall -O2 -o "$SCRIPT_DIR/ee_gl_mock" "$SCRIPT_DIR/ee_gl_mock.c" \
            -lGL -lGLU -lglut || exit 1
        echo "GLUT mock (A/B) on DISPLAY=$DISPLAY"
        echo "  product:  cd .. && sh button.sh r"
        echo "  arrows/digits/Tab/Enter/Esc  q=quit"
        exec "$SCRIPT_DIR/ee_gl_mock"
        ;;
esac
