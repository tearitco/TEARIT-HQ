# 🎯 AU10 Task Resume · Pals-Canonical Runtime

## 📍 Current Status

| | Status | Notes |
|---|--------|-------|
| 🎯 **Goal** | 🟢 DESIGNED | Pals-canonical runtime: entities run from xyzfs pals registry (1 pal = 1 hash = 1 copy) + pals popup |
| ✅ **Implementation** | 🟢 BUILT ✅ | Code in `tp_taskbar.c` complete; rebuild EXIT=0 ✅ |
| 🔧 **Fixed** | 🟢 FIXED ✅ | `livedesk_default_session`: no longer snapshots empty desks |
| 🧪 **Verification** | 🟢 **PASSING** ✅ | **Pals migration working!** All 6 entities running from `/pals/` registry, not dev folders. Pal manifests (name/hash/glyph) created. Migration auto-happens on desk spawn. |
| 📚 **Docs** | 🟢 MODEL LOCKED | §3.1 §4.8 §4.9 rewritten; need as-built addendum after C2b verification |

---

## 📋 IMMEDIATE TASK QUEUE (In Order)

### ✅ 1️⃣ **Build Confirmed** 
✅ **DONE:** `EXIT=0` — no compile errors ✅

### ✅ 2️⃣ **Restart Taskbar**
✅ **DONE:** Restarted with new binary (PID=361836) ✅

### ✅ 3️⃣ **Verify Pals Migration: PASSED** 
✅ **DONE:** 
- All 6 entities running from `/pals/` registry paths ✅
- Pal manifests exist (name/hash/glyph) ✅
- Spawn auto-migrates old paths → pals on load ✅
- Dev folders untouched ✅
- livedesk_open.txt shows correct pals paths ✅

### 4️⃣ **Verify Pals Popup** 
🟢 **CODE VERIFIED** ✅ (manual UI click needed):
- ✅ Button exists in strip (nav 5) + wired to taskbar.pdl
- ✅ Dispatch code handles "livedesk:pals" → calls livedesk_open_pals_popup
- ✅ Popup builder scans pals/, builds rows: `🤖 self #1fd354…` (glyph+name+hash10)
- ✅ Place command `livedesk:pal:<name>` wired to livedesk_place_pal
- 🟡 **Manual test needed:** Click nav 5 in UI to see popup, verify can place entity + popup stays open
- 🟡 **Known design gap:** Strip buttons hardcoded + taskbar.pdl (should be dynamic from pals) — defer to khtpm redesign

### 5️⃣ **save-as Layout-Only & C2b Round-Trip**
🟡 **Pending manual test:**
- File→new → File→save-as → confirm new session has `desks/` + `session.pdl` only (no `entities/`)
- Desk rows reference pals (house-relative paths)
- C2b cycle: File→new/save/save-as/load should work with entities from pals

### 6️⃣ **Update Docs as-Built**
📝 **Partial:** 
- ✅ Design doc §11 added: KHTPM refactor plan for dynamic UI from pals
- 🟡 §4.8/§4.9/§3.1: Need to mark "BUILT ✅ 2026-08-10" after C2b manual test
- 🟡 Worklog: Mark NEXT item 0 DONE
- 🟡 Report: Add pals migration as-built addendum

---

## 🔮 FUTURE ITEMS

### After Pals Verification ✨
📋 **Desk Copy/Paste/Delete** (right-click on desk name in dropdown)
- Right-click desk in file menu dropdown → popup with **Copy**, **Paste**, **Delete** options
- Workaround for migration artifact: copy office.pdl rows to desk_01, etc.
- UX: quick desk management without manual file editing

### After khtpm Port
🪟 **Windows Feature:** livepal index bar position
- Add option (default ON for Windows) to move nav bar from bottom→top
- Instead of livepal bar at bottom, put each `nav+name` under each deskpal in grid
- Document in design/README after khtpm port complete

---

## 🎛️ Critical Context

**Build Command:**
```
gcc -std=c11 -Wall -O2 "&.widgits/livedesk-taskbar/ops/tp_taskbar.c" \
  -o "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x" -lX11
```

**Key Files:**
- `&.widgits/livedesk-taskbar/ops/tp_taskbar.c` — all pals implementation
- `#.livedesk/livedesk-editor-design.md` — MODEL LOCKED §3.1 §4.8 §4.9
- `#.livedesk/livedesk-sessions-worklog.md` — resume/checklist
- `#.livedesk/livedesk-report-2026-08-10.md` — delivered report

**Sessions:**
- `s1` = pre-design canonical (6 entities, hp=42) ← primary test
- `s2/s3` = empty
- `s4` = clone (test evidence, K6 probe overwritten to 42)

**Constraints:**
- ✋ NEVER: `build_khtpm.sh`, plain `pkill`, skip taskbar restart recipe
- 🏠 Stay in LEGACY `tp_taskbar.c`; khtpm port deferred until this + report done
- 📄 Dev folders = store catalog only (not runtime homes)
- 🎭 Pals = Pokédex (all sessions always); placing keeps popup open
- 1️⃣ 1 pal = 1 hash = 1 canonical copy (NFT-ready)

---

## 🤔 Ready to Start?

Would you like me to:
1. **Start now** with rebuild confirmation (Haiku 4.5) ⚡
2. **Switch to Sonnet 5** first for this work 🧠
3. **Something else?** (ask me first)
