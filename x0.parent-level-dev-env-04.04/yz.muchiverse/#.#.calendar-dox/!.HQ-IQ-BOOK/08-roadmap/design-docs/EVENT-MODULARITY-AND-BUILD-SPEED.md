# Event Modularity (drag-and-drop 🧩 pages) and Build Speed (hash-based incremental compile)

**Status: DESIGN ONLY, not started.** Two real, separate house-quality
concerns raised 2026-09-28, deliberately parked here as work to do
*before* continuing the AI push further, per direct instruction:
address auditability/sharability/modularity/mutability of events going
forward, and the embarrassingly slow compile times, before piling more
AI feature work on top of the current event architecture.

Both items are documented together because they were raised together,
but they are independent and can land in either order.

---

## 1. Events as real, draggable 🧩 entities

### The problem today

An event/Common Event currently only exists as files buried inside an
entity's own `event_pkg/pages/page_N/` tree (`event.ir.pdl` →
`event.pal` → `cmd_N.sh`, per `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`
and this session's own `robot_chat_001` build). It is:

- **Not auditable** as a standalone thing — you can't look at "an
  event" without opening the specific entity that happens to own it.
- **Not shareable** — copying an event to a different entity means
  manually copying files inside `event_pkg/pages/`, not a real,
  intentional user action.
- **Not modular** — an event has no independent identity; it's a
  side effect of which entity's directory it happens to sit in.
- **Not mutable in a controlled way** — editing it means editing
  files nested three directories deep inside a specific pal.

### Direct instruction (2026-09-28)

> "may have it create 'page' or 'puzzle' each time an event/page was
> created. so it can literally be drag and dropped and it would
> populate that entity event view. i also like that i feel more in
> control of that setup. do it."
>
> "it should just create the event pdl/pal (whichever it is) and have
> it have a page number" — 🧩

This also resolves the emoji-reservation question raised earlier the
same day ("actual 'event' pages, if they are ever separable as
entities, should reserve puzzle piece") — 🧩 is now spoken for
specifically for this purpose, distinct from 🤖 (chat/companion pals,
see `ROBOT-CHAT-BLUEPRINT.md` in `XO/6.robot-blue-print/`).

### The design (not yet built)

**Every time an event/page is created, it also becomes a real, separate
pal** — same house-wide pal shape every other desktop entity already
uses (a real directory, real `pal.pdl`/`meta.pdl`, real
`livedesk_index`, placeable on a desk), glyph 🧩, holding:

- The event's own compiled shape (`event.ir.pdl` / `event.pal` /
  `cmd_N.sh` — whichever of these actually needs to travel with it;
  to be confirmed once building starts, since `event.pal`/`cmd_N.sh`
  are currently *compiled output* keyed to a specific `pkg=`/`page=`
  pair per their own header comments, e.g. `robot_chat_001`'s own
  `cmd_1.sh`: `# pkg=robot_chat_001 page=page_1` — this reference
  needs to either become relative/portable, or get rewritten on drop).
- **A page number** — its own identity, not borrowed from whatever
  entity it's currently attached to.

**Drag-and-drop populates the target entity's event view** — dropping
a 🧩 pal onto another entity's `event_pkg` is the real, intended
authoring action: it copies/links the puzzle's event data into that
entity's own `pages/page_N/`, the same way any other inventory
drag-and-drop already works in this house (matches the existing
drag-into-inventory convention documented in
`13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md` for
the 🤖 robot pal).

**What this buys, per the direct instruction's own stated motivation**
("I feel more in control of that setup"):

- **Auditability**: a 🧩 sitting on a desk, in an inventory, or in a
  shared folder is inspectable on its own — open it, see what it does,
  without needing to know which entity currently "hosts" it.
- **Sharability**: copy/paste a 🧩 pal exactly like `robot_chat_001`
  was copy/pasted this session — no special export/import tooling
  needed, it's already just a directory.
- **Modularity**: one event definition, usable as a template dragged
  into N different entities, rather than N independently-authored
  copies drifting apart.
- **Mutability**: editing the 🧩's own files is editing "the event,"
  full stop — not "one entity's copy of an event that happens to also
  exist elsewhere."

### Open questions to resolve before building

1. Does dropping a 🧩 **copy** its event data into the target (each
   target gets its own independent copy, edits don't propagate), or
   **link** it (edits to the 🧩 propagate to every entity it's been
   dropped onto)? This is the exact same copy-vs-link tension
   `AI-PUSH-ROADMAP-AND-NUANCES.md`'s template/delta bank design
   already reasons through for chatbot personalities — worth checking
   whether the same template/delta shape applies here too, rather than
   inventing a second answer to the same underlying question.
2. Does a 🧩's own `page number` need to be house-wide unique (like
   `LIVEDESK_INDEX`), or scoped per-🧩 (each one starts its own
   page_1)? Leans toward the latter since a 🧩 is meant to be
   self-contained.
