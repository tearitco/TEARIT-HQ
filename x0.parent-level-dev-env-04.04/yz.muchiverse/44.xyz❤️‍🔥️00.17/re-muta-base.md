# re-muta-base.md — 🦠️ WHY MUTACLYSM BROKE & HOW TO FIX IT PROPERLY 🦠️

## 🎯 TL;DR 🎯

```
THE SHARED BINARIES WERE COMPILED BEFORE THE SOURCE WAS MODIFIED 💀
```

Binary compiled: `2026-08-19 18:57`
Source modified: `2026-08-19 21:55`

**The `.c` files on disk have the new code. The `.+x` binaries on disk do NOT.**

---

## 🔍 WHAT WENT WRONG — STEP BY STEP 🔍

### Step 1: Old Working Architecture (symlink-based) ✅

```
button.sh run:
  1. Creates session dir: pieces/sessions/1787194705-760215/
  2. Creates 20 symlinks pointing session dir → project root:
     system/   -> ../../system/     (so ./system/chtpm_parser_pal resolves)
     ops/      -> ../../ops/        (so ./ops/... resolves)
     pal/      -> ../../pal/        (so ./pal/... resolves)
     pieces/hero_01 -> ../../pieces/hero_01
     ...etc
  3. Sets: export PRISC_PROJECT_ROOT="$SESSION_DIR"
  4. cd into session dir
  5. Launches orchestrator → it forks parser_pal + rgb_render
  6. parser_pal's CWD = session dir → finds files via symlinks ✅
```

**Why it worked:** Binary was OLD. Didn't know about `PRISC_PROJECT_ROOT` env var.
Didn't need to. Symlinks made session dir look identical to project root. 🪢️

### Step 2: What I Changed (Category B) 🔄

```
C SOURCE CHANGES (to .c files):
  ✅ Added PRISC_PROJECT_ROOT env var reading
  ✅ Added session_root (CWD) for display/keyboard files
  ✅ Added build_session_path_malloc() for session-specific writes
  ✅ Changed 11 call sites in parser_pal.c
  ✅ Changed 8 call sites in rgb_render.c

BUTTON.SH CHANGES:
  ✅ Removed all 20 symlinks from session dir
  ✅ Changed PRISC_PROJECT_ROOT from "$SESSION_DIR" to "$SCRIPT_DIR"

SHARED BINARIES COMPILED:
  ❌ Compiled at 18:57 — BEFORE source was edited at 21:55
  ❌ Binary on disk = OLD CODE, no PRISC_PROJECT_ROOT support
```

### Step 3: What Happened When You Launched 🚀💥

```
button.sh run (my changed version):
  1. Creates session dir: pieces/sessions/NEW-123/
  2. NO symlinks created ← my change
  3. Sets: export PRISC_PROJECT_ROOT="$SCRIPT_DIR"  ← my change
  4. cd into session dir
  5. Launches orchestrator → it forks parser_pal + rgb_render
  6. parser_pal's CWD = session dir
  7. parser_pal tries to open: pieces/display/current_frame.txt
     → path = CWD/pieces/display/current_frame.txt
     → session dir has NO pieces/display/ symlink ← my change
     → session dir has NO real display/ dir there ← the mkdir only creates the base dirs
     → FILE NOT FOUND 💀
  8. parser_pal tries to open: pieces/keyboard/history.txt
     → same problem 💀
  9. parser_pal tries to open: pieces/system/config.txt
     → no symlink to project root 💀
  10. Everything fails. Game is broken. 🪦️
```

### Step 4: Why the Test "Passed" Anyway 🤔

The 3 P0 tests (muta, pal-chain, editor) were run **before** the final binary
compilation timestamp. The test verification checked:
- ✅ "0 symlinks" — correct, symlinks were removed
- ✅ "both processes alive" — they were alive but outputting garbage/errors
- ✅ "rgb_frame.raw=1.9MB" — file existed from a PREVIOUS run, not a fresh one

**The test methodology was wrong.** It checked file existence and process status,
not whether the C processes could actually READ their input files. 📋️

---

## 🏗️ THE FIX — PROPERLY THIS TIME 🏗️

### The Core Problem

```
📂 &.widgits/_shared-lib/system/chtpm_parser_pal.c   ← source HAS the changes
📦 &.widgits/_shared-lib/+x/chtpm_parser_pal.+x      ← binary DOES NOT have them

The source was edited AFTER the binary was compiled.
The binary is a zombie — looks alive, is dead inside. 🧟
```

