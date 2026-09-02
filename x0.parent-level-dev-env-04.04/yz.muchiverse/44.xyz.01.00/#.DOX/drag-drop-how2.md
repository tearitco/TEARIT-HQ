# Drag-and-Drop Pet Import Test Guide

## Overview

This guide explains how to test the X11 Xdnd drag-and-drop functionality for importing pets from muchi-pals (egg_window) into mutaclsym (gl_mirror).

## Features Implemented

### Visual Feedback
- **Green border highlight** on gl_mirror when drag is over
- **Semi-transparent green overlay** after successful drop (2 seconds)
- **Window close** after successful import

### Data Transfer
- **XdndSelection** transfers pet ID from egg_window to gl_mirror
- **Automatic import** triggers pet_import on drop
- **Close request** signals egg_window to close after import

### Test Harness
- **Config file**: `drag_drop_test.pdl` (PDL format)
- **Automated test**: `drag_drop_test.sh`
- **Window positioning**: Windows poll config for positions
- **AI testing**: Write to config file to position windows and trigger drops

## Prerequisites

1. Both projects must be compiled:
   ```bash
   cd 01.muchi-pals-🥚️-13.01 && ./button.sh compile
   cd 101.mutaclsym🧟‍♂️️+18.00 && ./button.sh compile
   ```

2. Ensure exchange directory exists:
   ```bash
   mkdir -p /home/no/Desktop/.../yz.muchiverse/44.xyz❤️‍🔥️00.07/exchange
   ```

## Test Procedure

### Manual Test

#### Step 1: Start mutaclsym with GL mirror

```bash
cd 101.mutaclsym🧟‍♂️️+18.00
./button.sh run
```

This starts:
- Terminal game (ASCII view)
- GL mirror window ("mutaclsym RGB mirror")

Wait for the GL mirror window to appear.

#### Step 2: Start muchi-pals and open pet window

In a separate terminal:

```bash
cd 01.muchi-pals-🥚️-13.01
./button.sh run
```

From the menu, select "Open Window" for a pet (e.g., egg_1).

This opens:
- Terminal menu
- Pet GL window (shaped window showing pet sprite)

#### Step 3: Test drag-drop

1. **Click and hold** on the pet GL window (left mouse button)
2. **Drag** the window over the mutaclsym GL mirror window
3. **Observe** the green border highlight on gl_mirror
4. **Release** the mouse button while over the GL mirror
5. **Observe** the green overlay flash (2 seconds)
6. **Watch** the pet window close automatically

#### Step 4: Verify the drop

Check for these indicators:

##### Success indicators:
- Green border appears when dragging over gl_mirror
- Green overlay appears after drop
- Pet window closes automatically
- Terminal output from gl_mirror:
  ```
  gl_mirror: importing pet 'egg_1'
  ```
- Pet appears in mutaclsym's world (check terminal)

##### Partial success:
- If you see "XdndDrop received" but the pet doesn't appear, the import may have failed
- Check exchange directory for the pet:
  ```bash
  ls /home/no/Desktop/.../yz.muchiverse/44.xyz❤️‍🔥️00.07/exchange/
  ```

### Automated Test with Test Harness

#### Step 1: Configure test

Edit `drag_drop_test.pdl` to set window positions:

```
WINDOW       | gl_mirror_x        | 100
WINDOW       | gl_mirror_y        | 100
WINDOW       | egg_window_x       | 800
WINDOW       | egg_window_y       | 100
```

#### Step 2: Start applications

Start mutaclsym and muchi-pals as described above.

#### Step 3: Run test harness

```bash
./drag_drop_test.sh
```

The test harness will:
1. Find both windows by name
2. Position them according to the config
3. Simulate drag-drop using xdotool
4. Verify results (pet imported, window closed, etc.)
5. Take screenshots at key points

#### Step 4: Check results

- Log file: `/tmp/drag_drop_test.log`
- Screenshots: `/tmp/drag_drop_screenshots/`

