# iqamoji-xo5 ops

This directory holds the first executable methods for the bot scaffold.

Initial ops:
- `traverse_dirs.c`
- `reward_signal.c`
- `punish_signal.c`
- `label_session.c`

The intent is to keep these small and composable:
- traversal should inspect directories and report structure
- reward and punishment should update on-disk RL bookkeeping
- session labeling should set current goal/session state

Future agents should keep the ops file-backed and auditable.
