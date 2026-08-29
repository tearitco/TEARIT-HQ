# 🎨 KHTPM Enhancement: CSS Styling & Rich UIs

**Status:** Design Phase  
**Author:** User Direction (2026-08-12)  
**Audience:** UI developers, anyone building events-hq, ai-hq, or styled desktop applications

---

## 📌 Vision

**Problem:** Current KHTPM layouts are functional but minimal — grid-locked, plain text, no visual hierarchy.

**Solution:** Add a **CSS styling layer** to KHTPM that:
- Keeps the same `.chtpm` file structure (no new markup language)
- Adds `.css` files for visual customization (fonts, colors, positioning, sizes)
- Enables positionable elements (not just grid-based)
- Uses .pal files to script logic (same as before)
- Leverages X11 rendering (native performance, proven)

**Impact:** Move from "functional terminal UI" to "modern game-quality UI" while keeping KHTPM's deterministic architecture and .pal scripting.

---

## 🎯 Goals

### Short-term (Phase 1)
1. **Prettier event editor** (events-hq) — replace event-ez with styled KHTPM UI (better fonts, colors, positioning, visual hierarchy)
2. **AI interface** (ai-hq) — chat-like layout for agent45 + SCM (like Claude/Grok Electron interface)
3. Extend khtpm_strip_parser to support CSS styling, prove the pattern works

### Medium-term (Phase 2)
1. **Modern db cell** (db-hq) — Items, State Variables, Switches as styled cards/tables (not menus)
2. **Network applications** — forum (threaded posts), IRC (message panes), exchange (trading board)
3. Expand CSS feature set: animations, responsive layouts, theme switching

### Long-term (Vision)
1. Full CSS support: flexbox/grid positioning, custom fonts, animations, shadows
2. Theme system (dark/light, game skins, user-customizable)
3. Rich form components (date pickers, autocomplete, sliders)
4. Collaborative editing (multiple users in same interface simultaneously)

---

## 🏗️ Technical Approach

### Architecture Overview (KHTPM + CSS Layer)

```
.chtpm File (existing KHTPM layout structure)
  ↓
khtpm_strip_parser.c (existing parser)
  ↓ (reads .chtpm, validates, builds element tree)
  ↓
CSS Resolver (NEW: loads .css, applies styles to elements)
  ↓
X11 Renderer (ENHANCED: uses CSS styles — colors, fonts, positioning, sizes)
  ↓
Logic Layer (.pal modules handle state, events, actions — UNCHANGED)
  ↓
Game State / Persistence
```

### What It Looks Like

**KHTPM Layout** (events-hq/editor.chtpm — existing format):

```xml
<window name="event_editor" width="800" height="600">
  <vbox>
    <button id="save" label="Save"/>
    <button id="close" label="Close"/>
    <textbox id="event_text"/>
    <button id="test" label="Test Event"/>
  </vbox>
</window>
```

**CSS Styling** (events-hq/editor.css — NEW):

```css
#event_editor {
  background-color: #1a1a1a;
  font-family: "Ubuntu Mono", monospace;
}

button {
  background-color: #4a9eff;
  color: #ffffff;
  font-size: 14px;
  padding: 8px 16px;
  border-radius: 4px;
  position: absolute;  /* NEW: positionable, not grid-locked */
}

#save {
  left: 10px;
  top: 10px;
}

#close {
  left: 100px;
  top: 10px;
}

#event_text {
  left: 10px;
  top: 50px;
  width: 780px;
  height: 500px;
  font-family: monospace;
  font-size: 12px;
  background-color: #2a2a2a;
  color: #00ff00;
}

#test {
  left: 700px;
  top: 560px;
}

button:hover {
  background-color: #2a7eff;
}
```

**Companion .pal module** (events-hq/hqml_events.pal — UNCHANGED):

```pal
# Handle save button
EVENT:save -> {
  CALL save_event_to_file(current_event)
  CALL refresh_frame()
}

# Handle test button
EVENT:test -> {
  CALL run_event_preview(current_event)
}
```

