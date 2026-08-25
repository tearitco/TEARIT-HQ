#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (!a || !a[0]) {
        snprintf(out, out_sz, "%s", b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        snprintf(out, out_sz, "%s", a);
        return;
    }
    if (a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static void copy_value(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
}

static void derive_repo_root(const char *project_root, char *repo_root, size_t repo_root_sz) {
    const char *needle = "/projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_root, needle);
    if (p) {
        size_t len = (size_t)(p - project_root);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_root, len);
        repo_root[len] = '\0';
        return;
    }
    copy_value(repo_root, repo_root_sz, project_root);
}

static void load_current_dir(const char *root, char *current_dir, size_t current_dir_sz) {
    char path[MAX_PATH_LEN];
    char line[1024];
    FILE *f;

    snprintf(current_dir, current_dir_sz, "projects");
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "current_dir=", 12) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            copy_value(current_dir, current_dir_sz, line + 12);
            break;
        }
    }
    fclose(f);
}

static unsigned long long hash_bytes(unsigned long long h, const unsigned char *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= (unsigned long long)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static unsigned long long fingerprint_dir(const char *repo_root, const char *current_dir) {
    char abs_dir[PATH_MAX];
    DIR *dir;
    struct dirent *de;
    unsigned long long h = 1469598103934665603ULL;

    if (current_dir && current_dir[0]) path_join(abs_dir, sizeof(abs_dir), repo_root, current_dir);
    else snprintf(abs_dir, sizeof(abs_dir), "%s", repo_root);

    dir = opendir(abs_dir);
    if (!dir) {
        return hash_bytes(h, (const unsigned char *)abs_dir, strlen(abs_dir));
    }

    while ((de = readdir(dir))) {
        struct stat st;
        char entry_path[PATH_MAX];
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        path_join(entry_path, sizeof(entry_path), abs_dir, de->d_name);
        if (lstat(entry_path, &st) != 0) continue;
        h = hash_bytes(h, (const unsigned char *)de->d_name, strlen(de->d_name));
        h = hash_bytes(h, (const unsigned char *)&st.st_mtime, sizeof(st.st_mtime));
        h = hash_bytes(h, (const unsigned char *)&st.st_size, sizeof(st.st_size));
        h = hash_bytes(h, (const unsigned char *)&st.st_mode, sizeof(st.st_mode));
    }

    closedir(dir);
    return h;
}

static void write_pid_file(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/fs_watch.pid");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", (long)getpid());
    fclose(f);
}

static void append_marker(const char *root, const char *current_dir, unsigned long long fp) {
    char path[MAX_PATH_LEN];
    FILE *f;
    time_t now = time(NULL);
    path_join(path, sizeof(path), root, "session/fs_watch.marker");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "[%ld] dir=%s hash=%llu\n", (long)now, current_dir && current_dir[0] ? current_dir : ".", fp);
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[PATH_MAX];
    char current_dir[PATH_MAX];
    char last_dir[PATH_MAX] = "";
    unsigned long long last_fp = 0;
    int initialized = 0;

    if (argc < 2) return 2;
    root = argv[1];

    derive_repo_root(root, repo_root, sizeof(repo_root));
    write_pid_file(root);

    for (;;) {
        unsigned long long fp;
        load_current_dir(root, current_dir, sizeof(current_dir));
        fp = fingerprint_dir(repo_root, current_dir);

        if (!initialized) {
            copy_value(last_dir, sizeof(last_dir), current_dir);
            last_fp = fp;
            initialized = 1;
        } else if (strcmp(last_dir, current_dir) != 0 || last_fp != fp) {
            append_marker(root, current_dir, fp);
            copy_value(last_dir, sizeof(last_dir), current_dir);
            last_fp = fp;
        }

        usleep(300000);
    }

    return 0;
}
