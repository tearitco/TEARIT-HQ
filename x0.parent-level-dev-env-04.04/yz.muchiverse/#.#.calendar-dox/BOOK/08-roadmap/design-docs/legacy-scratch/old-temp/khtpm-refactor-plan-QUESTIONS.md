# Questions for the plan's author before starting Option C

Read the whole plan first. These are the things I need answered (or confirmed as "use your judgment") before I start §6 step 1, in the order they'll actually block me. Please answer inline under each question.

---

## Blocking (need an answer before step 1 — the GLX mirror prototype)

**Q1. §8.1 — one thin window or two processes?**
The plan itself flags this as unresolved and says it must be resolved before §6 step 2, but step 1 (prototype the mirror) already has to be built as either "one window, two jobs" or "capture process + display process" — the prototype's shape depends on the answer, not just step 2's. Which do you want prototyped first? My default guess is "one thin window, two jobs" (matches your stated lean, and avoids a second IPC hop between capture and display that CHTPM's real split doesn't actually need here since both jobs are local to one popup surface) — confirm or override.

**A (rephrased for clarity):** Yes — one thin window, confirming your default guess. Build it so it both **reads from and writes to the same relay file** — the window doesn't just translate real X11 events into one-way relay writes and forget about it; it also consumes the relay as its own input source. That dual read/write is the intended design, not an accidental side effect to engineer around.


