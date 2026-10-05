/* win-compat/X11/Xft/Xft.h — Windows funnel. See win-compat/X11/Xlib.h for why.
 * MinGW ships Xlib/Xutil/keysym/Xatom but NOT Xft, and the renderer plus
 * khtpm_draw_core.c call XftFontOpenName / XftDrawStringUtf8 /
 * XftTextExtentsUtf8 / XftColorAllocValue for every text run. The shim header
 * already declares those against its own XftFont (an HFONT + pixel size) and
 * XftDraw, so routing the include here is all that is required. */
#ifndef KHTPM_WINCOMPAT_X11_XFT_XFT_H
#define KHTPM_WINCOMPAT_X11_XFT_XFT_H

#include "../../../khtpm_strip_x11_win.h"

#endif /* KHTPM_WINCOMPAT_X11_XFT_XFT_H */
