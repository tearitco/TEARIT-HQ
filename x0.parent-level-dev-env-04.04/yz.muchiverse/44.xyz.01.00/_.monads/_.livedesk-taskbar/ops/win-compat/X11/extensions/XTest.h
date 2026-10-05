/* win-compat/X11/extensions/XTest.h — Windows funnel.
 * See win-compat/X11/Xlib.h for why. MinGW has no extensions/XTest.h. The
 * renderer synthesises synthetic clicks via XTestFakeButtonEvent; on Windows
 * the shim maps that onto SendInput()/mouse_event so the code path and its
 * behaviour are preserved rather than stubbed to a no-op. */
#ifndef KHTPM_WINCOMPAT_X11_EXT_XTEST_H
#define KHTPM_WINCOMPAT_X11_EXT_XTEST_H

#include "../../../khtpm_strip_x11_win.h"

#endif /* KHTPM_WINCOMPAT_X11_EXT_XTEST_H */
