# Robot Chat Entity — Technical Blueprint

**Moved into house docs 2026-09-29** from
`XO/6.robot-blue-print/ROBOT-CHAT-BLUEPRINT.md` (original location,
now removed) — referenced by name in Grok's addendum to
`RUSSIAN_DOLL_HOUSE_DESIGN/00-INDEX.md`'s 2026-09-29 thread, and
worth a permanent home alongside the other design-docs (`TERUMON-SPEC.md`,
`PALCRAFT-DESIGN.md`) it's a sibling of. Content unchanged from the
original below.

Status: **Track 1 MVP round-trip is real and working.** Personality/
bank, template-delta propagation, and the live chat window are
**designed, not built**. This document says which is which throughout
— do not read a "designed" section as "shipped."

Companion doc: `groks-thots.txt` in this same directory (strategic/
business framing). This doc is the engineering counterpart, written
2026-09-28 for other teams who need to build against or extend this
without re-deriving it from source.

---

## 1. The one-sentence architecture

**A robot is a pal (desktop entity) like any other**, its "AI-ness" is
just one more registered event `COMMAND` calling out to Gemma through
already-proven plumbing, and its personality (once built) will be a
Concept Bank — the same DESCRIBE → SCORE → VALIDATE → PROMOTE loop
every other AI feature in this house uses. There is no separate "bot
subsystem." This is deliberate: it means every tool that already works
on a pal (inventory, events-hq, desk placement, copy/paste) works on a
robot for free.

---

## 2. What is REAL and BUILT today (2026-09-28)

### 2.1 The Gemma call plumbing (proven twice now)

Two real ops exist, both hitting the same house-shared LAN Gemma
backend config (`#.desktop/ai_backend.pdl`: `gemma_lan_url`,
`gemma_lan_model` — currently `gemma3:270m` on a Mac Ollama node):

| Op | File | Mode | Output |
|---|---|---|---|
| `ai_describe.+x` | `&.widgits/entity-cli/ops/ai_describe.c` | **Constrained-format** (`TARGET: x \| STRENGTH: high\|medium\|low \| REASON: text`) | Appends a candidate line to `<entity>/pending_review.txt` |
| `ai_chat.+x` | `&.widgits/entity-cli/ops/ai_chat.c` | **Free natural-language text**, no parsing/rejection | Appends `USER: ...` / `<label>: ...` turns to `<entity>/chat_history.txt` |

