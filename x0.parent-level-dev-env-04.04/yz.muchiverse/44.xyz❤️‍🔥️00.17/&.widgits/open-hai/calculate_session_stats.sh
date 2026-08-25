#!/bin/bash
# Calculate per-session stats retroactively from hai transcripts

SESSIONS_DIR="$(cd "$(dirname "$0")" && pwd)/sessions"
HOUSE_ROOT="$(cd "$(dirname "$0")" && pwd)/../.."
STATS_DIR="$HOUSE_ROOT/%.harnesses/harnecient-fsm/session-stats"

mkdir -p "$STATS_DIR"

for session_dir in "$SESSIONS_DIR"/*; do
    [ -d "$session_dir" ] || continue

    timestamp=$(basename "$session_dir")
    transcript="$session_dir/transcript.txt"
    stats_file="$STATS_DIR/$timestamp.txt"

    [ -f "$transcript" ] || continue

    # Count messages
    user_msgs=$(grep -c "^U|" "$transcript")
    ai_msgs=$(grep -c "^A|" "$transcript")
    tool_calls=$(grep -c "TOOL\|tool\|delegat" "$transcript")

    # Get session timestamp (human readable)
    session_date=$(date -d @"$timestamp" '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo "unknown")

    # Calculate stats
    total_turns=$((user_msgs + ai_msgs))

    # Generate stats file
    cat > "$stats_file" << EOF
Session: $timestamp
Date: $session_date
User Messages: $user_msgs
AI Responses: $ai_msgs
Total Turns: $total_turns
Tool Calls Detected: $tool_calls
Generated: $(date '+%Y-%m-%d %H:%M:%S')
EOF

    echo "✓ Stats for session $timestamp"
done

echo "Session stats calculation complete"
