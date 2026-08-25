# Chat-Op: P2P Ring-Broadcast Hybrid Chat
**Status:** Step 2 (Discovery & Visibility) Complete

## 1. Overview
Chat-Op is a TPMOS-compliant chat application featuring a decentralized P2P backend. It uses a hybrid discovery model where nodes register themselves in a shared registry and maintain a heartbeat to ensure network visibility.

## 2. P2P Architecture
- **Identity:** Each instance is identified by a unique `node_hash` (generated from PID + Timestamp).
- **Discovery:** Nodes write status files to `pieces/network/registry/`.
- **Heartbeat:** The `p2p_heartbeat` Op runs every 5 seconds to prune stale nodes (>15s inactivity) and update the UI state.
- **Port Mapping:** Instances automatically bind to the first available port in the 8000-8010 range.

## 3. Testing Multiple Instances (Same Codebase)
Currently, `run_chtpm.sh` and `kill_all.sh` are designed for a single global session. To test multiple chat instances from the same directory for speed, follow these guidelines:

### Immediate Manual Method
1. Launch the first instance normally: `./button.sh run`.
2. Launch subsequent chat managers manually in separate terminals:
   ```bash
   cd 1.TPMOS_c_+rmmp_84.05
   ./projects/chat-op/manager/+x/chat-op_manager.+x .
   ```
   *Note: These instances will share the same `gui_state.txt` unless using the session-based architecture below.*

### Future: Session-Based Architecture
To support robust multi-instance testing without process collisions, the following "Subnet Abstraction" is planned:

1.  **Session IDs:** Launch the system with a session flag (e.g., `./button.sh run --session A`).
2.  **Isolated Data Dirs:** The manager and parser will resolve paths using the session ID:
    - `pieces/apps/player_app/sessions/A/history.txt`
    - `pieces/apps/player_app/sessions/B/history.txt`
3.  **Process Tracking:** `proc_list.txt` will be partitioned by session ID to allow `kill_all --session A` to function without affecting Session B.
4.  **Registry Scoping:** The P2P registry will remain global to allow discovery across sessions, but communication will happen over the unique ports assigned during the `pick_node_port` phase.

## 4. Technical Details
- **UI State:** `projects/chat-op/manager/gui_state.txt` (Parsed by CHTPM).
- **Registry:** `pieces/network/registry/node_<hash>.txt`.
- **Logic:** `chat-op_manager.c` (Dispatcher) and `p2p_heartbeat.c` (Discovery).