### Key Features

**1. KHTPM Layout (.chtpm)**
- Same as today (no new markup language)
- Familiar structure: window, vbox, hbox, button, textbox, etc.
- No changes needed to parser logic

**2. CSS Styling (.css files — NEW)**
- Familiar CSS syntax (selectors, properties)
- Support: colors, fonts, sizes, positioning, padding, margins, border-radius
- Selectors: element type, IDs (#id), classes (.class)
- Pseudo-selectors: :hover, :focus, :active
- Positioning: absolute (positionable), relative (anchored to parent)

**3. Scripted Logic (.pal modules — UNCHANGED)**
- Modules handle events, state, persistence
- Same .pal conventions as existing code
- khtpm elements emit events: click, input, etc.
- Modules call khtpm functions: set_text(), get_value(), etc.

**4. Renderer Enhancement (khtpm_strip_parser.c)**
- Load .chtpm as before
- NEW: Load corresponding .css file
- Apply CSS styles to elements before rendering
- Render with Xlib + calculated positions/colors/fonts
- Same deterministic, X11-native output

---

## 📋 Use Cases (Phase 1-2)

### Use Case 1: events-hq (Phase 1)

**Current:** event-ez with plain text pages, minimal UI  
**Goal:** Styled event editor with improved visual hierarchy:
- Better fonts, colors, spacing
- Organized layout (title bar, toolbar, editor pane)
- Page tabs (styled, easy to switch)
- Command list (colored by category)
- Live preview of what text/choices look like

**Implementation:**
```
events-hq/
├── editor.chtpm       (KHTPM layout: window, buttons, textbox)
├── editor.css         (styling: colors, fonts, positioning)
├── hq_events.pal      (load/save events, handle page ops)
└── hq_commands.pal    (Show Text, Show Choices, Change Gold, etc.)
```

**Success Criteria:**
- [ ] Load existing entity event in events-hq
- [ ] Create new page with Show Text command
- [ ] Edit and save, verify runs identically to event-ez
- [ ] Visual appearance noticeably improved (colors, fonts, layout)

### Use Case 2: ai-hq (Phase 1)

**Goal:** GUI representation of agent45 + SCM (like Claude/Grok Electron interface):
- Chat-like message history (left pane, scrollable)
- Input field (bottom, send button)
- Agent status panel (right: thinking, running tool, done)
- Tool results (formatted, code syntax highlighting)
- Context window usage visualization

**Implementation:**
```
ai-hq/
├── chat.chtpm         (KHTPM layout: message pane, input, status panel)
├── chat.css           (styling: chat bubbles, colors, layout)
├── hq_ai.pal          (send message, format responses, manage session)
└── hq_tools.pal       (display tool outputs, handle interrupts)
```

**Success Criteria:**
- [ ] Launch ai-hq interface
- [ ] Send message to agent, see response formatted nicely
- [ ] Watch agent status update (thinking → tool running → done)
- [ ] See tool outputs with syntax highlighting
- [ ] Looks like a modern chat interface, not a terminal

### Use Case 3: db-hq (Phase 2 — became the ACTUAL FIRST PROOF of this
whole CSS engine, direct instruction 2026-08-12: "css and db-hq as
first proof" / "the real point is hqml". Promoted ahead of events-hq
below since db work was already in flight when this got decided.)

**Current:** db cell has a plain-chtpm db-ez (own separate GL window,
same launch pattern as event-ez) with 14 placeholder section rows,
Common Events wired up first, the rest built out later.

**Reference mockup:** au11-hq/rpg-maker-database.html (RPG Maker-style
database window — tab bar of 14 categories, sidebar id-list with
selected-highlight, bordered "settings-block" panels each with a
floating title label overlapping the top border, grid-2col/3col form
layouts, colored percentage-width stat bars, a traits box-list, a note
textarea, bottom Apply/OK/Cancel buttons). This supersedes the earlier
db-0000.html mockup referenced below.

