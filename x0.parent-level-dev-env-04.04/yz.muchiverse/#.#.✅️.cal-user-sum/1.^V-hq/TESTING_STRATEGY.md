# Testing Strategy — Relay-Based (No Cheating)

**Version:** 2026-08-11  
**Principle:** All testing through inject/relay, matching user keyboard input  
**Owner:** claude-0001 and all agents

---

## The Rule: Relay First, Not Direct CLI

### ❌ Wrong (Direct CLI)
```bash
# This tests the binary, not the UI:
userpal_create_account.+x testuser "Test User"
ls xyzfs/users/*/meta.txt | wc -l
# ✗ Misses UI bugs (missing button, broken relay dispatch, wrong field binding)
```

### ✅ Right (Relay) — this exact sequence is VERIFIED, not hypothetical
Ran for real 2026-08-11 against the USER cell's New User flow (see USER_CREATION.md for the full
trace) — a real account + xyzfs home was created purely through this:
```bash
NAV="#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"
HOUSE="$PWD" bash "$NAV" esc                     # clean slate first (see Resilience section below)
HOUSE="$PWD" bash "$NAV" nav 2                   # digit-jump opens the USER cell submenu directly
HOUSE="$PWD" bash "$NAV" key Return              # activate focused row ("New User...")
HOUSE="$PWD" bash "$NAV" key Return              # Enter again arms cli-io typing mode
HOUSE="$PWD" bash "$NAV" type "testuser"
HOUSE="$PWD" bash "$NAV" key Return              # submit user_id -> auto-advances to display-name stage
HOUSE="$PWD" bash "$NAV" type "Test User"
HOUSE="$PWD" bash "$NAV" key Return              # submit -> creates account + auto-logs-in
cat "0.user-pal👤️/00.login-signup/users/testuser/profile.txt"
# ✓ Tests entire pipeline: relay -> parser -> manager -> cli-io -> userpal_create_account.+x -> filesystem
```

---

## Relay System (livedesk_agent_relay.txt)

### What It Is
- Single ASCII-per-line file at `#.desktop/livedesk_agent_relay.txt`
- Parser (khtpm_strip_parser.c) polls it every ~300ms
- Each line = one key code (decimal)

### Key Codes (Most Used)
- **Digits 0-9:** `48-57` (or use `nav.sh nav N` which handles encoding)
- **Enter:** `13` (or `nav.sh key Return`)
- **Escape:** `27` (or `nav.sh key Esc`)
- **Backspace:** `8` (or `nav.sh key BackSpace`)
- **Printable chars:** ASCII 32-126 (or `nav.sh type "text"`)

### Harness Command Reference (nav.sh)
```bash
nav.sh nav <n>          # Type digit(s) <n>, press Enter
nav.sh row <n>          # Same as nav (alias)
nav.sh key <name>       # Send one key (Return, Esc, BackSpace, or char)
nav.sh type <text>      # Type text char-by-char (no Enter)
nav.sh esc              # Short for nav.sh key Esc
nav.sh frame            # Print last frame-history line (current state)
nav.sh wait [sec]       # Sleep (default 0.6)
```

---

## Testing REAL X11 Input (not the relay) — no `xdotool` on this machine

