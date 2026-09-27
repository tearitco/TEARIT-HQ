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

/* --- <sys/wait.h> is shimmed by win-compat/sys/wait.h; <sys/select.h> and
 * <X11/*> by the rest of this directory. Nothing to declare here. */

#ifdef __cplusplus
}
#endif

#endif /* KHTPM_WINCOMPAT_PRELUDE_H */
