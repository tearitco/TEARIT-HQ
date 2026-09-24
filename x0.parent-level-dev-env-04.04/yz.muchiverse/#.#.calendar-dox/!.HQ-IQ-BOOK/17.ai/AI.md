# AI

AI in this house is not a feature of the game. The game is one
machine it will drive. Dev work and productivity are others. Keeping
them in this chapter is what stops a game doc from pretending it
covers a coding agent, and what stops an agent doc from pretending
it knows how playtest works.

## What is real

Co-lab is an approval room for outside agents. A line is invisible
until a person approves it. The window clips long lines. The full
text is in the session files. See `13.agent-coms`.

A compiled event page can look at a receipt and append one
`KEY_PRESSED` line. That page lives in `16.game`. It is the muscle.
Nothing in this chapter chooses the key yet.

`ai_describe`, `ai_fsm_transition`, and `ai_goap_plan` are the only
new C the kilo note names for AI. They are not in the event registry.
Tomom is the learner in the design. It does not chat as the desk,
and it does not take tool calls against actor rows. IRL is not a
loop. Gemma is not maintaining the tree.

## The loop, when it exists

Describe, then score, then store. The model writes a description. A
deterministic harness scores it. A person can reject the store. The
model does not classify the world in one token. The same loop is
what would let it append one event command, open a dev task, or file
a productivity note. The command, the task, and the note are
different machines. The permission to append one of them is this
chapter. A house-specific event command is allowed when a page must
name the act. A house app is allowed when the act is not a page.
hq-ftp is the second kind: LAN peer drop, design only, row on the
network menu. See `08-roadmap/design-docs/HQ-FTP.md` and `16.game`
for the picker rule.

## What stays next door

Play, stop, reset, actors, event commands, and the two
representations of a physics fact are `16.game`. When this chapter
says "drive the game," it means append one command that chapter
already knows how to run. It does not mean a second play button.
