# Creating User Input with TPMOS/CHTPM

This document outlines how user input is handled within the TPMOS and CHTPM infrastructure. CHTPM (or GL-OS for graphical applications) serves as the primary user interface, responsible for capturing input and rendering the system's state.

## The Role of Input in the TPMOS Pipeline

User input is the very first step in the 12-step TPMOS pipeline. Whether it's a keyboard press, a command, or data submitted through an application interface, it initiates the flow of information that drives the system's state changes.

## Input Mechanisms

1.  **Direct Key Presses:**
    The fundamental method for capturing user interaction is through direct key presses. These events are typically logged in a format similar to:
    `[YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <code>`
    This structured logging allows for precise playback and debugging, ensuring that user actions are auditable and repeatable. The `<code>` represents the specific key or input event.

2.  **Application-Specific Input:**
    Applications built within the TPMOS framework, such as those using `.chtpm` layout files (e.g., `quiz.chtpm`), define their own input handling logic. While the underlying mechanism might still rely on key presses or command inputs, the CHTPM layer interprets these inputs within the context of the specific application's screens and requirements. For example, when interacting with a quiz, the user would input answers, which are then processed by the quiz application's module.

3.  **Command & PAL Integration:**
    User input can also manifest as commands executed directly or through PAL scripts. The CHTPM layer facilitates the entry and execution of these commands, which then leverage the TPMOS pipeline to interact with Pieces and Ops.

## Processing User Input

Once captured, user input is routed through the TPMOS pipeline:
*   **Input → Routing → Relay:** The raw input is passed through initial stages.
*   **Module Tick:** A relevant Module polls the input.
*   **Trait/Op:** The Module decides on an action and delegates it to a specific Op.
*   **Piece Sovereignty:** The Op interacts with the state of Pieces.
*   **Mirror Sync → Stage → Parser → Composition → Render → Display:** The system's state is updated, and the changes are reflected back to the user through the CHTPM or GL-OS.

By adhering to this structured pipeline, user input is managed deterministically, ensuring system integrity and auditability.