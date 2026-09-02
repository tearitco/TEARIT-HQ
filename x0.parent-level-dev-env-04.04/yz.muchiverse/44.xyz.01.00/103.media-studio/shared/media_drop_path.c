/* media_drop_path.c — robust file:// / path resolve for emoji & special house paths */
#define _GNU_SOURCE
#include "media_drop_path.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void media_url_decode_inplace(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(hex, NULL, 16);
            p += 2;
        } else if (*p == '+') {
            *o++ = ' ';
        } else {
            *o++ = *p;
        }
    }
    *o = '\0';
}

static void set_err(char *err, size_t err_sz, const char *msg) {
    if (err && err_sz) snprintf(err, err_sz, "%s", msg);
}

int media_path_is_readable_file(const char *path) {
    if (!path || !path[0]) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) return 0;
    return access(path, R_OK) == 0;
}

int media_kind_from_path(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || !dot[1]) return 0;
    char ext[24];
    size_t n = 0;
    for (const char *p = dot + 1; *p && n < sizeof(ext) - 1; p++)
        ext[n++] = (char)tolower((unsigned char)*p);
    ext[n] = 0;
    if (!strcmp(ext, "mp4") || !strcmp(ext, "webm") || !strcmp(ext, "mkv") ||
        !strcmp(ext, "mov") || !strcmp(ext, "avi") || !strcmp(ext, "m4v") ||
        !strcmp(ext, "mpeg") || !strcmp(ext, "mpg") || !strcmp(ext, "wmv") ||
        !strcmp(ext, "ts") || !strcmp(ext, "flv") || !strcmp(ext, "ogv") ||
        !strcmp(ext, "3gp") || !strcmp(ext, "webm"))
        return 1;
    if (!strcmp(ext, "wav") || !strcmp(ext, "mp3") || !strcmp(ext, "aac") ||
        !strcmp(ext, "ogg") || !strcmp(ext, "flac") || !strcmp(ext, "m4a") ||
        !strcmp(ext, "opus") || !strcmp(ext, "wma") || !strcmp(ext, "aiff") ||
        !strcmp(ext, "aif") || !strcmp(ext, "oga"))
        return 2;
    if (!strcmp(ext, "png") || !strcmp(ext, "jpg") || !strcmp(ext, "jpeg") ||
        !strcmp(ext, "webp") || !strcmp(ext, "gif") || !strcmp(ext, "bmp") ||
        !strcmp(ext, "tif") || !strcmp(ext, "tiff") || !strcmp(ext, "svg") ||
        !strcmp(ext, "heic") || !strcmp(ext, "jxl"))
        return 3;
    if (!strcmp(ext, "obj") || !strcmp(ext, "fbx") || !strcmp(ext, "gltf") ||
        !strcmp(ext, "glb") || !strcmp(ext, "dae") || !strcmp(ext, "ply") ||
        !strcmp(ext, "stl") || !strcmp(ext, "3ds") || !strcmp(ext, "blend"))
        return 4; /* mesh / 3D */
    return 0;
}

