/* win-compat/X11/extensions/shape.h — Windows funnel.
 * See win-compat/X11/Xlib.h for why. MinGW has no extensions/shape.h. The
 * renderer's tile mode folds in build_shape_mask()/cursword_update_shape()
 * from tp_desktop_window_rgb.c and calls XShapeCombineMask, which the shim
 * implements as a real SetWindowRgn() from the pixmap's alpha. */
#ifndef KHTPM_WINCOMPAT_X11_EXT_SHAPE_H
#define KHTPM_WINCOMPAT_X11_EXT_SHAPE_H

#include "../../../khtpm_strip_x11_win.h"

#endif /* KHTPM_WINCOMPAT_X11_EXT_SHAPE_H */
