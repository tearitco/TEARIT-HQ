/* chain_create_wallet - creates a new pal-chain wallet, per
 * PAL-CHAIN-STANDARD.txt sec. 1. Real wallet_id + password login, NOT
 * an anonymous wallet_id-only scheme (direct user instruction: "wallet
 * id. password."). Password hashed with real SHA-256 (OpenSSL's
 * libcrypto, confirmed available/linkable on this system) - no salt in
 * v1 (a real, named gap, see PAL-CHAIN-STANDARD.txt sec. 7 - the
 * user's own stated risk tolerance is "sha256 or even weaker because we
 * will refund users if coins are stolen").
 *
 * Refuses if wallet_id already exists (same collision-refusal
 * convention as wsr-pal's own startup_new_corp()).
 *
 * Self-contained, no shared headers (per this family's own convention -
 * OpenSSL is an external system library, not a project header, so
 * linking it here doesn't violate that convention).
 *
 * Usage: chain_create_wallet.+x <wallet_id> <password> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <openssl/sha.h>

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

/* wallet_id restricted to safe path-component characters - it's used
 * directly as a directory name below, and this family's own ops
 * routinely shell out via system()/popen() with paths built from these
 * fields (see chain_send.c), so a wallet_id containing shell
 * metacharacters or path separators is a real injection/traversal risk,
 * not a hypothetical one. */
static int valid_wallet_id(const char *id) {
    if (!id[0]) return 0;
    for (const char *p = id; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chain_create_wallet.+x <wallet_id> <password>\n");
        return 1;
    }
    resolve_root();
    const char *wallet_id = argv[1];
    const char *password = argv[2];

    if (!valid_wallet_id(wallet_id)) {
        fprintf(stderr, "Invalid wallet_id - letters, digits, _ and - only.\n");
        return 1;
    }
    if (strlen(password) < 1) {
        fprintf(stderr, "Password must not be empty.\n");
        return 1;
    }

    char wallet_dir[PATH_BUF];
    snprintf(wallet_dir, sizeof(wallet_dir), "%s/wallets/%s", project_root, wallet_id);

    struct stat st;
    if (stat(wallet_dir, &st) == 0) {
        fprintf(stderr, "Wallet '%s' already exists.\n", wallet_id);
        return 1;
    }

    if (mkdir(wallet_dir, 0755) != 0) {
        fprintf(stderr, "Could not create wallet directory.\n");
        return 1;
    }

    char password_hash[65];
    sha256_hex(password, password_hash);

    char wallet_path[PATH_BUF];
    snprintf(wallet_path, sizeof(wallet_path), "%s/wallet.txt", wallet_dir);
    FILE *f = fopen(wallet_path, "w");
    if (!f) {
        fprintf(stderr, "Could not write wallet.txt.\n");
        return 1;
    }
    fprintf(f, "wallet_id=%s\n", wallet_id);
    fprintf(f, "password_hash=%s\n", password_hash);
    fprintf(f, "created_at=%ld\n", (long)time(NULL));
    fprintf(f, "cached_balance=0\n");
    fprintf(f, "last_processed_block=0\n");
    fclose(f);

    printf("Wallet '%s' created.\n", wallet_id);
    return 0;
}