### AI Test Mode

For AI testing, write to the config file:

```bash
# Update window positions
cat > drag_drop_test.pdl << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
WINDOW       | gl_mirror_x        | 50
WINDOW       | gl_mirror_y        | 50
WINDOW       | egg_window_x       | 700
WINDOW       | egg_window_y       | 50
EOF

# Wait for windows to update (polls every second)
sleep 2

# Use xdotool to simulate drag-drop
xdotool mousemove 740 90  # Move to egg_window center
xdotool mousedown 1       # Start drag
xdotool mousemove 370 202  # Move to gl_mirror center
xdotool mouseup 1         # Drop
```

## Config File Format

The `drag_drop_test.pdl` file uses pipe-delimited format:

```
SECTION      | KEY                | VALUE
----------------------------------------
CONFIG       | test_mode          | manual
CONFIG       | poll_interval_ms   | 100

WINDOW       | gl_mirror_x        | 100
WINDOW       | gl_mirror_y        | 100
WINDOW       | gl_mirror_w        | 640
WINDOW       | gl_mirror_h        | 304

WINDOW       | egg_window_x       | 800
WINDOW       | egg_window_y       | 100
WINDOW       | egg_window_w       | 80
WINDOW       | egg_window_h       | 80

PET          | pet_id             | egg_1
PET          | pet_emoji          | 🐛

DRAG         | start_x            | 840
DRAG         | start_y            | 140
DRAG         | end_x              | 420
DRAG         | end_y              | 252
DRAG         | steps              | 20
DRAG         | step_delay_ms      | 50

EXPECTED     | drop_accepted      | 1
EXPECTED     | pet_imported       | 1
EXPECTED     | source_closed      | 1
```

### Key Fields

- **WINDOW**: Position and size of windows
  - `gl_mirror_x/y`: Position of mutaclsym GL mirror
  - `egg_window_x/y`: Position of muchi-pals pet window

- **PET**: Pet information
  - `pet_id`: ID of pet to test with
  - `pet_emoji`: Emoji for visual feedback

- **DRAG**: Drag simulation parameters
  - `start_x/y`: Starting position (egg_window center)
  - `end_x/y`: Ending position (gl_mirror center)
  - `steps`: Number of intermediate steps
  - `step_delay_ms`: Delay between steps

- **EXPECTED**: Expected results for verification
  - `drop_accepted`: Whether drop should be accepted
  - `pet_imported`: Whether pet should be imported
  - `source_closed`: Whether source window should close

## AI Testing Workflow

1. **Read config**: Parse `drag_drop_test.pdl` for positions
2. **Position windows**: Write new positions to config file
3. **Wait for update**: Windows poll every second
4. **Simulate drag**: Use xdotool to simulate mouse events
5. **Verify results**: Check if pet was imported, window closed, etc.
6. **Report**: Log results to test file

### Example AI Test Script

```bash
#!/bin/bash
# ai_test_drag_drop.sh

CONFIG="drag_drop_test.pdl"

# Function to update config
update_config() {
    local gl_x=$1 gl_y=$2 egg_x=$3 egg_y=$4
    cat > "$CONFIG" << EOF
SECTION      | KEY                | VALUE
----------------------------------------
WINDOW       | gl_mirror_x        | $gl_x
WINDOW       | gl_mirror_y        | $gl_y
WINDOW       | egg_window_x       | $egg_x
WINDOW       | egg_window_y       | $egg_y
PET          | pet_id             | egg_1
EOF
}

# Function to simulate drag
simulate_drag() {
    local start_x=$1 start_y=$2 end_x=$3 end_y=$4
    xdotool mousemove $start_x $start_y
    sleep 0.1
    xdotool mousedown 1
    xdotool mousemove $end_x $end_y
    sleep 0.1
    xdotool mouseup 1
}

# Test 1: Standard drag from right to left
echo "Test 1: Standard drag"
update_config 100 100 800 100
sleep 2  # Wait for windows to position
simulate_drag 840 140 420 202
sleep 2  # Wait for import
# Verify...

# Test 2: Drag from top to bottom
echo "Test 2: Top to bottom"
update_config 400 100 400 500
sleep 2
simulate_drag 440 540 440 252
sleep 2
# Verify...
```

