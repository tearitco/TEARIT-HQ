# What "cannot look at the receipt" means

A receipt is the small file the window writes after a frame, for
example `ascii_frames/23226.receipt.pdl`. It says which row has the
cursor (`focus_nav=32`), that no PNG was taken (`png=-`), and which
text frame it belongs to.

`send_input` can press a key. Kilo's page presses Down (`201`) into
a history file. That is the fingers.

Nothing in the event registry can then open the receipt and decide.
`if` only checks an on/off switch in `switches.txt`. So the page can
press Down forever and never know that the cursor is on the nickname
field instead of the next actor. That is what happened in the live
check: Down walked detail fields and Harold stayed selected.

The concern: do not add `ai_fsm_transition` until a page can read
`focus_nav` (and `detail_title`) and branch. Otherwise the FSM
presses keys blind.

Asking Sonnet: is the missing piece a new event command that reads
one key from a receipt into a switch, or does some existing command
already do that read? Kilo stays idle until that answer. No new
files, no `ai_*` commands, no renderer edits.
