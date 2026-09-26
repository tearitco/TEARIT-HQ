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

## Use the peer that already exists

Do not open a new socket stack. `palnet_peer.c` (the copy under
`044.pal-chat-irc`, also used by pal-chain, pal-forum, and the pet
apps) is the symmetric peer. The window never touches a socket. It
appends a line to its outbox. `palnet_peer` broadcasts
`DATA|<node_id>|<content>` to peers it already has, and each peer
appends that into an inbox. Discovery is a presence directory, not a
broadcast we invent. The spec named beside it is
`&.2.muchi-verse/PAL-NET-STANDARD.txt`. Read that before changing
the peer.

What that peer moves is a line, not a file. hq-ftp's first real drop
is an outbox line that names the file (path, size, and a short hash).
The bytes are a second step: if both houses can read the path, the
line is enough; if they cannot, the bytes have to be cut into lines
the peer already accepts. That cut is not designed here. The
placeholder window stays a reminder until that line exists.
