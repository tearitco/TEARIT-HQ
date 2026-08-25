# Per-Session Stats Feature

**Status**: Stable (Complete and working)

## Overview

Per-session stats allow users to view delegation metrics and conversation analytics for each hai chat session individually. Each session is tracked retroactively, and stats are accessible both from the hai menu and as standalone windows.

## Architecture

### Components

1. **Session Stat Calculation** (`&.widgits/open-hai/calculate_session_stats.sh`)
   - Runs retroactively to extract metrics from all existing session transcripts
   - Generates stats files for each session
   - Metrics tracked:
     - User message count
     - AI response count
     - Total conversation turns
     - Tool/delegation call detection
     - Session timestamp (human-readable)

2. **Stats Storage** (`%.harnesses/harnecient-fsm/session-stats/`)
   - Per-session stats stored as `{timestamp}.txt`
   - One file per session
   - Simple text format for easy parsing

3. **Stats Viewer** (`&.hq-apps/stats-hq/`)
   - `open_stats_hq.sh <house_root> [session_id]`
   - Generates CHTPM dashboard from template
   - Supports both aggregate stats (no session_id) and per-session (with session_id)

4. **Hai Integration** (`&.widgits/open-hai/ops/khtpm_open_hai_render.c`)
   - "Stats" menu item in hai sidebar (below Model selector)
   - Click to open that session's stats window
   - `NAV_STATS` constant in navigation enum
   - Calls `open_session_stats.sh` wrapper

### User Flows

#### View Session Stats
1. In hai chat window, click "Stats" menu item (in HQ submenu)
2. Stats window opens in a separate subwindow
3. Shows tabbed view with all recent sessions (up to 20)
4. Each tab displays:
   - Session date/time
   - User message count
   - AI response count
   - Total conversation turns
   - Tool/delegation call count and percentage
5. First session (newest) tab is active by default
6. Window is read-only (stats are calculated retroactively)

## Files Modified/Created

- `&.widgits/open-hai/calculate_session_stats.sh` — NEW
- `&.widgits/open-hai/open_session_stats.sh` — NEW
- `&.hq-apps/stats-hq/open_stats_hq.sh` — MODIFIED (added session_id parameter)
- `&.widgits/open-hai/ops/khtpm_open_hai_render.c` — MODIFIED (added NAV_STATS menu item)
- `%.harnesses/harnecient-fsm/session-stats/` — NEW directory

## Completed

- [x] Multi-session tabs in stats window (mirrors hai's session UI structure)
- [x] Per-session retroactive metric calculation from transcripts
- [x] Menu item in hai sidebar for quick access

## Future Enhancements

- [ ] Comparative stats across sessions (delta view)
- [ ] Export stats to CSV/JSON for external analysis
- [ ] Configurable metrics display (choose which metrics to show)
- [ ] Stats configuration menu item for preferences
- [ ] Per-session cost tracking (tokens × model pricing)
- [ ] Session filtering (by date range, model, etc.)

## Testing

### Verified Working (2026-08-13)
- ✅ Stats menu item appears in hai HQ menu
- ✅ Clicking "Stats" opens stats window with tabbed interface
- ✅ Stats window displays all recent sessions (up to 20)
- ✅ Tabs show correct metrics: messages, responses, turns, tool calls
- ✅ `calculate_session_stats.sh` calculates stats retroactively
- ✅ New sessions auto-appear in stats window after first message
- ✅ Window launched as independent X11 subwindow

### Known Issues
- None (all core features working)

## Related Features

- [[features/open-hai]] — The hai chat UI that integrates stats
- [[features/harnecient-fsm]] — Delegation tracking system

