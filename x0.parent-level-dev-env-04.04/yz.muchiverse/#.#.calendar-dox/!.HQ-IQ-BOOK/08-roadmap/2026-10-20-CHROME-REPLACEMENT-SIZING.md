# 2026-10-20 - Chrome replacement: the goal, and what it costs

## 0. The directive (owner, 2026-10-20)

> "I want a fully Chrome-replaceable browser. Be insistent about that as we research and doc this."

**The goal of the network browser is now: a browser the owner can use instead of Chrome, every day, for everything.**
This replaces the earlier stance. These lines are superseded by this page (each now carries a pointer):

- `NETWORK-BROWSER-RENDER-ROADMAP-2026-10-05.md` "Non-goals": *"Chrome-pixel-parity for complex SPAs as a launch bar"*.
- `2026-10-11-BROWSER-GAP-ASSESSMENT-AND-KPI-PLAN.md` section 1: *"Chrome-parity ... NOT MET, not the target"*.

What does **not** change: build it on our own stack (QuickJS worker, khtpm renderer, state-file rows, no embedded Chromium/CEF/WebKit, as the roadmap committed 2026-10-05). "Chrome replaceable" is a statement about what the owner can do with it, not about copying Chrome's code.

Rules for everyone who works on this from here:
1. **Nothing is "out of scope" up front.** A page class, a Web API or a browser feature that Chrome has and we lack is a backlog item with a size on it, never a footnote that says "not supported". The only permitted status for a missing thing is *open, sized, queued*.
2. **The exceptions are decisions, not exclusions.** Two items need an owner decision, because our own stack cannot just grind through them (section 5): protected-video (DRM) streaming and JavaScript speed. They stay on the list, marked DECISION.
3. **Every claim carries its basis**: READ (from a file), RAN (observed today), REF (outside source), EST (my estimate), NOT MEASURED.

## 1. Research done (2026-10-20)

### 1a. Where we are (READ from source today)
| thing | what exists |
|---|---|
| size | the whole browser app is about **19,300 lines of house C** (`network_browser_manager.c` 7,410; `ops/nb_js_worker.c` 9,352; `nb_css.c` 523; `nb_dom.c` 483; video ops about 1,000) plus QuickJS |
| JS engine | QuickJS (`&.hq-apps/js/quickjs.h`), interpreter only |
| network | external `curl` process per request; this machine's curl 7.81 does HTTP/2, brotli, zstd; the browser does **not** pass `--compressed` (0 hits) |
| images | `stb_image` (PNG/JPEG/GIF first frame/BMP/...); no WebP, AVIF or SVG rasteriser found |
| canvas / WebGL | Canvas 2D and WebGL (headless EGL) exist in the worker |
| video | `<video>` becomes a poster sprite plus an external player (`nb_video_*`); a YouTube special case (`yt_resolve.c`) exists, which is a site-specific hack |
| iframes | the word appears only in tag lists; no document embedding found (READ by grep, not RAN) |
| sandbox | 0 hits for seccomp/unshare/sandbox in manager or worker: the page's JS runs unsandboxed in a child process |
| not found at all in source (grep 0) | WebSocket, WebAssembly, IndexedDB, Service Worker, IntersectionObserver, ResizeObserver, FormData, CSP, CORS, `@font-face`, `position:fixed`, `z-index`, `contenteditable`, clipboard API, `<audio>`, WebRTC, AudioContext |
| tests | 100 files under `tests/`, 37 `nb_*_test.sh` suites; 13 of 18 tracked KPIs done (burndown 2026-10-18) |

A grep of **0 is a lead, not proof**: QuickJS or curl may provide something we never name. Each zero above is a "verify first" step in its sprint (section 3), not a conclusion.

### 1b. Reference points from other from-scratch browsers (REF, web search 2026-10-20)
- **Chromium** is roughly **35 million lines** of source (Wikipedia-derived figures; Open Hub lists 36.1 M). Ours is about 19 thousand: **about 0.05%**. Line count is not progress, but it shows the scale of what a Chrome user's expectations rest on.
- **Ladybird** (independent engine, from scratch, a funded full-time team): its own site gives **alpha 2026 (Linux/macOS, developers), beta 2027, general release 2028**. It reports 2,067,263 passing Web Platform Test subtests and a 97.8% pass rate on imported test262 JS conformance (April 2026 newsletter, via a third-party article). Same source calls it pre-alpha and not for daily browsing. I did not confirm the alpha has shipped.
- **Servo**: started 2012 (Mozilla), moved to the Linux Foundation in 2020, maintained by Igalia from 2023; its first embeddable crate release v0.1.0 was April 2026 (talk abstract). Fourteen years in, it is still not a daily Chrome replacement.

