# PAL ↔ TPMOS Parity Standards

Core standards for PAL-based games/apps to maintain parity with TPMOS architecture patterns. These are not suggestions—they're the proven patterns from real TPMOS/Wraith.

## 1. Frame History & Terminal Scrollback (No Flicker)

**Standard**: Frames render to **stdout directly**, not to files. Terminal's natural scrollback captures history.

### Why:
- User can scroll up to see previous frames (standard terminal behavior)
- No flicker (frames don't need to be "loaded" from disk)
- Cleaner rendering pipeline
- Matches real TPMOS exactly

### How (PAL):
```c
/* compose_frame.c should output to stdout AND file:
   fprintf(out, "...frame content...\n");  // out = stdout OR file
   fflush(out);  // ensure immediate display
*/
```

Instead of:
```c
FILE *f = fopen("current_frame.txt", "w");
fprintf(f, "...frame...");
fclose(f);
```

Do:
```c
fprintf(stdout, "...frame...");
fflush(stdout);  // immediate display
```

The renderer can still poll `current_frame.txt` for fallback, but stdout is primary.

---

## 2. Cursor-Based Menu Navigation (Bracket Display)

**Standard**: All menus use bracket-cursor display: `[>]` for active, `[ ]` for inactive.

### Navigation Methods (all supported simultaneously):
- **Arrow keys** (1002/1003) — move cursor up/down
- **WASD** (w/W = up, s/S = down) — alternative to arrows
- **Digit typing** (1-9, 0) — jump directly to that option
- **Enter** (10/13) — select current option

### Display Format (from real chtpm_parser.c):
```
[>] 1. [Startup New Corp]
[ ] 2. [Capital Contribution]
[ ] 3. [Public Stock Offering]
```

### State Tracking:
```
pieces/<piece_id>/state.txt must track:
  cursor=<current_option_number>
  digit_accum=<partial_digit_entry>  (for multi-digit jumps like "13")
  active_menu_piece=<piece_id>
```

---

## 3. Submenus Must Have Same Navigation as Main Menu

**Standard**: Every submenu (financing, management, trade, etc.) supports:
- Bracket cursor display
- Arrow/WASD movement
- Digit jumping
- Enter to select

Every menu piece must go through the **same** input dispatcher (`wsr_menu_input.c` equivalent).

---

## 4. Piece.PDL as the Single Source of Truth for Menus

**Standard**: A menu **only exists in piece.pdl**. No hardcoded option arrays in C.

### Format:
```
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | wsr_main_menu
META         | version            | 1.0

METHOD       | Label Here         | COMMAND_HERE
METHOD       | Another Option     | GOTO:wsr_financing_menu
```

### Adding a Menu Option = Editing piece.pdl (1 line, no recompile)

This matches real chtpm_parser.c's load_dynamic_methods() pattern.

---

## 5. Special Command Prefixes (Dispatch Routing)

Recognized in piece.pdl METHOD rows:

| Prefix | Behavior | Example |
|--------|----------|---------|
| `GOTO:<piece_id>` | Navigate to submenu | `GOTO:wsr_financing_menu` |
| `TICK_ALL:<turns>` | Advance world N turns | `TICK_ALL:1` |
| `CYCLE_CORP` | Next corporation | `CYCLE_CORP` |
| `TOGGLE_TICKER` | Toggle ticker display | `TOGGLE_TICKER` |
| `NEW_GAME` | Reset world from template | `NEW_GAME` |
| `START_WIZARD:<wizard_id>` | Launch input wizard | `START_WIZARD:new_corp` |
| `RUN:<command>` | Run op, capture stdout as message | `RUN:./ops/+x/player_trade.+x buy 10` |
| `STUB` | "Not yet available" message | `STUB` |
| *anything else* | Shell the command directly | `some_custom_op.+x arg1` |

---

## 6. Auto-Set Active Entity on Creation/Selection

**Standard**: When a player creates a corporation or selects one for the first time, it automatically becomes the "active" entity.

### Implementation:
- `corp_ipo.c`: After creating new corp, set `active_corp_index` to point to it
- Menu navigation (CYCLE_CORP): Update `active_corp_index` in state.txt
- Auto-select on any corp-affecting action

### Why:
Matches real WSR / TPMOS behavior—newly created entities are immediately selected for interaction.

---

## 7. Portfolio Listing (Real Financial Data, Not Placeholder)

**Standard**: "List Portfolio" shows actual holdings with real price/% change data.

### Display Format:
```
HOLDINGS
--------
Ticker | Buy Price | Current | Change | Change %
-------|-----------|---------|--------|----------
  ORB  |   $100    |  $116   |  +$16  |  +16.0%
  AFL  |   $50     |   $45   |   -$5  |   -10%
  TGT  |   $200    |  $220   |  +$20  |   +10%

Click/Enter for details:
- Assets & Liabilities (balance sheet)
- Lending banks & loan details
- Debt structure
- Shareholder distribution
```

### Implementation:
- Read player holdings from `pieces/player_you/holdings.txt` (ticker|shares pairs)
- Lookup current stock price from `pieces/corp_<ticker>/state.txt`
- Calculate buy price from transaction history or average cost basis
- Display change as delta and percentage
- On select, drill into corp's full financial statements

---

## 8. Rendering Pipeline Order

**Standard**: Frame render → stdout → file → renderer reads file (for external displays).

1. **compose_frame** outputs to **stdout** (primary, for terminal scrollback)
2. **compose_frame** also writes **current_frame.txt** (fallback, for external renderer)
3. **renderer.c** (separate process) can:
   - Read from stdout via pipe, OR
   - Poll current_frame.txt for GUI/external display

This dual path supports both TUI (terminal) and GUI (GL window) rendering simultaneously.

---

## 9. Terminal I/O Constants (Arrow Keys & Special Keys)

**Standard**: Use these exact keycodes everywhere (same as real chtpm_parser.c):

```c
#define ARROW_UP    1002
#define ARROW_DOWN  1003
#define ARROW_LEFT  1004
#define ARROW_RIGHT 1005
#define ENTER       10    // also 13 (CR)
#define BACKSPACE   127   // or 8
#define ESCAPE      27
#define ETX         3     // Ctrl-C
```

These are decoded by `keyboard_input.c` from raw terminal sequences and used consistently across all input handlers.

---

## 10. State Persistence Files

**Standard**: Every piece has `state.txt` (key=value format). Every piece state includes:

```
cursor=<menu_cursor_pos>
digit_accum=<partial_digits>
active_menu_piece=<current_screen_id>
last_message=<status_line>
prompt_active=<0|1>  (if wizard is running)
```

State files are the **single source of truth** for UI state. No in-memory-only state that survives process restarts.

---

## 11. Wizard Flows (Multi-Step Input)

**Standard**: Complex input (like creating a corp with industry/country/funding/name/ticker) uses a **wizard** pattern:

- One `.+x` op per wizard (`wsr_wizard_input.c`)
- Each step is a `prompt_step` in state.txt
- Every keypress routes through the wizard until `prompt_active=0`
- Real examples: Startup New Corp (5 steps), Character creation, Equipment crafting

---

## Summary: Three-Process Architecture

```
┌─────────────────┐
│ keyboard_input  │ Raw keypresses → history.txt (append-only)
└────────┬────────┘
         │
    ┌────v────┐
    │ prisc+x │ Reads history.txt + state.txt
    │(main    │ Calls ops (input dispatchers, tick logic)
    │ loop)   │ Ops update state.txt
    └────┬────┘
         │
    ┌────v──────────┐
    │ compose_frame │ Reads state.txt, renders to stdout + current_frame.txt
    └────┬──────────┘
         │
    ┌────v──────────┐
    │ renderer      │ TUI: polls stdout; GUI: polls current_frame.txt
    └───────────────┘
```

**Key**: Each process is independent. Input doesn't block rendering. State is the single source of truth. Frame history lives in terminal scrollback (stdout) naturally.

---

## Verification Checklist

- [ ] All menus use bracket cursor `[>]` / `[ ]`
- [ ] Arrow keys + WASD + digit typing all work in every menu
- [ ] Frames render to stdout (terminal scrollback works)
- [ ] piece.pdl is the only menu definition (no C option arrays)
- [ ] New corp/selection auto-sets as active
- [ ] Portfolio shows real buy price vs current price with % change
- [ ] State.txt tracks cursor position (survives restarts)
- [ ] All special commands (GOTO, TICK_ALL, RUN, etc.) work consistently
- [ ] No flicker (smooth frame updates, no file-read lag)
