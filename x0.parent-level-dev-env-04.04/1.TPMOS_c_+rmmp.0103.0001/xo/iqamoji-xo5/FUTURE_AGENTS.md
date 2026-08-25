# iqamoji-xo5

This directory is the sovereign workspace for `iqamoji-xo5`, copied from
`xo/bot5` as the starting point for a much simpler external bot.

Current status:
- phase 0 planning
- bot5 code copied in as the baseline
- no RL/FSM/LLM redesign is implemented yet

Primary goal:
Teach an exo bot to traverse directories, observe files, learn from reward and
punishment, and eventually operate as a small FSM/RL/chat system.

Important scope note:
The future system should not be a large general-purpose LLM first.
The intended shape is:

- FSM for control flow and state transitions
- RL for reward, punishment, and policy adaptation
- curriculum files for staged learning
- knowledge distillation for compact language behavior
- one LLM node, not the whole system

That means the LLM is only one module inside a larger machine. The bot should
learn from structure, memory, and curriculum more than from a big standalone
model.

Immediate learning targets:
- directory traversal
- file inspection
- safe read/write operations inside the bot tree
- session recording
- memory labeling
- reward / punishment bookkeeping
- curriculum progression
- scenario-based actions

Planned architecture direction:

`ops/`
- small executable methods for observation, labeling, traversal, reward,
  punishment, and session handling

`cognition/`
- brain-like support code for policy selection, memory handling, and future FSM
  state management

`pieces/`
- sovereign data pieces for memories, weights, curriculum maps, and goals

`curriculum/`
- staged training tasks
- scenario files
- increasingly difficult directory traversal and reasoning goals

`memory_map.kvp`
- registry from goals to memory folders

Future implementers should read `#.IQABOD🪞️]z6]RMS/main_orchestrator.c` for the
curriculum-oriented orchestration style the user wants, then simplify it for
`iqamoji-xo5`.

Design principle:
keep the bot explainable, file-backed, and testable. The bot should be able to
show what it learned, why it was rewarded or punished, and which curriculum step
it is on.

Today:
Use this directory as the planning anchor only.
Do not try to finish the FSM/RL/LLM redesign in one pass.
