# Local LLM and Curriculum
Date: 2026-06-24

## Purpose

This lane is the local intelligence layer:

- an FSM-driven chatbot
- a small local model in or near `#.standby/`
- a `gem-dev` / `gem-xo` style interface
- theme/curriculum distillation into pieces with words, weights, and structure

## Why It Is Separate

This is not the same problem as the visual shell or the simulation engine.
It can grow independently if the interface is clean.

## What It Should Do

- talk to a local model
- keep the conversation state inspectable
- turn recurring themes into reusable memory/curriculum artifacts
- degrade cleanly when the model is unavailable or weak

## What It Should Not Do

- own gameplay logic
- replace the editor
- become the truth source for project state

## Short-Term Goal

Build a narrow local-chat proof of concept that can:

- accept prompts
- maintain a small FSM
- emit structured summaries or curriculum pieces
- remain useful without depending on Wraith

## Success Signatures

- the chatbot is a real tool, not just a demo shell
- it can summarize project themes into reusable artifacts
- it helps later agents strategize faster because the knowledge is structured
