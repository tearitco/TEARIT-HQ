# TPMOS Wraith Browser

Date: 2026-07-02
Status: Architecture note with first implementation pass active

## Implementation Status

Current first pass lives at:

- `projects/wraith-alpha/wraith-projects/wraith-browser`

What is implemented now:

- Wraith-hosted browser project scaffold
- local page fixtures
- session truth for source, DOM, layout, receipts, and browser state
- separate `browser_exec_js` op for controlled script actions

What is not implemented yet:

- remote fetch
- real CSS system
- general JavaScript runtime
- standards-grade HTML parsing
- tabbed browsing

## 1. Purpose

This document defines how to think about a real browser inside TPMOS/Wraith.

The goal is not merely:

- "open websites somehow"

The goal is:

- a browser that fits TPMOS file/ops/audit standards
- a browser that can live inside Wraith as a first-class project/window
- a browser where parsing, rendering, state, and scripting are inspectable
- a browser where agents can reuse ops instead of treating the browser as an opaque black box

## 2. What "WebKit" Is And Why It Matters

Short answer:

- WebKit is a mature browser engine
- it already knows how to do:
  - HTML parsing
  - CSS parsing/layout
  - DOM
  - JavaScript execution
  - image/media loading
  - network/resource handling
  - browser-like rendering behavior

So when people say "use WebKit", they usually mean:

- embed an existing browser engine rather than build the whole web stack from scratch

Why it is relevant here:

- it is the fastest path to broad web compatibility
- it is the worst path if we want TPMOS-native inspectability and ops-driven sovereignty from day one

So WebKit matters as a comparison point, and maybe as a later compatibility lane, but it should not automatically define the architecture.

## 3. Core Design Tension

There are really three possible directions:

### A. Pure DIY browser

Build:

- HTML tokenizer/parser
- CSS tokenizer/parser
- DOM tree
- style system
- layout engine
- paint model
- JavaScript runtime / bindings
- fetch/resource loading
- media/image support

Pros:

- maximum sovereignty
- maximum auditability
- perfect TPMOS alignment
- ops can own every stage

Cons:

- very slow to reach practical web compatibility
- JavaScript/browser semantics are huge
- modern web compatibility becomes a long campaign, not a short project

### B. Pure embedded engine browser

Use something like:

- WebKit
- CEF / Chromium Embedded Framework
- maybe another embeddable engine

Pros:

- practical compatibility fast
- real websites work sooner

Cons:

- engine becomes a giant opaque dependency
- TPMOS ops become wrapper commands over a foreign runtime
- DOM/layout/script truth is much harder to inspect as file/receipt truth
- agents lose a lot of uniformity unless we build a big introspection bridge anyway

### C. TPMOS hybrid browser

This is the recommended direction.

Meaning:

- browser is TPMOS/Wraith-hosted
- browser state is file/ops/audit driven
- parser/layout/render/script are separated into TPMOS-owned seams
- start with a small sovereign browser model
- later, optionally add a compatibility engine lane for hard sites

This gives:

- TPMOS-native architecture first
- room for an embedded engine later
- no requirement that the first browser be "the whole modern web"

## 4. Recommended Interpretation

The first real TPMOS browser should not try to defeat Chrome/Firefox on day one.

The first real TPMOS browser should instead prove:

1. page/resource fetching
2. HTML parsing
3. CSS/style parsing
4. DOM/layout tree
5. Wraith/GL rendering of page output
6. basic JavaScript execution for controlled browser APIs
7. auditable receipts and file-backed session truth

That is enough to make the browser real.

Modern full compatibility can come later.

## 5. First-Class TPMOS Rule

Browser functionality should be split into reusable ops, not one giant mystery binary.

That means things like:

- fetch op
- HTML parse op
- CSS parse op
- DOM build op
- layout op
- paint/display-list op
- script exec op
- navigation/history op
- storage/cookie op
- media/image op

Even if some of those eventually share a library internally, the user- and agent-facing contract should still be ops-based.

## 6. Proposed Browser Ownership Model

Treat the browser as:

- a Wraith internal project first
- with TPMOS-shared ops beneath it

So:

- Wraith hosts the browser window(s)
- TPMOS ops do the browser work
- browser project session files hold live page/session truth

This matches existing Wraith direction better than making the first browser a totally separate non-Wraith stack.

## 7. Proposed Layering

### Layer 1: Browser project/session truth

Likely project:

- `projects/wraith-alpha/wraith-projects/browser`

Likely session truth:

- current URL
- history stack
- tab/session registry
- current DOM snapshot
- current layout tree
- current display list
- current browser body/scene files
- receipts

### Layer 2: TPMOS browser ops

Shared ops should eventually live in a TPMOS-shared area, not only under one project.

Possible families:

- `browser_fetch`
- `browser_parse_html`
- `browser_parse_css`
- `browser_build_dom`
- `browser_compute_style`
- `browser_layout`
- `browser_paint_list`
- `browser_exec_js`
- `browser_nav`
- `browser_storage`
- `browser_media`

### Layer 3: Wraith presentation

Wraith should present:

- address bar
- back/forward/reload controls
- tab strip later
- page surface
- debug/audit views

Page rendering should be consumable in:

- ASCII audit form
- GL/RGB graphical form

## 8. HTML / CSS / JS Interpretation

### HTML

HTML should not be treated as arbitrary string soup forever.

We should build:

- tokenizer
- parse tree / DOM-like tree
- normalized node records

That tree can be dumped to file/receipt truth.

### CSS

