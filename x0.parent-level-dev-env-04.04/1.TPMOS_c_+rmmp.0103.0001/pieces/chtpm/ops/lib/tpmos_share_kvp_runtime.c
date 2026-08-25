#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define TPMOS_SHARE_KVP_SHM_NAME "/tpmos_share_kvp_v1"
#define TPMOS_SHARE_KVP_MAX_ROOT 1024
#define TPMOS_SHARE_KVP_MAX_KEY 256
#define TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES 128
#define TPMOS_SHARE_KVP_MAX_TEXT_VALUE 8192
#define TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES 4
#define TPMOS_SHARE_KVP_MAX_BLOB_SIZE (4 * 1024 * 1024)

typedef struct {
    int used;
    char key[TPMOS_SHARE_KVP_MAX_KEY];
    char value[TPMOS_SHARE_KVP_MAX_TEXT_VALUE];
} TpmosShareKvpTextEntry;

typedef struct {
    int used;
    char key[TPMOS_SHARE_KVP_MAX_KEY];
    size_t size;
    unsigned long generation;
    unsigned char data[TPMOS_SHARE_KVP_MAX_BLOB_SIZE];
} TpmosShareKvpBlobEntry;

typedef struct {
    pthread_mutex_t mutex;
    int initialized;
    pid_t server_pid;
    unsigned long dump_generation;
    unsigned long dump_request_generation;
    char root[TPMOS_SHARE_KVP_MAX_ROOT];
    TpmosShareKvpTextEntry text_entries[TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES];
    TpmosShareKvpBlobEntry blob_entries[TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES];
} TpmosShareKvpDB;

static void tpmos_share_kvp_join_path(char *out, size_t out_sz, const char *root, const char *rel) {
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "%s/%s", root && root[0] ? root : ".", rel ? rel : "");
}

static void tpmos_share_kvp_mkdirs_for_file(const char *path) {
    char tmp[TPMOS_SHARE_KVP_MAX_ROOT * 2];
    size_t i;
    snprintf(tmp, sizeof(tmp), "%s", path ? path : "");
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
}

static char *tpmos_share_kvp_strdup_local(const char *in) {
    size_t len;
    char *out;
    if (!in) in = "";
    len = strlen(in);
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, in, len + 1);
    return out;
}

static void tpmos_share_kvp_trim_newline(char *s) {
    if (s) s[strcspn(s, "\r\n")] = '\0';
}

static void tpmos_share_kvp_shell_quote(char *out, size_t out_sz, const char *in) {
    size_t oi = 0;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;
    if (oi + 1 < out_sz) out[oi++] = '\'';
    while (*in && oi + 1 < out_sz) {
        if (*in == '\'') {
            if (oi + 4 >= out_sz) break;
            out[oi++] = '\'';
            out[oi++] = '\\';
            out[oi++] = '\'';
            out[oi++] = '\'';
        } else {
            out[oi++] = *in;
        }
        in++;
    }
    if (oi + 1 < out_sz) out[oi++] = '\'';
    out[oi] = '\0';
}

static int tpmos_share_kvp_find_repo_root(const char *project_root, char *out, size_t out_sz) {
    char current[TPMOS_SHARE_KVP_MAX_ROOT];
    char candidate[TPMOS_SHARE_KVP_MAX_ROOT];
    char resolved[TPMOS_SHARE_KVP_MAX_ROOT];
    struct stat st;
    if (!project_root || !out || out_sz == 0) return -1;
    if (!realpath(project_root, current)) return -1;
    while (current[0]) {
        snprintf(candidate, sizeof(candidate), "%s/pieces/chtpm/ops", current);
        if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, out_sz, "%s", current);
            return 0;
        }
        if (strcmp(current, "/") == 0) break;
        {
            char *slash = strrchr(current, '/');
            if (!slash) break;
            if (slash == current) {
                current[1] = '\0';
            } else {
                *slash = '\0';
            }
        }
    }
    if (realpath(".", resolved)) {
        snprintf(out, out_sz, "%s", resolved);
        return 0;
    }
    return -1;
}

