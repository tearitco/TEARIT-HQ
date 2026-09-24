# hq-ftp

LAN peer drop between two houses. Not built. The network menu row
and the placeholder window exist so the name is not only in this file.

## What it is

An X11-HQ window, `&.hq-apps/hq-ftp/`. Two houses on the same LAN
open it. A file dragged onto one window is offered to the other.
There is no central server. The other house is a peer running the
same app, found on the LAN, not an account on a website.

This is a house app, not an RPG Maker command. House-specific event
commands are also allowed when a page needs to name a network act.
The events picker lists every `COMMAND` in
`event_commands.registry.pdl` (the projector cap is 128 types). Adding
`read_receipt`, `send_window_key`, and `advance_fact` did not require
a second picker. A future `offer_file` command would be the same
kind of addition, and only when a page must do it. The window can
exist before that command does.

## What the first test is

Run the placeholder, then the real app, on this machine and on a
second machine reached by ssh (a Mac or another desktop on the LAN).
Drop one small file each way. The proof is the file arriving in the
other house's drop folder, and the window saying whose peer it came
from. No battle, no actor row, no cloud account.

## What is deliberately absent

No protocol is chosen here. No port, no discovery broadcast, no
encryption story. Those get written when the first drop is being
built, not before. The placeholder window's text is the reminder.