3. What triggers 🧩 creation — every event authored in events-hq
   automatically also becomes a 🧩 pal, or is it an explicit "export
   this event as a puzzle" action? The direct instruction ("each time
   an event/page was created") reads as automatic, but this should be
   confirmed before building, since automatic creation means every
   event now has TWO real representations to keep in sync.

---

## 2. Hash-based incremental compile

### The problem today

Every `build_*.sh` script in this house does a full recompile of its
target(s) on every invocation, regardless of what actually changed.
Direct quote: "its embarrassing to have such slow compile time."
`SHARED-SOURCE-COMPILE-IN-PLACE.md` (2026-09-09/11) already fixed
*where* shared source compiles from (canonical file via `-I`, never a
copied duplicate) — this is a different, complementary problem: *how
often* anything recompiles at all.

### Direct instruction (2026-09-28)

> "surely could track last changed files thru a stored hash and only
> compile those. esp since u can use that later for blockchain
> distribution" 🧩♻️

### The design (not yet built)

A real, stored manifest (plain house `.pdl`, matching every other
house convention — no new file format) mapping each real source
file's path to the hash of its contents at last successful compile:

```
HASH | <path/to/file.c> | <sha256 or similar>
```

A build script, before compiling a target, hashes its current source
file(s) and compares against the stored manifest:

- **Hash unchanged** → skip compiling this file, reuse the existing
  `.+x`/`.o` output.
- **Hash changed** → recompile, then update the manifest entry.

This needs to correctly account for the **shared-source** convention
`SHARED-SOURCE-COMPILE-IN-PLACE.md` already established — a canonical
shared file (e.g. `khtpm_render_core.c`) is `-I`'d into *multiple*
binaries, so a hash-based skip needs to track "this binary's inputs,"
not just "this file changed," or a shared-file change could correctly
trigger 6 rebuilds but a hash-skip naively keyed per-binary-name could
miss that. The manifest granularity (per source file vs. per compiled
binary vs. both) needs to be worked out before building, not assumed.

**The blockchain-distribution angle, named directly in the
instruction**: this same content-hash-per-file mechanism is exactly
the kind of thing `AI-PUSH-ROADMAP-AND-NUANCES.md`'s already-locked-in
default ("every game's entity/gold/item creation should default to
writing against a personal, per-game blockchain backend from day one")
would want anyway — a hash-addressed build manifest is a natural,
free byproduct that could later double as the content-addressing layer
for distributing compiled binaries/assets over the (not-yet-built)
cross-machine networking layer (`CROSS-MACHINE-NETWORKING-PLAN.md`).
Worth designing the hash manifest format with that reuse in mind from
day one, rather than building a compile-cache-only format now and a
separate content-addressing format later.

### Open questions to resolve before building

1. Per-file hash, or per-binary hash-of-all-inputs (handles the
   shared-source fan-out case above more simply, at the cost of
   re-hashing every input on every build check even when most are
   unchanged)?
2. Where does the manifest live — one house-wide file, or one per
   project/binary (matching the per-project-duplication convention
   already established for AI-caller ops)?
3. Does a build script's own hash-check logic get written once as a
   real shared shell function sourced by every `build_*.sh` (a genuine
   case FOR sharing, unlike the AI-op duplication convention — build
   scripts aren't compiled binaries, so the "no shared headers between
   compiled units" rule doesn't obviously apply the same way here)?

---

## Related

- `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md` — the current, real event
  COMMAND registry mechanism §1 builds a portability layer on top of.
- `AI-PUSH-ROADMAP-AND-NUANCES.md` — the chatbot template/delta design
  §1's copy-vs-link question should check against before inventing a
  second answer to the same tension.
- `SHARED-SOURCE-COMPILE-IN-PLACE.md` — solved the compile-from-copy
  problem; §2 solves the compile-too-often problem, distinct and
  complementary.
- `CROSS-MACHINE-NETWORKING-PLAN.md` — where §2's content-addressing
  reuse would eventually plug in, once that plan's own first milestone
  lands.
- `XO/6.robot-blue-print/ROBOT-CHAT-BLUEPRINT.md` — 🤖's own real
  build, referenced above for why 🧩 needed to be a different emoji.