static const char *tpmos_share_kvp_backend_mode(const char *project_root) {
    static char mode[32];
    char path[TPMOS_SHARE_KVP_MAX_ROOT];
    FILE *f;
    const char *env = getenv("TPMOS_SHARE_KVP_BACKEND");
    if (env && env[0]) {
        snprintf(mode, sizeof(mode), "%s", env);
        return mode;
    }
    tpmos_share_kvp_join_path(path, sizeof(path), project_root, "session/kvp_backend.txt");
    f = fopen(path, "r");
    if (f) {
        if (fgets(mode, sizeof(mode), f)) {
            tpmos_share_kvp_trim_newline(mode);
            fclose(f);
            if (mode[0]) return mode;
        } else {
            fclose(f);
        }
    }
    snprintf(mode, sizeof(mode), "shmem");
    return mode;
}

static int tpmos_share_kvp_mode_is_file(const char *project_root) {
    return strcmp(tpmos_share_kvp_backend_mode(project_root), "file") == 0;
}

static int tpmos_share_kvp_mode_is_shmem(const char *project_root) {
    const char *mode = tpmos_share_kvp_backend_mode(project_root);
    return strcmp(mode, "shmem") == 0 || strcmp(mode, "file+mirror") == 0;
}

static int tpmos_share_kvp_mode_is_mirror(const char *project_root) {
    return strcmp(tpmos_share_kvp_backend_mode(project_root), "file+mirror") == 0;
}