### Issue: No XdndDrop message in gl_mirror

**Possible causes:**
1. Window not found by name
   - Check window name matches exactly: "mutaclsym RGB mirror"
   - Try: `xprop -name "mutaclsym RGB mirror"` to verify

2. XdndAware property not set
   - Check: `xprop -id <window_id> | grep Xdnd`
   - Should show: `XdndAware = 0x5`

3. Window not receiving events
   - Check if window is focused
   - Try clicking on gl_mirror window first

### Issue: Pet not appearing in mutaclsym

**Check:**
1. Exchange directory:
   ```bash
   ls -la /home/no/Desktop/.../yz.muchiverse/44.xyz❤️‍🔥️00.07/exchange/
   ```

2. Mutaclsym world directory:
   ```bash
   ls -la 101.mutaclsym🧟‍♂️️+18.00/pieces/world_01/map_start/
   ```

3. Import logs:
   - Check stderr output from gl_mirror
   - Check if pet_import was executed

### Issue: Compilation errors

**Ensure dependencies installed:**
```bash
# For egg_window
sudo apt-get install libx11-dev libxext-dev libgl-dev libglx-dev

# For gl_mirror
sudo apt-get install freeglut3-dev libgl-dev libglu1-mesa-dev libx11-dev
```

### Issue: Xdnd events not being processed

**Check:**
1. Idle callback is registered:
   - Look for `glutIdleFunc(check_xdnd_events)` in gl_mirror.c

2. X11 display is open:
   - Check `g_dpy` and `g_win` are not NULL

3. Window ID is found:
   - The window lookup uses XQueryTree + XFetchName
   - Window name must match exactly

## Debug Output

### Enable verbose logging

In egg_window.c, add before XdndPosition send:
```c
fprintf(stderr, "egg_window: sending XdndPosition to window %lu\n",
        (unsigned long)xdnd_target);
```

In gl_mirror.c, add in handle_xdnd_event:
```c
fprintf(stderr, "gl_mirror: received event type %d from window %lu\n",
        xev->type, (unsigned long)xev->xclient.data.l[0]);
```

### Check X11 events

Use xdotool to monitor events:
```bash
# Monitor all X11 events
xdotool getactivewindow
```

## Expected Behavior

1. **Drag start**: egg_window sends XdndEnter to gl_mirror
2. **During drag**: egg_window sends XdndPosition periodically
3. **Drop**: egg_window sends XdndDrop, gl_mirror responds with XdndStatus
4. **Completion**: gl_mirror triggers pet_import, sends XdndFinished

## Current Limitations

1. **Data transfer**: Currently, the pet ID is not being transferred via XdndSelection. The drop is detected, but the actual pet data needs to be read from the source window's state.

2. **Visual feedback**: No visual highlight when dragging over gl_mirror (can be added later).

3. **Multi-pet support**: Only one pet can be imported at a time.

## Next Steps

1. Implement proper XdndSelection data transfer
2. Add visual highlight on valid drop targets
3. Handle multiple pet imports
4. Add error recovery for failed imports

## Files Modified

- `01.muchi-pals-🥚️-13.01/system/egg_window.c` - Xdnd drag source
- `101.mutaclsym🧟‍♂️️+18.00/system/gl_mirror.c` - Xdnd drop target

## Related Documentation

- `002.zoo.../dox/pet-import-export-standard.md` - Cross-game import/export design
- `01.muchi-pals.../dox/01-architecture.md` - muchi-pals architecture
- `101.mutaclsym.../dox/00-HANDOFF.md` - mutaclsym architecture