Both share the exact same internal shape (duplicated per-file, not a
shared header — this house's own established convention for AI-caller
ops, see either file's own header comment for why):

```
gemma_ask(prompt) {
    write JSON request  -> /tmp/<op>_request_<pid>.json
    fork/exec connect_op.+x <url> <request> <response>   (real curl, --max-time bounded)
    fork/exec json_parser.+x <response> 'message.content' (dot-notation JSON extract, via popen)
    remove() both temp files ONLY after popen/pclose completes
    return the extracted string
}
```

**Real bug already found and fixed** (documented in `ai_describe.c`'s
own header): calling `remove(response_path)` immediately after
`popen()` returns is a race — `popen()` only forks the child
asynchronously, so the file can be deleted before `json_parser.+x` has
opened it. Fix: `remove()` only after `pclose()`. Any new op copying
this pattern must keep that ordering.

`connect_op.c` / `json_parser.c` are themselves duplicated verbatim
per-project (not shared via header/library) — confirmed, deliberate
house convention, not an oversight.

### 2.2 The event COMMAND registry (how a robot gets an action)

`#.ref/menu/event_commands.registry.pdl` is a real, live,
**zero-recompile** data-driven registry. Any already-running
events-hq/manager process picks up a new `COMMAND` block with no
restart. Format:

```
COMMAND <name>
  LABEL <shown in the event editor UI>
  FIELD1 <prompt text, or "-" if unused>
  FIELD2 <prompt text, or "-" if unused>
  PARAMS <comma-separated field names, matching {name} tokens below>
  TEMPLATE exec "$D/<path-to-op>.+x" "$ENT" "$D" '{param}' ...
END
```

- `$ENT` — resolves to the entity this event actually runs ON, via the
  real, already-built **host-context bridge**
  (`khtpm_events_hq_manager.c` / `khtpm_core_render.c`,
  `MUCHI_TARGET_ENT`, documented in `12.calendar/2026-09-20/2do.md`
  §8b/8c). This is what makes a robot's own event, triggered from
  *another* entity's inventory right-click, correctly resolve to the
  robot itself rather than the host it's sitting inside. **This bridge
  already existed before any of this AI work — it is not new.**
- `$D` — house root.

Two real `ai_*` commands are registered as of this doc:

```
COMMAND ai_describe
  LABEL AI Describe
  FIELD1 -
  FIELD2 -
  TEMPLATE exec "$D/&.widgits/entity-cli/ops/ai_describe.+x" "$ENT" "$D"
END

COMMAND ai_chat
  LABEL AI Chat
  FIELD1 Message:
  FIELD2 -
  PARAMS message
  TEMPLATE exec "$D/&.widgits/entity-cli/ops/ai_chat.+x" "$ENT" "$D" '{message}'
END
```

**Important nuance, corrected live during this build (direct user
question: "wasn't chat going to be written using events, not
hardcoded?")**: `ai_chat` registered this way is the **"/command"
scripted-line mode** — an event author picks a fixed message once,
at authoring time, and it's baked into the compiled event (same as
`show_text`'s `text=` param). It is **not** a live "type anything,
get a reply" chat box. That's mode 2, described in §4 — not built yet.

### 2.3 A real robot pal exists

`xyzfs/users/<uuid>/home/livedesk/pals/robot_chat_001/` — a real pal,
copied from `door_civ`'s minimal skeleton (the simplest existing pal
that already has a real `event_pkg/`), then customized:

- `pal.pdl` — `PAL | name | robot_chat_001`, `PAL | glyph | 🤖`
- `meta.pdl` — standard `METHOD` block (Events (hq), Dir, Inventory,
  Close, Cancel — all copied verbatim from `door_civ`, same real
  mechanism every pal uses) plus one new method, `Chat (test line)`,
  a direct shell invocation of `ai_chat.+x` for quick manual testing
  outside the event system.
- `event_pkg/pages/page_1/` — one real Common Event, hand-authored to
  match the exact compiled shape `khtpm_events_hq_manager.c`'s own
  compiler produces (`event.ir.pdl` → `event.pal` → `cmd_1.sh`),
  same precedent `door_civ`'s own page_1 already used. Trigger:
  `player-touch`. Body: calls the registered `ai_chat` COMMAND's
  underlying op with a fixed test message.
- Placed on the `teru-test` desk (`sessions/s1/desks/teru-test.pdl`),
  position `720,80`, `LIVEDESK_INDEX=75` (next free index house-wide
  at time of writing — 74 was the prior max).
- **Copy/paste-able like any other pal** — it's just a directory tree
  plus one `DESK` line, exactly the same shape as every terumon/asa/
  door_civ/book-stack already on that desk. No special-cased code
  path was added anywhere to make a robot "a robot" — the glyph and
  the files are the only thing that make it one.

**Verified live** (2026-09-28): ran `cmd_1.sh` directly —

```
[2026-09-28 02:19:00] USER: Hello! What are you?
[2026-09-28 02:19:00] robot_chat_001: Hello. I am a small robot.
```

Real reply, from the real production LAN Gemma model, through the
real registered-COMMAND op, on a real pal, appended to a real
`chat_history.txt`. This is the full extent of what's proven live —
one scripted round trip, on one entity, nothing more.

### 2.4 A real, separate incident found and fixed along the way

`terumon_001_ember`'s own desktop-icon process (`khtpm_entity.+x
<entity_dir>`) was found dead — every other pal on `teru-test` had a
live process, ember did not, so it silently vanished from the running
taskbar (though its `DESK` line in `teru-test.pdl` was never touched —
this was a live-process gap, not a data-loss bug). Root cause not
fully diagnosed (stale `module_parent.pid` suggests a crash sometime
after this session's own house boot at 00:49:57). **Relaunched** with
the same 1-argument invocation every other pal's process uses; visibly
confirmed back on the taskbar. Documented here because the same
silent-death failure mode could hit a robot pal too, and there is
currently no automated health-check that would notice.

---

## 3. What is DESIGNED but NOT built (do not implement without re-checking with the house first — designs can and do get revised)

### 3.1 Personality = an instance-scoped Concept Bank

Not a free-text personality string. The entity's own accumulated data
(history, chat log) feeds `ai_describe`'s existing DESCRIBE mechanism
to build a bank of weighted concepts — same master/spoke hub-and-spoke
shape (`&.widgits/concept-bank/data/masters/`) every other Concept
Bank consumer in this house already uses. The bank *is* the
personality; there is no second channel to keep in sync.

### 3.2 Template / delta split (population-scale bots)

One shared **TEMPLATE** bank per "species" (e.g. one shared
`robot_chat` template) holds the starting personality. Each dropped-in
**copy** holds only a **DELTA** bank — slots it has genuinely
personalized through its own conversations. Resolution: template
value, overridden by delta where the delta slot is non-empty (same
two-step lookup already specified for spoke/master resolution). A
promotion written to the TEMPLATE benefits every copy with no delta at
that slot — this is the actual mechanism that would let "teaching one
robot" propagate to a fleet, without every copy independently
re-learning the same thing, and without one copy's bad conversation
corrupting every other copy (feedback lands in delta, only a
multi-copy-convergent insight gets promoted to template).

### 3.3 The live chat window (mode 2: free natural-language chat)

What most people mean by "chat with the robot" — a persistent input
surface (a `<cli_io>` field, same generic mechanism `events-hq`'s own
`fld_amount` field already uses, or a small dedicated window shaped
like `ai-cell`'s own session view) where a human types anything and
gets a reply, not a pre-authored scripted line. Dispatch design (not
built): the model **never** calls a tool directly — it produces plain
text, and the house's own `HARNECIENT-HACK.md` pattern-match mechanism
(same "illusion of tool use" trick already proven elsewhere in this
house) turns recognized text into a real `COMMAND` dispatch
underneath. Explicit `/command` syntax (already working, §2.2) is the
fast path that skips the pattern-match step. **This window does not
exist yet** — §2.3's `robot_chat_001` currently only has the
scripted-line mode.

### 3.4 Visibility / debugging UI

Reuse an existing house UI shape (a db-hq-style real editor window) to
show a chatbot's live bank weights, recent observations, and the
reasoning behind a specific response — rather than a bespoke new
surface. A reusable **chart/dashboard-generation op** is flagged as a
real future house primitive (useful beyond just this feature), not
started.

---

## 4. Build order (why this sequence, not another)

1. ~~Prove the Gemma round trip on a real, generic op~~ — **done**
   (`ai_describe.c`, then `ai_chat.c`, same plumbing).
2. ~~Register it as a real event COMMAND, not a hardcoded call~~ —
   **done** (`ai_chat` in the registry).
3. ~~Put it on a real, dedicated robot pal, not borrowed off an
   existing terumon~~ — **done** (`robot_chat_001`).
4. **Next**: the live chat window (§3.3) — this is what turns "one
   scripted test line works" into something a human would actually
   call "chatting with a robot." Blocked on nothing technical; it's
   the next real engineering task.
5. **After that**: wire the entity's own `chat_history.txt` /
   `history.txt` into `ai_describe` as real observation input (the
   plumbing already reads `history.txt` today — `chat_history.txt` is
   the same shape, not a new mechanism) to start building the
   instance-scoped bank (§3.1).
6. **Only after 4+5 are stable**: template/delta propagation (§3.2) —
   there's nothing to propagate between copies until individual copies
   can actually accumulate real deltas.

Do not skip ahead to 5/6 without 4 — a "personality" built only from
scripted test lines isn't real personality data, it's the same fixed
string over and over.

---

## 5. Known gaps / open risks for whoever picks this up next

- **No health-check on pal desktop-icon processes.** Ember's silent
  death (§2.4) could happen to a robot pal too, with the same
  symptom: it vanishes from the taskbar while its desk-file entry
  looks completely normal. Nothing currently detects or auto-recovers
  this.
- **The scripted-line event (§2.3) hardcodes its test message.**
  That's correct for what it is (a proof of plumbing) but is not a
  template for how real event authors should use `ai_chat` — a real
  author would pick their own `message=` value per event instance,
  same as `show_text`.
- **No constrained-format safety net for chat.** `ai_describe`
  rejects and logs malformed output rather than guessing.
  `ai_chat` is deliberately unconstrained (free text is the point),
  so there is currently no defense against the model producing
  something off-brand/off-character. Once §3.1's bank exists, the
  bank itself is the natural place to ground responses — not
  something to hack around in `ai_chat.c` itself.
- **Testing this via the real relay/UI (not direct binary calls) is
  still an open item.** This session proved the round trip by
  invoking `cmd_1.sh` directly, which is honest but is not the same
  as proving a human can trigger it by actually touching the robot on
  a running desktop. `#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt`
  is the canonical testing-methodology doc for driving khtpm/events-hq
  windows via relay files rather than xdotool/PNG dumps — read it in
  full before building the live chat window's own test harness.

---

## 6. File map (for anyone extending this)

```
&.widgits/entity-cli/ops/
  ai_describe.c / .+x        constrained-format DESCRIBE call
  ai_chat.c / .+x            free-text chat call (this doc's subject)
  connect_op.c / .+x         shared-shape curl fork/exec wrapper (duplicated per project)
  json_parser.c / .+x        shared-shape dot-notation JSON extractor (duplicated per project)

#.ref/menu/event_commands.registry.pdl   real, live, zero-recompile COMMAND registry
#.desktop/ai_backend.pdl                 house-wide Gemma LAN url/model config

xyzfs/users/<uuid>/home/livedesk/
  pals/robot_chat_001/                   the real robot pal (this doc's proof)
    pal.pdl, meta.pdl, glyph.txt, instance_id.txt
    chat_history.txt                     grows with each real conversation turn
    event_pkg/pages/page_1/              the registered ai_chat event
  sessions/s1/desks/teru-test.pdl        the desk file robot_chat_001 is placed on
```

---

*Written 2026-09-28 alongside the live build described above. If a
claim here and the actual code/files disagree, trust the code — this
doc describes a real moment in time, not a contract.*
