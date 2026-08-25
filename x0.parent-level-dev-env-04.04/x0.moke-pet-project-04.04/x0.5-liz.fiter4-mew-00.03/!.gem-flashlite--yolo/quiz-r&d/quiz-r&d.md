# Quiz Engine: Input/Response Pipeline and Framework Integration

This document details the input and response mechanism within the TPM-AI Quiz Engine project, providing a guide for integrating similar interactive applications within the TPMOS framework.

## 1. Core Philosophy: Thin Brain & Data-Driven Interaction

The Quiz Engine adheres to the **Thin Brain** philosophy, where the UI (CHTPM) is a hollow shell that reflects the state and capabilities (Methods) of the active Piece. User input is captured and directed to specific handlers, which then update the application's state, leading to a re-render of the UI with new information.

## 2. Architecture Overview

### A. Layout (`.chtpm`)
The `quiz.chtpm` layout defines the user interface, including:
*   **Display Elements:** `<text>` for static labels and dynamic variables (`${q_name}`, `${q_formula}`, `${last_result}`, `${correct_answer}`).
*   **Input Fields:** `<cli_io id="answer" label="${answer_input}" />` for capturing user text input.
*   **Action Triggers:** `<button>` elements with `onClick="KEY:n"` attributes, mapping user actions (like submitting an answer or getting the next question) to specific keyboard keys.
*   **Input Routing:** The `<interact src="pieces/apps/player_app/history.txt" />` tag directs all keyboard input originating from this layout to a central history file managed by the `player_app`.

### B. The Manager (`quiz-engine_manager.c`)
The manager acts as the central controller for the Quiz Engine project. Its responsibilities include:
1.  **Initialization:** Resolving project paths via `pieces/locations/location_kvp` and performing an initial question load using `load_question.+x`.
2.  **Input Monitoring:** Continuously monitoring `pieces/apps/player_app/history.txt` for new key presses.
3.  **Input Processing (`process_key`):**
    *   Translating raw key codes into meaningful actions (e.g., '1' for submit, '2' for next question).
    *   Reading user input from `pieces/apps/player_app/cli_buffers.txt` for submission.
    *   Calling specific Ops (e.g., `check_answer.+x`, `load_question.+x`) via `run_command` to execute core logic.
    *   Clearing input buffers after actions.
4.  **State Management:**
    *   Updating application-specific state in `projects/quiz-engine/manager/state.txt` (e.g., `q_id`, `last_result`, `correct_answer`).
    *   Ensuring shared `player_app/manager/state.txt` is updated with `project_id` and `last_key` for global context.
5.  **Lifecycle Management:** Handling signals (`SIGINT`, `SIGTERM`) for graceful shutdown.

### C. The Operations (Ops)
These are executables (`.+x` files) responsible for the core logic of the quiz engine:
*   **`load_question.+x`:** Selects a question (either randomly or a specified one), reads its details (name, formula, type, groups, relevance) from `pieces/question_bank/<q_id>/state.txt`, and updates the UI-relevant variables in `player_app/manager/state.txt`.
*   **`check_answer.+x`:** Compares the user's submitted answer (read from `cli_buffers.txt`) against the correct answer stored in `question_bank/<q_id>/state.txt`. It logs the result to `rl_training_data.txt` and updates `last_result` and `correct_answer` in `player_app/manager/state.txt`.

### D. State and History Files

*   **`pieces/apps/player_app/manager/state.txt`:** Central hub for shared application state, including UI-relevant question data (`q_id`, `q_name`, etc.), user interaction results (`last_result`, `correct_answer`), and global context (`project_id`, `last_key`).
*   **`projects/quiz-engine/manager/state.txt`:** Stores project-specific state, currently holding the `q_id`.
*   **`pieces/question_bank/<q_id>/state.txt`:** Contains the detailed information for each question.
*   **`pieces/apps/player_app/history.txt`:** The primary channel for directing keyboard input from the UI to the manager.
*   **`pieces/apps/player_app/cli_buffers.txt`:** Used to temporarily store user input from `<cli_io>` elements before it's processed by an Op.

## 3. The Lifecycle of a User Interaction

1.  **Render:** The `quiz.chtpm` layout is rendered, displaying the current question details from `player_app/manager/state.txt`.
2.  **Input:** The user types an answer into the `<cli_io>` field and/or presses a button (e.g., '1' for Submit, '2' for Next).
3.  **Injection:** The CHTPM parser directs the key press to `pieces/apps/player_app/history.txt`.
4.  **Monitoring:** The `quiz-engine_manager.c` detects the new input in `history.txt`.
5.  **Processing:** `process_key()` interprets the key press:
    *   If '1' (Submit): Reads the answer from `cli_buffers.txt`, retrieves `q_id` from `quiz-engine/manager/state.txt`, and executes `check_answer.+x` with the answer.
    *   If '2' (Next): Executes `load_question.+x` and clears the answer buffer.
