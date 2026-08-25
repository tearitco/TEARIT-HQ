# 1. Build everything
make clean && make

# 2. Initialize state
mkdir -p state config
echo '[{"role":"system","content":"You are a technical CLI agent."}]' > state/context.json
# YOLO starts OFF (file doesn't exist). Run `/yolo` in CLI to toggle.

# 3. Run
./agent