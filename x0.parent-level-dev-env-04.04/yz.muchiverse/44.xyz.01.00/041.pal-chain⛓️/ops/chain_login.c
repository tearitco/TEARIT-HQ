/* chain_login - verifies wallet_id + password (SHA-256 compare against
 * wallets/<wallet_id>/wallet.txt's own password_hash, per
 * PAL-CHAIN-STANDARD.txt sec. 1), and on success writes
 * net/session.txt naming who is now "logged in" on THIS node - the
 * chtpm UI layer (chain_menu_input.c/chain_compose_frame.c) reads that
 * file to know which wallet's own screens to show. On failure, writes
 * nothing to session.txt and returns a real nonzero exit code (checked
 * by the caller, not just its stdout text).
 *
 * Self-contained, no shared headers.
 * Usage: chain_login.+x <wallet_id> <password> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/sha.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void sha256_hex(const char *input, char out_hex[65]) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), digest);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(out_hex + i * 2, 3, "%02x", digest[i]);
    }
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chain_login.+x <wallet_id> <password>\n");
        return 1;
    }
    resolve_root();
    const char *wallet_id = argv[1];
    const char *password = argv[2];

    char wallet_path[PATH_BUF];
    snprintf(wallet_path, sizeof(wallet_path), "%s/wallets/%s/wallet.txt", project_root, wallet_id);

    char stored_hash[128];
    read_kv_str(wallet_path, "password_hash", stored_hash, sizeof(stored_hash));
    if (!stored_hash[0]) {
        fprintf(stderr, "No such wallet.\n");
        return 1;
    }

    char entered_hash[65];
    sha256_hex(password, entered_hash);

    if (strcmp(stored_hash, entered_hash) != 0) {
        fprintf(stderr, "Incorrect password.\n");
        return 1;
    }

    char session_path[PATH_BUF];
    snprintf(session_path, sizeof(session_path), "%s/net/session.txt", project_root);
    FILE *f = fopen(session_path, "w");
    if (f) {
        fprintf(f, "wallet_id=%s\n", wallet_id);
        fprintf(f, "logged_in_at=%ld\n", (long)time(NULL));
        fclose(f);
    }

    printf("Logged in as '%s'.\n", wallet_id);
    return 0;
}
