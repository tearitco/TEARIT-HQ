/* yt_resolve.c — pure-C replacement for the yt-dlp --get-url call the
 * network-browser used for YouTube stream resolution. Posts the
 * public innerTube youtubei/v1/player endpoint with the ANDROID client
 * context (the only client that mints DIRECT googlevideo URLs — no
 * n-sig / signatureCipher / poToken deciphering is needed, verified
 * 2026-09-14), then parses streamingData JSON and prints the best
 * playable mp4 stream URL to stdout.
 *
 * usage: yt_resolve <youtube-watch-url|youtu.be/<id>|yt:<id>>
 *
 * exit 0 + URL on stdout on success; exit 1 with a reason on stderr on
 * failure (DRM/age-gate/regions/no-streamingData).
 *
 * Deps: system curl binary only (same convention as the manager's own
 * run_curl_interruptible). No libcurl, no libav, no third-party JSON.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define API_KEY "AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w"
#define API_URL "https://www.youtube.com/youtubei/v1/player?key=" API_KEY
#define MAX_RESP 8 * 1024 * 1024
#define MAX_URL 64 * 1024
#define TEMPL 32

/* Client context table: ANDROID clientVersions that still pass the
 * server-side precondition check (older ones 400 FAILED_PRECONDITION,
 * verified live 2026-09-14). Each is tried until one mints a URL. */
static const struct {
    const char *name;
    const char *ver;
} CLIENTS[] = {
    { "ANDROID", "20.02.35" },
    { "ANDROID", "20.05.36" },
};
#define N_CLIENTS (sizeof CLIENTS / sizeof CLIENTS[0])

/* ---------- video id ---------- */

static int valid_id_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static int extract_video_id(char *out, size_t n, const char *url) {
    const char *p = NULL;
    if (strncmp(url, "yt:", 3) == 0) {
        p = url + 3;
    } else if ((p = strstr(url, "youtube.com/watch")) != NULL) {
        const char *v = strstr(p, "v=");
        if (v) p = v + 2;
        else return 0;
    } else if ((p = strstr(url, "youtu.be/")) != NULL) {
        p += strlen("youtu.be/");
    } else {
        return 0;
    }
    if (!p || !*p) return 0;
    /* copy the [0-9A-Za-z_-]{6,30} token */
    size_t len = 0;
    while (p[len] && valid_id_char(p[len]) && len < 30) len++;
    if (len < 6) return 0;
    if (len >= n) len = n - 1;
    memcpy(out, p, len);
    out[len] = 0;
    return 1;
}

/* ---------- tiny JSON walker (depth-1 object fields + arrays) ---------- */

/* return pointer one past the JSON value that starts at s */
static const char *skip_json(const char *s) {
    if (*s == '"') {
        s++;
        while (*s) {
            if (*s == '\\') { s += 2; continue; }
            if (*s == '"') return s + 1;
            s++;
        }
        return s;
    }
    if (*s == '{' || *s == '[') {
        int depth = 0;
        const char *p = s;
        while (*p) {
            if (*p == '"') {
                p++;
                while (*p) {
                    if (*p == '\\') { p += 2; continue; }
                    if (*p == '"') { p++; break; }
                    p++;
                }
                continue;
            }
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') {
                depth--;
                if (depth == 0) return p + 1;
            }
            p++;
        }
        return p;
    }
    while (*s && *s != ',' && *s != '}' && *s != ']' &&
           *s != ' ' && *s != '\n' && *s != '\r' && *s != '\t')
        s++;
    return s;
}

static int js_isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/* parse one quoted string starting at s (which points at '"'), into out.
 * Handles \\ \" \/ \b\f\n\r\t + \uXXXX. Returns number of source chars
 * consumed (one past closing quote) or 0. */
