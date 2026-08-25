# Informed Plan: Quiz-Bot Platform (Phase 1)

## 1. Project Overview
**Title:** Quiz-Bot Platform
**Path:** `projects/quiz-bot/`
**Objective:** To refactor the current `mcat-quiz` into a modular "Piece-Op" format. This platform serves two primary purposes:
1. **Manual Education:** A UI-driven study tool for users.
2. **AI Testbed:** A standardized environment where "External Bots" can interact with system ops to test their knowledge or verify OS stability.

## 2. Short-Term Architecture (Piece-Op Format)
The platform will be built to ensure compatibility with future AI bots without requiring the quiz logic itself to be "intelligent."

- **manager/quiz_orchestrator.+x**: 
    - Exposes an API of Ops (e.g., `quiz::start`, `quiz::submit_answer`).
    - Handles the core loop: loading question pieces, verifying answers, and updating session state.
- **pieces/question_bank/**: 
    - Each question is a standalone folder with a `state.txt` containing raw metadata (Name, Formula, Type, Groups, Relevance).
- **pieces/session_logs/**: 
    - Stores `rl_training_data.txt`—a raw log of every interaction (Question ID, Result, Time). This is the "fuel" for future external training.
- **layouts/quiz.chtpm**: 
    - A standard CHTPM layout for manual use via GL-OS.

## 3. The "Bot-Friendly" Interface
To support external AI bots, the `quiz_orchestrator` will implement a specific "Bot Op-Map":
- **`bot::interact_quiz`**: Allows a PAL-driven FSM to "read" the current question from the `orchestrator` state and "write" an answer back.
- **Why?**: This allows the AI to be a separate, decoupled entity that "plays" the quiz just like a human user would.

## 4. Implementation Phases

### Phase 1: Scaffolding (Short Term)
- Create `projects/quiz-bot/` structure.
- Define the Question Piece schema.
- **KPI**: Manual quiz flow working with at least 5 migrated MCAT questions.

### Phase 2: Data Logging (Short Term)
- Implement the `rl_training_data.txt` logger.
- **KPI**: Log file correctly records [Timestamp | Q_ID | Score] for manual sessions.

### Phase 3: Bot-Interface (Medium Term)
- Expose the Quiz Ops to the `prisc` interpreter.
- **KPI**: A basic "Dummy Bot" PAL script can start a quiz and submit a hardcoded answer.