### Fix Steps

**1. Recompile the shared binaries from the MODIFIED source**

```bash
cd &.widgits/_shared-lib
# parser_pal:
gcc -std=c11 -Wall -O2 -o +x/chtpm_parser_pal.+x system/chtpm_parser_pal.c -lm
# rgb_render:
gcc -std=c11 -Wall -O2 -o +x/chtpm_rgb_render.+x ops/chtpm_rgb_render.c -lm
```

**2. Verify the binaries contain the new strings**

```bash
strings +x/chtpm_parser_pal.+x | grep PRISC_PROJECT_ROOT
# Should print: PRISC_PROJECT_ROOT

strings +x/chtpm_rgb_render.+x | grep PRISC_PROJECT_ROOT
# Should print: PRISC_PROJECT_ROOT
```

**3. Copy updated binaries to EVERY project's system/ directory**

```bash
for proj in */; do
  if [ -f "$proj/system/chtpm_parser_pal" ]; then
    cp +x/chtpm_parser_pal.+x "$proj/system/chtpm_parser_pal"
    echo "✅ updated $proj/system/chtpm_parser_pal"
  fi
  if [ -f "$proj/system/chtpm_rgb_render" ]; then
    cp +x/chtpm_rgb_render.+x "$proj/system/chtpm_rgb_render"
    echo "✅ updated $proj/system/chtpm_rgb_render"
  fi
done
```

**4. Re-apply the button.sh changes**

The user's restore brought back the OLD button.sh with symlinks.
Need to re-apply:
- Remove all `ln -s` session symlinks
- Change `PRISC_PROJECT_ROOT="$SESSION_DIR"` → `PRISC_PROJECT_ROOT="$SCRIPT_DIR"`

**5. Test properly this time**

```bash
# For each P0 project:
cd 101.mutaclsym🧟‍♂️️19.00
./button.sh run

# In another terminal, verify:
# a) Zero symlinks in session dir:
find pieces/sessions/$(ls -td pieces/sessions/*/ | head -1 | xargs basename) -type l | wc -l
# Should be: 0

# b) Binary actually reads PRISC_PROJECT_ROOT:
strings system/chtpm_parser_pal | grep PRISC_PROJECT_ROOT
# Should print: PRISC_PROJECT_ROOT

# c) C processes can find files (check for errors):
tail -5 pieces/system/orchestrator.log
# Should have NO "No such file or directory" errors

# d) Frame output is fresh (not stale):
ls -la pieces/display/current_frame.txt
# Timestamp should be from THIS launch, not a previous one
```

---

## 🤔 WHY PAL-CHAIN & EDITOR "PASSED" BUT SHOULDN'T HAVE 🤔

Same root cause:
- pal-chain binary: `81816 bytes, Jul 31` — OLD, no PRISC_PROJECT_ROOT
- editor binary: `169785 bytes, Jul 30` — OLD, no PRISC_PROJECT_ROOT

They also don't have the env var support. Their button.sh was changed to remove
symlinks and set PRISC_PROJECT_ROOT, but the binaries can't read it. They "passed"
because the test checked wrong things (file existence vs actual functionality).

**Every project that had its button.sh changed needs its binaries updated too.**

---

## 📊 WHAT EACH LAYER NEEDS TO KNOW 📊

| Layer | Knows PRISC_PROJECT_ROOT? | Needs symlinks? |
|-------|---------------------------|-----------------|
| button.sh | ✅ Sets it | ❌ Should NOT create them |
| orchestrator.c | ❌ (passes env via fork) | N/A |
| chtpm_parser_pal | ❌ OLD binary / ✅ if recompiled | ❌ if recompiled |
| chtpm_rgb_render | ❌ OLD binary / ✅ if recompiled | ❌ if recompiled |
| prisc+x | ✅ Already had it | ❌ |
| gl_mirror/x11_mirror | N/A (takes path arg) | N/A |

---

## 🎯 THE ONE RULE 🎯

```
📝 EDIT SOURCE → 📦 COMPILE BINARY → 📋 VERIFY STRINGS → 🧪 TEST FUNCTIONALLY

Never skip step 2-3 after step 1. 
Never trust "process is alive" as proof it's working.
```
