#ifndef SELF_EXE_H
#define SELF_EXE_H
/* macOS leg (2026-08-22): /proc/self/exe does not exist on macOS, so
 * every "resolve my own binary path" readlink silently failed there.
 * Same helper shape as *.monads/*.livedesk-taskbar/ops/
 * tp_desktop_window_rgb.c's own self_exe_path() - _NSGetExecutablePath
 * + realpath on Darwin, plain readlink elsewhere. Drop-in for the
 *   ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
 *   if (n <= 0) fail; buf[n] = '\0';
 * pattern: call self_exe_readlink(buf, sizeof(buf)) instead. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
static ssize_t self_exe_readlink(char *buf, size_t sz) {
    uint32_t size = (uint32_t)sz;
    if (_NSGetExecutablePath(buf, &size) != 0) return -1;
    char resolved[PATH_MAX];
    if (realpath(buf, resolved)) snprintf(buf, sz, "%s", resolved);
    return (ssize_t)strlen(buf);
}
#else
static ssize_t self_exe_readlink(char *buf, size_t sz) {
    ssize_t n = readlink("/proc/self/exe", buf, sz - 1);
    if (n > 0) buf[n] = '\0';
    return n;
}
#endif
#endif /* SELF_EXE_H */
