# 🔧 DB Cell Implementation Handoff

**SUPERSEDED — this doc is now purely historical.** db-hq went real/working
later in the 2026-08-12 session, then on 2026-08-16 was merged into the
shared `khtpm_entity_menu_render.c` binary (Stage 5 of the khtpm merge —
see `khtpm-merge-how2.md` §5d.10 for current, real status). Do not trust
this file's "BROKEN/INCOMPLETE" framing below — kept as a historical
record of the original bug only.

**Status (historical, as of 2026-08-12):** BROKEN / INCOMPLETE  
**Date:** 2026-08-12  
**Problem:** db cell (cell 9) is not showing a menu when clicked. Previous agent added db-ez + db-hq rows but handler incomplete.

---

## 🚨 Current State (BROKEN)

### What's Wrong
1. **db cell doesn't show menu** — clicking cell 9 shows nothing (empty menu)
2. **Handler incomplete** — "livedesk:open-common-events-hq" command has no handler in ktb_hq_activate()
3. **Haiku's attempt** — added db-ez and db-hq rows to livedesk_build_db_menu(), but only db-ez handler exists

### Files Involved
- `/44.xyz.../.monads/.livedesk-taskbar/ops/khtpm_taskbar_manager.c` — menu builder + handler
- `xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/sessions/session.pdl` — session state (format: key=value, NOT pipe-delimited)

### What We Know Works
- event-ez launches correctly when called via relay (tested 2026-08-12)
- open_event_ez.sh script works perfectly
- khtpm_taskbar_manager.c compiles and runs
- Session file exists and has active_session=s1

---

## ✅ Task: Implement DB-EZ and DB-HQ

### Goal
- **db-ez:** Launch event-ez for common_events editing (simple, no CSS)
- **db-hq:** Launch styled version with CSS (future: requires KHTPM+CSS enhancement)

### What Needs Doing

#### Part 1: Fix DB-EZ (Simpler, Do This First)

**File:** khtpm_taskbar_manager.c  
**Location:** livedesk_build_db_menu() function (~line 1904)

Currently has:
```c
static int livedesk_build_db_menu(const char *house_root, HQMenuItem *menu, int max) {
    int n = 0;
    char sroot[KTB_PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char cur[KTB_PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (!cur[0]) return 0;

    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "db-ez");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-common-events:%s", cur);
        n++;
    }
    if (n < max) {
        snprintf(menu[n].label, sizeof(menu[n].label), "db-hq");
        snprintf(menu[n].command, sizeof(menu[n].command), "livedesk:open-common-events-hq:%s", cur);
        n++;
    }
    return n;
}
```

**Debugging First:** The menu is probably returning 0 (empty). Debug:
1. Check if livedesk_sessions_root() works (should find user UUID and return sessions path)
2. Check if livedesk_root_read() reads session.pdl correctly (format: `active_session=s1`, not pipe-delimited)
3. Add temporary debug output: `fprintf(stderr, "db_menu: n=%d, cur=%s\n", n, cur);` before return

**To Fix:**
1. Verify session.pdl has correct format: `active_session=s1` (key=value, no pipes)
2. Verify livedesk_root_read() can parse key=value format (it uses read_key_value())
3. Once menu shows: add handler for "livedesk:open-common-events:" in ktb_hq_activate()

#### Part 2: Implement Handler for DB-EZ

**File:** khtpm_taskbar_manager.c  
**Location:** ktb_hq_activate() function (~line 2051)

Add handler after the "livedesk:pal:" handler (~line 2162):

```c
} else if (strncmp(m->command, "livedesk:open-common-events:", 28) == 0) {
    char sroot[KTB_PATH_BUF];
    if (livedesk_sessions_root(s->house_root, sroot, sizeof(sroot))) {
        char sid[64];
        snprintf(sid, sizeof(sid), "%s", m->command + 28);
        char ce_path[KTB_PATH_BUF];
        snprintf(ce_path, sizeof(ce_path), "%s/%s/common_events", sroot, sid);
        
        // Ensure directory exists
        if (access(ce_path, F_OK) != 0) {
            mkdir(ce_path, 0755);
        }
        
        // Launch event-ez with common_events package
        char sh[KTB_PATH_BUF * 3];
        snprintf(sh, sizeof(sh), "setsid nohup sh -c 'sh \"%s/xyzfs/bin/muchi-pet/ops/open_event_ez.sh\" \"%s\" \"%s\"' >/dev/null 2>&1 &",
                 s->house_root, ce_path, s->house_root);
        int rc = system(sh);
        (void)rc;
    }
    ktb_hq_close(s);
```

**Test:** Rebuild, restart livedesk, click db → select "db-ez" → event-ez should launch with sessions/s1/common_events/

#### Part 3: Placeholder Handler for DB-HQ (Minimal)

**File:** khtpm_taskbar_manager.c  
**Location:** ktb_hq_activate() function

