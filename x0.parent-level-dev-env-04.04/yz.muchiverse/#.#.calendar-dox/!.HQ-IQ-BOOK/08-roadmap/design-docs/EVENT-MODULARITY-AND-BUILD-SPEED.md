# Event Modularity (drag-and-drop 🎬️ pages) and Build Speed (hash-based incremental compile)

**CORRECTION 2026-09-28 (same day, before anything below was built):**
event pages use **🎬️** (a gear ⚙️ was the other option offered), not
🧩. Direct instruction: "we may use puzzle piece for 'scratch' coding
stuff later" — 🧩 is now reserved for a DIFFERENT, not-yet-designed
future feature, not this one. Anywhere below quoting the original
same-day instruction verbatim keeps 🧩 in the quote (historical
accuracy), but the real, current symbol for this feature is 🎬️.

**Status, updated 2026-09-28 (later the same day): §2 (hash-based
incremental compile) is BUILT** (`hash_gate.sh`, wired into
`build_core_render.sh`) - direct instruction: "its not like it can
break anything... wanna knock it out really quick." **§1 (🎬️ event
pages) is still DESIGN ONLY, not started** - explicitly next in queue.

Both items were documented together because they were raised together
- they were always independent and always could land in either order;
§2 simply turned out to be the quicker one to actually build.

---

## 1. Events as real, draggable 🎬️ entities

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
> it have a page number" — 🧩 (superseded by 🎬️, see correction above)

### The design (not yet built)

**Every time an event/page is created, it also becomes a real, separate
pal** — same house-wide pal shape every other desktop entity already
uses (a real directory, real `pal.pdl`/`meta.pdl`, real
`livedesk_index`, placeable on a desk), glyph 🎬️, holding:

- The event's own compiled shape (`event.ir.pdl` / `event.pal` /
  `cmd_N.sh` — whichever of these actually needs to travel with it;
  to be confirmed once building starts, since `event.pal`/`cmd_N.sh`
  are currently *compiled output* keyed to a specific `pkg=`/`page=`
  pair per their own header comments, e.g. `robot_chat_001`'s own
  `cmd_1.sh`: `# pkg=robot_chat_001 page=page_1` — confirmed by direct
  inspection this is currently just an informational COMMENT, not a
  functionally load-bearing reference: the real body of every hand-
  authored `cmd_N.sh` this session (door_civ's, robot_chat_001's own)
  resolves its own entity dir at runtime via
  `cd "$(dirname "$0")/../../.."` — genuinely portable already, no
  baked absolute path. **Not yet confirmed for a REAL, compiler-
  generated `cmd_N.sh`** (one produced by `khtpm_events_hq_manager.c`'s
  own IR→pal→sh compiler from a real events-hq authoring session,
  rather than hand-authored) — check this before assuming every
  existing event is this portable; a real compiled one may bake in
  more than a comment.
- **A page number** — its own identity, not borrowed from whatever
  entity it's currently attached to.

**Drag-and-drop populates the target entity's event view** — dropping
a 🎬️ pal onto another entity's `event_pkg` is the real, intended
authoring action: it copies/links the page's event data into that
entity's own `pages/page_N/`, the same way any other inventory
drag-and-drop already works in this house (matches the existing
drag-into-inventory convention documented in
`13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md` for
the 🤖 robot pal).

**What this buys, per the direct instruction's own stated motivation**
("I feel more in control of that setup"):

- **Auditability**: a 🎬️ sitting on a desk, in an inventory, or in a
  shared folder is inspectable on its own — open it, see what it does,
  without needing to know which entity currently "hosts" it.
- **Sharability**: copy/paste a 🎬️ pal exactly like `robot_chat_001`
  was copy/pasted this session — no special export/import tooling
  needed, it's already just a directory.
- **Modularity**: one event definition, usable as a template dragged
  into N different entities, rather than N independently-authored
  copies drifting apart.
- **Mutability**: editing the 🎬️'s own files is editing "the event,"
  full stop — not "one entity's copy of an event that happens to also
  exist elsewhere."

