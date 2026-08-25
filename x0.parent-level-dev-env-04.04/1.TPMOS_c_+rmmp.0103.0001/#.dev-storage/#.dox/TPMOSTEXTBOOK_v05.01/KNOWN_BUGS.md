# 🐛 Appendix: Known Bugs & Research Tasks
TPMOS is a living system. This chapter tracks high-severity bugs that are currently being researched and provides guidance for future fix implementations. 🛠️🪲

---

## 1. The Windows Keyboard Bug (Arrow/Enter Keys)
**Severity:** HIGH (Blocks navigation on Windows native consoles)
**Status:** Researching Fix

### Symptom:
When running the TPMOS orchestrator on Windows (especially via MSYS2 or native CMD), physical arrow keys, the Enter key, and other special keys may fail to register. Users see raw escape sequences (like `^[[A`) or nothing at all, making menu navigation impossible.

### Root Cause:
Windows consoles handle input differently than Linux `termios`. The standard `getchar()` or `read()` calls often buffer input or fail to capture the 2-byte sequences sent by special keys.

### Reference Fix (Research Tool):
We have successfully captured these keys using the `_getch()` method in `#.tools/keylog-win+aro+c-1/keylog.c`.

#### Implementation Guidance:
To fix the orchestrator, the `readKey` function must be updated to use the Windows-specific `0xE0` prefix pattern:

```c
// Reference pattern from keylog.c
#include <conio.h>

int get_windows_key() {
    int ch = _getch();
    if (ch == 0xE0) { // Special key prefix
        int ch2 = _getch();
        if (ch2 == 0x48) return KEY_UP;
        if (ch2 == 0x50) return KEY_DOWN;
        if (ch2 == 0x4B) return KEY_LEFT;
        if (ch2 == 0x4D) return KEY_RIGHT;
    }
    if (ch == 13) return KEY_ENTER; // Windows Enter is 13 (\r)
    return ch;
}
```

### Future Tasks:
- [ ] Integrate `conio.h` logic into `pieces/chtpm/plugins/orchestrator.c`.
- [ ] Ensure `ENABLE_PROCESSED_INPUT` is set via `SetConsoleMode` to allow Ctrl+C to function correctly alongside raw key capture.

---

## 2. Multi-Digit Jump Latency
**Severity:** LOW
**Symptom:** Jumping to items like "11" or "12" requires very fast typing or multiple attempts.
**Task:** Research a more forgiving `digit_accum` timer in `chtpm_parser.c`.

---
*If you find a bug, document it. If you fix a bug, reforge the binary. 🕯️*