The relay (`livedesk_agent_relay.txt`/`nav.sh` above) is the right tool for testing DISPATCH logic
(does the right thing happen once a code reaches the manager), but it never touches the real X11
`KeyPress`/`ButtonPress` path at all — anything that specifically needs to verify real X11 CAPTURE
(e.g. does a real physical keypress get read and translated correctly, not just "if this code
arrives, what happens") needs real X11 event injection, not the relay.

**Confirmed 2026-08-18: `xdotool` is not installed on this machine.** Real, working alternative
already exists in this house, written for exactly this reason: `&.widgits/tile-picker/ops/
tp_test_send_key.c` and `tp_test_send_click.c` (both pre-compiled to `+x/`) use `XTest`
(`libXtst`, confirmed installed) directly — a genuine X11 event, not a fake file-based shortcut.

```bash
# Usage (both match target window by WM_NAME substring via XFetchName):
tp_test_send_key.+x "<window_name_substring>" <keysym_name>     # e.g. Return, Escape, 3
tp_test_send_click.+x "<window_name_substring>" <button:1|2|3> [rel_x] [rel_y]
```

**Gotcha, found live 2026-08-18**: these two match windows by TITLE (`XFetchName`/`WM_NAME`), not
`WM_CLASS`. Some real apps (the taskbar's own `khtpm_strip_parser.c` windows, confirmed via
`XSetClassHint`) set `WM_CLASS` but never a title at all — `find_by_name()` in both tools won't find
them. If a target window has no title, use `XQueryTree` + `XGetClassHint` to walk and match by
`WM_CLASS` instead (a small, disposable variant of the same `find_by_name()` shape works fine - the
underlying `XTestFakeKeyEvent`/`XTestFakeButtonEvent` calls don't need to change, just the lookup
step). Don't assume a window without a title can't be targeted - it just needs a different lookup.

---

## Frame History (Your Ground Truth)

### Location
`#.desktop/khtpm_strip_frame_history.txt` — one frame per relay poll

### What It Contains
```
header.focus=0[type=button label=HQ onClick=ACTIVATE:1] \
  header.active=-1 bottom.focus=-1 unified_nav_focus=0 \
  has_real_x11_focus=1 cliio_active=0 nav_armed=1 \
  digit_buf= hq_focus=-1 element_count=20
```

### How to Read It
- `header.focus=0` → Focus on element 0 (HQ cell)
- `hq_active=11` → Submenu for cell 11 (menus) is open
- `hq_focus=2` → Focus on row 2 of the submenu
- `cliio_active=1` → Text input modal is open
- `cliio_buffer=alice` → User typed "alice"

### In a Test Harness
```bash
# Before action:
nav.sh frame > /tmp/before.txt

# Do something (e.g., press key):
nav.sh nav 2

# After action:
nav.sh frame > /tmp/after.txt

# Verify change:
if grep -q "hq_focus=2" /tmp/after.txt; then
  echo "PASS: Focus moved to row 2"
else
  echo "FAIL: Focus did not move"
  diff /tmp/before.txt /tmp/after.txt
fi
```

---

## Test Harness Template

### Basic Structure
```bash
#!/bin/bash
# test_<feature>.sh — relay-based test for <feature>
# Tests: <what this verifies>
# Usage: bash test_user_creation.sh

set -e
HOUSE="${HOUSE:-$PWD}"
NAV="$HOUSE/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"
RESULTS_DIR="${RESULTS_DIR:-.}/results"
mkdir -p "$RESULTS_DIR"

# Logging helper
log_step() {
  local msg="$1"
  local timestamp=$(date '+%H:%M:%S')
  echo "[$timestamp] $msg" | tee -a "$RESULTS_DIR/log.txt"
}

# Frame capture helper
capture_frame() {
  local name="$1"
  $NAV frame > "$RESULTS_DIR/frame_$name.txt"
  log_step "Frame: $name"
}

# Test starts here
log_step "=== Starting Test: User Creation ==="

# Step 1: Check initial state
capture_frame "initial"

# Step 2: Navigate to USER cell (cell 2)
log_step "Navigating to USER cell..."
$NAV nav 2
$NAV key Return
capture_frame "after_user_cell"

# Step 3: Verify submenu opened
if grep -q "hq_active=2" "$RESULTS_DIR/frame_after_user_cell.txt"; then
  log_step "✓ USER cell opened"
else
  log_step "✗ USER cell did not open"
  exit 1
fi

# Step 4: Type new username
log_step "Creating new user 'testuser'..."
$NAV type "testuser"
$NAV key Return

# Step 5: Type display name
$NAV type "Test User"
$NAV key Return
capture_frame "after_create"

# Step 6: Verify files exist
log_step "Verifying filesystem..."
if [ -f "$HOUSE/users/testuser/profile.txt" ]; then
  log_step "✓ Profile file exists"
  cat "$HOUSE/users/testuser/profile.txt" >> "$RESULTS_DIR/proof.txt"
else
  log_step "✗ Profile file not found"
  exit 1
fi

log_step "=== TEST PASSED ==="
```

### Output Structure
```
results/
├── log.txt                     # Timestamped log of each step
├── frame_initial.txt           # Frame before anything
├── frame_after_user_cell.txt   # Frame after opening USER submenu
├── frame_after_create.txt      # Frame after creating user
├── proof.txt                   # Filesystem evidence (profile.txt content)
└── summary.md                  # PASS/FAIL summary for PM
```

---

## Harness Discovery (Finding Tests)

### Registered Harnesses
```
xyzfs/users/claude-0001/harnesses/
├── README.md                    (index of all tests + descriptions)
├── test_user_creation.sh       
├── test_change_gold.sh         
└── [others registered here]
```

### Harness README Pattern
```markdown
# Test Harnesses — claude-0001

## Quick Start
bash test_user_creation.sh

## Available Tests
- **test_user_creation.sh** — Create new account, verify xyzfs home created
  - Expected time: ~5 seconds
  - Prerequisites: livedesk running
  - Output: results/summary.md

- **test_change_gold.sh** — Create + run Change Gold event, verify persistence
  - Expected time: ~30 seconds
  - Prerequisites: livedesk running, one session created
  - Output: results/summary.md + state proof

[... etc ...]
```

---

## Common Patterns

### Pattern 1: Navigate to Cell + Open Submenu
```bash
$NAV nav <cell_number>
$NAV key Return
$NAV frame > /tmp/state.txt
# Verify: grep "hq_active=<cell_number>" /tmp/state.txt
```

### Pattern 2: Select Row from Submenu + Execute
```bash
$NAV nav <row_number>
$NAV key Return
$NAV wait 1.0  # If backend action is slow
$NAV frame > /tmp/state.txt
# Verify: Check filesystem or app state
```

### Pattern 3: Text Input (cli-io Modal)
```bash
# After triggering modal (e.g., Save As):
$NAV type "my_filename"      # Type text
$NAV key Return              # Submit
$NAV frame > /tmp/state.txt
# Verify: file created with correct name
```

### Pattern 4: Verify State Persists
```bash
# Create state:
$NAV nav 3  # desks
$NAV key Return
# Switch away and back:
$NAV key Esc
$NAV nav 3
$NAV key Return
# Verify frame matches initial (or expected state)
```

---

## Example: Full Test Harness (test_user_creation.sh)

See `xyzfs/users/claude-0001/harnesses/test_user_creation.sh` for complete working example. This file will be created during harness building phase.

---

## Resilience: Tolerate Live-Human Interruption — AND Don't Overreact to One Bad Read (2026-08-11)

**Real incident this session, both halves worth knowing:**

**Half 1 — the collision.** While an agent was mid-relay-sequence, the human clicked Enter on the
live X11 window at the same time, colliding with the automated input and leaving state that didn't
match what the harness expected. This is not an edge case to design around later — livedesk is a
real interactive desktop, a human can and will touch it while a harness is running.

**Half 2 — the overreaction, caught and corrected live.** From that one collision, the agent built an
elaborate theory that `nav.sh nav <n>` couldn't reach header cells at all (a supposed architecture
gap), and was about to write that false conclusion into a testing doc as fact. Told to slow down and
retry clean first, the SAME command worked exactly as intended on a fresh attempt — the "gap" never
existed; it was one contaminated reading. **One failed/confusing result on a shared, live, interactive
desktop is not proof of a code-level bug.** Retry clean (fresh `esc` first, no known human activity in
flight) before writing up a "confirmed gap" — see `_.0.aigent-testing-k9.txt`'s own 2026-08-11 addendum
for the full corrected account of what actually works.

