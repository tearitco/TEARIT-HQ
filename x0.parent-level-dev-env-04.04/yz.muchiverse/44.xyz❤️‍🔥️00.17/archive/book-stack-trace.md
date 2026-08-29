# 📚 Book-Stack Navigation & Entity Flow Trace

## 🎬 The Big Picture
Book-stack is a **MONAD** (standalone entity + reader UI). It shows **structured choices** → **branches** → **text display** via a unified book-stack entity window.

```
┌─────────────────────────────────────────────────────────────┐
│ BOOK-STACK MONAD (*.monads/*.book-stack/)                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐                                           │
│  │  Entity      │  ← THE WINDOW (tp_desktop_window)         │
│  │ book-stack   │    - Lives in: entities/book-stack/       │
│  │ (entity ID)  │    - Has: atlas.png, sprite.csv, etc      │
│  └──────────────┘                                           │
│         ↓                                                    │
│  ┌──────────────────────────────┐                           │
│  │  Reader App (prisc+x)         │                          │
│  │  - Runs event.pal scripts     │                          │
│  │  - Dispatches choices         │                          │
│  │  - Shows text via khtpm       │                          │
│  └──────────────────────────────┘                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## 🚀 Launch Sequence (User runs `sh button.sh run`)

```
Step 1️⃣  START: button.sh receives 'run' action
         ↓
Step 2️⃣  start_entity_window()
         ├─ Check: is book-stack window already open?
         │  (pgrep for entities/book-stack path)
         ├─ If not: setsid nohup tp_desktop_window entities/book-stack
         └─ Result: ✅ Window opens, registers in livedesk_open.txt
         ↓
Step 3️⃣  prisc+x pieces/reader/event_pkg/pages/page_1/event.pal
         ├─ Runs: exec dispatch.sh
         └─ Result: ✅ Reader app starts
         ↓
Step 4️⃣  dispatch.sh → khtpm_show_choices
         ├─ Reads: choices.objects.pdl (bible_text, bible_tts, tao)
         ├─ Display: Popup menu with 3 choices
         └─ Result: ✅ User sees "Choose:" dialog
         ↓
Step 5️⃣  User picks → Branch runs
         ├─ bible_text: bash branches/bible_text/run.sh
         ├─ Result: Runs bible_verses binary, gets text
         ├─ Calls: khtpm_show_text package="entities/book-stack" text="verse.txt"
         └─ Result: ✅ Text displays in SAME entity window
```

## 🔀 Navigation: What Happens on "Read Next" / href?

### Scenario A: Text Display → Another Branch (bible_text → bible_tts)

```
Current State:
├─ Entity window: book-stack (open, has X focus)
├─ Display: Bible text from bible_text branch
└─ Nav focus: entity's context menu (if open)

User Action: Click "Listen Instead" link
       ↓
⚡ FLOW:
  1. khtpm_show_text displays link
  2. User clicks link
  3. Link calls: bash branches/bible_tts/run.sh
  4. Bible TTS binary runs, outputs audio
  5. khtpm_show_text called AGAIN with SAME package
     └─ REUSES book-stack entity window (still same PID)
  6. Display updates with new content

✅ Result: SAME entity window, SAME context menu
   → Context menu nav focus should work ✓
```

### Scenario B: Text Display → Different Entity? ("href to next")

```
⚠️ QUESTION: When user "hrefs to next", what actually happens?

Possibilities:
A) Another branch in book-stack (stays in same window)
B) Links within text open a NEW entity window (different PID)?
C) Text display can invoke: tp_desktop_window entities/DIFFERENT_ENTITY?

IF IT'S B OR C:
├─ New entity window opens (new PID, new process)
├─ New window registers in livedesk_open.txt
├─ Taskbar shows new tab ✓
├─ BUT: New window's context menu doesn't get nav focus ❌
└─ Reason: Taskbar doesn't route keyboard to second window's menu

