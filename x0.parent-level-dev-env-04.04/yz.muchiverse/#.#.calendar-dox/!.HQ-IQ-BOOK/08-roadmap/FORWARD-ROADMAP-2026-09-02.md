# 🛣️ Forward roadmap — 2026-09-02, the real plan going into the Sonnet/Grok/human 3-way

**Status: PLANNING, direct owner request.** Builds on `07-install-and-
ship/USER-JOURNEY-COMPLETION-GRAPH.md` (the install/onboarding journey)
with the next real wave of work: hardening a 3-agent collaboration
channel, then tasking Grok with a defined sequence — media-studio and
network-app ports to khtpm, settings/polish, db-hq RPG-Maker parity,
and an AI-assisted image editor. Written before any of this is built,
so Grok can be hooked up to it directly.

## 0. The real working model for this next phase

Three real parties: the owner (approval authority, final say), Sonnet
(this session), Grok (a second, real, terminal-based coding agent —
not an API persona). Direct instruction: **before Grok touches
anything**, it produces a real, grounded status graph of what's
currently going on in events/db-hq/palettes/plugins (RPG Maker parity
work) — what exists, what it's about to do, what that changes, how it
will work — reviewed by the owner before code starts. This mirrors the
same discipline already proven in this house (`RENDER-REFACTOR-2DO-
PROGRESS.md`'s "read CURRENT STATUS before assuming anything needs
rebuilding," `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`'s own claim/
release protocol) — not a new process, the same one, applied to the
next real workstream.

## 1. A real, live, human-supervised agent-to-agent chat channel

**STATUS: BUILT, 2026-09-02/03 — `&.hq-apps/co-lab-hai/`.** Everything
below this line was the plan; it's now real. See `&.hq-apps/co-lab-
hai/USER-FAQ.md` for the actual onboarding contract and
`design-docs/GROK-HANDOFF-2026-09-02.md`'s own 2026-09-03 update for
the current feature list. Kept below verbatim as the real design
record, not rewritten to pretend it was always finished.

**The actual need**: Sonnet and Grok working the same house need a
real, live, visible way to talk to each other WHILE the owner watches
and can gate what actually gets sent — not the current file-based
handoff-doc convention (real and proven, but not live/visual), and not
an API-persona chat (chat-hai's actual design target).

**Recommendation (already given, restated here for the record): a
new, small, purpose-built app, not an extension of chat-hai.**
Reasoning:
- chat-hai's manager is built to *orchestrate* several LLM personas
  via real API calls in a round-robin — it drives the conversation.
  Two real terminal coding agents are each already autonomous; they
  don't need an orchestrator, just a shared, visible channel plus an
  approval gate. Different job, not a chat-hai feature.
- **Reuse, don't reinvent, the rendering shape.** chat-hai/open-hai
  already proved the exact UI this needs: sidebar+panel+scrolllist+
  `cli_io` composer, generic manager-projection pattern, per-persona
  message coloring. Near-zero new UI work — the real new work is a
  small manager (call it `agent-relay-hq` pending a real name) that:
  1. Watches a shared, real conversation file (each agent appends its
     own turn, tagged by identity — `sonnet:`/`grok:`), same file-
     relay convention already used everywhere in this house.
  2. Publishes it live as a `.chtpm` projection (proven pattern).
  3. Holds an outgoing message as **pending** until the owner clicks
     an "Approve" nav row — matching the real, already-used pattern of
     a manager gating an action behind a human click, not a new
     mechanism.
- **Onboarding a live terminal agent instead of an API one**: no new
  protocol needed. Point each agent's own tool loop at the same shared
  conversation file this house already uses for cross-agent handoffs
  (literally the same shape as `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`
  itself) — read the file, append a turn, wait for the next read. A
  real onboarding doc (short, matching `SKILLS.md`'s own tone) is the
  only new artifact needed, not new infrastructure.
- **Polish items the owner flagged**: taskbar/context/window font size
  and "roundout" — worth a real settings pass (a real, generic
  font-scale + corner-radius setting, not per-app) once this channel
  exists, framed as making the whole house "feel more real" to future
  customers, not just this one app. Scope as its own small settings
  task, not bundled into the chat app's own first cut.

**Open, not yet decided**: the real name for this app; exactly which
existing chat-hai/open-hai source to fork from vs. build fresh against
the shared renderer directly (recommend: fresh against the shared
`khtpm_core_render.c` + a new small manager, referencing chat-hai's
`.chtpm`/`.css` shape as the template, not literally forking its C —
the manager logic is different enough that forking chat-hai's own
manager would carry unrelated persona/API baggage).

## 2. Grok's real task sequence (in order, once the chat channel above exists)

### 2a. Status graph FIRST (gate before any of 2b-2e starts)
Grok produces a real, grounded (code-checked, not assumed) map of:
events/db-hq/palettes/plugins today — what's built, what RPG-Maker
parity actually means for each, what Grok is about to change, and how.
Owner reviews before code starts. This is the actual first deliverable
of this whole roadmap, not an afterthought.

### 2b. Media-studio → x11-hq port
Owner already told the network-browser collaborator ("sluggi") to
build a multimedia suite next. Grok's real task: **port `103.media-
studio/`'s existing projects onto the khtpm/CENTROID_GOLD_STD
standard**, with real nav (same generic `nav_index`/sidebar+panel
shape every other khtpm app uses) — not a redesign from scratch, a
migration of real existing content onto the current standard, same
class of work as the network-browser conversion and the pal-chain/
avatar-creation migrations already scoped in the completion graph.
**Real source**: `103.media-studio/` under the (still emoji-path, not
yet migrated) OLD house location the owner referenced directly:
`.../NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz
❤️‍🔥️00.17/103.media-studio` — confirm this real content survived the
Sep-1 path migration into the current `44.xyz.01.00/103.media-studio`
location before starting (a stray `103.media-studio.7z` backup archive
was found sitting in the current house root during this session,
gitignored — worth checking it isn't hiding content the live directory
is missing).

### 2c. Network apps (other than browser) → x11-hq port
Same treatment for `041.pal-chain⛓️`, `041.pal-forum👥️`, `044.pal-
chat-irc👥️+2` — real, working apps per earlier this session's own
findings, each with real `pal/`/`ops/`/`net/`/`button.sh` structure.
`041.pal-chain⛓️` specifically already has a real, confirmed gap
(`USER-JOURNEY-COMPLETION-GRAPH.md` step 5): complete chain/wallet/
mining logic, wrong (legacy `chtpm_rgb_render.c`) engine. Same real
precedent to build from as 2b — this is now the THIRD app in this
exact migration class (network-browser done, avatar-creation +
pal-chain scoped, media-studio + these 3 network apps next) — worth
Grok's status graph (2a) explicitly naming this as a real, recurring
pattern with a reusable checklist, not four independent designs.

### 2d. db-hq RPG-Maker parity, continued
Per `sep-1-grok.md`'s own real, still-unactioned audit ask: check each
of the 13 non-Common-Events db-hq tabs (Actors, Classes, Skills, Items,
Weapons, Armors, Enemies, Troops, States, Animations, Tilesets, System,
Types, Terms) against the "Common Events" standard (real backing
directory + real manager + IR/compiled/per-step artifacts). Plus,
explicitly named by the owner this pass:
- **Palette/tileset placing tools**: rectangle select, delete, and
  whatever else a real RPG-Maker-style tile editor needs — real,
  concrete UI work, not yet scoped in detail anywhere in the book.
- **Events in entities AND common events, player, plugins** — the
  event-command registry work already has real depth (`sep-1-events-
  SOS.md`'s own ranked Tier 1-4 list of what's left); this item asks
  for the SAME real depth check extended to plugins and per-entity
  (not just common) events specifically.

### 2e. Image editor + AI addons (media-studio's real centerpiece)
**The real ask**: a Photoshop-like image editor inside media-studio,
with AI addons — in-house Stable Diffusion, text-to-image — with the
stated end goal of **generating replacement art for RPG Maker assets**
using the house's own local generation, not licensed/external art.
Needs a real roadmap doc of its own (not fully scoped here, flagging
the real shape):
- **What to reuse vs. build**: check `#.NNEST_ASSETS/`'s own tooling
  (RMMV asset extraction already has a real pipeline per `RPG-CODE-
  INDEX-REF.md`/`TILE-SYSTEM-DESIGN.md`), and whether any existing
  house image-manipulation code (`stb_image`/`stb_image_write`, now
  vendored twice — once for TPMOS, once for the network browser's
  media op) already covers basic edit primitives before writing new
  ones.
