# Execution plan — Option C, first pass (step 1 prototype + parallel steps 3/5 prep)

Written after all 12 Q&A rounds in `khtpm-refactor-plan-QUESTIONS.md` (all resolved — see that doc for full answers). This is what I intend to actually do, before doing it, per direct request to share with the plan's author for a sanity check.

**Scope of this pass:** §6 steps 1, 3, 5 (confirmed independent, safe to parallelize per Q8). Checkpoint after step 1 specifically before continuing (per Q10) — that's the highest-risk item.

---

## Environment findings so far (real, checked this session, not assumed)

- This machine runs GNOME Shell on **Wayland**, with XWayland (rootless) serving X11 clients (confirmed via `ps aux` — `gnome-shell` + `Xwayland ... -rootless`). `_NET_WM_CM_S0` is not set on the XWayland root window the way a classic X compositing manager would set it — this is expected for Wayland-hosted XWayland (Mutter composites at the Wayland level), but it means the usual X11 "is a compositor running" check doesn't give a clean yes/no answer here. I can't reason my way to "translucency will/won't work" from introspection alone — it has to be tested empirically by actually building a window and rendering it, per the confirmed-novel status from Q2.
- GLUT/freeglut, GL/GLU/GLX libraries and headers are all present and linkable (`libglut.so`, `/usr/include/GL/{glut.h,gl.h,glx.h,...}`, `freeglut` found via pkg-config).
- No screenshot tool found yet (`scrot`/`import`/`gnome-screenshot`/`grim` all missing). I'll need one to visually verify translucency renders correctly (not just "compiles and doesn't crash") — will check for other options (e.g. a quick Python/PIL X11 grab, or ask you if there's a house-standard way this project already verifies GL output) before assuming I need to install something.

## Step 1 — GLX/GLUT mirror prototype (the checkpoint item)

**Goal:** the smallest possible program that proves (a) a translucent, always-on-top, undecorated window can render real per-pixel alpha against the live desktop background on this specific machine/session, and (b) it can be driven fast/responsively enough for a persistent taskbar (not just a one-shot demo).

**Technology:** GLUT/freeglut, matching `gl_mirror.c`'s proven pattern (per the Q11 resolution — GLX raw is X11-only and unproven cross-platform; GLUT already has real Windows/Mac branches even though we're not chasing those today).

**Concrete build, in order:**
1. Minimal freeglut program, `glutInitDisplayString("rgba alpha double")` (or the freeglut-equivalent call to request an alpha-capable framebuffer — need to confirm exact API, `glutInitDisplayMode` alone doesn't request per-pixel alpha, may need `GLUT_ALPHA` or the display-string form — will verify against freeglut's actual header/docs rather than guessing).
2. Window flags: undecorated (`glutInitWindowSize`/border removal — freeglut has limited direct support for this; may need to reach into the underlying X window via `glutGetWindow`/`glXGetCurrentDisplay` and set `override_redirect`-equivalent properties directly the way `tp_taskbar.c` does for its own popups, since freeglut alone won't give override_redirect or always-on-top).
3. Draw: `glClearColor(0,0,0,0)` (fully transparent clear, not opaque black like `gl_mirror.c`'s own `display()`), then one opaque solid rectangle + one ~50%-alpha rectangle, with `glEnable(GL_BLEND)` + `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` (same blend mode already proven for entity sprites, per the plan's §3).
4. Run it, position it over a busy part of the desktop (terminal text, an open window edge — something with real visual content behind it), and visually confirm: opaque rect looks solid, translucent rect shows the desktop bleeding through, and the window's OWN background (outside both rects) is fully invisible (not black, not white — actually transparent).
5. Basic responsiveness check: drive the color of the translucent rect from a fast timer loop (e.g. pulse alpha every frame) and confirm no visible lag/tearing under a normal `glutTimerFunc` cadence — this is the "responsive enough for a taskbar" half of the checkpoint, distinct from the "translucency renders at all" half.

**What I will NOT do in step 1:** wire it to the relay, real popup content, or any taskbar/entity code. This is strictly an isolated prototype, not integrated with `tp_taskbar.c` or `tp_desktop_window.c` at all yet — matches §6's own ordering intent (de-risk the unknown before touching real code).

**Deliverable:** a short report back — did it work, what had to be worked around (e.g. the override_redirect-via-freeglut question above), and whether the "responsive enough" bar is genuinely met, before I touch step 2's decision-point or any real taskbar code.

## Step 3 — extract `livedesk_*` into `livedesk_core.c` (parallel, low risk per Q9)

Confirmed via grep (Q9's answer) that only the ~10 popup-opening `livedesk_*` functions take X11 params, and those are exactly the functions Option C replaces anyway. Plan:
1. Re-grep to get the definitive current list of all `livedesk_*` function signatures in `tp_taskbar.c` (the Q9 count of 61 was from that earlier pass — I'll re-verify rather than trust a stale count).
2. Split into `livedesk_core.c` (pure business logic — sessions/desks/pals, zero X11/GC params) + leave the popup-opening wrappers in `tp_taskbar.c` for now (they'll be replaced by the module/mirror later, not today).
3. Compile-check only — I will NOT change `tp_taskbar.c`'s actual runtime behavior in this pass, just prove the split compiles and the existing harness (`button.sh demo`) still passes unchanged. This is a pure refactor-with-a-safety-net step, not a behavior change.

## Step 5 — layout format design (parallel, research/design only, no code)

Per Q6's resolution (lean on `.chtpm`'s existing shape, adapted for pixel UI, not `.pdl`/`meta.pdl`'s shape). Plan:
1. Read `.chtpm`'s actual element vocabulary in full detail again from `chtpm_parser.c` (the plan's §1.3 lists `panel`/`module`/`interact`/`text`/`br`/`button`/`cli_io`/`${var}` — I'll confirm this is the complete list, not just the plan's summary).
2. Draft (as a doc, not code) what a pixel-adapted version needs beyond that vocabulary: explicit x/y or row/col positioning, icon/glyph references (taskbar buttons aren't just text), hover-state styling — since `.chtpm` was built for a monospace text grid where position is implicit from character order, and pixel UI needs real explicit layout.
3. Cross-check against `tp_desktop_window.c`'s existing `meta.pdl`/`objects.pdl` METHOD/OBJECT rows and the taskbar's own `.pdl` strip config (§2.3) — not to base the new format on them, but to make sure nothing they currently express gets lost in translation.
4. Output: a draft layout-format spec doc, not implementation. This is the "genuinely undesigned" item (§8.2) — I want your read on the draft before any parser code gets written for it.

---

## What I'm explicitly deferring / not doing yet

- Step 2 (resolved per Q1, no longer blocking) — no separate action needed.
- Steps 4, 6, 7 — all correctly gated behind step 1 (and 3/5 for step 6) per Q8's dependency graph; not starting these now.
- Any change to real, currently-working `tp_taskbar.c` input handling or `tp_desktop_window.c` popup behavior — nothing in this pass touches live behavior for either program.
- Windows/Mac portability work — deferred per your direct instruction, Linux-only for this pass.

Let me know if any of the above should change before I start step 1.
