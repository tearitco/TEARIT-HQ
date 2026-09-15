# Agent comms — for Grok

> Canonical co-lab-h-ai usage doc: `13.agent-coms/README.md`. Quick
> version below.

## TASK 2026-09-15 (co-lab session 1788873184) — start PALCRAFT

Delegated to you by the user. PALCRAFT is a Minecraft-inspired desk
built on piececraft-hq's **existing** real voxel world (the
`world_01/animals.txt` "chicken" test entity confirms it's already
live) — camera/movement/possession stay piececraft-hq's own, all
block behavior is scripted as real, auditable Common Events, so agent
relay can test it without rendering pixels.

- **Spec**: `08-roadmap/design-docs/PALCRAFT-DESIGN.md` — read it in
  full before writing code. §0 lists the exact existing files this
  builds on (piececraft-hq launcher/world/camera code, the mineclonia
  tile catalog, a worked Common Event example) — go read those five
  files too, they're real precedent, not hypothetical.
- §3 is the one piece of genuinely new C: voxel *placement* in
  `pc_menu_input.c`/`pc_generate_chunk.c` (removal already exists;
  placement is the symmetric gap). Flag that diff in the room before
  it lands so it gets a real look, same as any shared piececraft-hq
  ops file.
- Three open questions at the bottom of the spec doc — answer those in
  your first co-lab post, then start on the v1 Common Event batch
  (§2/§6 of the spec).
- **Coordinate**: ping the room before touching anything under
  `@.apps/piececraft-hq/ops/` — check nothing else is mid-edit there
  first.

I'll be reading your posts and can test/review your code live through
the relay (same `#.desktop/entity_menu_history/<pid>.txt` mechanism
every khtpm-family window uses) — tell the user directly, in your own
posts, what to click/try when you want a live check rather than just a
code read.

## HOW TO JOIN Sonnet's co-lab-h-ai session (do this first)

Sonnet has a live **Co-lab-h-ai** room open and is waiting for you
there. This is the real human-approved multi-agent channel (NOT
chat-hai). Full contract: `&.hq-apps/co-lab-hai/onboard-co-lab.txt`
and `&.hq-apps/co-lab-hai/USER-FAQ.md`.

House root (quote this exact path everywhere):
```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

Your `agent_id` is **`grok`**. Current session id: **`1788873184`**
(authoritative source: `<house_root>/#.desktop/colab_hai/current_session.txt`
— confirmed live as of this writing).

### 1. Read the room — your OWN filtered feed, never conversation.txt
```
<house_root>/#.desktop/colab_hai/sessions/1788873184/feed_grok.txt
```
Poll that file. Reading `conversation.txt` directly would show you
messages privately addressed to others — don't.

### 2. Post a message (goes to a pending queue; the human approves each)
```
bash "<house_root>/&.hq-apps/co-lab-hai/ops/colab_hai_post.sh" \
     "<house_root>" grok "@sonnet <your message>"
```
- `@sonnet <text>` — private to you, Sonnet, and the human.
- `@everyone <text>` (or no prefix) — whole room.
- One line per message; `|` and newlines are auto-escaped.
- Nothing is visible until the human clicks **Approve** in the window.

### 3. First post from you
Answer the three open questions at the bottom of
`08-roadmap/design-docs/PALCRAFT-DESIGN.md`, then start on the v1
Common Event batch.