static size_t json_string(const char *s, char *out, size_t outsz) {
    if (*s != '"') return 0;
    size_t oc = 0;
    const char *p = s + 1;
    while (*p && oc + 1 < outsz) {
        if (*p == '"') { out[oc] = 0; return (size_t)(p - s) + 1; }
        if (*p == '\\') {
            p++;
            switch (*p) {
            case 'n': out[oc++] = '\n'; p++; break;
            case 't': out[oc++] = '\t'; p++; break;
            case 'r': out[oc++] = '\r'; p++; break;
            case 'b': out[oc++] = '\b'; p++; break;
            case 'f': out[oc++] = '\f'; p++; break;
            case '/': out[oc++] = '/';  p++; break;
            case '"': out[oc++] = '"';  p++; break;
            case '\\': out[oc++] = '\\'; p++; break;
            case 'u': {
                long v = strtol(p + 1, NULL, 16);
                p += 5;
                if (v > 0 && v < 128) out[oc++] = (char)v;
                break;
            }
            default: p++;
            }
            continue;
        }
        out[oc++] = *p++;
    }
    out[oc] = 0;
    return 0;
}

/* object starting at obj ('{'); find top-level key, copy its raw quoted
 * string value into out. Returns 1 if found, 0 otherwise. Walking only
 * top-level members via skip_json keeps nested {thumbnails:[...]} noise
 * out of the way. */