**Goal:** db-hq is its own SEPARATE X11 WINDOW (own process/launch,
NOT rendered inside the taskbar's hq popup — same window-per-app model
as db-ez/event-ez, just styled via CSS instead of plain grid chtpm),
matching rpg-maker-database.html's visual structure section by
section, starting with Common Events (no stat bars needed there —
those come once an actor/stat-bearing section gets built).

**Two CSS features rpg-maker-database.html needs that the Phase 1 spec
above doesn't yet call out:**
- Percentage widths for stat-bar fills (`width: 55%` inside a
  fixed-width `.stat-bar` container), not just fixed px
- Floating title-over-border effect (`.block-title` is
  `position:absolute; top:-8px` so it visually sits ON the parent's
  top border) — an overlap/z-order case, not plain box flow

**Implementation:**
```
db-hq/
├── dashboard.chtpm    (KHTPM layout: sections for Common Events/Items/
│                        Actors/etc, one X11 window, own launch)
├── dashboard.css      (styling: bordered settings-blocks, stat bars,
│                        sidebar list, grid layouts, colors)
├── hq_db.pal          (load/save database sections)
└── hq_items.pal       (item CRUD operations)
```

### Use Case 4: network-applications (Phase 2)

**Forum UI:**
```
forum-hq/
├── forum.chtpm        (thread list view, thread detail view)
├── forum.css          (card-based layout, threading indent)
└── hq_forum.pal       (fetch posts, submit reply)
```

**IRC UI:**
```
irc-hq/
├── chat.chtpm         (channels, messages, users)
├── chat.css           (chat bubble layout, user list styling)
└── hq_irc.pal         (connect, send, format messages)
```

---

## 🪟 Window Chrome Convention (all khtpm/-hq windows, no exceptions)

**Every khtpm window is `override_redirect = True`, no window-manager
decoration, no custom title bar/close-button chrome drawn by us either.**
Direct instruction (2026-08-12, corrected on db-hq's first build which
mistakenly used `XCreateSimpleWindow` and got a WM title bar): "we dont
want those in khtpm windows" / "tb doesn't have gl chrome header either" /
"context window doesn't have one, get it?" — this matches BOTH existing
precedents already in the codebase:
- `khtpm_strip_parser.c`'s own three windows (`win`/`hq_win`/`popup_win`,
  around khtpm_strip_parser.c:1481-1517) all set
  `XSetWindowAttributes.override_redirect = True` and create via
  `XCreateWindow(..., CWOverrideRedirect | CWBackPixel | CWEventMask, &swa)`
  — never `XCreateSimpleWindow` (which implicitly invites WM decoration).
- Entity/context GL windows (e.g. `01.muchi-pals-🥚️-13.01/system/egg_window.c`)
  follow the same override_redirect pattern — no header there either.

**Rule for any new khtpm/-hq window (db-hq, events-hq, ai-hq, etc.):**
copy khtpm_strip_parser.c's exact `XSetWindowAttributes` +
`CWOverrideRedirect|CWBackPixel|CWEventMask` shape, `XMapRaised` (not
`XMapWindow`), no `XStoreName`/no WM_DELETE_WINDOW protocol (there's no WM
to send it - override_redirect windows are invisible to window managers).
Since there's no WM close button, give the window its own keyboard close
(e.g. Escape when no other input is pending) instead of relying on
WM_DELETE_WINDOW.

**CORRECTION 2026-08-28:** that rule is true for **strip / override_redirect
popups** (taskbar Settings, entity menu). It is **false** if read as the
recipe for **WM-managed HQ** windows in `khtpm_entity_menu_render.c`.
Live: chat-hai `XMapRaised` stole the human browser; HQ now uses
`XMapWindow`. Popups stay `XMapRaised` + no `XSetInputFocus` on map.
Do not gate file-history poll on X focus. Cite:
`HQ-WINDOW-MAP-AND-AGENT-INPUT.md` + `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`.

