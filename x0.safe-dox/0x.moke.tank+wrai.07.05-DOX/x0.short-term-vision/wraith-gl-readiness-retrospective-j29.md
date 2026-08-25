# Wraith GL Readiness Retrospective J29

Date: 2026-06-29
Status: Progress checkpoint and next-lane guidance.

==================================================
1. WHY THIS DOC EXISTS
==================================================

We are at a point where it would be easy to keep polishing framework seams forever.

That would be a mistake.

Wraith is now good enough to support real project/game/application work if we keep fixing small engine problems in context instead of trying to finish every abstraction in advance.

This doc records:

- what is already good enough
- what still matters immediately
- what should wait for project-driven pressure
- how to steer toward longer-term HTML/browser compatibility without derailing short-term momentum

==================================================
2. CURRENT POSITION
==================================================

Wraith is no longer just a proof shell.

What is now working well enough:

- hosted projects under `projects/wraith-alpha/wraith-projects/`
- launcher discovery in the right ownership location
- ASCII mother-terminal audit path
- GL desktop/window host path
- project-owned `wraith_body.txt` + `scene.objects.pdl` contract
- project-owned input ops
- project-owned watcher/marker update seam
- nav truth across ASCII and GL for the `fs` project
- reusable scroll/thumb strip pattern
- image/audio/video proof in the active Wraith lane
- project-owned redraw markers for dynamic surfaces such as video

This means we are no longer blocked on "can Wraith host projects at all?"

It can.

==================================================
3. WHAT IS GOOD ENOUGH TO STOP OVER-POLISHING
==================================================

For now, we do not need to fully finish `chtpmgl` as a grand system before making projects.

We mainly need confidence that Wraith can do:

- ASCII/GL surface switching
- image loading
- audio loading/playback
- video loading/playback
- object / 3D asset display in `game_map` / `canvas` style zones

Everything else can harden while building real projects.

That is the right trade now.

==================================================
4. WHAT SHOULD BE COPIED EXACTLY VS REBUILT
==================================================

Near-term `chtmgl-wraith` goal:

- render the same practical media/layout features that `projects/chtmgl-alpha` is demonstrating

But:

- do not blindly copy `chtmgl-alpha` as the final architecture
- do not create a second unrelated Wraith-only UI language
- do not let exact visual matching outrank auditability

Use `chtmgl-alpha` as a reference for:

- image surface handling
- menu/container vocabulary
- slider/checkbox/menuitem presence
- canvas / 3D preview zones

Do not use it as proof that the deeper architecture is already finished.

==================================================
5. THE BIGGEST REMAINING GAPS
==================================================

The main glaring gaps I currently see are:

1. media support parity is still not finished as a generalized platform contract
   - but image/audio/video are now explicitly proven in the active lane
   - the remaining gap is standardization and reuse, not first proof

2. `chtmgl-wraith` is still more probe than full pipeline
   - it is not yet proving the same end-to-end certainty that `fs` now proves for ASCII/nav/state

3. GL and ASCII still need a cleaner shared semantic middle
   - we have the right rule now
   - we have not fully normalized the implementation around it

4. widget semantics are still shallow
   - `button`, `checkbox`, `slider`, `menuitem` still need cleaner state/render/action separation over time

These are real, but they are not a reason to delay all project work.

==================================================
6. RECOMMENDED SHORT-TERM STRATEGY
==================================================

Short-term strategy should be:

1. Start building real Wraith projects.
2. Fix engine issues only when a real project exposes them.
3. Keep the ASCII audit contract non-negotiable.
4. Harden media support first because it unlocks many projects.

Practical immediate priorities now:

- keep using the proven media lane while building real projects
- standardize the object/3D asset contract for `canvas` / `game_map`
- keep all of that mirrored semantically into ASCII
- widen reusable ops only when project pressure justifies it

That gives us a fast lane into:

- tools
- media apps
- games
- AI/mechanics experiments

==================================================
7. CHTPMGL VS HTML DIRECTION
==================================================

Long-term, the real target should not be "custom forever markup."

The healthier direction is:

- become more structurally compatible with HTML concepts over time
- without abandoning the TPM/Wraith audit-first contract

What to align with HTML now:

- container vocabulary:
  - `window`
  - `div`
  - `panel`
  - `header`
  - `menu`
  - `canvas`
- media vocabulary:
  - `img`
  - `audio`
  - `video`
- control vocabulary:
  - `button`
  - `input`-like concepts later
  - `checkbox`
  - `slider` / range-style control
- explicit attributes:
  - `id`
  - `src`
  - `width`
  - `height`
  - `label`
  - `value`
  - `checked`
  - `autoplay`
  - `loop`

What not to chase yet:

- CSS completeness
- DOM completeness
- browser-grade JS behavior
- standards-perfect layout engine

That would slow us down too much right now.

==================================================
8. GUIDANCE TOWARD A FUTURE BROWSER PROJECT
==================================================

One day, a browser-style project makes sense.

When that happens, the clean architecture is probably:

1. parser layer
   - HTML-like markup parser
2. semantic UI layer
   - normalized objects, actions, state, hierarchy
3. scripting layer
   - later JS-like parser/runtime
4. surface projections
   - ASCII audit projection
   - GL presentation projection

That means the future browser should not bypass the audit model.

It should still expose:

- semantic tree
- nav/action truth
- media state
- selection/focus

through the ASCII surface.

So the future browser lane should inherit from the mirror contract, not replace it.

==================================================
9. WHAT TO STANDARDIZE IN A DEDICATED SESSION LATER
==================================================

A future standardization session should probably focus on:

1. tag vocabulary normalization toward HTML-like names
2. attribute normalization
3. media contract:
   - `img`
   - `audio`
   - `video`
4. `canvas` / `game_map` / 3D asset contract
5. widget state model
6. semantic object intermediate format

That work is valuable, but it does not need to block current project development.

==================================================
10. CURRENT RECOMMENDATION
==================================================

Current recommendation:

- treat Wraith as ready enough
- build projects/games/apps now
- harden media and semantic mirror seams as they arise
- avoid spending whole sessions polishing abstract framework purity unless a real project needs it

In short:

Wraith is far enough along that the fastest path forward is to leave the nest and make real things with it.
