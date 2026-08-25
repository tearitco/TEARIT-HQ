#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <stdlib.h>
    #include <io.h>
    #include <process.h>

    #define GLUT_DISABLE_ATEXIT_HACK

    #define MKDIR(path, mode) _mkdir(path)
    #define REALPATH(path, resolved) _fullpath(resolved, path, _MAX_PATH)
    #define SETENV(name, value, overwrite) _putenv_s(name, value)

    // Windows: Use PowerShell to execute .+x binaries
    #define RUN_PLUS_X(path, args) run_plus_x_powershell(path, args)

    // Inline helper to run .+x executables via PowerShell
    static inline int run_plus_x_powershell(const char* path, const char* args) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "powershell -NoProfile -ExecutionPolicy Bypass -Command \"& { & '%s' %s }\"",
            path, args ? args : "");
        return system(cmd);
    }
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <limits.h>

    #define MKDIR(path, mode) mkdir(path, mode)
    #define REALPATH(path, resolved) realpath(path, resolved)
    #define SETENV(name, value, overwrite) setenv(name, value, overwrite)
    
    // Linux: Direct execution
    #define RUN_PLUS_X(path, args) execl(path, path, args, NULL)
#endif

#endif
