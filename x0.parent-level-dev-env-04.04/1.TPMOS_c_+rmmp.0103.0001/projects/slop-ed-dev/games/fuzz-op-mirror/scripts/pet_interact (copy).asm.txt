# pet_interact.asm - Interaction script for Fuzzball
# Purpose: Show a response when the player interacts with the pet

# (Assuming we have a way to set response.txt)
# exec echo "Fuzzball: *purr*" > pieces/apps/editor/response.txt

# Or use a dedicated sync op if available
exec ./pieces/ops/file-op/+x/sync_op.+x pieces/apps/player_app/manager/state.txt last_response "Fuzzball: *purr*"
hit_frame
