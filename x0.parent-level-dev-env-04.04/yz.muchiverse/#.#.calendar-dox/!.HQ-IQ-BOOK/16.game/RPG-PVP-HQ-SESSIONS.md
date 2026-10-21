# rpg-pvp-hq: match session, network tab, local and networked seats

Status: ledger, seats, and the Network tab are in the shooter. Written 2026-10-21 from the owner's ask, checked against the house docs, then built the same day. Cross-machine play is still the palnet limit below.

## 1. The ask (before the house check)

A match needs a session id. The game may already have one.

The top bar of the shooter window gets a Network option.

Two ways to sit in that match:

- Hardware-local. People at this desk, including new logins on this machine.
- Networked. People on other machines join the same session id.

A seat can belong to a login that also has an AI (the pet that already walks by itself). While that login is in the seat, the person plays. If they log out, the AI takes the seat back and keeps playing. The match does not wait, and the seat is not left empty.

## 2. What the house already has (read 2026-10-21)

**Match sessions are a house pattern, and rpg-pvp-hq has not adopted it.**
`08-roadmap/design-docs/GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md` (2026-10-07) is the standard. One match is one session: a `game.pdl` setup, one append-only session ledger, a result row at the end. Seats are `human` (this desk), `agent` (an AI on this machine, same commands as a human, no extra rights), `peer` (another node, `palnet_peer`, room = session id), or `spectator`. Transport is local, then local agents, then p2p. Cross-machine is the same code only after the networking plan. That plan is not started: `palnet_peer` still hardcodes 127.0.0.1. Remote results stay advisory until identities are signed. The pattern's game table lists TSC_ELO, DSR, wager chess, and TSOTS. rpg-pvp-hq is not on it. `RPG-PVP-HQ-DESIGN.md` P9 says many humans on several machines are not planned.

**The id this game already has is a board-instance name, not a match id.**
`PCHQ-SESSIONS-DESIGN.md` gives each pc-hq board its own data directory. rpg-pvp-hq's C points `PCHQ` at `@.apps/rpg-pvp-hq/rpg-pvp-hq-pchq`. That folder is not on disk right now. The engine session under a board (`pieces/sessions/<id>`, `bv_session=`) is the viewer session. It is created when a board engine runs. It is not the id other players would join. There is no `SESSION | id` row and no seat file in this app.

**"AI takes the seat back" already exists for one local pet, not for a login.**
The resident header in `ops/rpg_pvp_hq.c` says every pet walks by itself, INT takes control of the selected pet, and INT off hands that pet back to itself. The other pets keep walking the whole time. That is the local form of the rule. It is not tied to a house login, a seat, or a match session. Logins in this house are the `user/<id>` data branches. They are not seats in rpg-pvp-hq.

**The top bar has no Network tab. The house network menu is a different window.**
`rpg-pvp-hq.xhtpm` top bar is INT, book, page, Shop, Menu, Help, Inventory, Craft, Body, Look, Player, time. A Network tab would be a new tab on that bar. The livedesk network dropdown (`livedesk_taskbar.pdl`, `livedesk:open-network:`) is the desk-wide network cell. hq-ftp is a placeholder row there. It is not a match lobby, and it must not grow its own sockets. Peers append an outbox line. `palnet_peer` moves the lines.

## 3. How the ask sits on that

Use the seats pattern. Do not invent a second session system.

- The match session id is a new id, written as the first row of a session ledger. It is not `bv_session` and not the `rpg-pvp-hq-pchq` folder name. Those stay the board and the viewer.
- A seat row says who holds it: `human` at this desk, `agent` (the pet's own walk and, later, the P4 gun AI), or `peer` (a `palnet_peer` room equal to the session id).
- Hardware-local means `human` and `agent` seats only. Two logins on one machine are two `human` seats, same as TSC_ELO hot-seat. No socket.
- Networked means a `peer` seat. It waits on the existing peer. It does not open a port inside rpg-pvp-hq. Until `palnet_peer` leaves 127.0.0.1, "network" can only mean another process on this machine.
- Logout hands the seat to `agent`. The same rule as INT off, but the trigger is the login leaving, not the INT key. The pet that already walks keeps the body. A login with no AI does not get this. The seat stays empty or closes. That case is not designed yet.
- The Network tab on the shooter top bar lists this match's session id, the seats, and local versus peer. It does not replace the livedesk network menu. Joining a lobby across games stays the house lobby ledger when that screen exists. This tab is only the one match.

## 4. What is in, and what is still open

In, as of the build that added `net` to `ops/rpg_pvp_hq.c`:

- `state/session.txt` is the match ledger. The first row is `SESSION|<id>`. Later rows are `SEAT|<who>|<kind>|<link>`. The last row for a name wins. The id is not `bv_session` and not the pchq folder.
- Each pet is a seat. INT on the selected pet writes `human` / `local`. INT off, and Network > Log out (AI back), write `agent` / `local`. The pet walk that already runs is that agent. The trainer has no walk AI: while INT controls the player the seat is `human`; after logout it is `closed`. A login with no AI is still that closed seat.
- A second person at this desk is another `human` / `local` seat. No socket.
- Network > Open peer room appends `SEAT|peerN|peer|peer` and one line `ROOM|<session id>|SEAT|peerN|peer` in `<app>/net/outbox.txt`. palnet_peer is what would move that line. rpg-pvp-hq does not open a port.
- The top bar Network tab lists the session id and the current seats (who, kind, local or peer). It does not replace the livedesk network menu.

Still open: palnet_peer is still 127.0.0.1, so a peer seat is another process on this machine. Seats are not bound to `user/*` login branches. P4 combat AI is not built, so an agent walks and fires only when something writes `equip` / `fire` / `reload`. P3 hit points are not in, so a fight still has nothing to score.