**Q2. ARGB visual + GLX together — has this specific combination been proven anywhere in the house already?**
§6 step 1 asks me to confirm translucency works "end-to-end" (ARGB-capable X visual + GL blending together), and flags it as the single biggest unresolved risk. Before I spend time on it: has any existing program in this house already done a transparent-background GLX window on this same X server/compositor (Mutter/XWayland per §2.2's focus note), even for an unrelated purpose? If yes, point me at it and I'll confirm the pattern instead of discovering it from scratch. If no, I'll treat this as genuinely novel and budget real time for it.

**A (rephrased for clarity):** Only the RGB→CHTPM path (`chtpm_rgb_render.c`→`gl_mirror.c`) has been proven anywhere in this house so far. A transparent-background GLX window specifically has **not** been demonstrated by anything yet. Treat ARGB+GLX translucency as genuinely novel — budget real time for it, don't assume it'll be quick because the adjacent RGB pipeline already works.

**Q3. Target: is Windows/WGL portability a real near-term requirement, or aspirational?**
§5 point 2 justifies GLX-over-Xlib partly on portability grounds ("thinner port path to Windows"). Is there an actual planned Windows port in a realistic timeframe, or is that a nice-to-have that shouldn't drive today's design if it adds risk? Trying to calibrate how much I should let portability concerns slow down step 1 versus just optimizing for "works well on this Linux/X11 box now."

**A (rephrased for clarity):** Yes — real, not aspirational. This program needs to run on Linux, Windows, and Mac. Let portability genuinely drive today's design; don't deprioritize it in favor of "just make it work on this one Linux/X11 box."

---

## Needed before step 2 (relay unification) / step 4 (module dispatch)

**Q4. §8.4 — HQ popup relay coverage.**
You flagged that only strip-popup and File→Save-As were verified end-to-end through the relay; HQ popup's digit-select path was implemented but not fully tested. Should I do that verification pass myself as part of step 1/2 prep, or has it already happened since 2026-08-10 and the doc just hasn't been updated?

**A (researched, confirmed by the plan's author 2026-08-10):** Not tested — confirmed directly by checking the harness itself, not just memory: `#.desktop/harnesses/livedesk-taskbar/scenarios/demo_relay_nav.sh` has zero references to `g_hq_popup_open` or HQ at all; it only exercises `nav 3` (File menu). The doc's claim is accurate and current. Do the verification pass yourself as part of step 1/2 prep.

**Q5. Entity relay file — one per entity, or shared?**
§5 point 3 says entities "would need an equivalent" relay to the taskbar's `livedesk_agent_relay.txt`, "likely per-entity." Given `nav_claim_rows()` already coordinates per-entity popup numbering against the shared claims pool (§2.3), should the per-entity relay file live at a predictable path derived from the entity's existing identity (e.g. alongside wherever its own state already lives), or do you already have a naming scheme in mind? I don't want to invent a convention that collides with something that already exists elsewhere in the house.

**A (researched, confirmed by the plan's author 2026-08-10):** Per-entity, and there's already a real, existing convention to follow — confirmed by directly listing a live entity's own directory (`xyzfs/.../pals/m8_redhorned/`): `interact_relay.txt` already lives directly inside the entity's own `package_dir`, alongside `history.txt`, `meta.pdl`, `desktop_pos.txt`, etc. Put the new agent relay file there too (e.g. `<package_dir>/livedesk_agent_relay.txt`, matching `interact_relay.txt`'s naming pattern), not in some new shared or house-relative location. This has a real, additional benefit: it sidesteps the ENTIRE class of house_root-derivation bugs documented in `xyz-installer-dev/dev-doc/03.hardcoded-path-fragility-and-portability.md` — `package_dir` is already correctly known and passed everywhere the entity's own code runs, so this needs zero new path-derivation logic at all.

---

## Needed before step 5 (layout format design)

**Q6. §8.2 — how far should I lean on `.chtpm`'s shape vs. inventing fresh?**
You've flagged the layout format as "genuinely undesigned." Given `tp_desktop_window.c` already has a crude declarative format (`meta.pdl`/`objects.pdl`, METHOD/OBJECT rows), is the intent that the new shared format *evolve from that* (since it's real, working, and already entity-side), or evolve from `.chtpm`'s XML shape (since that's the KHTPM-wide convention elsewhere in the house), or something new that owes nothing to either? I want to avoid designing something that's stylistically orphaned from both.

**A (rephrased for clarity):** Lean on `.chtpm`'s existing shape, adapted for pixel UI, rather than evolving from `meta.pdl`/`objects.pdl` or inventing something owing nothing to either.

**Q7. §8.5 — is `chtpm_rgb_render.c`'s glyph-blitting code intended to be reused, or is that a "go read it and decide" open question you haven't formed an opinion on?**
Just want to know if you have a lean here already (you flag it as unread-in-depth) so I don't duplicate effort if you already know the answer from earlier work.

**A (rephrased for clarity):** Yes — always try to reuse existing, working code/standards before writing something new. Treat `chtpm_rgb_render.c`'s glyph-blitting as a real candidate for reuse; check whether it already does the job before duplicating it.

---

## Scope / sequencing

**Q8. Can steps run out of the §6 order, or is the order load-bearing?**
Step 3 (extracting `livedesk_*` into `livedesk_core.c`) is explicitly called out as safe to do first/in-parallel. Is anything else in the list similarly reorderable — specifically, can I do Q1-Q3's mirror prototype work concurrently with the `livedesk_core.c` extraction, or do you want strict sequencing so each step is fully validated via the harness before the next starts?

**A (researched — real dependency graph worked out from what each step actually needs, not a guess):**
- Steps **1** (GLX mirror prototype), **3** (extract `livedesk_core.c`), and **5** (design layout format) have **no dependencies on each other** — safe to run fully in parallel, any order.
- Step 2 is now resolved (Q1 above) — no longer a blocker for anything.
- Step **4** (module dispatch) needs step 3's extraction to exist first — the module calls directly into that logic.
- Step **6** (wire the compositor) needs steps 1, 4, AND 5 all done — it's the first real integration point, don't start it early.
- Step **7** (cut real X11 handling over to capture+translate only) needs step 6 proven working via the harness first — this is the actual risky cutover, don't rush it.
- Step 8 (harness re-run) isn't really a discrete step — run it after every real change, continuously, regardless of phase.

So: order is genuinely NOT load-bearing for {1, 3, 5} — parallelize freely. It becomes load-bearing starting at step 4 (needs 3 done) and step 6 (needs 1+4+5 done).

**Q9. §8.3 — do you already know the answer for whether `livedesk_*` has a hard same-process dependency, or is this a real open question for me to investigate?**
If you already suspect "no, purely historical," I'll treat the extraction as low-risk and just do it; if you think there's a real chance of a hidden coupling, tell me what you'd check first.

**A (researched, not assumed):** Confirmed via direct grep of `tp_taskbar.c`: of 61 `livedesk_*` functions, only ~10 take `Display*`/`GC` parameters at all — `livedesk_dispatch`, `livedesk_open_sessions_popup`, `livedesk_open_desks_popup`, `livedesk_open_pals_popup`, `livedesk_open_desk_props_popup`, `livedesk_open_rename_modal`, `livedesk_save_as` (opens the cli-io modal), `livedesk_open_dyn_popup`, `livedesk_edit_focused_desk` — and every single one of them is specifically "open a popup UI for this business-logic result." That's exactly the class of function Option C's module/mirror split replaces anyway, not something you need to preserve X11-coupled. The actual business logic underneath (`livedesk_ensure_session`, `livedesk_snapshot_desk`, `livedesk_spawn_desk`, `livedesk_switch_desk`, `livedesk_pals_root`, `livedesk_ensure_pal`, `livedesk_hash_dir`, `livedesk_save_as_with_name`, `livedesk_rename_desk`, `livedesk_delete_desk`, etc.) takes **zero** X11 parameters. **This confirms: the same-process coupling is historical (grew in the same file because that's where `main()` already was), not architectural.** Low risk — proceed with the extraction as planned; the popup-opening functions you'll leave behind or rewrite are exactly the ones this refactor is replacing anyway.

**Q10. Definition of done for this phase.**
The plan describes 8 phased steps but doesn't say how much of this you want attempted in one sitting versus checkpointed with you. Do you want me to stop and report back after step 1 (the highest-risk item) before continuing, or work through as many steps as I can and report at natural breakpoints?

**A (recommendation, not researched — this one is genuinely a process preference, confirm or override):** Checkpoint after step 1 specifically — it's the single biggest unresolved risk (ARGB+GLX translucency, confirmed novel per Q2), and its real-world outcome could reasonably reshape what comes after it, so it's worth a deliberate stop before committing further. After that, natural breakpoints (e.g. after step 3's extraction lands, after step 6's first working integration) seem reasonable without needing a check-in after literally every step — but say so explicitly if you'd rather checkpoint more tightly than that.

---

*Answer inline or in a reply doc — whichever's easier. I'll start on step 1 prep (re-reading `gl_mirror.c` and checking for prior ARGB+GLX precedent per Q2) while waiting, since that's useful regardless of how the other questions land.*

---

## Round 2 — a real finding that affects the whole approach, before I build anything

**Q11. The plan's own cross-platform justification for GLX (§5 point 2) doesn't hold up against the actual code — which technology should the mirror prototype target?**

Given Q3's answer confirmed Linux/Windows/Mac is a real, near-term requirement, I checked the two GL-using precedents the plan cites, and they contradict each other on portability:

- `tp_desktop_window.c:1547-1585` (the "already working" entity-sprite GLX pipeline §3 calls out as precedent) is **raw GLX** — `glXChooseVisual`/`glXCreateContext`, zero `#ifdef _WIN32` or `__APPLE__` anywhere nearby. Raw GLX is X11-only; it has no Windows or Mac equivalent at all. It only runs today because it's never been built anywhere but Linux/X11.
- `gl_mirror.c` (the file §5 point 2 calls "the direct model" for the shared thin mirror) does **not** use raw GLX. It uses **GLUT/freeglut** (`glutInit`, `glutCreateWindow`, etc.), and its portability comes specifically from freeglut abstracting the platform context layer — it already has real, working `#ifdef _WIN32` (freeglut/WGL) and `#ifdef __APPLE__` (`<GLUT/glut.h>`) branches, tested in practice, not hypothetical.

So the plan's §5 point 2 describes the target as "GLX on X11, WGL on Windows, EGL on Wayland — a thin context-attachment shim," implying someone hand-rolls that shim. But the actual precedent that's proven to work cross-platform in this house doesn't hand-roll that shim at all — it uses GLUT/freeglut and gets the shim for free. The entity-sprite pipeline, meanwhile, *is* hand-rolled raw GLX, and is NOT actually proven cross-platform (it's simply never been asked to run anywhere but here).

Given cross-platform is a real requirement (Q3): should the step-1 mirror prototype follow `gl_mirror.c`'s GLUT/freeglut approach (proven pattern, but a new dependency for the taskbar/entity programs if they don't already link it), or hand-roll the GLX/WGL/EGL shim as §5 originally described (more control, but genuinely unproven — nothing in this house has done it)? My default lean is GLUT/freeglut, matching the one precedent that's actually demonstrated working on multiple platforms — confirm or override.

**Q12. Should the entity-sprite GLX pipeline (`tp_desktop_window.c`) be flagged as a latent Windows/Mac gap, separate from this plan's scope?**
Not asking you to fix it now — it's explicitly out of scope (sprite rendering, per the doc's own scope note). But since Q3 confirmed real multi-platform targets, is this a known, already-tracked gap, or new information worth a note somewhere (e.g. `#.livedesk/livedesk-editor-design.md` or wherever the sprite pipeline's own debt is tracked)? Just want to make sure this doesn't quietly become "the taskbar ports cleanly but the entities it's coordinating with can't build on Windows" without anyone having decided that on purpose.

**A (2026-08-10, user direct):** Don't block on this. Prioritize Linux for now; Windows/Mac portability will be a separate pass done on those actual machines later. Proceeding with best-effort choice — going with GLUT/freeglut for the step-1 prototype since it's the one pattern already proven in this house (`gl_mirror.c`) and costs nothing extra on Linux, but not spending further time resolving the portability question itself right now.