For now, add a placeholder that shows a message or does nothing:

```c
} else if (strncmp(m->command, "livedesk:open-common-events-hq:", 31) == 0) {
    // TODO: Implement styled db-hq UI
    // For now: show placeholder or do nothing
    ktb_hq_close(s);
```

**Future (Phase 2):** Replace with actual db-hq launch once CSS enhancement is built.

---

## 🏗️ Implementation Steps

### Step 1: Debug Menu Builder
```bash
# Add temp debug to khtpm_taskbar_manager.c in livedesk_build_db_menu():
fprintf(stderr, "DEBUG db_menu: sroot=%s, cur=%s, returning n=%d\n", sroot, cur, n);

# Rebuild:
cd *.monads/*.livedesk-taskbar/ops && sh run_khtpm_strip.sh new

# Test via relay:
HOUSE=... bash nav.sh hqcell 9
# Check terminal for DEBUG output
```

### Step 2: Verify session.pdl Format
```bash
# Must be key=value, not pipes:
cat sessions/session.pdl
# Should show:
# active_session=s1
# last_session=s7

# NOT this (wrong):
# STATE | active_session | s1
```

### Step 3: Add DB-EZ Handler
1. Open khtpm_taskbar_manager.c
2. Find ktb_hq_activate() function
3. Add handler for "livedesk:open-common-events:" (see code above)
4. Rebuild

### Step 4: Test DB-EZ
```bash
# Via relay:
bash nav.sh hqcell 9          # Click db
bash nav.sh nav 1              # Select db-ez
sleep 1
# event-ez should launch, showing sessions/s1/common_events/event_pkg

# Verify manually:
ls -la sessions/s1/common_events/event_pkg/
# Should exist with pages/page_1/ directory
```

### Step 5: Add DB-HQ Placeholder
Add the placeholder handler (see code above) so "livedesk:open-common-events-hq:" doesn't crash

### Step 6: Test Both Rows
```bash
bash nav.sh hqcell 9
# Menu should show:
# [1] db-ez
# [2] db-hq

bash nav.sh nav 1
# db-ez launches event-ez

bash nav.sh key Escape
bash nav.sh hqcell 9
bash nav.sh nav 2
# db-hq does nothing (placeholder) - that's ok for now
```

---

## 🔮 Future: DB-HQ with CSS

Once KHTPM+CSS enhancement is built (see HQML-DESIGN+PLANS.md):

1. Create `&.hq-apps/db-hq/` directory with:
   - `dashboard.chtpm` (KHTPM layout)
   - `dashboard.css` (CSS styling)
   - `hq_db.pal` (logic)

2. Update handler for "livedesk:open-common-events-hq:" to launch db-hq app

3. db-hq shows styled database UI (Items, State Variables, Switches, Common Events)

---

## 🔧 Key Files to Know

| File | Purpose | Status |
|------|---------|--------|
| khtpm_taskbar_manager.c | Menu builders + handlers | NEEDS: db-ez handler, db-hq placeholder |
| khtpm_taskbar_manager.h | Constants, structs | OK |
| session.pdl | Active session state | FIXED (key=value format) |
| open_event_ez.sh | Launches event-ez | WORKS |

---

## ⚠️ Common Pitfalls

1. **session.pdl format** — MUST be `key=value`, not `STATE | key | value`
   - If wrong, livedesk_root_read() fails, menu builder returns 0, shows empty

2. **strncmp length** — "livedesk:open-common-events:" is exactly 28 chars
   - Verified: `printf '%s' "livedesk:open-common-events:" | wc -c` → 28

3. **Missing mkdir** — common_events directory must exist before launching event-ez
   - Solution: `if (access(ce_path, F_OK) != 0) mkdir(ce_path, 0755);`

4. **Event-EZ path** — must be `xyzfs/bin/muchi-pet/ops/open_event_ez.sh` (not old *.monads path)
   - Verified: script exists and works

---

## ✅ Success Criteria

- [ ] Click db cell → menu appears with "db-ez" and "db-hq" rows
- [ ] Select "db-ez" → event-ez launches with common_events package
- [ ] Create/edit event in event-ez → event saved to sessions/s1/common_events/event_pkg/
- [ ] Select "db-hq" → does nothing (placeholder, no crash)
- [ ] Rebuild and restart multiple times → consistent behavior

---

## 📚 References

- **HANDOFF.md** → Current architecture, session storage pattern
- **EVENTS_RUNTIME.md** → Event execution flow, open_event_ez.sh usage
- **HQML-DESIGN+PLANS.md** → Future db-hq styling vision (Phase 2)
- **DB_CONTEXT.md** → Explanation of db cell purpose for new agents

---

**Last Updated:** 2026-08-12  
**For:** Next agent working on db cell  
**Confidence:** Medium — architecture sound, execution incomplete
