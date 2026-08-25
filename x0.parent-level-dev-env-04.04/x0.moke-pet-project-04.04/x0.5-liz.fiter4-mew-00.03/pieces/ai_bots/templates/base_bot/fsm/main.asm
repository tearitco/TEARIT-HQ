# EXO_BOT LIFESTYLE LOOP (Example ASM)
# This script demonstrates the Skeleton (FSM), Hands (FS Ops), and Intuition (AI Ops)

# 1. SCAN THE ENVIRONMENT (The Hands)
OP bot-editor::fs_ls "."

# 2. READ A KNOWLEDGE LEDGER (The Intuition)
# The bot "attends" to its own name in the curriculum
OP bot-editor::ai_attend "knowledge.txt" "EXO_BOT"

# 3. MAKE A DECISION (The Logic)
# If curiosity is high, create a new memory
OP bot-editor::fs_mk "memory_001.txt" "F"

# 4. EVOLVE (The Learning)
# If the action was successful, give a positive reward to the "curiosity" token
OP bot-editor::ai_evolve "knowledge.txt" "curiosity" 10.0

# 5. REPEAT
HALT