### Making this real RETROACTIVELY too (direct question, 2026-09-28)

Direct question: what needs to be done to make this apply to events
that **already exist** today (every terumon's own events, `robot_chat_001`'s
own `page_1`, anything authored before this feature exists) — not just
events created going forward?

**The good news: there is no structural difference between "a brand
new event" and "an old event" from this feature's own point of view.**
An event is always the same real shape (`pages/page_N/` — `event.ir.pdl`
/ `event.pal` / `cmd_N.sh`), whether it was authored five minutes ago
or five weeks ago. That means **one real tool covers both cases** —
there's no separate "migration path," just "the export tool, run once
per existing event you want converted" instead of "run automatically
at creation time going forward." Concretely, what needs to exist:

1. **One real, generic op** — something like
   `event_page_to_pal.+x <event_pkg_dir> <page_N> <house_root>
   <target_pals_dir>`. Given ANY existing `pages/page_N/`
   (brand new or years old, doesn't matter), it:
   - Creates a new pal directory using the exact same skeleton this
     session's `robot_chat_001` build already proved out (copy a
     minimal real pal like `door_civ`'s shape for the boilerplate
     `pal.pdl`/`meta.pdl`/`event_pkg` structure, per
     `ROBOT-CHAT-BLUEPRINT.md`'s own §2.3 file map for the closest
     real precedent).
   - Generates the pal's own real 🎬️ sprite via the SAME real
     `pc_phymoji_gen.+x` / `emoji_gen_atlas.+x`+`emoji_xtract.+x`
     pipeline this session used for `robot_chat_001`'s own 🤖 —
     confirmed real, working, reusable as-is (see the bug-log/session
     notes for the exact two-binary invocation and why the FIRST
     attempt at this — pointing only at `voxels.csv` — was wrong: the
     desktop icon actually reads `sprite.csv`/`atlas.png`,
     `voxels.csv` is piececraft's 3D board only).
   - Copies the source `pages/page_N/` files into the new pal's own
     `event_pkg/pages/page_1/` (its own page numbering starts fresh,
     per the "leans toward per-🎬️ numbering" resolution to open
     question 2 below).
   - Rewrites (or strips) the `# pkg=... page=...` header comment in
     `cmd_N.sh` so it doesn't claim to belong to its old source entity
     — cosmetic if that line really is comment-only everywhere (see
     the "not yet confirmed" caveat above); load-bearing if a real
     compiled `cmd_N.sh` turns out to reference its origin somewhere
     that actually executes.

2. **A real "Export as 🎬️" button inside events-hq itself**, next to
   wherever a page is currently selected/edited — this is what makes
   "retroactive" actually mean something to a real user: open ANY
   existing entity's events-hq window, pick an existing page, hit
   Export, done. No separate bulk-migration tool needed for the normal
   case of "I want to pull this one specific event out."

3. **A real drop-target handler** for the OTHER direction (dropping a
   🎬️ onto a target entity) — this doesn't exist today in any form and
   needs to be built regardless of retroactive vs. new: it copies the
   🎬️'s own `pages/page_1/` into the target's next free
   `pages/page_N/` slot and updates that target's own
   `.hq_manager/pages.state.txt`/`ui.txt` roster (the same real files
   this session's own `ai_describe.c`/events-hq testing already
   confirmed are the real, live page-listing state) so events-hq
   actually shows the new page without a restart.

4. **Optional, only if a one-shot bulk sweep is wanted**: a thin shell
   script that walks every `xyzfs/users/*/home/livedesk/pals/*/
   event_pkg/pages/*` in the house and calls step 1's op on each one
   automatically. Not required for retroactive support to WORK (step 2
   already covers "convert this one existing event whenever I want
   to") — only useful if the goal is "convert everything that already
   exists, right now, in one pass" rather than as-touched.

**So, plainly: nothing about "retroactive" adds new design work beyond
what's already scoped above** — building the real export op (step 1)
and the events-hq button (step 2) *is* the retroactive story, not an
extra thing bolted on afterward. The only genuinely open technical risk
specific to old/existing events is the `cmd_N.sh` portability question
flagged above (comment-only vs. load-bearing `pkg=`/`page=` reference)
— worth checking against a REAL compiler-generated file before writing
step 1's op, not assumed from this session's two hand-authored
examples alone.

### Open questions to resolve before building

1. Does dropping a 🎬️ **copy** its event data into the target (each
   target gets its own independent copy, edits don't propagate), or
   **link** it (edits to the 🎬️ propagate to every entity it's been
   dropped onto)? This is the exact same copy-vs-link tension
   `AI-PUSH-ROADMAP-AND-NUANCES.md`'s template/delta bank design
   already reasons through for chatbot personalities — worth checking
   whether the same template/delta shape applies here too, rather than
   inventing a second answer to the same underlying question.
2. Does a 🎬️'s own `page number` need to be house-wide unique (like
   `LIVEDESK_INDEX`), or scoped per-🎬️ (each one starts its own
   page_1)? Leans toward the latter since a 🎬️ is meant to be
   self-contained.
3. What triggers 🎬️ creation for NEW events going forward — every
   event authored in events-hq automatically also becomes a 🎬️ pal, or
   is it always the explicit "export this event" action from the
   retroactive section above, used for old and new events alike? The
   original direct instruction ("each time an event/page was created")
   reads as automatic-for-new, but automatic creation means every new
   event has TWO real representations to keep in sync from birth —
   worth confirming this is actually wanted vs. "export is always
   explicit, for old and new events both, one consistent mental model."

---

## 2. Hash-based incremental compile

**✅ BUILT 2026-09-28 (same day as designed) — `hash_gate.sh`
(`&.widgits/_shared-lib/`), wired into `build_core_render.sh`.** All
three open questions below are resolved (answers inline, struck
through the questions themselves) and verified live. Everything below
this point is now a historical record of the design, not a to-do list
— see `hash_gate.sh`'s own header comment for the authoritative
current description.

**Verified live**: cold build 10.9s; unchanged re-run 0.45s (~24x
faster); a `touch` (mtime only, no content change) correctly still
skips, confirming this is genuinely content-hashed, not mtime-based;
a real content edit to one source correctly rebuilds only its own
dependent binary, leaving an unrelated one untouched.

**Not yet done**: rolling this out to every OTHER `build_*.sh` in the
house — only `build_core_render.sh` adopted it so far (real, working
example to copy from, not a house-wide sweep). Do that incrementally,
per-project, same as `SHARED-SOURCE-COMPILE-IN-PLACE.md`'s own rollout
was.

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
> distribution" ♻️

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

### Open questions — RESOLVED 2026-09-28, see `hash_gate.sh`'s own header for the full reasoning

1. ~~Per-file hash, or per-binary hash-of-all-inputs?~~ **Resolved:
   per-binary, combined hash of ALL real inputs.** Handles the
   shared-source fan-out case for free (a shared file changing changes
   the combined hash of every binary that lists it), no dependency
   graph needed. Re-hashing a handful of `.c` files on every build
   check is genuinely free (milliseconds) next to a real compile.
2. ~~Where does the manifest live?~~ **Resolved: one per project**
   (`.build_hashes.pdl`, next to the build script that owns it,
   gitignored as derived build-cache state — same reasoning as the
   `*.+x` binaries themselves).
3. ~~Shared shell function, or duplicated per project?~~ **Resolved:
   shared, sourced** (`hash_gate.sh`) — a build script is never linked
   into a runtime binary, so there's no drift risk to guard against by
   duplicating it; same shape as sourcing `khtpm_css_parser.c` via
   `-I`.

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
  build, cited above for the real, reusable emoji-to-sprite pipeline
  and the sprite-vs-voxels lesson §1's export op needs to repeat.
- `04-bugs/BUG-LOG.md` (2026-09-28 entry) — the always-on-top context
  menu bug, unrelated to this doc but logged the same session.
