# ^.hai-robot — desktop robots (🤖)

A long-lived robot entity: its own persona, its own chats, its own tasks, a phone (`^.hai-phone`), a brain (HORN or a local model),
and the right to ask the server (`^.hai-server`) to **spawn sub-bots**. Design:
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md` (read first). Working name.

- A robot is a pal like any other (no special bot subsystem): see `ROBOT-CHAT-BLUEPRINT.md` and the first real robot pal `robot_chat_001`.
- Add one: copy `_TEMPLATE/` to `<robot-id>/` and fill `robot.pdl`; placed from the h-ai menu palette (`tp_place_desktop.+x`).
- Its chat with the owner is the pal's own `chat_history.txt`; its messages to other entities go by phone.

Nothing here runs yet (design phase 0).