---

## 🎞️ Why db-hq doesn't render through raw-RGB + GL textures

Question that came up building db-hq (2026-08-12): the house already has a
whole raw-RGB rendering family - `system/chtpm_rgb_render.c` /
`ops/compose_rgb_frame.c` rasterize text/tiles into a plain RGBA8 byte
buffer (`pieces/display/rgb_frame.raw` + a `.receipt.txt` for dimensions),
and `system/gl_mirror.c` uploads that buffer via `glTexImage2D` and draws
ONE textured quad in a GLUT window (`gl_mirror.c:429-432,608`). Why doesn't
db-hq (or any khtpm window) use that same pattern?

**Because that pattern exists to solve a GL-specific problem db-hq doesn't
have.** A bare OpenGL/GLUT context has no text or font API at all - if you
want a single character on screen, you either hand-rasterize it into a
pixel buffer yourself and upload it as a texture, or you don't get text.
`chtpm_rgb_render.c`'s whole `load_glyphs()`/`blit_char()`/`blit_text()`
machinery (an 8×16 fixed bitmap glyph grid, one static per-character
bitmap loaded from disk) is that workaround, and the raw-RGB-buffer-then-
texture-upload step is just how you get an arbitrary pixel buffer onto a
GL surface once you've built it.

khtpm windows (the taskbar, db-hq, any future `-hq` app) never open a GL
context in the first place - they're plain Xlib windows. Xlib already has
a native, non-GL version of "compose a frame somewhere off-screen, then
present it": `khtpm_hq_render.c`'s `redraw()` draws into an off-screen
`Pixmap` (`buf`) using real Xlib/Xft calls - `XFillRectangle` for boxes,
`XftDrawStringUtf8` for antialiased proportional text (real font metrics,
not a fixed bitmap grid) - then `XCopyArea(dpy, buf, win, ...)` blits that
Pixmap to the visible window. Same two-step shape as the GL pipeline
(compose off-screen → present), just using X11's own compositing
primitive (`XCopyArea`) instead of a GL texture upload, because Xft
already provides real text rendering - which is the exact capability the
GL family had to build raw-RGB rasterization to work around not having.

**The one place db-hq DOES touch raw RGB bytes:** `dump_frame_png()`
(bound to the 'p' key) reads the composed Pixmap back with `XGetImage`
and writes a PNG via the same vendored `stb_image_write.h` the house's
own `dump_rgb_png.c` uses - same debugging need (an agent can't look at a
live window directly), same output format, just `XGetImage` instead of
`glReadPixels` since there's no GL context to read from here.

---

## 🔧 Technical Requirements

### Phase 1: Foundation (Weeks 1-4)

