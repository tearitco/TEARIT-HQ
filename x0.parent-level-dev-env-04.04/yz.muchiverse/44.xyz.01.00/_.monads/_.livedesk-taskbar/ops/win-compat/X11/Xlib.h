/* win-compat/X11/Xlib.h — Windows funnel header.
 *
 * khtpm_core_render.c is canonical shared source and hard-includes
 * <X11/Xlib.h> with ZERO _WIN32 branches, exactly like its Linux build. The
 * house convention (see &.widgits/_shared-lib/README.md) is to compile
 * canonical sources IN PLACE and steer the build with -I rather than editing
 * the canonical file, so this directory satisfies those includes on Windows
 * instead of patching the renderer.
 *
 * There is no X server on this platform and the existing
 * khtpm_strip_x11_win.h is a complete self-contained stand-in (its own
 * Window/GC/XEvent/Xft types), so the real MinGW Xlib.h must NOT be used —
 * the two definitions are mutually exclusive. This funnel therefore routes
 * every X11 include to that one canonical shim header, so the renderer's
 * view and the shim implementation's view can never drift apart.
 *
 * -I ordering puts this dir ahead of MinGW's, so <X11/Xlib.h> resolves here.
 */
#ifndef KHTPM_WINCOMPAT_X11_XLIB_H
#define KHTPM_WINCOMPAT_X11_XLIB_H

#include "../../khtpm_strip_x11_win.h"

#endif /* KHTPM_WINCOMPAT_X11_XLIB_H */
