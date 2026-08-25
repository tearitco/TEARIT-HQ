#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "lib/tpmos_share_kvp_runtime.c"

#include <limits.h>

static volatile sig_atomic_t g_stop = 0;
static int g_shm_fd = -1;
static TpmosShareKvpDB *g_db = NULL;

static void tpmos_share_kvp_signal_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

static void tpmos_share_kvp_snapshot_and_dump(const char *root) {
    TpmosShareKvpTextEntry *text_copy = NULL;
    TpmosShareKvpBlobEntry *blob_copy = NULL;
    int i;

    text_copy = (TpmosShareKvpTextEntry *)calloc(TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES, sizeof(TpmosShareKvpTextEntry));
    blob_copy = (TpmosShareKvpBlobEntry *)calloc(TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES, sizeof(TpmosShareKvpBlobEntry));
    if (!text_copy || !blob_copy) {
        free(text_copy);
        free(blob_copy);
        return;
    }

    pthread_mutex_lock(&g_db->mutex);
    memcpy(text_copy, g_db->text_entries, sizeof(TpmosShareKvpTextEntry) * TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES);
    memcpy(blob_copy, g_db->blob_entries, sizeof(TpmosShareKvpBlobEntry) * TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES);
    g_db->dump_generation++;
    pthread_mutex_unlock(&g_db->mutex);

    if (fork() != 0) {
        free(text_copy);
        free(blob_copy);
        return;
    }

    for (i = 0; i < TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES; i++) {
        char out_path[TPMOS_SHARE_KVP_MAX_ROOT * 2];
        FILE *f;
        if (!text_copy[i].used || !text_copy[i].key[0]) continue;
        tpmos_share_kvp_join_path(out_path, sizeof(out_path), root, text_copy[i].key);
        tpmos_share_kvp_mkdirs_for_file(out_path);
        f = fopen(out_path, "w");
        if (!f) continue;
        fwrite(text_copy[i].value, 1, strlen(text_copy[i].value), f);
        fclose(f);
    }

    for (i = 0; i < TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES; i++) {
        char out_path[TPMOS_SHARE_KVP_MAX_ROOT * 2];
        FILE *f;
        if (!blob_copy[i].used || !blob_copy[i].key[0] || blob_copy[i].size == 0) continue;
        tpmos_share_kvp_join_path(out_path, sizeof(out_path), root, blob_copy[i].key);
        tpmos_share_kvp_mkdirs_for_file(out_path);
        f = fopen(out_path, "wb");
        if (!f) continue;
        fwrite(blob_copy[i].data, 1, blob_copy[i].size, f);
        fclose(f);
    }

    free(text_copy);
    free(blob_copy);
    _exit(0);
}

static int tpmos_share_kvp_init(const char *root) {
    pthread_mutexattr_t attr;
    int created = 0;

    g_shm_fd = shm_open(TPMOS_SHARE_KVP_SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (g_shm_fd >= 0) {
        created = 1;
        if (ftruncate(g_shm_fd, sizeof(TpmosShareKvpDB)) != 0) return -1;
    } else {
        g_shm_fd = shm_open(TPMOS_SHARE_KVP_SHM_NAME, O_RDWR, 0666);
        if (g_shm_fd < 0) return -1;
    }

    g_db = (TpmosShareKvpDB *)mmap(NULL, sizeof(TpmosShareKvpDB), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_db == MAP_FAILED) return -1;

    if (created || !g_db->initialized) {
        memset(g_db, 0, sizeof(*g_db));
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&g_db->mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        g_db->initialized = 1;
    }

    pthread_mutex_lock(&g_db->mutex);
    g_db->server_pid = getpid();
    snprintf(g_db->root, sizeof(g_db->root), "%s", root && root[0] ? root : ".");
    pthread_mutex_unlock(&g_db->mutex);
    return 0;
}

static void tpmos_share_kvp_shutdown(void) {
    if (g_db && g_db != MAP_FAILED) {
        tpmos_share_kvp_snapshot_and_dump(g_db->root);
        munmap(g_db, sizeof(TpmosShareKvpDB));
        g_db = NULL;
    }
    if (g_shm_fd >= 0) {
        close(g_shm_fd);
        g_shm_fd = -1;
        shm_unlink(TPMOS_SHARE_KVP_SHM_NAME);
    }
}

int main(int argc, char **argv) {
    const char *root = ".";
    int foreground = 0;
    unsigned long last_dump_request = 0;
    if (argc > 1 && argv[1][0]) root = argv[1];
    if (argc > 2 && strcmp(argv[2], "--foreground") == 0) foreground = 1;

    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) return 1;
        if (pid > 0) return 0;
        if (setsid() < 0) return 1;
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "a", stdout);
        freopen("/dev/null", "a", stderr);
    }

    signal(SIGTERM, tpmos_share_kvp_signal_handler);
    signal(SIGINT, tpmos_share_kvp_signal_handler);

    if (tpmos_share_kvp_init(root) != 0) {
        fprintf(stderr, "tpmos_share_kvp_db init failed\n");
        return 1;
    }

    pthread_mutex_lock(&g_db->mutex);
    last_dump_request = g_db->dump_request_generation;
    pthread_mutex_unlock(&g_db->mutex);

    while (!g_stop) {
        unsigned long request_generation;
        pthread_mutex_lock(&g_db->mutex);
        request_generation = g_db->dump_request_generation;
        pthread_mutex_unlock(&g_db->mutex);
        if (request_generation != last_dump_request) {
            last_dump_request = request_generation;
            tpmos_share_kvp_snapshot_and_dump(g_db->root);
        }
        usleep(200000);
    }

    tpmos_share_kvp_shutdown();
    return 0;
}
