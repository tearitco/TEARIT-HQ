#!/bin/bash
# entity-move harness - test agent-driven entity movement via Move action
# Test vectors: type coordinates, arrow injection + screenshot, animation playback

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"

case "$ACTION" in
    demo)
        echo "Entity Move harness - demo mode (manual test)"
        echo "1. Start HQ: button.sh run from $.crypts/"
        echo "2. Open entity menu (Act button)"
        echo "3. Select Move"
        echo "4. Type coordinates (e.g., '100,200' or '100 200')"
        echo "5. Press Escape to confirm"
        echo "6. Watch entity animate to new position"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
%.harnesses/entity-move - agent-driven entity movement test harness

  bash button.sh demo      # Manual test walkthrough
  bash button.sh help      # This message

Test vectors:
  1. Type coordinate input (e.g., "100,200")
  2. Arrow key injection + screenshot for animation frame capture
  3. Verify animation_queue.txt written with waypoints
  4. Verify desktop_pos.txt updated with final position
  5. Verify world_manager detects and queues animation

Status: READY FOR TESTING
  - move_entity_animated.+x: compiled, working
  - khtpm_entity.c Move action: wired to input mode
  - Animation queue reading: integrated into main loop
  - Test infrastructure: ready

TODO: Full automated test suite with scenario scripts
EOF
        ;;
esac