CSS should become:

- parsed rules
- selector matching results
- computed style records

Not just inline ad hoc rendering flags.

### JavaScript

This is the hardest part.

The correct near-term stance is:

- support only a controlled subset first
- expose TPMOS browser APIs intentionally
- do not promise full web JS compatibility immediately

Meaning:

- basic ECMAScript interpreter or embedded JS runtime later
- browser APIs wrapped as explicit TPMOS seams
- DOM mutation and event loop modeled explicitly

## 9. JavaScript Strategy Options

### Option A: Build a tiny JS interpreter

Pros:

- full sovereignty
- strong auditability

Cons:

- very slow path to usefulness

### Option B: Embed a small JS runtime

Examples conceptually:

- QuickJS-style small runtime
- similar embeddable interpreter

Pros:

- much faster than building JS from scratch
- still far more controllable than embedding a whole browser engine

Cons:

- browser APIs still must be implemented by us

This is probably the best near-term JS strategy.

### Option C: WebKit/Chromium JS

Pros:

- compatibility

Cons:

- least TPMOS-native

Recommendation:

- use a small embeddable JS runtime later, not WebKit JS as the first architecture anchor

## 10. Rendering Model

The browser should not directly "paint to the screen" as a hidden private act.

Instead:

1. parse HTML/CSS
2. build DOM
3. compute style
4. build layout tree
5. emit paint/display list
6. let Wraith/RGB consume that output

This keeps rendering inspectable and lets agents audit:

- what nodes existed
- what style was computed
- what boxes were laid out
- what was painted

## 11. Media And Images

This browser should inherit the current Wraith media lesson:

- still images
- audio
- video

should not force only one path.

Recommended media model:

- static assets can use file/snapshot truth
- live/streaming media can use live frame/audio cache truth
- both should still have logical file-path identities for audit

That means the browser can reuse:

- shared KVP for state-like things
- live frame cache for streaming visual media

## 12. Network / Fetch Model

Browser fetching should also be ops-based.

Examples:

- `browser_fetch_url <url> <out_resource>`
- `browser_fetch_resource <page_context> <resource_url>`
- `browser_cache_put`
- `browser_cache_get`

That keeps:

- cache behavior inspectable
- cookies/session storage inspectable
- agents able to reproduce navigation/resource loads through explicit ops

## 13. Browser State As Files

Browser session truth should remain inspectable in files such as:

- `session/browser_state.txt`
- `session/history.txt`
- `session/dom_tree.pdl`
- `session/style_tree.pdl`
- `session/layout_tree.pdl`
- `session/display_list.pdl`
- `session/wraith_body.txt`
- `session/scene.objects.pdl`

The hot path does not have to be file-only.

But these artifacts are what make the browser TPMOS-like instead of opaque.

## 14. WebKit's Proper Place

If WebKit becomes relevant, it should probably be one of two things:

### 1. A later compatibility backend

Meaning:

- TPMOS browser architecture stays sovereign
- some pages can optionally be rendered/fetched/executed through a WebKit backend
- receipts should say that a compatibility backend was used

### 2. A reference behavior source

Meaning:

- compare our layout/render/script behavior against a known browser engine
- use it to understand standards behavior

What it should probably not be:

- the first and only browser architecture

Because then TPMOS browser becomes mostly:

- "wrap WebKit"

instead of:

- "build a TPMOS/Wraith browser with optional compatibility backends"

## 15. Recommended Build Sequence

### Phase 1: Sovereign document browser

Build first:

- plain HTML
- minimal CSS
- no or tiny JS
- image support
- navigation/history
- Wraith rendering

Target:

- static pages
- docs
- internal TPMOS browser UIs
- predictable test pages

### Phase 2: Controlled JS browser

Add:

- small JS runtime
- DOM mutation
- events
- timers
- basic browser APIs

Target:

- interactive but limited pages

### Phase 3: Compatibility expansion

Add:

- broader CSS
- broader DOM APIs
- better JS compatibility
- optional backend experiments

### Phase 4: Optional engine bridge

Only if needed:

- WebKit or other compatibility engine lane

And even then:

- keep TPMOS receipts/ops/session truth around it

## 16. Browser Ops Examples

Possible initial op surface:

- `browser_nav_open <url>`
- `browser_nav_back`
- `browser_nav_forward`
- `browser_fetch`
- `browser_parse_html`
- `browser_parse_css`
- `browser_build_dom`
- `browser_compute_layout`
- `browser_render_page`
- `browser_exec_js`
- `browser_dump_dom`
- `browser_dump_layout`
- `browser_dump_display_list`

The exact names can change.

The important rule is:

- reuse ops with args
- do not weld all behavior into one monolith

## 17. What The First Browser Is Really For

The first TPMOS/Wraith browser is not only for browsing the public web.

It is also for:

- rendering internal TPMOS docs/pages
- testing CHTMGL/HTML-like standards direction
- giving agents a structured inspectable page environment
- proving a real DOM/layout/render architecture inside Wraith

That means even a partially compatible first browser is still strategically valuable.

## 18. Bottom Line

The clean direction is:

- Wraith-hosted browser project
- TPMOS-shared browser ops
- sovereign HTML/CSS/DOM/layout/paint model first
- small JS runtime later
- optional compatibility engine later
- keep WebKit as a possible backend/reference, not the first architecture anchor

So if we do this right, the browser becomes:

- TPMOS-native
- ops-reusable
- agent-auditable
- Wraith-presentable

instead of just embedding a foreign browser and hoping that is good enough.
