# Event Scripting: Progress & Goals

For future agents (and us). Read this before touching any event-editor
variant. Companion doc: `EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md` (deep
technical history of the real CHTPM editor's own build).

## The actual goal (why any of this exists)

We don't know yet what the easiest way for a non-programmer to author game
events is. Rather than commit to one design, we're running **parallel
variants side by side** and comparing them head-to-head, cross-pollinating
whatever works. Direct user framing (2026-08-05): "there can even be a 4th
variant... we're experimenting... even if it doesn't match our
preconceived notions."

The proof of competence for any variant is the same: **use it to author
real event scripts for the pets (dog/cat/chicken) that visibly change
their behavior**, verified via real key injection (not just "looks right"
inspection).

## The four variants

1. **Real CHTPM editor** — `&.widgits/event-editor/` (root). ASCII-chrome,
   real house nav pipeline (continuous digit-jump 1..N, `[>]` focus,
   Enter-to-activate). Ops: `ee_compose_frame.c` (render), `ee_menu_input.c`
   (dispatch). Layout: `pieces/chtpm/layouts/event_editor.chtpm`.
   - Reads REAL event content now: set `EE_PKG_DIR=<pkg>/event_pkg` (env,
     passed through `button.sh` → `ee_state.txt`'s own `pkg_dir=` key) and
     `ee_compose_frame.c`'s `load_real_rows()` parses that package's own
     `event.ir.pdl` NODE rows instead of the hardcoded door_guard demo.
   - Real sprite thumbnails work: reuses the exact same mechanism as
     map-tile emoji rendering (`chtpm_rgb_render`'s `load_emoji_assets_from()`
     reverse lookup against `pieces/registry/items/items.txt`). A pkg's
     photo gets downscaled to a 16×16 `voxels_16.csv` via
     `tp_asset_to_sprite.+x`, registered under a real Unicode marker in
     items.txt, and `ee_compose_frame.c` emits that marker byte sequence
     literally into `view.txt`. Wired for `pkg=="asa"` (🎤) and
     `pkg=="ava"` (💃) so far — see `ee_compose_frame.c`'s own comments.
2. **event-mock** — `&.widgits/event-editor/gl_mock/`. Pure freeglut, no
   CHTPM nav at all (raw `glutKeyboardFunc`/`glutSpecialFunc`). Look/feel
   reference only, NOT the product path. Reachable from asa/ava's real
   desktop context menu as "Events (mock)".
3. **event-ez** — 4th variant, just started 2026-08-05. See below.
4. Scratch/visual-block pal-scripting editor — proposed, not yet started.

## event-ez (built, proven working 2026-08-05)

Explicit target UX (direct user quote): **"click nav buttons and fill in
cli-io blanks."** Leans on the house's existing
`reference_cli_io_field_mechanic` memory as the PRIMARY authoring
mechanic, not a secondary feature: digit-jump → Enter activates a field →
type → **ESC (not Enter)** deactivates; field values persist across
screens that share a `target_id`.

Location: `&.widgits/event-ez/` (own `button.sh`, `ops/ez_compose_frame.c`,
`ops/ez_menu_input.c`, `pal/main_loop_chtpm.pal`,
`pieces/chtpm/layouts/event_ez.chtpm`) — same real session-isolated
architecture as the CHTPM editor (own `system/` symlink from mutaclysm,
own PAL loop). `pal/main_loop_chtpm.pal` uses the SAME proven "check
interact_relay.txt first, unconditionally" pattern the real editor's own
dispatch bug was fixed with this session (see below) — never re-introduce
the "check screen-changed marker first" ordering, it starves the relay
check once `ping()`/`bump()` keep that marker moving every render.

Current layout: 4 behavior buttons (Chase/Flee/Wander/Idle, KEY:1-4),
2 `<cli_io>` blanks (Target entity, Speed, `target_id="ez_target"`/
`"ez_speed"`), 1 Save button (KEY:5→7 depending on digit_accum — see
`event_ez.chtpm` for the live numbering). `EZ_PKG_NAME`/`EZ_PKG_DIR` env
vars (passed the same way `EE_PKG_NAME`/`EE_PKG_DIR` are) tell it which
pkg's chrome to show and which `event_pkg/` dir Save writes into.

**Proven via real k3-style key injection** (`tp_test_send_key.+x`, exact
same tool/method used to diagnose the real editor's dispatch bug): full
digit-jump → Enter → type → Esc → Save sequence, for all three pets,
verified by reading back the actual saved `event.ir.pdl` on disk — not by
eyeballing the rendered chrome. Real saved results:
- dog: `behavior=Chase target=cat speed=fast`
- cat: `behavior=Chase target=chicken speed=fast`
- chicken: `behavior=Wander target=(none) speed=slow`

**What Save currently produces**: only a descriptive `event.ir.pdl` (same
NODE-row format the real CHTPM editor already knows how to read back via
`EE_PKG_DIR`/`load_real_rows()` — so viewing a pet's saved script through
the real editor already works today). It does NOT yet produce anything
executable — see "Next: real running scripts" below, this is the
explicit next ask.

## Next: real running scripts, not just descriptions (requested 2026-08-05)

Direct user ask, verbatim intent: saving in event-ez should show the
saved script, and a pet's own desktop "Play" button should actually RUN
it (Play on dog → dog visibly chases cat). Concretely, still to design/
build:

1. **View the saved script** — event-ez (or any variant) should render
   back what's actually saved for the current pkg, not just a "Saved: ..."
   confirmation message. (The real CHTPM editor's `EE_PKG_DIR` read-back
   already does this structurally — reuse that read path rather than
   inventing a second one in event-ez.)
2. **Editable/reloadable** — right now Save always writes fresh; there's
   no "load existing event_pkg back into the form" step. A user re-opening
   a pet's event-ez session should see their previous Chase/cat/fast
   still populated, not a blank form.
3. **Real running PAL script, not just IR** — `event.ir.pdl` is currently
   pure description (NODE rows). Saving needs to ALSO emit a real,
   executable `event.pal` (see `ee_package_init.c`'s own stub
   `event.pal` for the existing convention: `show_text "..."` / `ret` -
   real prisc+x opcodes, not free text) that actually implements the
   chosen behavior when run - e.g. Chase needs a real PAL loop that reads
   the target entity's live position and moves toward it, not just a
   textual "Chase" label.
4. **A real Play button** — needs to live on the pet's own desktop
   context menu (`methods.pdl`/`meta.pdl`, same `METHOD | ... | <real
   command>` convention already used for `Events`/`Events (mock)`) and
   invoke the pet's own `event_pkg/event.pal` for real, the same way
   `Events` invokes `event-editor`'s/`event-ez`'s own `button.sh`.
5. **Trigger conditions** — a script may need conditions on WHEN it runs
   (not just what it does once running) — e.g. "only chase if cat is
   within N tiles." Needs its own real authoring surface, likely a
   separate page/section per variant's own convention (the real editor
   already has a Pages/Fields section for this kind of thing -
   `event_editor.chtpm`'s own "-- FIELDS --"/"-- PAGES --" sections may
   already be the right home for this, worth checking before inventing a
   new mechanism).
6. **Multiple pages, each its own start condition** — implies a single
   pet may need more than one saved behavior gated by different
   conditions (e.g. "wander normally" vs "chase when cat is near"),
   not just one flat behavior slot like event-ez's current schema.
7. **href-triggered subconditions** — direct user phrase, not yet fully
   specified; likely means CHTPM's own `onClick="..."` href-style
   navigation between condition pages/sub-screens, analogous to how
   `event_editor.chtpm`'s own `EE:PAGE:N` buttons already navigate
   between pages. Needs a concrete design pass before building - flag
   with the user which existing convention (if any) this should reuse.

None of items 1-7 above are built yet. This section is the explicit
scope for whichever session picks this up next.

## Design: event PAGES, RPG-Maker-modeled (proposed 2026-08-05, not yet built)

Direct user instruction: model this on real RPG Maker MV event pages —
"this is rpg maker like, use that info" — rather than inventing our own
scheme. Concrete worked example the user gave: ava should have an event
page whose trigger is "on spawn" and whose action opens the pets on
desktop. Below is the real RPG Maker MV page model, mapped field-by-field
onto what this house already has vs. what's genuinely new.

### Real RPG Maker MV event page fields (the actual reference)

An RPG Maker MV event is a **list of pages** (the editor's own numbered
tabs, 1..20, "New Event Page" button). Only the LAST page whose
conditions are all true is active at any moment — pages are evaluated
top to bottom, first match from the top wins in RPG Maker's real engine
... actually RPG Maker uses **last matching page**, so pages are usually
authored least-specific-first, most-specific-last. Each page has:
- **Conditions**: any combination of Switch (a global switch is ON),
  Switch 2 (second independent switch), Variable (compare an operator +
  value), Self Switch (A/B/C/D, this EVENT's own private switch, ON),
  Actor (specific party member present). All are AND'd together; no
  conditions = page always active.
- **Graphic**: character sprite + pattern/direction override for this page.
- **Autonomous Movement**: Fixed / Random / Approach / Custom route, speed, freq.
- **Options**: Walking Animation, Stepping Animation, Direction Fix,
  Through, Priority (Below/Same as/Above characters).
- **Trigger**: Action Button (player presses confirm while facing it),
  Player Touch (player walks into it), Event Touch (it walks into
  player), Autorun (fires once, blocks all other input, the MOMENT the
  page becomes active), Parallel Process (runs continuously in the
  background for as long as the page is active, non-blocking).
- **Command list**: the actual event commands (Show Text, Control
  Switches, Transfer Player, etc — what our own `event.pal`/
  `event.ir.pdl` NODE rows already represent).

### Mapping onto this house's real desktop-entity model

| RPG Maker MV field | This house's equivalent | Status |
|---|---|---|
| Self Switch A-D | `state.txt`'s own `self_A`/`self_B`/`self_C`/`self_D` fields | **Already real** — `ee_package_init.c` writes these into every package today, unused by any page/condition logic yet. Reuse as-is. |
| Switch / Switch 2 (global) | — | **Genuinely new.** No global switch registry exists in this house yet (confirmed: no `switches.txt`, nothing in `prisc+x.c` beyond the C `switch` statement). Needed only once a page's condition must depend on cross-entity global state (not needed for ava's own "on spawn" example — skip building this until a real page actually needs it). |
| Variable + compare | — | Same as above — genuinely new, defer until needed. |
| Actor in party | — | Doesn't apply to this house's model (no party system) — drop this condition type entirely, don't build a stand-in. |
| Trigger: Autorun | **New: "On Spawn"** | Not a stock RPG Maker trigger (there's no "map load" moment for a standalone desktop entity) but the closest real analog — fires once, the moment the entity's own desktop window process starts (i.e. inside `button.sh`'s own `_start_session`/`ensure_package`+spawn sequence). This is ava's own example. |
| Trigger: Action Button | **Desktop context-menu click** | Maps directly onto `tp_desktop_window.c`'s own right-click METHOD dispatch (same mechanism `Events`/`Play` already use) — a page with this trigger becomes a real menu row. |
| Trigger: Parallel Process | **Background loop while window is open** | Real, needed for continuous AI (wander/chase) — the pet's own `tp_desktop_window` process (or a companion process it spawns) runs the page's `event.pal` on a real loop for as long as the window lives. |
| Trigger: Player Touch / Event Touch | — | Doesn't apply yet — this house has no live player-avatar-on-a-shared-tile-grid concept for desktop entities (tile-picker's own grid-aligned desktop positions exist, so a future "On Proximity" trigger is a plausible later analog, but there's no concrete use case yet — don't build it speculatively). |
| Graphic / Autonomous Movement / Options / Priority | — | **REVISED 2026-08-05** — direct instruction: "this is basic stuff and stuff we should support, going forward" (re: `1.event-menu-flo-grok.md`'s own full page-field list). Desktop entities already have a real, SEPARATE sprite system (`sprite.csv`) and grid-position system, so these fields shouldn't be duplicated wholesale - but `condition.pdl`'s own schema should leave real room for a page to OVERRIDE them (e.g. `graphic=<sprite override>`, `move_type=fixed\|random\|approach\|custom`, `through=0\|1`, `priority=below\|same\|above`) the same way RPG Maker's own page-level graphic override works, rather than treating these as permanently out-of-scope. Not built yet - reserve the field names in `condition.pdl` when the page-editor's own Command Parameter screen (§ Full nested flow, level 4) gets built, don't retrofit later. |
| Command list | `event.ir.pdl` (description) + `event.pal` (real prisc+x opcodes) | Already the right shape (see `ee_package_init.c`'s own stub `event.pal`) — a page's command list IS its own `event.pal`. |

### Proposed real data shape (not yet built)

```
<pkg>/event_pkg/
  pages/
    page_1/
      condition.pdl      # SECTION|KEY|VALUE: self_switch=A|B|C|D or none,
                          #   trigger=on_spawn|on_click|parallel
      event.ir.pdl        # description (existing NODE-row format)
      event.pal            # real, executable prisc+x opcodes
    page_2/
      ...
```
`pages/` starts with exactly one blank `page_1/` on package init (matches
RPG Maker's own "an event always has at least page 1"). A page gallery
`[+]` action just runs `ee_package_init.c`-style scaffolding again under
the next `page_N/` slot.

### Concrete worked example: ava's "on spawn, open the pets"

- `pages/page_1/condition.pdl`: `trigger=on_spawn`, no self-switch
  condition (always fires).
- `pages/page_1/event.pal`: a real `exec` opcode (same mechanism
  `prisc+x.c`'s own `OP_EXEC`/`exec_custom_op` already uses for launching
  `Events`/`Play` from a context menu) invoking each pet's own
  `button.sh run` — e.g. `exec "<house>/@.apps/pets/pieces/dog/button.sh" run`
  and same for cat/chicken (or, once a pets-wide orchestrator exists,
  just that one script).
- Wiring: `asa-&-ava/pieces/ava/button.sh`'s own `_start_session` (or
  its `ensure_package`) would need to check for an `on_spawn` page and
  run its `event.pal` once, right after the window itself spawns —
  this is the one piece of NEW C/shell logic this example actually
  requires; everything else above is data shape + reused mechanism.

### Page-gallery-first UI (applies to every variant, not just event-ez) — REFINED 2026-08-05

Direct user instruction: "event pages should be the first thing user
sees in event editor." Confirmed via direct follow-up question/answer
that "page" here means the real page LIST itself (page_1, page_2... each
its own trigger+condition+script — the same `pages/` data model above),
not a command list inside one page.

**Confirmed refined layout**, direct user description:
```
+==================================+
| Event Name: [ Dog Behavior     ]  |   <- cli_io, TOP of screen
+==================================+
| -- CONDITIONS (fill in later) --  |
| Switch/Variable/Actor/Item: TBD   |   <- current Trigger/Target/Speed/
|                                    |      Command fields below are a
|                                    |      real, working, but PREMATURE
|                                    |      stand-in for this - direct
|                                    |      user framing: "the current
|                                    |      event examples are premature"
+==================================+
| (existing behavior/trigger/command|
|  fields stay here for now)        |
+==================================+
| -- PAGES --  (bottom of screen)   |
| 1. [ ] empty          <- click    |
+==================================+
```
Clicking an empty (or already-filled) page row navigates to that page's
own editor screen (trigger/condition/action - the existing event-ez
main screen). Saving there returns to THIS page-list screen, now showing
that page filled in (e.g. `1. Chase target=cat (on_click)`) with a NEW
empty page row appended below it (`2. [ ] empty`) - the list only ever
grows by one at a time, matching RPG Maker's own "always one more empty
slot at the end" convention.

**Real navigation mechanism, confirmed via direct chtpm_parser_pal.c
read (not guessed)**: a SEPARATE, real `href="<path>.chtpm"` attribute
exists on interactive elements, distinct from `onClick` - committing an
element with `href` set switches `current_layout` to that file (a real,
already-proven, already-bug-fixed screen transition - see that file's
own "REAL BUG, LIVE-CAUGHT (pal-chain's own 2-screen href test..." for
the leaked-process bug already found+fixed there). This is a genuinely
different, safer mechanism than trying to invent new `onClick` prefixes
(which are silently rejected unless recognized - see this doc's earlier
note on `EE:ROW:%d`'s own latent bug).

**Real open technical question from earlier, now RESOLVED (not guessed)**:
`href` is a single fixed string per element, so a dynamically-generated
page row needs some way to communicate WHICH page number the next screen
edits. Confirmed via direct source read of `chtpm_parser_pal.c`'s own
`parse_chtm()`: it writes the currently-active layout's own path into
`pieces/display/current_layout.txt` on EVERY screen switch ("EXPORT
CURRENT LAYOUT FOR MODULE HEARTBEAT" - a real, already-existing signal,
not invented for this). This means: generate a genuinely separate,
numbered `.chtpm` file per page (`event_ez_page_1.chtpm`,
`event_ez_page_2.chtpm`, ...), each row's `href` points at its own real
file, and whichever op composes that screen just reads
`pieces/display/current_layout.txt` and parses the page number straight
out of the filename (e.g. `strstr(path, "page_")` then `atoi`). No
onClick-carrying-a-parameter trick needed at all - matches approach (a)
from the original two candidates, now confirmed viable via real source,
not assumption.

### Full nested flow, RPG-Maker-MV-accurate (confirmed understanding 2026-08-05)

Direct user description, confirmed correct: this is 4 real nested screen
levels, not 2:

1. **Event Gallery** (per-pkg, one screen) - Event Name field, page list
   at bottom. Page-list actions: **New Page** (blank), **Copy Page**
   (duplicate an existing page's own condition+commands into a new
   page slot) - both real RPG Maker MV event-editor conventions
   (right-click a page tab → New Event Page / Copy Event Page).
2. **Page screen** (one per page, `event_ez_page_N.chtpm`) - this
   page's own Conditions (Switch/Variable/Self-Switch/Actor - deferred,
   see below) + Trigger (on_spawn/on_click/parallel - already real) +
   a growing **command list**, starting empty. Back button returns to
   the Gallery.
3. **Command Picker** (one shared screen, reached by clicking an empty
   command-list slot on a Page screen) - a categorized list of real
   command TYPES (RPG Maker's own real set: Show Text, Control
   Switches, Control Variables, Conditional Branch, Transfer/Move,
   Script/Exec, etc - our own house equivalent doesn't need the FULL
   RPG Maker command set, just whichever real commands this house's own
   `event.pal`/prisc+x opcodes actually support, e.g. `exec`,
   `show_text`, `ret` today). Picking a type navigates to step 4.
4. **Command Parameter screen** (one per command TYPE, e.g.
   `event_ez_cmd_exec.chtpm` for the `exec` command's own "what shell
   command?" field) - fills in that command's own real parameters,
   confirming returns to the PAGE screen (step 2) with a new numbered
   command-list row filled in, and a fresh empty row appended below it
   - same "always one more empty slot" growth pattern as the page list
   itself.

Each screen transition (1→2, 2→3, 3→4, 4→2, 2→1) is a real `href` to a
real, distinct `.chtpm` file, using the same `current_layout.txt`
self-identification mechanism confirmed above wherever a screen needs to
know "which page/command am I."

**Nothing in this section is built yet.** This is the agreed, now
CONFIRMED-CORRECT design shape (user confirmed understanding of the full
4-level flow directly) - next session's concrete job, smallest real
slice first: (a) the Gallery↔Page 2-screen pair (New Page/Copy Page +
per-page condition/trigger, no command list yet - this alone finishes
what's already ~80% built in event-ez), proven via real k3 injection,
THEN (b) the Command Picker/Parameter pair once (a) is solid. The Event
Name field and the real Switch/Variable/Actor/Item condition model are
both real, confirmed-wanted, but explicitly deferred - direct user
framing: "we can fill these out later."

**RESOLVED, 2026-08-05** (root-caused via the exact instrumentation this
section itself prescribed - `do_jump()`/href-commit tracing, this time
actually captured against real k3-injected sequences, env-gated behind
`CHTPM_NAV_DEBUG=1` so it's a permanent, harmless, opt-in diagnostic in
`chtpm_parser_pal.c`, not a throwaway).

**The real finding: `do_jump()`/href-commit were never actually broken.**
Every single real trace captured (digit "1"+Enter from the Gallery,
digit "13"+Enter for "Back to Pages" from a Page) showed CORRECT
targeting - `do_jump(1)` matched the real "page 1" button and committed
to `event_ez_page_1.chtpm`; `do_jump(13)` matched "Back to Pages" and
committed back to `event_ez.chtpm`. The ORIGINAL "wrong page" report is
now believed to have been a **test-environment artifact, not a real
product bug**: every `gl_mirror` window this session opens shares the
literal same window title, `"mutaclsym RGB mirror"` - if more than one
event-ez session is alive at once (very easy to do by accident - a
backgrounded `button.sh run-widget` whose trap-based cleanup didn't fire,
e.g. because the launching shell was itself backgrounded/disowned),
name-substring key-injection tools (`tp_test_send_key.+x` and similar)
have no way to tell the windows apart and may silently send real
keystrokes to a stale, unrelated session. This was confirmed live: this
exact session repeatedly found 2+ leftover `"mutaclsym RGB mirror"`
windows still alive from earlier test runs, and only after killing every
stray process and confirming exactly ONE such window existed did
navigation become fully predictable and correct on every single test.

**Two REAL bugs were found and fixed in the process** (genuine, unrelated
to the "wrong page" scare):
1. **Non-atomic `gui_state.txt` write race**, `ez_compose_frame.c`: this
   file gets rewritten by a SEPARATE process on every PAL-loop tick that
   processed a key, while `chtpm_parser_pal`'s own `load_vars()` reads
   the exact same file concurrently from a different process - a plain
   `fopen(gui, "w")` (truncate-in-place, not atomic) meant a torn read
   was possible (parser catching the file mid-truncate). Fixed: write to
   a real `.tmp` file, then `rename()` over the real path - same shape
   this house's own livedesk registry files already use correctly.
2. **Cosmetic double/nested numbering**, `ez_compose_frame.c`'s own
   Gallery row labels: it baked its OWN `"N. [trigger]"` numbering into
   the label text, while `chtpm_parser_pal`'s `render_element()` ALREADY
   prepends a real `[ ]`/`[>]` cursor + real auto-numbered `"N."` for
   every navigable button automatically - producing genuinely confusing,
   nested-looking output (`[ ] 1. [1. [on-click]]`) that LOOKED like
   element duplication but traced as one single, correctly-numbered real
   element every time. Fixed: labels now carry only the real content;
   chtpm's own real nav chrome supplies the number/bracket.

**Lesson for future k3-injection testing, written down so it isn't
relearned the hard way again**: before trusting ANY test result from a
GL-mirrored CHTPM session, confirm via `ps aux`/`xwininfo -root -tree`
that exactly ONE matching process/window exists - session cleanup
(`button.sh`'s own `trap ... EXIT INT TERM`) does not reliably fire when
the launching shell itself was backgrounded/disowned, and stale sessions
sharing the same window title are a real, easy-to-hit trap.

### Real RPG Maker MV RUNTIME loop — distinct from the EDITOR flow above (documented 2026-08-05, source: user-provided reference docs)

Everything above (§ Full nested flow) describes the real EDITOR's own
navigation. The real game's own RUNTIME behavior once an event actually
exists in a live game is a SEPARATE, real algorithm, confirmed via two
reference docs the user supplied (`/home/no/Downloads/1.event-menu-flo-
grok.md`, `/home/no/Downloads/2.event-play-flo-grok.md`) - important
because our own current "Play" button is a simplification of this, not
the real thing yet, and that gap should be tracked explicitly:

```
Game loop
  → For each map event:
      Find highest page whose conditions are all true → set as active page
      (update graphic / movement / trigger from that page)

  → If trigger of the active page is satisfied:
      Hand the active page's command list to the Interpreter
      Run commands top → bottom (with branching, waits, common-event calls, etc.)
```

Key real semantics, easy to get subtly wrong:
- Pages are checked **highest-numbered → lowest**, and the active page is
  the FIRST (highest) one whose conditions ALL pass (AND'ed, not OR'ed).
  This is NOT "first page that matches, low to high" - a later, more
  specific page should be authored with MORE conditions and a HIGHER
  number, so it "wins" over an earlier, more general fallback page when
  both would otherwise qualify.
- If NO page's conditions pass, the event is effectively invisible/inert
  - does nothing, shows nothing.
- Page activity (which page is "active") is evaluated continuously/on
  every relevant state change - completely independent of triggering.
  An event's active page can change mid-game the instant a switch/
  variable/self-switch changes, even with no player interaction at all.
- Once a page IS active, ITS OWN trigger type governs what happens next:
  Action Button/Player Touch/Event Touch wait for real player
  interaction; Autorun fires immediately once the page becomes active
  (and blocks other input while running); Parallel Process starts
  immediately and keeps running continuously in its own background
  interpreter.
- Only ONE page is ever active for a given event at a time - never a
  blend/stack of multiple pages' own commands.
- There is no separate "page common events" step - Common Events are
  only reachable via an explicit "Call Common Event" command inside a
  page's own command list, not automatic.

**Real gap to track**: our own current `Play` method (this session) just
runs `pages/page_1/event.pal` directly and unconditionally - it does
NOT yet implement highest-page-wins condition evaluation, and does not
yet distinguish trigger types at RUNTIME (on_click today just means "the
Play button was clicked," it doesn't yet gate/watch for that trigger the
way a real Parallel Process would run continuously, or the way Autorun
would fire without any click at all). This is fine for the current
smallest-slice goal (prove ONE page's real script executes for real via
Play), but the REAL condition-evaluation + trigger-watching runtime loop
above is real future work, not yet started, and depends on the deferred
Switch/Variable/Self-Switch/Actor condition model existing first.

## Pets: what's real right now (2026-08-05)

- Dog/cat/chicken are now spawned as real desktop windows for the first
  time (`@.apps/pets/pieces/{dog,cat,chicken}/button.sh`, mirrors
  `asa-&-ava`'s own per-entity button.sh pattern exactly — own glyph,
  own grid slot, own `methods.pdl`).
- Real sprites: extracted from a real RPG Maker MV character sheet
  (`.../RMMV+CODE/48/characters_48x48/Nature.png`, confirmed 576×384,
  slot 0=dog, slot 1=cat, slot 2=chicken) via
  `tp_rmmv_character_extract.+x`.
- **Real bug found and fixed this session**: that op's own
  `downscale_to_NxN()` box filter was called with `N=64` against a 48px
  source frame — an upscale (ratio 0.75), which silently left many
  destination pixels as **uninitialized heap memory** (`malloc`, not
  `calloc`) whenever the truncated box window came out empty. That's
  what the user was seeing as "grainy, translucent, multi-color static."
  Fixed by clamping the box window to at least 1 source pixel. Confirmed
  fixed via direct byte comparison against a clean Python crop of the
  same real source region.
- Each pet now has a real `event_pkg/event.ir.pdl`, authored through
  event-ez (see above): dog=Chase/cat/fast, cat=Chase/chicken/fast,
  chicken=Wander/(none)/slow. **Still missing**: an `Events` row on the
  pets' own `methods.pdl`/`meta.pdl` (asa/ava have one, pets don't yet -
  copy that exact wiring: `EE_PKG_NAME=<pet> EE_PKG_DIR=<pet>/event_pkg
  sh .../event-editor/button.sh run-widget`, same for event-ez).
- These `event.ir.pdl` files are pure description right now, not
  runnable — see "Next: real running scripts" above for the full,
  currently-unbuilt list this implies (real `event.pal`, a real Play
  button, trigger conditions, multi-page conditions, href subconditions).

## Open threads (roughly in the order the user raised them)

- [x] Real event scripts for dog/cat/chicken — done via event-ez, see above.
- [x] event-ez: built, proven via k3-style key injection.
- [ ] Wire pets' own `methods.pdl`/`meta.pdl` with real `Events` rows
      (currently only asa/ava have this).
- [ ] Real running scripts (event.pal), a real Play button, trigger
      conditions, multi-page conditions, href subconditions — see
      "Next: real running scripts" section above for the full breakdown.
      This is the current top priority.
- [ ] Later: male/female chickens, egg-laying/pooping (explicitly
      deferred by user, lowest priority).
- [ ] Scratch/visual-block editor (4th-or-5th variant) — not started.
- [ ] Line-of-sight AI for pets (dog chase cat, cat chase chicken,
      wander otherwise) — "eventable AI," i.e. the behavior itself should
      live in the pet's own event package, not hardcoded C. This is
      effectively the SAME work as "real running scripts" above, once
      event.pal actually implements Chase/Wander for real.