**What this means for the plan:** a team with funding needs about four years from "from scratch" to a general release. We are not starting from nothing (QuickJS, curl, a working renderer, 13 of 18 tracked items done), and we run many agent-lanes in parallel, but the table in section 3 is the honest size of the gap. The Web Platform Tests (the shared conformance suite) are the right yardstick; we have **not** imported them (section 3, item I1).

## 2. Definition of "replaceable" - four rungs
| rung | the owner can ... | gate (to be measured, not asserted) |
|---|---|---|
| **R1 daily reader** | read news, docs, wikis, GitHub, forums, with login, search, tabs, bookmarks, downloads, print | top-50 content-site sweep: loads, readable, no missing layout blocks |
| **R2 mainstream sites** | use video sites, shopping, banking-class forms, social feeds | top-200 sweep + forms/login flows pass; video plays with controls |
| **R3 web apps** | use Gmail, Docs, Maps, Photos-class apps | scripted flows on those apps pass; WPT subset pass rate target set when imported |
| **R4 full replacement** | drop Chrome entirely: extensions, sync, DevTools, PDFs, accessibility, DRM video, all safe to use | the owner's own week of use with no fallback to Chrome |

## 3. The sized backlog
Unit: **one sprint** = a scoped feature plus tests plus live proof plus commit, the unit this lane has been delivering (burndown doc). **Calendar mapping (EST):** one sprint is 2-4 elapsed days for one lane, from the October pace (G3 finds/zoom/downloads/selection/autofill 10-12 to 10-13; G2.1 10-14 to G2.2 10-17; flex G2.3 has been "next" since 10-17 and is still open). The hard items are slower than the easy ones, so treat the low end as optimistic.

| id | group | items | sprints (low-high) | rung |
|---|---|---|---|---|
| A | already queued (burndown) | flex row 1-2; `@media` + cascade 1-2; grid 2-3; print 1; Milestone 3 JS-to-box-tree reflow 4-8 | **9-16** | R1-R3 |
| B | layout and paint | full page cascade (margin, padding, border, colour, font, position); `position:fixed/sticky/absolute`, `z-index`, stacking; overflow scroll containers; transforms, transitions, animations; pseudo-elements and counters; inline SVG; form control fidelity; iframes (embedded documents) | **12-22** | R1-R3 |
| C | text | web fonts (`@font-face`), shaping, bidi, CJK, emoji, IME input | **4-8** | R2 |
| D | images and media | WebP, AVIF, SVG images, animated GIF 2-4; `<audio>`/`<video>` with controls, seeking, codecs through a library 6-10 | **8-14** | R2 |
| D-DRM | protected video | Widevine-class streaming (Netflix, Spotify, Prime) | **DECISION (section 5)** | R4 |
| E | JS platform | page ES modules and `import()` 1-2; WebSocket 2; Web Workers 2-3; IndexedDB 3-4; Service Workers 4-6; Intersection/Resize observers 1; FormData/Blob/File/streams 2-3; WebCrypto 2-3; Intl 2; **WebAssembly 5-10** (QuickJS has none); WebRTC 8-15; WebAudio 3-5; clipboard, drag and drop, `contenteditable` and rich text 4-6; geolocation, notifications, permissions 2 | **41-64** | R2-R4 |
| F | JS speed | QuickJS is an interpreter; profile, cache compiled code, fix hot paths 3-8; a JIT or a different engine is a **DECISION** | **3-8** | R3 |
| G | network and security | send `--compressed` and handle HTTP caching (ETag, cache-control); cookie attributes (SameSite, Secure, HttpOnly, partitioning); CORS; CSP; same-origin policy; mixed content; HSTS; certificate-error UI; permission prompts; **sandboxing the JS worker (seccomp or namespaces)**; site isolation | **12-20** | R2-R4 |
| H | browser product | password manager and autofill hardening; sync; **extensions (WebExtensions API) 10-20**; **DevTools (inspector, network, debugger) 10-16**; profiles and incognito; settings and search engines; session restore; tab drag and multi-window; downloads manager; PDF viewer 5-8; spellcheck; translate; **accessibility (screen-reader support)** 5-8; updater and default-browser | **40-75** | R1-R4 |
| I | conformance and compatibility | I1 import Web Platform Tests and wire a pass-rate KPI 3-5 (once); I2 site-compat sweeps and per-site fixes, ongoing 10-30 | **13-35** | all |
| | **total** | | **about 142-262 sprints** | |