**Enhance KHTPM Renderer:**
1. **CSS Parser** (C, in khtpm_strip_parser.c)
   - Read .css file alongside .chtpm
   - Parse selectors (#id, .class, element, :hover, :focus)
   - Parse properties: color, background-color, font-family, font-size, padding, margin, left, top, width, height, border-radius, position
   - width/height must accept PERCENTAGE values (not just px) — needed
     for db-hq's stat-bar fills (rpg-maker-database.html), a fill div's
     width is a % of its fixed-width parent, not an absolute size
   - position:absolute with a NEGATIVE top (e.g. top:-8px) must be able
     to render OUTSIDE/OVERLAPPING its parent's own border — needed for
     db-hq's floating block-title-over-border effect; simplest
     approach: absolute-positioned children draw in a later pass than
     their parent's border, so they always paint on top regardless of
     offset direction
   - ~1-2k lines of parsing + validation

2. **Style Application Engine**
   - Match CSS selectors to KHTPM elements
   - Merge computed styles (cascading: element → class → ID)
   - Apply to element before rendering
   - ~1-2k lines

3. **Enhanced X11 Rendering**
   - Use CSS values: font rendering with custom fonts, colors from CSS, positioned elements
   - Layout: absolute positioning (left, top, width, height) instead of fixed grid
   - Text styling: font family, size, color from CSS
   - Reuse existing Xlib code, add CSS application layer
   - ~1-2k lines of modifications to existing renderer

4. **First Complete Example: events-hq**
   - Convert event-ez to events-hq (.chtpm + .css)
   - Verify identical behavior to event-ez
   - Styled and positioned layout using CSS
   - Test with Show Text + Show Choices commands (already built)

**Testing:**
- Relay harness: load events-hq, edit event, save, run, verify output
- Visual comparison: event-ez vs events-hq (functionality identical, appearance better)

### Phase 2: Expansion (Weeks 5-8)

**Enhance Further:**
1. **AI-HQ Implementation**
   - Chat interface layout (.chtpm)
   - Message bubble styling (.css)
   - Status panel with colors
   - Integration with agent45 + SCM (.pal modules)

2. **CSS Feature Expansion**
   - Animations (fade, slide, grow)
   - Hover states, transitions
   - Responsive layout helpers
   - Theme variables (easy dark/light switching)

3. **db-hq Implementation**
   - Dashboard layout with tabs (.chtpm)
   - Card-based item grid (.css)
   - State variable table with styling
   - Switch toggles with visual feedback

4. **Network Applications**
   - Forum UI (threads, styling, layout)
   - IRC UI (channels, messages, users panel)
   - Exchange UI (trading board, order book)

---

## 📐 File Structure (Proposed)

```
house_root/
├── *.monads/*.livedesk-taskbar/ops/
│   ├── khtpm_strip_parser.c   (ENHANCED: add CSS parsing + styling)
│   ├── khtpm_strip_layout.c   (ENHANCED: support absolute positioning)
│   └── khtpm_taskbar_manager.c (launch -hq apps)
│
├── &.hq-apps/                 (Styled KHTPM applications)
│   ├── events-hq/
│   │   ├── editor.chtpm       (KHTPM layout)
│   │   ├── editor.css         (CSS styling)
│   │   ├── hq_events.pal      (load/save/edit logic)
│   │   └── README.md
│   ├── ai-hq/
│   │   ├── chat.chtpm
│   │   ├── chat.css
│   │   ├── hq_ai.pal
│   │   └── README.md
│   ├── db-hq/
│   │   ├── dashboard.chtpm
│   │   ├── dashboard.css
│   │   ├── hq_db.pal
│   │   └── README.md
│   ├── forum-hq/
│   ├── irc-hq/
│   └── exchange-hq/
│
├── &.hq-lib/                  (Shared CSS & patterns)
│   ├── base.css               (colors, fonts, standard styles)
│   ├── components.css         (buttons, cards, inputs, tables)
│   └── themes/
│       ├── dark.css
│       └── light.css
```

---

## 🔄 Relationship to Existing Systems

### KHTPM (X11 UI System — Enhanced)
- **Before:** Plain, grid-locked layouts (functional but minimal)
- **After:** CSS-styled, positioned layouts (modern appearance)
- **Same:** Parser, .pal integration, deterministic behavior
- **Added:** CSS parsing, style application, absolute positioning
- **Architecture:** No changes; CSS is purely visual layer

### CHTPM (GL/Game UI System — Unchanged)
- **Status:** Stays as-is for game rendering
- **Used for:** In-game entity rendering, game graphics
- **Not affected:** KHTPM CSS enhancements are X11-only

### Event Editors: event-ez vs events-hq

**Phase 1:** Both exist side-by-side
- livedesk entity → right-click → "Events (ez)" OR "Events (hq)"
- Users choose which one to use (events-hq has better appearance)
- Both save to identical event.ir.pdl format

**Phase 2:** events-hq proven stable
- Default to events-hq
- Keep event-ez as fallback for edge cases

**Phase 3:** Optional full migration
- Retire event-ez if events-hq fully replaces functionality

### Example: livedesk Launch Pattern

```
User clicks "ai" cell (hypothetical new cell)
  ↓
livedesk calls: open_styled_app("&.hq-apps/ai-hq/chat.chtpm")
  ↓
khtpm_strip_parser loads chat.chtpm + chat.css
  ↓
CSS styling applied to all elements
  ↓
Renders styled X11 window (colors, fonts, positioning from CSS)
  ↓
hq_ai.pal handles events, sends/receives messages
  ↓
Display updated with styled results
```

---

## 🚀 Implementation Path

### Quick Win (Week 1)
1. Design CSS subset spec (selectors, properties, syntax)
2. Add CSS parser to khtpm_strip_parser.c (basic tokenizer + selector matching)
3. Create first styled app: events-hq/ with editor.chtpm + editor.css

### MVP (Week 2-3)
1. Implement style application engine (apply CSS to KHTPM elements)
2. Enhance X11 renderer to use CSS values (colors, fonts, sizing)
3. Support absolute positioning (left, top, width, height from CSS)
4. Build events-hq layout + styling
5. Reuse event-ez logic, just render with styled KHTPM

### Polish (Week 4)
1. Add :hover, :focus, :active pseudo-selectors
2. Test events-hq end-to-end (create/edit/save/run event)
3. Compare output with event-ez (verify identical behavior)
4. Write CSS guide for app developers

### Phase 2 (Future)
1. ai-hq (chat interface with agent45 + SCM)
2. db-hq (dashboard, items, state, switches)
3. network-applications (forum, IRC, exchange)

---

## ⚠️ Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| CSS parser bugs | Wrong styles applied, visual glitches | Start with minimal CSS subset, test incrementally |
| Absolute positioning overlaps | Elements hidden behind others, unusable | Test layout carefully, add z-index support |
| Font/color rendering issues | Bad appearance, unreadable text | Test with multiple fonts, verify Xlib rendering |
| Breaking existing KHTPM apps | Regression, existing UIs broken | Don't modify existing .chtpm files; CSS is opt-in |
| Performance regression | Slower rendering with CSS parsing | Profile early, optimize CSS matching/caching |

---

## 📊 Success Criteria (End of Phase 1)

- [ ] CSS parser in khtpm_strip_parser.c parses .css files without crashes
- [ ] Style matching engine correctly applies CSS to KHTPM elements (by ID, class, element type)
- [ ] X11 renderer uses CSS values for colors, fonts, positioning
- [ ] events-hq loads, displays real entity event with CSS styling
- [ ] User can edit event in events-hq (change text, add page, save) — identical to event-ez
- [ ] Saved event runs identically to event-ez version
- [ ] Visual appearance is noticeably better than event-ez (colors, fonts, spacing)
- [ ] At least 2 other devs can use events-hq without major friction
- [ ] Documented: CSS syntax guide, KHTPM+CSS architecture, events-hq walkthrough

---

## 📚 References & Inspiration

- **CSS:** Standard CSS3 syntax (familiar to web devs)
- **Game engines:** Godot (UI toolkit with CSS-like styling), Qt (style sheets for native apps)
- **Our systems:** KHTPM (modular, X11-native), .pal (scriptable), event-ez (proven pattern)

---

## 🎓 Notes for Future Developers

- **Start small:** CSS parser doesn't need full CSS spec. Support selectors, basic properties. Incremental.
- **Leverage existing code:** Build on khtpm_strip_parser.c, don't reinvent. CSS is a styling layer.
- **Keep .pal unchanged:** All game logic stays in .pal modules. CSS is pure view.
- **Test visually:** Render one button with CSS, verify colors/font work. Then add more features.
- **Reuse patterns:** If KHTPM already handles event dispatch, use it. Don't add new event systems.
- **Documentation:** CSS syntax guide + examples. Show "before KHTPM" vs "after CSS styling" side-by-side.

---

**Last Updated:** 2026-08-12 (Design phase)  
**Status:** Ready for implementation planning  
**Next:** CSS parser prototype, events-hq first example