Two rules from this:

1. **Never assume one relay send landed as expected.** Send, then poll `frame` in a retry loop until
   the expected state appears (or a timeout is hit) — don't read the frame once and treat a mismatch
   as fatal.
2. **Before concluding "this is broken," retry clean once.** A collision or a stale read looks
   identical to a real bug from one sample. Escape, wait, resend, re-check. Only escalate to "this
   looks like a real gap" after a clean retry still fails — and even then, say so explicitly rather
   than asserting it as confirmed fact in a shared doc other agents will trust.

### Verified Working Recipe — Opening a Header Cell + Submenu (khtpm taskbar)
Confirmed end-to-end, 2026-08-11 (USER-cell New User + Switch-user flows, both worked purely through
this relay — see USER_CREATION.md for the full trace):
```bash
NAV="#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"
HOUSE="$PWD" bash "$NAV" esc                    # clean slate first
sleep 0.8
HOUSE="$PWD" bash "$NAV" nav 2                  # digit-jump DOES reach header cell 2 (USER) directly
sleep 1
HOUSE="$PWD" bash "$NAV" frame                  # look for header.active=<some index>, not necessarily "2" —
                                                 # active_index is header_doc's raw array position of the
                                                 # button element, NOT the manager's 1-based "which" — check
                                                 # the frame's focused label text instead of a specific number
HOUSE="$PWD" bash "$NAV" key Return             # activates the focused submenu row
HOUSE="$PWD" bash "$NAV" key 3                  # once a submenu is open, a bare digit selects THAT row (1-based)
HOUSE="$PWD" bash "$NAV" key Return             # activates the selected row
```
Don't check `header.active=2` literally — that number is an internal array index that shifts as
`${strip_hq_items}` content grows/shrinks. Check the frame's `label=...` text instead, or check
`strip_state.txt`'s `hq_open`/`hq_n_menu`/`hq_focus` fields directly (those ARE the manager's own
stable `which`-based numbering).