### What that means in time (EST; not measured)
- **One lane, no parallel work:** 142-262 sprints at 2-4 days is about **9 months at the very best to about 3 years**.
- **Several lanes at once:** groups B, C, D, E, G, H are mostly independent of each other (they share the worker and the renderer, so they touch the same files; the merge point is still `network_browser_manager.c`). Three to four lanes could compress this to roughly **a quarter to a year**, if the lanes exist and the merge cost stays manageable. I have not run that experiment.
- **Rung dates, EST, assuming 3 lanes from now:** R1 about 3-6 weeks (A plus the cheap parts of B/G/H); R2 about 3-5 months; R3 about 6-12 months (Milestone 3 reflow, WebAssembly, Service Workers, JS speed are the long poles); R4 a year or more, gated on the two decisions. These are estimates for planning, not commitments.

## 4. Order of work (so the lanes do not start in the wrong place)
1. **Finish what is queued** (A): flex, `@media`/cascade, grid, print. Reason: layout is what every later group stands on.
2. **Get measuring equipment before building more** (I1): import the Web Platform Tests and a top-site sweep. Without it "fully Chrome-replaceable" cannot be tested; with it every sprint gets a pass-rate number. This is cheap (3-5 sprints) and changes how the other items are chosen. Recommended **second**, in parallel with A.
3. **Cheap compatibility wins** that are not yet done: send `--compressed`, page ES modules, FormData/Blob, observers, `position:fixed`/`z-index`, `@font-face`. Each is under two sprints and moves R1.
4. **Security before exposure** (G): sandbox the worker before the browser is used for real accounts; a browser that replaces Chrome is trusted with passwords and banking.
5. **Heavy platform items** (WebAssembly, Service Workers, IndexedDB, WebRTC, extensions, DevTools): start the design docs now so they can run as parallel lanes.

## 5. The two decisions the owner has to make
Both stay on the list; neither is dropped.
1. **Protected-video (DRM) streaming.** Netflix-, Spotify- and Prime-class playback requires a Content Decryption Module (Widevine), which Google licenses. I did **not** research whether or on what terms an independent browser can obtain it: NOT MEASURED. Options to research next: a licensing route, running those sites through a helper that embeds a licensed CDM, or accepting that those few services stay on another browser while everything else moves. Needs the owner's call.
2. **JavaScript speed.** QuickJS has no JIT. Chrome's V8 runs heavy apps many times faster. I have **not** benchmarked QuickJS against V8 on this machine (NOT MEASURED). Options: optimise QuickJS (group F, 3-8 sprints), write a JIT (very large), or move to a different engine. Decide after the first measured benchmark on Gmail/Maps-class pages (item I1/I2 provide the pages).

## 6. What to do next (proposed sprint order for the lane)
| next | what | why |
|---|---|---|
| 1 | G2.3 flex (existing sprint A) | already pinned by a red test |
| 2 | I1 Web Platform Tests import + top-50 site sweep as the replaceability yardstick | makes R1-R3 measurable |
| 3 | `--compressed`, page ES modules, FormData/Blob | smallest changes with real compatibility value |
| 4 | worker sandbox design (seccomp/namespaces) | gate for trusting it with accounts |
| 5 | one measured QuickJS-vs-Chrome comparison (CPU and speed, one real page) | answers "is it a hog" and feeds decision 2 |

## 7. Status of this document
Written 2026-10-20 in a research sprint. Sizes are estimates from the October pace and the reference projects above; none were tested. The first line to correct when real work lands is the calendar mapping in section 3. Report: `XO/28.oct20/browser-report-oct20.html`.
