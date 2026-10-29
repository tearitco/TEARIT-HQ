/* hash_op - SHA-256 as a tiny house op, so no program needs a hash header or its own copy (owner 2026-10-28: "prefer ops/ events/ pals", not shared headers).
 * Uses OpenSSL libcrypto EVP, already linked by sig_op and chain_create_wallet. One-shot, no state, no network, no writes.
 *   hash_op sha256 <text>            prints the 64-hex SHA-256 of the argument bytes
 *   hash_op sha256 --file <path>     prints the 64-hex SHA-256 of the file
 *   hash_op wallet <uid>             prints the wallet id of a piece: `e` + the first 24 hex of SHA-256("wallet" 0x00 <uid>)  (owner 2026-10-28: derived BY HASH always, so a uid of any length 8-64 hex works;
 *                                    this replaces "e + first 24 hex of the uid" of CHAIN-ECONOMY-DESIGN section 3 for pieces). <uid> must be 8-64 lowercase hex.
 * Exit: 0 ok | 1 unreadable file / bad uid | 2 usage.   Build: sh build_hash_op.sh  (links -lcrypto) */
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

static int digest(const unsigned char *m, size_t n, const unsigned char *pre, size_t pn, char *hex) {
    unsigned char d[EVP_MAX_MD_SIZE]; unsigned dl = 0; EVP_MD_CTX *c = EVP_MD_CTX_new(); if (!c) return 0;
    int ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 && (!pn || EVP_DigestUpdate(c, pre, pn) == 1) && EVP_DigestUpdate(c, m, n) == 1 && EVP_DigestFinal_ex(c, d, &dl) == 1;
    EVP_MD_CTX_free(c); if (!ok || dl != 32) return 0;
    for (unsigned i = 0; i < dl; i++) sprintf(hex + 2 * i, "%02x", d[i]);
    return 1;
}
static int uid_ok(const char *s) { size_t n = strlen(s); if (n < 8 || n > 64) return 0; for (size_t i = 0; i < n; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0; return 1; }
int main(int argc, char **argv) {
    char hex[65];
    if (argc == 3 && !strcmp(argv[1], "sha256")) { if (!digest((const unsigned char *)argv[2], strlen(argv[2]), NULL, 0, hex)) return 1; printf("%s\n", hex); return 0; }
    if (argc == 4 && !strcmp(argv[1], "sha256") && !strcmp(argv[2], "--file")) {
        FILE *f = fopen(argv[3], "rb"); if (!f) { fprintf(stderr, "hash_op: cannot read %s\n", argv[3]); return 1; }
        EVP_MD_CTX *c = EVP_MD_CTX_new(); unsigned char b[65536], d[EVP_MAX_MD_SIZE]; unsigned dl = 0; size_t r; int ok = c && EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1;
        while (ok && (r = fread(b, 1, sizeof b, f)) > 0) ok = EVP_DigestUpdate(c, b, r) == 1;
        if (ferror(f)) { ok = 0; }
        ok = ok && EVP_DigestFinal_ex(c, d, &dl) == 1;
        if (c) { EVP_MD_CTX_free(c); }
        fclose(f);
        if (!ok) { return 1; }
        for (unsigned i = 0; i < dl; i++) { printf("%02x", d[i]); }
        printf("\n");
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "wallet")) {
        if (!uid_ok(argv[2])) { fprintf(stderr, "hash_op: uid must be 8-64 lowercase hex\n"); return 1; }
        if (!digest((const unsigned char *)argv[2], strlen(argv[2]), (const unsigned char *)"wallet", 7, hex)) return 1;
        printf("e%.24s\n", hex); return 0; }
    fprintf(stderr, "Usage: hash_op sha256 <text> | hash_op sha256 --file <path> | hash_op wallet <uid>\n"); return 2;
}