static int obj_str_field(const char *obj, const char *key, char *out, size_t outsz) {
    if (!obj || *obj != '{') return 0;
    const char *p = obj + 1;
    size_t klen = strlen(key);
    while (*p) {
        while (*p && js_isspace(*p)) p++;
        if (*p == '}') return 0;
        if (*p != '"') { p = skip_json(p); while (*p && *p != ',') p++; if (*p == ',') p++; continue; }
        const char *kstart = p + 1;
        const char *kend = strchr(kstart, '"');
        if (!kend) return 0;
        int hit = ((size_t)(kend - kstart) == klen && strncmp(kstart, key, klen) == 0);
        p = kend + 1;                        /* past the key's closing quote */
        while (*p && js_isspace(*p)) p++;
        if (*p != ':') continue;
        p++;
        while (*p && js_isspace(*p)) p++;
        if (hit) {
            if (*p == '"') {
                json_string(p, out, outsz);
                return out[0] != 0;
            }
            return 0;
        }
        p = skip_json(p);                    /* consume this member's value */
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return 0;
}

/* locate the array value for "key": at top level within `region` */
static const char *find_array(const char *region, const char *key) {
    char buf[64];
    snprintf(buf, sizeof buf, "\"%s\":", key);
    const char *p = region;
    while ((p = strstr(p, buf)) != NULL) {
        const char *arr = p + strlen(buf);
        while (*arr && js_isspace(*arr)) arr++;
        if (*arr == '[') return arr;
        p += strlen(buf);
    }
    return NULL;
}

/* scan a "[{...},{...}]" array of format objects; fill out[] with the
 * best candidate (highest height, mime containing "video/mp4" preferred,
 * then larger contentLength). Returns 1 if any usable url found. */
struct fmt_rec {
    char url[MAX_URL];
    char mime[96];
    char ql[24];
    long long len;
    int height;
};

static int height_of(struct fmt_rec *f) {
    if (f->height > 0) return f->height;
    int h = 0, i = 0;
    while (f->ql[i] && isdigit((unsigned char)f->ql[i])) h = h * 10 + (f->ql[i++] - '0');
    return h;
}

static int scan_format_array(const char *arr, struct fmt_rec *best) {
    if (!arr || *arr != '[') return 0;
    const char *p = arr + 1;
    int found = 0;
    struct fmt_rec cur;
    while (*p) {
        while (*p && js_isspace(*p)) p++;
        if (*p == ']' || *p == 0) break;
        if (*p == '{') {
            memset(&cur, 0, sizeof cur);
            obj_str_field(p, "url", cur.url, sizeof cur.url);
            if (cur.url[0]) {
                obj_str_field(p, "mimeType", cur.mime, sizeof cur.mime);
                obj_str_field(p, "qualityLabel", cur.ql, sizeof cur.ql);
                char nb[32];
                if (obj_str_field(p, "contentLength", nb, sizeof nb)) cur.len = atoll(nb);
                if (obj_str_field(p, "height", nb, sizeof nb)) cur.height = atoi(nb);
                /* prefer mp4; always beat on height, then on length */
                int cur_mp4 = strstr(cur.mime, "video/mp4") != NULL;
                int best_mp4 = strstr(best->mime, "video/mp4") != NULL;
                if (!found || (cur_mp4 && !best_mp4) ||
                    (cur_mp4 == best_mp4 && height_of(&cur) > height_of(best)) ||
                    (cur_mp4 == best_mp4 && height_of(&cur) == height_of(best) && cur.len > best->len)) {
                    *best = cur;
                    found = 1;
                }
            }
            p = skip_json(p);
        } else {
            p = skip_json(p);
        }
        while (*p && (*p == ',' || js_isspace(*p))) p++;
    }
    return found;
}

/* ---------- transport ---------- */

static int post_player(const char *client_name, const char *client_ver,
                       const char *vid, char *out, size_t outsz) {
    char body[1024];
    snprintf(body, sizeof body,
             "{\"context\":{\"client\":{\"clientName\":\"%s\","
             "\"clientVersion\":\"%s\",\"androidSdkVersion\":33,"
             "\"hl\":\"en\",\"gl\":\"US\","
             "\"userAgent\":\"com.google.android.youtube/%s "
             "(Linux; U; Android 14) gzip\"}},\"videoId\":\"%s\"}",
             client_name, client_ver, client_ver, vid);

    char btmp[TEMPL];
    snprintf(btmp, sizeof btmp, "/tmp/yt_resolve_body.XXXXXX");
    int bfd = mkstemp(btmp);
    if (bfd < 0) { fprintf(stderr, "yt_resolve: mkstemp failed\n"); return 0; }
    { ssize_t _w = write(bfd, body, strlen(body)); (void)_w; }
    close(bfd);

    char outtmp[TEMPL];
    snprintf(outtmp, sizeof outtmp, "/tmp/yt_resolve_out.XXXXXX");
    int ofd = mkstemp(outtmp);
    if (ofd < 0) { unlink(btmp); fprintf(stderr, "yt_resolve: mkstemp failed\n"); return 0; }
    close(ofd);

    char cmd[4096];
    snprintf(cmd, sizeof cmd,
             "curl -sS -m 30 -X POST "
             "-H 'Content-Type: application/json' "
             "-H 'User-Agent: com.google.android.youtube/%s "
             "(Linux; U; Android 14) gzip' "
             "-H 'X-YouTube-Client-Name: 3' "
             "-H 'X-YouTube-Client-Version: %s' "
             " --data-binary @'%s' '%s' -o '%s' 2>/dev/null",
             client_ver, client_ver, btmp, API_URL, outtmp);

    int rc = system(cmd);
    unlink(btmp);
    if (rc != 0) {
        unlink(outtmp);
        return 0;
    }

    FILE *f = fopen(outtmp, "r");
    if (!f) { unlink(outtmp); return 0; }
    size_t n = fread(out, 1, outsz - 1, f);
    fclose(f);
    unlink(outtmp);
    out[n] = 0;
    return n > 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <youtube-watch-url|youtu.be/<id>|yt:<id>>\n", argv[0]);
        return 1;
    }
    char vid[64];
    if (!extract_video_id(vid, sizeof vid, argv[1])) {
        fprintf(stderr, "yt_resolve: could not extract a video id from '%s'\n", argv[1]);
        return 1;
    }

    for (size_t ci = 0; ci < N_CLIENTS; ci++) {
        char *resp = malloc(MAX_RESP);
        if (!resp) return 1;
        if (!post_player(CLIENTS[ci].name, CLIENTS[ci].ver, vid, resp, MAX_RESP)) {
            free(resp);
            continue;
        }

        char *sd = strstr(resp, "\"streamingData\"");
        if (!sd) {
            /* surface the playability reason for diagnostics */
            char reason[256] = "";
            char status[64] = "";
            obj_str_field(strstr(resp, "\"playabilityStatus\""), "status", status, sizeof status);
            obj_str_field(strstr(resp, "\"playabilityStatus\""), "reason", reason, sizeof reason);
            if (strlen(reason) || strlen(status))
                fprintf(stderr, "yt_resolve: %s - %s\n", status, reason);
            free(resp);
            continue;
        }

        struct fmt_rec best;
        memset(&best, 0, sizeof best);
        int got = scan_format_array(find_array(sd, "formats"), &best);
        if (!got)
            got = scan_format_array(find_array(sd, "adaptiveFormats"), &best);
        free(resp);
        if (got && best.url[0]) {
            printf("%s\n", best.url);
            return 0;
        }
    }
    fprintf(stderr, "yt_resolve: no playable non-DRM stream for %s\n", vid);
    return 1;
}