OBSERVED SYMPTOM:
└─ "Second context isn't getting nav focus"
   = Second entity window's popup menu has no keyboard input
```

## 🪟 Entity Window Life Cycle (tp_desktop_window.c)

```
Window Opens:
  1. tp_desktop_window entities/book-stack
  2. Registers: livedesk_open.txt
     └─ PID=12345|ENTITY=book-stack|PATH=.../entities/book-stack
  3. Registers: livedesk_nav_claims.txt (tab claim)
     └─ KIND=tab|PID=12345|NAV=8|ENTITY=book-stack|PATH=...

Context Menu Opens (user right-clicks):
  4. Popup appears on window
  5. Claims rows in nav_claims.txt (KIND=row)
     └─ KIND=row|PID=12345|NAV=9|PATH=.../entities/book-stack
  6. Window grabs X focus for keyboard input
  7. Keyboard arrows/Enter navigate rows ✓

Context Menu Closes:
  8. Releases KIND=row claims
  9. Releases keyboard focus
```

## ❓ The Problem Zone: "Second Entity's Context Menu"

```
IF user navigates to a SECOND entity via href:

New Entity Opens:
  1. NEW tp_desktop_window entities/OTHER_ENTITY
  2. NEW PID (let's say: 67890)
  3. Registers: livedesk_open.txt
     └─ PID=67890|ENTITY=other|PATH=.../entities/other
  4. Registers: livedesk_nav_claims.txt (tab claim)
     └─ KIND=tab|PID=67890|NAV=9|ENTITY=other|PATH=...

User Right-Clicks on NEW Window:
  5. Popup opens ✓
  6. TRIES to claim rows:
     └─ KIND=row|PID=67890|NAV=10|PATH=.../entities/other
  7. ❌ PROBLEM: Taskbar still owns keyboard!
     └─ Taskbar doesn't know PID 67890's menu is open
     └─ Keyboard still routes to taskbar, not to PID 67890's window
  8. Context menu visible but can't use arrows/Enter 💔
  
Why First Entity's Menu Worked:
  └─ User right-clicked the window directly
  └─ Window grabbed X focus naturally
  └─ Keyboard went to that process automatically
  
Why Second Entity's Menu Fails:
  └─ Taskbar still has focus from previous context menu close
  └─ Taskbar doesn't explicitly release focus to new window
  └─ Window claims nav rows but taskbar doesn't know to stop listening
```

## 🔧 The Fix Needed

```
Taskbar should:

1️⃣  MONITOR livedesk_nav_claims.txt for KIND=row entries
    └─ Check: Are there any open entity menus? (PID ≠ taskbar's PID)

2️⃣  INHIBIT keyboard handling when foreign entity menu is open
    └─ If KIND=row claim exists with different PID:
       └─ DON'T process KeyPress in main event loop
       └─ Let the remote entity's process handle it

3️⃣  RESTORE keyboard handling when entity menu closes
    └─ When all KIND=row claims gone:
       └─ Taskbar can resume its own nav

Result:
  ✅ First context menu: Works (entity has focus)
  ✅ Second context menu: Works (taskbar releases focus to entity)
  ✅ Unified flow: Keyboard goes to whoever has menu open
```

## 📝 Summary: book-stack Entities Model

| Interaction | Entity | Window | PID | Context Menu |
|-------------|--------|--------|-----|--------------|
| First read (bible_text) | book-stack | Open | 12345 | ✅ Works |
| Switch branch (→ bible_tts) | book-stack | SAME | 12345 | ✅ Works |
| href to next (→ other entity) | other | NEW | 67890 | ❌ No focus |
| Close & reopen first | book-stack | NEW | 99999 | ❌ No focus* |

*= If second entity was opened during session, taskbar's focus routing is confused

---

**KEY INSIGHT**: The problem isn't visual unity (that's fixed). The problem is **cross-process keyboard routing**. Taskbar needs to detect when a remote entity's context menu is open and **yield** the keyboard to that entity's process.