void media_drop_debug_log(const char *project_root, const char *line) {
    char path[MEDIA_PATH_MAX];
    if (project_root && project_root[0])
        snprintf(path, sizeof(path), "%s/pieces/display/last_drop_debug.txt", project_root);
    else
        snprintf(path, sizeof(path), "/tmp/media_drop_debug.txt");
    FILE *f = fopen(path, "a");
    if (!f) f = fopen("/tmp/media_drop_debug.txt", "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

int media_uri_to_path(const char *uri, char *out, size_t out_sz, char *err, size_t err_sz) {
    if (!uri || !out || out_sz < 2) {
        set_err(err, err_sz, "null uri/out");
        return 0;
    }
    out[0] = 0;

    /* skip whitespace */
    while (*uri && isspace((unsigned char)*uri)) uri++;
    if (!*uri) {
        set_err(err, err_sz, "empty uri");
        return 0;
    }

    /* strip wrapping quotes from some file managers */
    char tmp[MEDIA_PATH_MAX];
    size_t ulen = strlen(uri);
    if (ulen >= sizeof(tmp)) ulen = sizeof(tmp) - 1;
    memcpy(tmp, uri, ulen);
    tmp[ulen] = 0;
    /* trim trailing whitespace */
    while (ulen > 0 && isspace((unsigned char)tmp[ulen - 1])) tmp[--ulen] = 0;
    if ((tmp[0] == '"' && ulen >= 2 && tmp[ulen - 1] == '"') ||
        (tmp[0] == '\'' && ulen >= 2 && tmp[ulen - 1] == '\'')) {
        tmp[ulen - 1] = 0;
        memmove(tmp, tmp + 1, ulen - 1);
    }

    const char *p = tmp;

    /* file: schemes — GNOME/Nautilus send file:///…, file://localhost/…, file:/… */
    if (strncmp(p, "file:", 5) == 0) {
        p += 5;
        if (p[0] == '/' && p[1] == '/' && p[2] == '/') {
            /* file:///absolute/path → keep one leading slash */
            p += 2;
        } else if (p[0] == '/' && p[1] == '/') {
            /* file://host/absolute  or  file://localhost/absolute */
            p += 2;
            if (strncmp(p, "localhost", 9) == 0 && (p[9] == '/' || p[9] == '\0')) {
                p += 9;
            } else {
                const char *slash = strchr(p, '/');
                if (!slash) {
                    set_err(err, err_sz, "file:// host without path");
                    return 0;
                }
                p = slash;
            }
        }
        /* else file:/absolute or file:relative — leave p as-is */
    }

    /* copy + percent-decode (emoji / ZWJ / VS16 in house paths) */
    if (strlen(p) >= out_sz) {
        set_err(err, err_sz, "path too long for buffer (need MEDIA_PATH_MAX)");
        return 0;
    }
    snprintf(out, out_sz, "%s", p);
    media_url_decode_inplace(out);

    /* strip trailing junk */
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = 0;

    if (!out[0]) {
        set_err(err, err_sz, "empty path after decode");
        return 0;
    }

    /* If still not absolute, leave as-is (cwd relative) */

    /* Verify readable; if fail, try realpath */
    if (media_path_is_readable_file(out))
        return 1;

    char resolved[MEDIA_PATH_MAX];
    if (realpath(out, resolved)) {
        if (media_path_is_readable_file(resolved)) {
            snprintf(out, out_sz, "%s", resolved);
            return 1;
        }
    }

    /* Last resort: path exists as any node? */
    struct stat st;
    if (stat(out, &st) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "stat failed (errno path too long or missing): %.180s", out);
        set_err(err, err_sz, msg);
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        set_err(err, err_sz, "path exists but is not a regular file");
        return 0;
    }
    if (access(out, R_OK) != 0) {
        set_err(err, err_sz, "path not readable");
        return 0;
    }
    return 1;
}

int media_import_uri_list(const char *data, const char *project_root,
                          media_drop_on_path_fn on_path, void *user,
                          char *status, size_t status_sz) {
    if (!data || !on_path) return 0;

    char logline[MEDIA_PATH_MAX + 128];
    snprintf(logline, sizeof(logline), "RAW_DROP_BYTES=%zu HEAD=%.200s",
             strlen(data), data);
    media_drop_debug_log(project_root, logline);

    /* work on a copy — data may be large */
    size_t len = strlen(data);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return 0;
    memcpy(buf, data, len + 1);

    int accepted = 0, failed = 0;
    char *save = NULL;
    /* split on CR or LF */
    for (char *line = strtok_r(buf, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        if (!line[0] || line[0] == '#') continue;
        char path[MEDIA_PATH_MAX];
        char err[256];
        err[0] = 0;
        if (!media_uri_to_path(line, path, sizeof(path), err, sizeof(err))) {
            failed++;
            snprintf(logline, sizeof(logline), "FAIL uri=%.120s err=%s", line, err);
            media_drop_debug_log(project_root, logline);
            continue;
        }
        if (!media_path_is_readable_file(path)) {
            failed++;
            snprintf(logline, sizeof(logline), "FAIL not_readable path=%.200s", path);
            media_drop_debug_log(project_root, logline);
            continue;
        }
        snprintf(logline, sizeof(logline), "OK path=%.300s kind=%d", path, media_kind_from_path(path));
        media_drop_debug_log(project_root, logline);
        on_path(path, user);
        accepted++;
    }
    free(buf);

    if (status && status_sz) {
        if (accepted == 0 && failed == 0)
            snprintf(status, status_sz, "Drop empty");
        else if (accepted == 0)
            snprintf(status, status_sz,
                     "Drop failed (%d). See pieces/display/last_drop_debug.txt", failed);
        else
            snprintf(status, status_sz, "Imported %d file(s)%s", accepted,
                     failed ? " (some failed — see last_drop_debug.txt)" : "");
    }
    return accepted;
}