6.  **Operation:** The respective Op (`check_answer.+x` or `load_question.+x`) performs its logic, reading from and writing to state files as needed.
7.  **State Update:** Ops update `player_app/manager/state.txt` with results or new question data.
8.  **Re-render:** The UI automatically refreshes to display the updated information from `player_app/manager/state.txt`, showing the result of the user's action or the next question.

## 4. Implementation Guide for Agents (Building Interactive Apps)

To create similar interactive applications within the TPMOS framework:
1.  **Define Layout:** Design the UI in `.chtpm` using `<text>`, `<button onClick="KEY:n">`, and `<cli_io>` elements. Ensure `<interact src="..." />` points to the correct history file (usually `pieces/apps/player_app/history.txt`).
2.  **Develop Manager:** Create a C manager responsible for:
    *   Resolving paths using `pieces/locations/location_kvp`.
    *   Monitoring the designated history file.
    *   Dispatching key presses to appropriate Ops.
    *   Managing project-specific state (`projects/<project>/manager/state.txt`).
    *   Updating shared state (`player_app/manager/state.txt`) for UI feedback.
3.  **Create Operations (Ops):** Develop C executables for core logic (e.g., checking answers, loading data). These Ops should:
    *   Read necessary data from state files or other sources.
    *   Perform logic.
    *   Update relevant state files (especially `player_app/manager/state.txt` for UI visibility).
    *   Log outcomes if necessary (e.g., for RL training).
4.  **Data Management:** Store question/content data in structured files (e.g., `state.txt`, `.pdl`). Use `player_app/manager/state.txt` for UI-driven variables and `projects/<project>/manager/state.txt` for project-specific context.
5.  **Path Resolution:** ALWAYS use `pieces/locations/location_kvp` to resolve `project_root` and avoid hardcoded paths.

## 5. Critical Pitfalls & Framework Considerations

The Quiz Engine's input/response pipeline is robust due to adherence to core TPMOS principles. However, consider the following general and project-specific pitfalls:

*   **Pitfall #31 & #32: State Context Confusion:** When switching between projects or apps, ensure `project_id` and other context variables in `player_app/manager/state.txt` are correctly set or cleared. The Quiz Engine manager explicitly sets `project_id` and `last_key`. The layout's `<interact>` tag correctly points to `player_app/history.txt`.
*   **Pitfall #55: Input Routing Path Confusion:** Ensure the `<interact src="..." />` in the `.chtpm` layout points to the correct history file that the manager is monitoring. `pieces/apps/player_app/history.txt` is the standard for user-input-driven applications.
*   **Pitfall #77: Hardcoded Path Drift:** The Quiz Engine correctly uses `resolve_paths()` to dynamically determine `project_root`, adhering to the `LOCATION_KVP` mandate. This prevents issues when the project directory is moved or renamed.
*   **Pitfall #80: ASCII vs. Integer Injection:** The `quiz-engine_manager.c` correctly uses key codes like `'1'` and `'2'` which are then translated into ASCII character codes for injection into history files by the parser (as per `check_answer.c` and `load_question.c` usage of `printf` to history file).
*   **Pitfall #87: Render Trigger Discipline:** While the Quiz Engine doesn't directly use `frame_changed.txt`, its reliance on `state.txt` updates, which are implicitly picked up by the parser for re-rendering, aligns with the principle of state-driven UI updates. Avoid direct manipulation of `dirty=1` flags.
*   **Pitfall #33: CLI_IO Input Persistence:** The `<cli_io>` element in `quiz.chtpm` is used for text input. If the framework's handling of `cli_buffers.txt` and buffer clearing (as seen in `quiz-engine_manager.c` for key '2') is correctly implemented, input persistence should behave as expected.
*   **Pitfall #41 & #46: Missing Binaries:** Ensure all Ops (`check_answer.+x`, `load_question.+x`, and any others) are compiled correctly and exist in their respective `+x` directories. The manager relies on these binaries to function.
*   **Pitfall #70: Silent Failure:** The Ops should ideally return meaningful exit codes or log errors if they fail. `check_answer.c` logs to `rl_training_data.txt` and prints to stderr if states are not found, which is good practice.

## 6. Pro-Tip: Response Conjugation

For a more intuitive user experience, the manager can conjugate action verbs in the `last_response` field displayed in the UI.
*   `submit` (from submitting an answer) -> `Submitted`
*   `next question` -> `Next question loaded`
*   (Refer to `check_answer.c` for actual updates to `last_result` and `correct_answer`).

---
*Documented by Gemini CLI for TPMOS Future Agents - 2026-04-21*
