#!/bin/bash
# button.sh - muchi-pet launcher (standalone open).
# muchi-pet is a MONAD: *.monads/*.muchi-pet/ is the app bundle, its
# entities live at *.monads/*.muchi-pet/entities/<name>, and the active
# monster is chosen by active_monster.pdl (see pieces/monster/button.sh).
# This root button.sh is the house-standard monad entry point, matching
# *.monads/*.book-stack/button.sh: window opens the active monster, run
# opens it too (muchi-pet has no always-on reader like book-stack).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

MONSTER_LAUNCHER="$SCRIPT_DIR/pieces/monster/button.sh"

case "$ACTION" in
    run|r|start|window|w)
        exec bash "$MONSTER_LAUNCHER" run
        ;;
    read|open)
        echo "muchi-pet: no reader app (use 'run' to open the active monster)"
        exit 1
        ;;
    kill|k|stop)
        exec bash "$MONSTER_LAUNCHER" kill
        ;;
    check|verify)
        exec bash "$MONSTER_LAUNCHER" check
        ;;
    help|h|-h|--help|*)
        cat <<'EOF'
muchi-pet launcher

  sh button.sh run       # open the active monster (active_monster.pdl)
  sh button.sh window    # same as run (window only)
  sh button.sh kill      # close the active monster window
  sh button.sh check     # verify binaries/paths
  sh button.sh help      # this help
EOF
        ;;
esac
