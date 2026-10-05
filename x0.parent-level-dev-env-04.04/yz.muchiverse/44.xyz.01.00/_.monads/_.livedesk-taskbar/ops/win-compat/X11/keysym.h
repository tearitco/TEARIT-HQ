/* win-compat/X11/keysym.h — Windows funnel. See win-compat/X11/Xlib.h for why.
 * KeySym plus the XK_* constants the renderer actually tests live in the shim
 * header. Only the subset the house code references is defined; an unknown
 * keysym degrades to XK_VoidSymbol (0) exactly as real Xlib does. */
#ifndef KHTPM_WINCOMPAT_X11_KEYSYM_H
#define KHTPM_WINCOMPAT_X11_KEYSYM_H

#include "../../khtpm_strip_x11_win.h"

#endif /* KHTPM_WINCOMPAT_X11_KEYSYM_H */