static int tpmos_share_kvp_try_attach_direct(TpmosShareKvpDB **out_db) {
    int shm_fd;
    TpmosShareKvpDB *db;
    if (!out_db) return -1;
    *out_db = NULL;
    shm_fd = shm_open(TPMOS_SHARE_KVP_SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) return -1;
    db = (TpmosShareKvpDB *)mmap(NULL, sizeof(TpmosShareKvpDB), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (db == MAP_FAILED) return -1;
    *out_db = db;
    return 0;
}

static void tpmos_share_kvp_detach_direct(TpmosShareKvpDB *db) {
    if (db) munmap(db, sizeof(TpmosShareKvpDB));
}

static int tpmos_share_kvp_start_daemon(const char *project_root) {
    pid_t pid;
    char repo_root[TPMOS_SHARE_KVP_MAX_ROOT];
    char daemon_path[TPMOS_SHARE_KVP_MAX_ROOT];
    char *argv[4];
    if (tpmos_share_kvp_find_repo_root(project_root, repo_root, sizeof(repo_root)) != 0) return -1;
    tpmos_share_kvp_join_path(daemon_path, sizeof(daemon_path), repo_root, "pieces/chtpm/ops/+x/tpmos_share_kvp_db.+x");
    argv[0] = daemon_path;
    argv[1] = (char *)(project_root && project_root[0] ? project_root : ".");
    argv[2] = "--foreground";
    argv[3] = NULL;
    if (access(daemon_path, X_OK) != 0) return -1;
    if (posix_spawn(&pid, daemon_path, NULL, NULL, argv, environ) != 0) return -1;
    usleep(200000);
    return 0;
}

static int tpmos_share_kvp_ensure_daemon(const char *project_root) {
    TpmosShareKvpDB *db = NULL;
    if (!tpmos_share_kvp_mode_is_shmem(project_root)) return 0;
    if (tpmos_share_kvp_try_attach_direct(&db) == 0) {
        tpmos_share_kvp_detach_direct(db);
        return 0;
    }
    if (tpmos_share_kvp_start_daemon(project_root) != 0) return -1;
    if (tpmos_share_kvp_try_attach_direct(&db) == 0) {
        tpmos_share_kvp_detach_direct(db);
        return 0;
    }
    return -1;
}

static int tpmos_share_kvp_run_write_stdin(const char *project_root, const char *op, const char *key, const char *value) {
    char repo_root[TPMOS_SHARE_KVP_MAX_ROOT];
    char adapter_path[TPMOS_SHARE_KVP_MAX_ROOT];
    char quoted_path[TPMOS_SHARE_KVP_MAX_ROOT * 2];
    char quoted_key[TPMOS_SHARE_KVP_MAX_KEY * 2];
    char cmd[(TPMOS_SHARE_KVP_MAX_ROOT * 2) + (TPMOS_SHARE_KVP_MAX_KEY * 2) + 64];
    FILE *pipef;
    int status;
    if (tpmos_share_kvp_find_repo_root(project_root, repo_root, sizeof(repo_root)) != 0) return -1;
    if (tpmos_share_kvp_ensure_daemon(project_root) != 0) return -1;
    tpmos_share_kvp_join_path(adapter_path, sizeof(adapter_path), repo_root, "pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x");
    tpmos_share_kvp_shell_quote(quoted_path, sizeof(quoted_path), adapter_path);
    tpmos_share_kvp_shell_quote(quoted_key, sizeof(quoted_key), key);
    snprintf(cmd, sizeof(cmd), "%s %s %s", quoted_path, op, quoted_key);
    pipef = popen(cmd, "w");
    if (!pipef) return -1;
    if (value && value[0]) fwrite(value, 1, strlen(value), pipef);
    status = pclose(pipef);
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static int tpmos_share_kvp_run_simple(const char *project_root, const char *op, const char *key, const char *extra) {
    char repo_root[TPMOS_SHARE_KVP_MAX_ROOT];
    char adapter_path[TPMOS_SHARE_KVP_MAX_ROOT];
    char quoted_path[TPMOS_SHARE_KVP_MAX_ROOT * 2];
    char quoted_key[TPMOS_SHARE_KVP_MAX_KEY * 2];
    char quoted_extra[TPMOS_SHARE_KVP_MAX_ROOT * 2];
    char cmd[(TPMOS_SHARE_KVP_MAX_ROOT * 4) + (TPMOS_SHARE_KVP_MAX_KEY * 2) + 96];
    int status;
    if (tpmos_share_kvp_find_repo_root(project_root, repo_root, sizeof(repo_root)) != 0) return -1;
    if (tpmos_share_kvp_ensure_daemon(project_root) != 0) return -1;
    tpmos_share_kvp_join_path(adapter_path, sizeof(adapter_path), repo_root, "pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x");
    tpmos_share_kvp_shell_quote(quoted_path, sizeof(quoted_path), adapter_path);
    tpmos_share_kvp_shell_quote(quoted_key, sizeof(quoted_key), key ? key : "");
    quoted_extra[0] = '\0';
    if (extra && extra[0]) tpmos_share_kvp_shell_quote(quoted_extra, sizeof(quoted_extra), extra);
    if (key && key[0] && extra && extra[0]) snprintf(cmd, sizeof(cmd), "%s %s %s %s", quoted_path, op, quoted_key, quoted_extra);
    else if (key && key[0]) snprintf(cmd, sizeof(cmd), "%s %s %s", quoted_path, op, quoted_key);
    else snprintf(cmd, sizeof(cmd), "%s %s", quoted_path, op);
    status = system(cmd);
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static int tpmos_share_kvp_read_text_shmem(const char *project_root, const char *key, char *out, size_t out_sz) {
    char repo_root[TPMOS_SHARE_KVP_MAX_ROOT];
    char adapter_path[TPMOS_SHARE_KVP_MAX_ROOT];
    char quoted_path[TPMOS_SHARE_KVP_MAX_ROOT * 2];
    char quoted_key[TPMOS_SHARE_KVP_MAX_KEY * 2];
    char cmd[(TPMOS_SHARE_KVP_MAX_ROOT * 2) + (TPMOS_SHARE_KVP_MAX_KEY * 2) + 64];
    FILE *pipef;
    size_t used = 0;
    int c;
    if (!out || out_sz == 0) return -1;
    out[0] = '\0';
    if (tpmos_share_kvp_find_repo_root(project_root, repo_root, sizeof(repo_root)) != 0) return -1;
    if (tpmos_share_kvp_ensure_daemon(project_root) != 0) return -1;
    tpmos_share_kvp_join_path(adapter_path, sizeof(adapter_path), repo_root, "pieces/chtpm/ops/+x/tpmos_share_kvp_adapter.+x");
    tpmos_share_kvp_shell_quote(quoted_path, sizeof(quoted_path), adapter_path);
    tpmos_share_kvp_shell_quote(quoted_key, sizeof(quoted_key), key);
    snprintf(cmd, sizeof(cmd), "%s read %s", quoted_path, quoted_key);
    pipef = popen(cmd, "r");
    if (!pipef) return -1;
    while ((c = fgetc(pipef)) != EOF && used + 1 < out_sz) out[used++] = (char)c;
    out[used] = '\0';
    if (used > 0 && out[used - 1] == '\n') out[used - 1] = '\0';
    pclose(pipef);
    return 0;
}

static int tpmos_share_kvp_read_text(const char *project_root, const char *key, char *out, size_t out_sz) {
    char path[TPMOS_SHARE_KVP_MAX_ROOT];
    FILE *f;
    size_t got;
    if (!out || out_sz == 0) return -1;
    out[0] = '\0';
    if (tpmos_share_kvp_mode_is_shmem(project_root) && tpmos_share_kvp_read_text_shmem(project_root, key, out, out_sz) == 0 && out[0]) return 0;
    tpmos_share_kvp_join_path(path, sizeof(path), project_root, key);
    f = fopen(path, "r");
    if (!f) return -1;
    got = fread(out, 1, out_sz - 1, f);
    out[got] = '\0';
    fclose(f);
    return 0;
}

static int tpmos_share_kvp_write_text_file(const char *project_root, const char *key, const char *value, int append) {
    char path[TPMOS_SHARE_KVP_MAX_ROOT];
    FILE *f;
    tpmos_share_kvp_join_path(path, sizeof(path), project_root, key);
    tpmos_share_kvp_mkdirs_for_file(path);
    f = fopen(path, append ? "a" : "w");
    if (!f) return -1;
    if (value && value[0]) fwrite(value, 1, strlen(value), f);
    fclose(f);
    return 0;
}

static void tpmos_share_kvp_write_text_file_async(const char *project_root, const char *key, const char *value, int append) {
    pid_t pid;
    char *root_copy;
    char *key_copy;
    char *value_copy;
    if (!project_root || !key) return;
    root_copy = tpmos_share_kvp_strdup_local(project_root);
    key_copy = tpmos_share_kvp_strdup_local(key);
    value_copy = tpmos_share_kvp_strdup_local(value ? value : "");
    if (!root_copy || !key_copy || !value_copy) {
        free(root_copy);
        free(key_copy);
        free(value_copy);
        return;
    }
    pid = fork();
    if (pid < 0) {
        free(root_copy);
        free(key_copy);
        free(value_copy);
        return;
    }
    if (pid == 0) {
        tpmos_share_kvp_write_text_file(root_copy, key_copy, value_copy, append);
        free(root_copy);
        free(key_copy);
        free(value_copy);
        _exit(0);
    }
    free(root_copy);
    free(key_copy);
    free(value_copy);
}

static void tpmos_share_kvp_copy_file_async(const char *project_root, const char *key, const char *src_path) {
    pid_t pid;
    char *root_copy;
    char *key_copy;
    char *src_copy;
    if (!project_root || !key || !src_path) return;
    root_copy = tpmos_share_kvp_strdup_local(project_root);
    key_copy = tpmos_share_kvp_strdup_local(key);
    src_copy = tpmos_share_kvp_strdup_local(src_path);
    if (!root_copy || !key_copy || !src_copy) {
        free(root_copy);
        free(key_copy);
        free(src_copy);
        return;
    }
    pid = fork();
    if (pid < 0) {
        free(root_copy);
        free(key_copy);
        free(src_copy);
        return;
    }
    if (pid == 0) {
        char dst_path[TPMOS_SHARE_KVP_MAX_ROOT];
        FILE *src;
        FILE *dst;
        unsigned char buf[65536];
        size_t got;
        tpmos_share_kvp_join_path(dst_path, sizeof(dst_path), root_copy, key_copy);
        tpmos_share_kvp_mkdirs_for_file(dst_path);
        src = fopen(src_copy, "rb");
        if (!src) _exit(1);
        dst = fopen(dst_path, "wb");
        if (!dst) {
            fclose(src);
            _exit(1);
        }
        while ((got = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, got, dst);
        fclose(src);
        fclose(dst);
        free(root_copy);
        free(key_copy);
        free(src_copy);
        _exit(0);
    }
    free(root_copy);
    free(key_copy);
    free(src_copy);
}

static int tpmos_share_kvp_write_text(const char *project_root, const char *key, const char *value) {
    int rc = 0;
    if (tpmos_share_kvp_mode_is_shmem(project_root)) rc = tpmos_share_kvp_run_write_stdin(project_root, "write-stdin", key, value ? value : "");
    if (tpmos_share_kvp_mode_is_shmem(project_root)) {
        tpmos_share_kvp_write_text_file_async(project_root, key, value ? value : "", 0);
    } else if (tpmos_share_kvp_mode_is_file(project_root) || tpmos_share_kvp_mode_is_mirror(project_root)) {
        if (tpmos_share_kvp_write_text_file(project_root, key, value ? value : "", 0) != 0 && rc == 0) rc = -1;
    }
    return rc;
}

static int tpmos_share_kvp_append_text(const char *project_root, const char *key, const char *value) {
    int rc = 0;
    if (tpmos_share_kvp_mode_is_shmem(project_root)) rc = tpmos_share_kvp_run_write_stdin(project_root, "append-stdin", key, value ? value : "");
    if (tpmos_share_kvp_mode_is_shmem(project_root)) {
        tpmos_share_kvp_write_text_file_async(project_root, key, value ? value : "", 1);
    } else if (tpmos_share_kvp_mode_is_file(project_root) || tpmos_share_kvp_mode_is_mirror(project_root)) {
        if (tpmos_share_kvp_write_text_file(project_root, key, value ? value : "", 1) != 0 && rc == 0) rc = -1;
    }
    return rc;
}

static int tpmos_share_kvp_write_file_blob(const char *project_root, const char *key, const char *src_path) {
    int rc;
    if (!tpmos_share_kvp_mode_is_shmem(project_root)) return -1;
    rc = tpmos_share_kvp_run_simple(project_root, "write-file", key, src_path);
    if (rc == 0) tpmos_share_kvp_copy_file_async(project_root, key, src_path);
    return rc;
}

static int tpmos_share_kvp_dump_async(const char *project_root) {
    if (!tpmos_share_kvp_mode_is_shmem(project_root)) return 0;
    return tpmos_share_kvp_run_simple(project_root, "dump", NULL, NULL);
}

static int tpmos_share_kvp_read_blob_direct(const char *key, unsigned char **out_data, size_t *out_size, unsigned long *out_generation) {
    TpmosShareKvpDB *db = NULL;
    int i;
    unsigned char *buf = NULL;
    if (!out_data || !out_size) return -1;
    *out_data = NULL;
    *out_size = 0;
    if (out_generation) *out_generation = 0;
    if (tpmos_share_kvp_try_attach_direct(&db) != 0) return -1;
    pthread_mutex_lock(&db->mutex);
    for (i = 0; i < TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES; i++) {
        if (db->blob_entries[i].used && strcmp(db->blob_entries[i].key, key) == 0 && db->blob_entries[i].size > 0) {
            buf = (unsigned char *)malloc(db->blob_entries[i].size);
            if (!buf) break;
            memcpy(buf, db->blob_entries[i].data, db->blob_entries[i].size);
            *out_data = buf;
            *out_size = db->blob_entries[i].size;
            if (out_generation) *out_generation = db->blob_entries[i].generation;
            pthread_mutex_unlock(&db->mutex);
            tpmos_share_kvp_detach_direct(db);
            return 0;
        }
    }
    pthread_mutex_unlock(&db->mutex);
    tpmos_share_kvp_detach_direct(db);
    return -1;
}
