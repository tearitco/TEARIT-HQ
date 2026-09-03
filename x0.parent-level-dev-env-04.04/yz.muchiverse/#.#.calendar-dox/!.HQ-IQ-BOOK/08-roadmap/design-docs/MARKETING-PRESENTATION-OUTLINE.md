# 🎬 Long-Form Marketing Presentation — Outline / Script (DRAFT for review)

**Status: outline only, not produced.** This organizes everything from
the 2026-08-27 brain-dump into a real structure with a clear line
between "real, demoable today" and "roadmap, describe honestly as
coming soon." Review/edit this outline before anyone builds the actual
video — a produced piece can only DEMO what's real; the rest gets
narrated as vision, clearly labeled as such.

**Legal note (resolved):** "RPG Maker" is a registered trademark. Safe
to mention it factually/comparatively ("similar to RPG Maker's event
system") — that's standard, legal comparative use. Just never imply
endorsement/affiliation or use their logo/branding. Default script
language below uses genre-neutral terms ("RPG-Maker-style event
scripting," "similar engines like RPG Maker, Unreal, etc.") and only
names specific engines when making an actual comparison point.

---

## Structure

### 1. Cold open — what this actually is (30-45s)
A real, from-scratch desktop environment ("muchiverse") built around
one idea: everything is a small program talking through plain files,
not a monolith — and because of that, an AI agent can build, test, and
show its own work on video, live, the same way a human would. That
transparency IS the pitch, not just a feature.

**Real footage source for this segment**: whole-desktop video capture
(not the per-window PNG-dump approach section 2 uses for feature
proof), showing the living desktop with its real tiles/entities
together. The right tool for this already exists in-house:
`151.screen-rec+01.02` (real PipeWire-based screen recorder, not a
per-window relay dump) — use it for this cold open and any other
whole-desktop b-roll, reserve the relay+PNG/`make_presentation_video.py`
pipeline for section 2's individual feature proofs.

### 2. Real, working, demo-able TODAY (the bulk of the video — screen-recorded live)
Each of these gets a real screen-capture demo (relay-driven, matching
this house's own testing convention — no faked footage):

- 🎮 **Event scripting** — Common Events, Triggers (None/Autorun/
  Parallel), Control Switches/Variables, Conditional Branch, Call
  Common Event (incl. nesting) — the RPG-Maker-style authoring flow,
  with the real distinction that it compiles to our OWN small VM
  (`prisc+x`), not a closed engine.
- 🗄️ **db-hq** — the database-style editor (Actors/Classes/Items/etc
  tabs, Common Events sidebar+panel).
- 🎨 **Palettes, bookmarks, color/theme change** — real, working,
  small QoL features.
- 🧸 **Toys — Mutaclysm / PieceCraft (3D)** — a real 3D game space:
  drag a desk-pal OFF the 2D desktop and INTO the 3D world to actually
  play with it there, or pull one back out to sit on the desk again.
  The desktop and the 3D world are two views of the same living
  entities, not separate silos.
- 📂 **"X11 folders" — living, runnable folders** — a folder you can
  drag things into, drag more living entities into, and then choose to
  **freeze** (pause it as a static folder) or **let run on its own**
  (same real live/frozen duality the 3D windows already have) — a
  folder that's also a small running world when you want it to be.
- 🎨 **Palettes as world design tools** — the SAME palette system shown
  in section 2's QoL features is also how 2D or 3D desk-worlds
  themselves get designed/laid out — one real tool, two real uses (UI
  theming AND world-building).
- 🌍 **Desktop → 3D world conversion** — a desk can be loaded/converted
  directly into a blank 3D world — the desktop isn't just an interface
  to the system, it's a real on-ramp into building/playing in it.
- ⏰ **Timed cron jobs** — scheduled/recurring automation.
- 🏪 **Store** — where you buy more worlds or desk-pals, or **sell your
  own** — a real marketplace for user-created content, not just a
  one-way asset shop.
- 🤖 **Watching AI agents work, live** — arguably the most unique
  differentiator: every feature in this video was itself built and
  PROVEN via real agent-driven testing (text-relay input, real state
  dumps, real recorded video) — the audience isn't just seeing a demo,
  they're seeing the actual proof-of-work process the house uses for
  itself.
- 💰 **Harnecient / token-saving delegation** — cheaper models handling
  scoped, well-specified work so the expensive model's budget goes
  further — a real, working cost-efficiency story.
- 💬 **cursword assistant** — **be honest on camera about scope**: text-
  based interaction works today; a dedicated chat UI + TTS voice is
  planned, not built yet. Say so directly rather than implying it's
  further along than it is.
- 🖥️ **The desktop itself** — logins, file system (xyzfs), sessions,
  users — the actual OS-like substrate everything above runs on.

### 3. Roadmap — described honestly as vision, not demoed as real (narrated over concept slides, no fake screen recordings)
- 🌐 **Networking** — multiplayer/shared-state across machines.
- 💬 **Better IRC/forum** — real community/communication tooling.
- 🔗 **Blockchain-backed economies** — trades, PvP, collaborative
  building economies between players — the "why" here is real and
  worth stating plainly: it's a trust/ownership layer for player-driven
  economies in games built on this system, not blockchain for its own
  sake. **Real callback to section 2**: the Store already lets users
  sell their own worlds/pals today — blockchain is the trust layer that
  extends that SAME real feature into player-to-player trades/PvP
  stakes, not a bolt-on unrelated to what already exists.
- 🕹️ **Upcoming games, purely event-driven** — built entirely on the
  event-scripting system shown in section 2 — proof that the engine
  isn't just a tech demo, it's what real upcoming titles will run on.
- 🧩 **Cross-platform plugins/customization** — user-authored
  extensions, targeting Linux/Mac/Windows from one codebase — the long-
  term "make this genuinely extensible by other people" story.
- 💬 **cursword's own chat/TTS** — call back to section 2's honest gap.

### 4. Close — the ask
"This isn't finished — that's the point of showing it now." Direct
call-to-action: DM/comment for early access, a copy, feature requests,
or the GitHub link. (Insert real GitHub URL once ready to publish
publicly — do NOT include an unpublished/private repo URL in a public
video.)

### 5. Bonus segment — 🤖 "A prompt for future agents to keep improving this"
A short, on-screen/narrated prompt block (reuse the SAME real pattern
as `share-sluggi/c-house-onboard-agent-prompt.md` and
`c-htpm-agent-onboard-prompt.md` in this same directory) - literally:
"if you're an AI agent picking this project up, here's how to get
productive fast" - pointing at the real onboarding docs already built
this session. This doubles as a real, functional artifact (viewers who
run their own agents can actually use it), not just a marketing beat.

---

## What still needs a real decision before scripting section 2 fully

- Whether section 2's demos are one continuous video or a chaptered
  series (this house's own `make_presentation_video.py` already
  supports chaptered narration — reuse it, don't build new tooling).
- The real GitHub URL to display in section 4 (once ready to be public).

## Real precedent for HOW to actually produce this once approved

This house already has: a chaptered-narration MP4 builder
(`make_presentation_video.py`), a real archive convention for finished
media (`🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/`), and real
relay-driven screen capture for every GUI feature in section 2 — no new
production tooling needed, just real footage of real features stitched
together with the vision narration from section 3.
