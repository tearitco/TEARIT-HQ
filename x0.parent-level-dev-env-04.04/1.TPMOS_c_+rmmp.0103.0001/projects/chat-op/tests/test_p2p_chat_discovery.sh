#!/bin/bash
# test_p2p_chat_discovery.sh - Verify P2P Chat Peer Discovery
# Path: 1.TPMOS_c_+rmmp_84.05/projects/chat-op/tests/test_p2p_chat_discovery.sh

PROJECT_ROOT="1.TPMOS_c_+rmmp_84.05"
MGR_BIN="$PROJECT_ROOT/projects/chat-op/manager/+x/chat-op_manager.+x"
HEARTBEAT_BIN="$PROJECT_ROOT/projects/chat-op/ops/+x/p2p_heartbeat.+x"
GUI_STATE="$PROJECT_ROOT/projects/chat-op/manager/gui_state.txt"
REG_DIR="$PROJECT_ROOT/pieces/network/registry"

echo "=== P2P CHAT DISCOVERY TEST ==="

# 1. Setup
mkdir -p "$REG_DIR"
rm -f "$REG_DIR"/*.txt
rm -f "$GUI_STATE"

# 2. Mock location_kvp (ALWAYS overwrite for test)
LOC_KVP="$PROJECT_ROOT/pieces/locations/location_kvp"
mkdir -p "$PROJECT_ROOT/pieces/locations"
# Use absolute path for robustness in this test
ABS_ROOT=$(readlink -f "$PROJECT_ROOT")
echo "project_root=$ABS_ROOT" > "$LOC_KVP"
echo "pieces_dir=$ABS_ROOT/pieces" >> "$LOC_KVP"

# 3. Simulate Instance 1 (Manually calling heartbeat Op)
echo "Launching Instance 1 (Mock)..."
"$HEARTBEAT_BIN" "$PROJECT_ROOT" "node_alpha" 8000 "CHAT"
echo "--- Instance 1 registered ---"

# 4. Simulate Instance 2 (Running manager)
echo "Launching Instance 2 (Manager)..."
# We need to set project_root env or ensure relative paths work
# The manager reads pieces/locations/location_kvp relative to CWD
cd "$PROJECT_ROOT"
./projects/chat-op/manager/+x/chat-op_manager.+x &
MGR_PID=$!
cd ..

sleep 2
echo "--- Check discovery after 2s ---"
if [ -f "$GUI_STATE" ]; then
    echo "✓ gui_state.txt created. Content:"
    cat "$GUI_STATE"
else
    echo "✗ gui_state.txt MISSING"
fi

# 5. Verify Instance 2 created its own registry file
echo "Registry contents:"
ls "$REG_DIR"
for f in "$REG_DIR"/*.txt; do
    echo "--- $f ---"
    cat "$f"
done

# 6. Cleanup
kill $MGR_PID
echo "=== TEST COMPLETE ==="
