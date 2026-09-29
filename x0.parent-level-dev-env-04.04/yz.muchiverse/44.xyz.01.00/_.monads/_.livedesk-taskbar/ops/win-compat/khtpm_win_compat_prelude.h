/* win-compat/khtpm_win_compat_prelude.h — force-included into every
 * Windows build of the canonical renderers via:
 *
 *     gcc -include win-compat/khtpm_win_compat_prelude.h ...
 *
 * Why force-include rather than shadow a system header: the POSIX functions
 * below live in <stdlib.h>/<unistd.h>, which DO exist under MinGW-w64. If I
 * put a file at win-compat/stdlib.h it would take precedence over MinGW's
 * real one and I'd be reimplementing half the CRT. -include adds only the
 * declarations that are genuinely absent, and leaves every other header
 * exactly as the platform provides it.
 *
 * Each declaration here is one MinGW-w64 really lacks. Anything MinGW does
 * provide (stat's real declaration, fdopen, fileno, getenv, ...) is
 * deliberately NOT redeclared, to avoid conflicting with the CRT's own
 * prototypes.
 */
#ifndef KHTPM_WINCOMPAT_PRELUDE_H
#define KHTPM_WINCOMPAT_PRELUDE_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

/* REAL, NEW 2026-09-26 - ShellExecuteA. Both canonical sources reach for it
 * in their own _WIN32 blocks to open a directory in Explorer
 * (khtpm_entity.c:634 and khtpm_taskbar_manager.c:5726), and neither
 * includes <shellapi.h> on that path. It lives in shell32, which is already
 * on the link line, so the declaration is all that is missing - and the
 * prelude is the only place a Windows build can get one without editing
 * canonical source.
 *
 * windows.h has to come first, and that ordering is not optional:
 * shellapi.h uses DECLARE_HANDLE and DECLSPEC_IMPORT out of windef.h without
 * including it itself, so on its own it fails with ~200 "unknown type name
 * 'HWND'" errors. shellapi.h also guards on _INC_SHELLAPI, so getting that
 * first parse wrong does not fail loudly and fall back - it caches a broken
 * _INC_SHELLAPI and every later include of it (windows.h:89 does exactly
 * that) becomes a no-op, leaving ShellExecuteA undeclared and every call site
 * silently implicit.
 *
 * The same WIN32_LEAN_AND_MEAN the X11 shim sets is set here, so the Win32
 * macro surface that ends up in the translation unit is byte-for-byte the one
 * these sources already compile against - the X11 shim's own <windows.h>
 * include later in the file is a no-op, because windows.h guards itself. */
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- <stdlib.h> POSIX additions, absent from MinGW-w64 -------------------
 * core_render.c:563-565 sets KHTPM_HOUSE / KHTPM_PKG / PRISC_PROJECT_ROOT
 * for spawned children, and 9485/9492/11487 set+unset DROP_PATH around a
 * drag-and-drop and pass an extra dir to a child. All of these are load-
 * bearing: the child processes read them out of their environment. Backed
 * by the CRT's _putenv_s (see khtpm_win_compat.c). */
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

/* --- <unistd.h> --------------------------------------------------------- */
int kill(pid_t pid, int sig);

/* REAL, NEW 2026-09-26 - readlink. MinGW-w64 does not provide it, and
 * khtpm_entity.c's self_exe_path() calls
 *     readlink("/proc/self/exe", out, out_sz - 1)
 * with no _WIN32 branch (entity.c:743), because the whole point of that
 * function is portable self-discovery. Implemented for real in
 * khtpm_win_compat.c over GetModuleFileNameW.
 *
 * The return is a byte count excluding the NUL, which is what readlink(2)
 * does and what the caller needs to terminate the buffer itself. */
ssize_t readlink(const char *path, char *buf, size_t bufsiz);

/* --- house path helper, called under #ifdef _WIN32 by TWO canonical
 * sources: khtpm_entity.c:3809 and khtpm_core_render.c:15043, both right
 * after copying argv[1] into a mutable buffer as the pal's package_dir.
 * Neither one defines it, and it was never in the tree - the original
 * static definition lived in the now-deleted tp_desktop_window_rgb.c and was
 * lost in the 2026-09-01 consolidation ("cleanup: delete dead source files
 * from the consolidation"), which is why it has been an implicit-declaration
 * error in the entity build ever since.
 *
 * It converts an absolute house path into the house-relative form the
 * entity's own IPC files expect, in place. Declared here rather than in the
 * X11 shim header because it is a path concern, not an X11 one, and this
 * prelude is force-included into every Windows build that needs it.
 * Implemented in khtpm_win_compat.c. */
void win_package_rel(char *path);

/* --- <stdio.h> ----------------------------------------------------------
 * core_render.c:8089 and :8164 build the frame snapshot/dump entirely
 * through open_memstream, then hand the buffer to the writer. That dump is
 * the evidence used to verify the dock actually rendered, so it is
 * implemented for real (khtpm_win_compat.c) rather than stubbed: a temp
 * FILE* that, on close, is slurped back into the caller's buffer.
 *
 * Closing such a stream is what publishes the buffer, so fclose() is routed
 * through khtpm_fclose(), which recognises registered memstreams and
 * finalises them, and otherwise defers to the real fclose unchanged. */
FILE *open_memstream(char **ptr, size_t *sizeloc);
int khtpm_fclose(FILE *fp);

#ifndef fclose
#define fclose(fp) khtpm_fclose(fp)
#endif

/* --- <stdio.h> rename ---------------------------------------------------
 * MinGW-w64's CRT rename() does NOT atomically replace an existing
 * destination: it returns EEXIST (errno 17) like the C89 CRT it wraps,
 * where POSIX rename(2) is "the one real atomic job" the writers in this
 * codebase rely on (khtpm_taskbar_manager_main.c's write_small_file(), and
 * every tmp-frame -> frame publish in khtpm_core_render.c — the dock's
 * entity_menu_frame_<pid>.txt round trip). On Windows each writer therefore
 * publishes its tmp once (when no destination exists yet) and then every
 * later tick silently FAILS to replace, freezing the final file at its
 * very first snapshot. Direct live incident 2026-09-28: the bottom dock
 * bar's frame sat pinned to its boot-time EMPTY snapshot while its .tmp
 * grew the full 17 live items every tick — the cells rendered "empty sized
 * cells" with only the live-tree separators, exactly the user's report.
 *
 * The POSIX spelling is preserved for every call site (the canonical
 * sources keep calling rename(tmp, path) unmodified); the prelude reroutes
 * the function call to khtpm_win_rename(), implemented for real over
 * MoveFileExA(..., MOVEFILE_REPLACE_EXISTING) in khtpm_win_compat.c. */
int khtpm_win_rename(const char *oldp, const char *newp);
#ifndef rename
#define rename khtpm_win_rename
#endif

/* --- <sys/wait.h> is shimmed by win-compat/sys/wait.h; <sys/select.h> and
 * <X11/*> by the rest of this directory. Nothing to declare here. */

#ifdef __cplusplus
}
#endif

#endif /* KHTPM_WINCOMPAT_PRELUDE_H */
