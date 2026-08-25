/* khtpm_plat.h — thin platform surface (Win32 or X11).
 * Design logic stays in khtpm_core; plat only draws windows and pumps events.
 */
#ifndef KHTPM_PLAT_H
#define KHTPM_PLAT_H

#include "khtpm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run entity window until close. Returns process exit code. */
int khtpm_plat_run_entity(KhtpmEntity *e);

/* Optional: ensure taskbar process for house (may no-op if already up). */
void khtpm_plat_ensure_taskbar(const char *house_root);

#ifdef __cplusplus
}
#endif
#endif
