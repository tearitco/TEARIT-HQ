#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "lib/tpmos_share_kvp_runtime.c"

static int tpmos_share_kvp_dump_snapshot(TpmosShareKvpDB *db) {
    TpmosShareKvpTextEntry *text_copy = NULL;
    TpmosShareKvpBlobEntry *blob_copy = NULL;
    char root[TPMOS_SHARE_KVP_MAX_ROOT];
    int i;
    pid_t pid;

    text_copy = (TpmosShareKvpTextEntry *)calloc(TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES, sizeof(TpmosShareKvpTextEntry));
    blob_copy = (TpmosShareKvpBlobEntry *)calloc(TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES, sizeof(TpmosShareKvpBlobEntry));
    if (!text_copy || !blob_copy) {
        free(text_copy);
        free(blob_copy);
        return 1;
    }

    pthread_mutex_lock(&db->mutex);
    memcpy(text_copy, db->text_entries, sizeof(TpmosShareKvpTextEntry) * TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES);
    memcpy(blob_copy, db->blob_entries, sizeof(TpmosShareKvpBlobEntry) * TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES);
    snprintf(root, sizeof(root), "%s", db->root);
    db->dump_generation++;
    pthread_mutex_unlock(&db->mutex);

    pid = fork();
    if (pid < 0) {
        free(text_copy);
        free(blob_copy);
        return 1;
    }
    if (pid > 0) {
        free(text_copy);
        free(blob_copy);
        return 0;
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

static int tpmos_share_kvp_find_text(const TpmosShareKvpDB *db, const char *key) {
    int i;
    for (i = 0; i < TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES; i++) {
        if (db->text_entries[i].used && strcmp(db->text_entries[i].key, key) == 0) return i;
    }
    return -1;
}

static int tpmos_share_kvp_find_blob(const TpmosShareKvpDB *db, const char *key) {
    int i;
    for (i = 0; i < TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES; i++) {
        if (db->blob_entries[i].used && strcmp(db->blob_entries[i].key, key) == 0) return i;
    }
    return -1;
}

static int tpmos_share_kvp_claim_text(TpmosShareKvpDB *db, const char *key) {
    int i = tpmos_share_kvp_find_text(db, key);
    if (i >= 0) return i;
    for (i = 0; i < TPMOS_SHARE_KVP_MAX_TEXT_ENTRIES; i++) {
        if (!db->text_entries[i].used) {
            db->text_entries[i].used = 1;
            snprintf(db->text_entries[i].key, sizeof(db->text_entries[i].key), "%s", key);
            db->text_entries[i].value[0] = '\0';
            return i;
        }
    }
    return -1;
}

static int tpmos_share_kvp_claim_blob(TpmosShareKvpDB *db, const char *key) {
    int i = tpmos_share_kvp_find_blob(db, key);
    if (i >= 0) return i;
    for (i = 0; i < TPMOS_SHARE_KVP_MAX_BLOB_ENTRIES; i++) {
        if (!db->blob_entries[i].used) {
            db->blob_entries[i].used = 1;
            snprintf(db->blob_entries[i].key, sizeof(db->blob_entries[i].key), "%s", key);
            db->blob_entries[i].size = 0;
            db->blob_entries[i].generation = 0;
            return i;
        }
    }
    return -1;
}

static int tpmos_share_kvp_read_stdin(char *out, size_t out_sz) {
    size_t used = 0;
    int c;
    if (!out || out_sz == 0) return -1;
    out[0] = '\0';
    while ((c = fgetc(stdin)) != EOF && used + 1 < out_sz) out[used++] = (char)c;
    out[used] = '\0';
    return 0;
}

static int tpmos_share_kvp_write_text_value(TpmosShareKvpDB *db, const char *key, const char *value, int append) {
    int idx;
    size_t cur_len;
    size_t add_len;
    idx = tpmos_share_kvp_claim_text(db, key);
    if (idx < 0) return 1;
    if (!append) db->text_entries[idx].value[0] = '\0';
    cur_len = strlen(db->text_entries[idx].value);
    add_len = strlen(value ? value : "");
    if (cur_len + add_len + (append && cur_len > 0 ? 1 : 0) >= sizeof(db->text_entries[idx].value)) return 1;
    if (append && cur_len > 0) strcat(db->text_entries[idx].value, "\n");
    if (value && value[0]) strcat(db->text_entries[idx].value, value);
    return 0;
}

static int tpmos_share_kvp_write_blob_file(TpmosShareKvpDB *db, const char *key, const char *src_path) {
    int idx;
    FILE *f;
    size_t got;
    idx = tpmos_share_kvp_claim_blob(db, key);
    if (idx < 0) return 1;
    f = fopen(src_path, "rb");
    if (!f) return 1;
    got = fread(db->blob_entries[idx].data, 1, sizeof(db->blob_entries[idx].data), f);
    fclose(f);
    db->blob_entries[idx].size = got;
    db->blob_entries[idx].generation++;
    return 0;
}

int main(int argc, char **argv) {
    TpmosShareKvpDB *db = NULL;
    const char *op;
    const char *key = NULL;
    int rc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: tpmos_share_kvp_adapter <op> [key] [path]\n");
        return 1;
    }

    op = argv[1];
    if (argc > 2) key = argv[2];

    if (tpmos_share_kvp_try_attach_direct(&db) != 0) {
        fprintf(stderr, "tpmos_share_kvp db not running\n");
        return 1;
    }

    if (strcmp(op, "dump") == 0) {
        rc = tpmos_share_kvp_dump_snapshot(db);
        tpmos_share_kvp_detach_direct(db);
        return rc;
    }

    pthread_mutex_lock(&db->mutex);

    if (strcmp(op, "read") == 0) {
        int idx = key ? tpmos_share_kvp_find_text(db, key) : -1;
        if (idx >= 0) fputs(db->text_entries[idx].value, stdout);
    } else if (strcmp(op, "write-stdin") == 0) {
        char value[TPMOS_SHARE_KVP_MAX_TEXT_VALUE];
        tpmos_share_kvp_read_stdin(value, sizeof(value));
        rc = tpmos_share_kvp_write_text_value(db, key, value, 0);
    } else if (strcmp(op, "append-stdin") == 0) {
        char value[TPMOS_SHARE_KVP_MAX_TEXT_VALUE];
        tpmos_share_kvp_read_stdin(value, sizeof(value));
        rc = tpmos_share_kvp_write_text_value(db, key, value, 1);
    } else if (strcmp(op, "write-file") == 0) {
        const char *src_path = argc > 3 ? argv[3] : NULL;
        if (!key || !src_path) rc = 1;
        else rc = tpmos_share_kvp_write_blob_file(db, key, src_path);
    } else {
        rc = 1;
    }

    pthread_mutex_unlock(&db->mutex);
    tpmos_share_kvp_detach_direct(db);
    return rc;
}