---

## Debugging Tips

### If Test Fails: Inspect Frame History
```bash
tail -100 #.desktop/khtpm_strip_frame_history.txt
# Look for unexpected focus position, wrong element_count, etc.
```

### If Relay Seems Stuck
```bash
# Check relay file is readable:
ls -la #.desktop/livedesk_agent_relay.txt

# Manually read last few codes sent:
tail -20 #.desktop/livedesk_agent_relay.txt

# Restart if needed:
killall khtpm_strip_parser
cd $.crypts && bash button.sh run
```

### Trace Issue: Add Verbose Logging
```bash
# In your harness:
set -x  # Bash debug mode
# ... rest of harness ...
# Check frame after each nav.sh call
$NAV frame | tee -a /tmp/relay_trace.txt
```

---

## Best Practices

1. **Always capture frame before + after:** Diff shows what changed
2. **One logical thing per test:** Don't combine "create user" + "create session" in one harness
3. **Use timestamps in logs:** Makes it easy to correlate with frame history
4. **Store results somewhere durable:** Not /tmp (gets cleaned up)
5. **Make harnesses idempotent:** Run 2x, should work both times (or explicitly handle cleanup)
6. **Document prerequisites:** What must be true before test starts
7. **Make output PM-scannable:** Summary first, details in appendix

---

## Future: Harness Composition

Once we have working individual harnesses, we can compose them:
```bash
# run_all_tests.sh — meta-harness
bash harnesses/test_user_creation.sh || exit 1
bash harnesses/test_change_gold.sh || exit 1
bash harnesses/test_session_switch.sh || exit 1
# ... etc ...
echo "All tests passed!" > final_summary.md
```

This becomes the **CI equivalent** for game development (tests run via relay, matching user behavior, proving everything works end-to-end).

---

**Last Updated:** 2026-08-11  
**Testing Lead:** claude-0001  
**Questions:** Ask in au11-hq/ if blocked on relay behavior
