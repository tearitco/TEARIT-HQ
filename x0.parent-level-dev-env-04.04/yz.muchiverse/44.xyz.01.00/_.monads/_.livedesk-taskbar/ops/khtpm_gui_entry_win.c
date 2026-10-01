/* khtpm_gui_entry_win.c - Windows-only: give the taskbar binaries a
 * GUI-subsystem entry point so launching them never opens a console.
 *
 * DIRECT REPORT (2026-09-26): launching the bars put black CLI windows
 * on screen, and they were still there. The cause was not the launcher:
 * run_khtpm_strip_win.ps1 correctly uses Win32_Process.Create with no
 * console handle, but a MinGW binary is CONSOLE subsystem by default,
 * and Windows allocates a console for such a process the moment it
 * starts - WMI's inability to set std handles is irrelevant here. The
 * fix has to be in the binary's subsystem, not in the spawn flags.
 *
 * So these binaries are linked with -mwindows, which sets the PE
 * subsystem to Windows and makes the CRT look for WinMain instead of
 * main. The canonical sources all define `int main(int, char**)` and
 * this house does not fork its canonical sources per platform, so the
 * WinMain that -mwindows wants is provided here instead, in a
 * Windows-owned file, and it simply forwards to the real main.
 * That keeps khtpm_core_render.c / khtpm_entity.c /
 * khtpm_taskbar_manager_main.c completely unedited.
 *
 * __argc/__argv are the MinGW CRT's own parsed copies of the process
 * command line, so the real main() sees exactly the arguments it would
 * have seen under a console subsystem. No re-parsing, no drift.
 *
 * TRADE-OFF, stated rather than hidden: a GUI-subsystem process has no
 * console, so anything these programs wrote to stdout/stderr is now
 * discarded rather than displayed. In practice that costs nothing -
 * all three self-log to files (#.desktop/khtpm_strip_parser.log) and
 * none of them draw anything a user is meant to read on the terminal.
 * The one real loss is khtpm_core_render's usage text on the argc==2
 * error path ("pal/tile process is khtpm_entity"), which now goes
 * nowhere; run it from a shell that has opted back into a console if
 * that message is ever actually needed.
 *
 * This file is Windows-owned. It has no Linux counterpart and is not
 * part of the canonical sources - see
 * ..\..\..\#.#.calendar-dox\!.HQ-IQ-BOOK\09-appendix\
 * WINDOWS-TASKBAR-PORT.md.
 */
#include <windows.h>
#include <stdlib.h>

/* The canonical entry point, defined by whichever canonical .c this
 * binary is being linked with. Declared, not included: including the
 * canonical source would compile it twice. */
extern int main(int argc, char **argv);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
