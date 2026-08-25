# 🪟 Chapter 15: Cross-Platform TPMOS (Windows/Linux/macOS)
TPMOS is designed from the ground up to run on any platform. This chapter covers the differences, patterns, and setup guides for Windows, Linux, and macOS. 🖥️🍎🐧

---

## 🏗️ Build Systems

### Linux
```bash
# Compile all binaries
./#.dev-storage/#.tools/compile_all.sh

# Run CHTPM
./button.sh run

# Kill all processes
./button.sh kill
```

### Windows (MSYS2/MinGW)
```powershell
# Compile all binaries
.\pieces\buttons\windows\legacy\compile_all.ps1

# Run CHTPM
.\pieces\buttons\windows\run.ps1

# Kill all processes
.\pieces\buttons\windows\kill.ps1
```

### macOS
```bash
# Same as Linux (bash scripts work on macOS)
./#.dev-storage/#.tools/compile_all.sh
./button.sh run
```

---

## 🔧 Platform-Specific Code

### Input Handling
| Platform | File | API |
|----------|------|-----|
| Linux | `pieces/keyboard/src/keyboard_input_linux.c` | `termios`, raw terminal |
| Windows | `pieces/keyboard/src/keyboard_input_win.c` | `GetAsyncKeyState()`, `ReadConsoleInput()` |
| macOS | `pieces/keyboard/src/keyboard_input_linux.c` | Same as Linux (termios works) |

### Rendering
| Platform | File | API |
|----------|------|-----|
| Linux | `pieces/display/renderer.c` | Terminal ASCII |
| Windows | `pieces/display/windows_renderer.c` | Windows Console API |
| macOS | `pieces/display/renderer.c` | Terminal ASCII |

### Process Spawning
| Platform | Pattern |
|----------|---------|
| Linux/macOS | `fork()` / `exec()` / `waitpid()` |
| Windows | `CreateProcess()` / `WaitForSingleObject()` |

### Cross-Platform Pattern
```c
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define usleep(us) Sleep((us)/1000)
    #define access _access
    #define F_OK 0
    #define getcwd _getcwd
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
#endif
```

---

## 📁 Path Differences

| Aspect | Linux/macOS | Windows |
|--------|-------------|---------|
| Path separator | `/` | `\` |
| Binary extension | `.+x` | `.exe` |
| Home directory | `/home/user/` | `C:\Users\user\` |
| Font path | `/usr/share/fonts/` | `C:\Windows\Fonts\` |

### Solution: Use `build_path_malloc()` Helper
```c
char* build_path_malloc(const char* rel) {
    size_t sz = strlen(project_root) + strlen(rel) + 2;
    char* p = (char*)malloc(sz);
    if (p) snprintf(p, sz, "%s/%s", project_root, rel);
    return p;
}
```

---

## 🪟 Fondu on Windows

### PowerShell Installer vs Bash Script
- **Linux/macOS:** `./fondu --install <project>` (bash)
- **Windows:** `.\fondu.ps1 -Install <project>` (PowerShell)

### Binary Convention
- **Linux/macOS:** `.+x` extension
- **Windows:** `.exe` extension

The compile scripts handle this automatically. Fondu looks for the correct extension based on platform.

---

## 🧪 Testing Across Platforms

### Cross-Platform Ops
Most ops are cross-platform because they only do file I/O:
- `move_entity.+x` ✅
- `render_map.+x` ✅
- `create_profile.+x` ✅
- `auth_user.+x` ✅

### Platform-Specific Ops
These ops need platform-specific versions:
- `keyboard_input.+x` ❌ (different per platform)
- `renderer.+x` ❌ (different per platform)
- `joystick_input.+x` ❌ (XInput on Windows, evdev on Linux)

### CI/CD Considerations
- **GitHub Actions:** Run tests on Linux, Windows, macOS runners
- **Compile verification:** Ensure all ops compile on all platforms
- **Integration tests:** Run full test suite on each platform

---

## 🛠️ Developer Setup Guides

### Linux (Already Working)
Your current setup is Linux. You're good to go!

### Windows (MSYS2)
1. **Install MSYS2:** Download from https://www.msys2.org/
2. **Install GCC:** `pacman -S mingw-w64-x86_64-gcc`
3. **Clone TPMOS:** `git clone <repo>`
4. **Compile:** `.\pieces\buttons\windows\legacy\compile_all.ps1`
5. **Run:** `.\button.ps1 run`

### macOS
1. **Install Xcode Command Line Tools:** `xcode-select --install`
2. **Install Homebrew:** `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
3. **Install dependencies:** `brew install glfw freetype`
4. **Compile:** `./#.dev-storage/#.tools/compile_all.sh`
5. **Run:** `./button.sh run`

---

## ⚠️ Common Pitfalls

### Pitfall 1: Hardcoded Linux Paths on Windows
**Symptom:** "File not found" errors on Windows.
**Cause:** Paths use `/` but Windows expects `\`.
**Fix:** Use `build_path_malloc()` or `snprintf()` with platform-aware path construction.

### Pitfall 2: Missing MSYS2 Dependencies on Windows
**Symptom:** Compile fails with "undefined reference to `glfwInit`".
**Cause:** GLFW not installed in MSYS2.
**Fix:** `pacman -S mingw-w64-x86_64-glfw`

### Pitfall 3: Signal Handling Differences
**Symptom:** Ctrl+C doesn't clean up children on Windows.
**Cause:** Windows signal handling differs from POSIX.
**Fix:** Use `SetConsoleCtrlHandler()` on Windows:
```c
#ifdef _WIN32
BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT) {
        g_shutdown = 1;
        return TRUE;
    }
    return FALSE;
}
SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
signal(SIGINT, handle_sigint);
#endif
```

### Pitfall 4: Font Path Not Found on GL-OS
**Symptom:** GL-OS crashes with "Failed to load font".
**Cause:** Font path hardcoded to Linux location.
**Fix:** Use platform-specific font paths:
```c
#ifdef _WIN32
    const char* font_path = "C:/Windows/Fonts/arial.ttf";
#elif __APPLE__
    const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    const char* font_path = "/usr/share/fonts/truetype/arphic/uming.ttc";
#endif
```

---

## 🏛️ Scholar's Corner: The "Great Migration"
When TPMOS was first ported to Windows, the developer assumed it would be a simple matter of changing `#include` headers. Instead, they discovered that every single path in the codebase was hardcoded with `/home/user/`. It took three weeks to refactor every file to use `build_path_malloc()`. But when the first Windows build finally compiled and ran, the developer watched in awe as the ASCII terminal rendered perfectly, the zombie chased the player, and the entire TPMOS universe existed natively on Windows. This became known as **"The Great Migration."** It taught us that **"Portability is not an afterthought—it is a design principle."** 🌍🪟

---

## 📝 Study Questions
1.  What is the Windows equivalent of `fork()/exec()`?
2.  Why does TPMOS use `.+x` on Linux but `.exe` on Windows?
3.  **Write the code** to make signal handling work on both Windows and Linux.
4.  **Scenario:** Your op compiles on Linux but fails on Windows with "undefined reference to `usleep`". What's the fix?
5.  **True or False:** All TPMOS ops are cross-platform by default.

---
[Return to Index](INDEX.md)
