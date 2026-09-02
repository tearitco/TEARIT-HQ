/* khtpm_main.c — shared entry for tp_desktop_window on all platforms.
 *
 * Usage: tp_desktop_window <package_dir>
 *
 * Linux primary development of design = edit khtpm_core.c (+ PDL/sprites).
 * Platform: khtpm_plat_win.c or khtpm_plat_x11.c (windowing only).
 *
 * Build Win:
 *   gcc -O2 -o tp_desktop_window.exe khtpm_main.c khtpm_core.c khtpm_plat_win.c \
 *       -lopengl32 -lgdi32 -luser32
 * Build Linux (when plat_x11 ready):
 *   gcc -O2 -o ops/+x/tp_desktop_window.+x khtpm_main.c khtpm_core.c khtpm_plat_x11.c \
 *       -lX11 -lXext -lGL -lm
 *
 * Legacy full X11 binary: still tp_desktop_window.c until plat_x11 is complete.
 */
#include "khtpm_core.h"
#include "khtpm_plat.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define khtpm_pid() ((int)GetCurrentProcessId())
#else
#  include <unistd.h>
#  define khtpm_pid() ((int)getpid())
#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window <package_dir>\n");
        return 1;
    }

    /* Callers (crypt_autostart / button.ps1) chdir house first. */
    const char *house = ".";
    const char *pkg = argv[1];

    KhtpmEntity ent;
    if (!khtpm_entity_init(&ent, pkg, house)) {
        fprintf(stderr, "khtpm: init failed\n");
        return 1;
    }
    if (!khtpm_package_exists(&ent)) {
        fprintf(stderr, "khtpm: package missing: %s\n", ent.package_dir);
        khtpm_entity_shutdown(&ent);
        return 1;
    }

    if (khtpm_sprite_load(&ent))
        khtpm_history(&ent, "sprite.csv loaded (core)");
    else
        khtpm_history(&ent, "sprite.csv missing — plat fallback");

    khtpm_menu_load(&ent);
    khtpm_registry_add(&ent, khtpm_pid());
    khtpm_history(&ent, "WINDOW_OPEN index=%d id=%s", ent.livedesk_index, ent.full_id);

    khtpm_plat_ensure_taskbar(ent.house_root);
    int rc = khtpm_plat_run_entity(&ent);

    khtpm_registry_remove(ent.house_root, khtpm_pid());
    khtpm_history(&ent, "WINDOW_CLOSE");
    khtpm_entity_shutdown(&ent);
    return rc;
}