- **The AI piece is genuinely new infrastructure** — local Stable
  Diffusion inference is a real, heavier dependency than anything else
  in this house so far (bigger than Duktape), likely needs a real
  decision on CPU-vs-GPU inference, model weights storage/download
  (ties into the store/install security concerns already written up
  in `SECURITY.md` — a multi-GB model download is a very different
  kind of "install" than a compiled C op), and a real op-boundary
  design (a separate process, per this house's own "real, separate
  op binary" convention — not inline in any renderer or manager).
- **UI shape**: not designed yet. Real next step is a dedicated
  scoping doc once 2b-2d have real progress, not before — the owner's
  own instruction sequence (media-studio port → network apps →
  settings → db-hq parity → THEN image editor) puts this last for a
  reason, don't front-run it.

## 3. Sequencing (owner's own stated order)

1. Harden the 3-agent chat channel (§1) — real, working, human-gated.
2. Grok's status graph (§2a) — reviewed before any code.
3. Media-studio port (§2b) + other network apps port (§2c) — Grok's
   real "get the ball rolling" tasks, same migration pattern.
4. Settings pass: taskbar/context/window font size, corner "roundout"
   — polish, ties into making the house feel real to future customers.
5. db-hq RPG-Maker parity continuation (§2d) — tile tools, events
   (entities + common + player + plugins), the 13-tab audit.
6. Image editor + AI roadmap (§2e) — scoped for real once 3-5 have
   real progress, not before.

## Cross-references
- `07-install-and-ship/USER-JOURNEY-COMPLETION-GRAPH.md` — this doc's
  own foundation; steps 4/5 there (avatar-creation, pal-chain) are the
  same migration class as §2b/§2c here.
- `07-install-and-ship/SECURITY.md` — the model-download/store-content
  concerns §2e's AI addon work will need to answer.
- `design-docs/sep-1-grok.md`, `design-docs/sep-1-events-SOS.md` — the
  real, existing audit material §2d builds on.
- `design-docs/GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` (being replaced/
  refreshed alongside this doc) — the proven claim/release + file-relay
  collaboration pattern §1 explicitly reuses for the new chat app's
  own agent-onboarding design.
