/* sig_op - Ed25519 keygen / sign / verify for the house (design: 16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 19; owner 2026-10-27: "we will do the crypto sigs").
 * Uses OpenSSL libcrypto EVP (already linked by chain_create_wallet for SHA-256). One-shot op, no state, no network.
 *
 * Usage:  SIG_PASSPHRASE=... sig_op keygen <key_file>             writes the PRIVATE key to <key_file> (PKCS#8, AES-256-CBC, PBKDF2; mode 0600, refuses to overwrite) and prints the PUBLIC key (64 hex chars)
 *         SIG_PASSPHRASE=... sig_op sign   <key_file> <context> <message>   prints the signature (128 hex chars)
 *                          sig_op verify <pubkey_hex> <context> <message> <sig_hex>    exit 0 = valid, 1 = invalid
 *         sig_op pubkey  <key_file>  (passphrase in env)             prints the public key
 * The signed bytes are  <context> 0x00 <message>  (domain separation: a social signature can never be a payment signature, a signature for chain A is not valid on chain B when the chain id is in the
 * context/message). The passphrase comes ONLY from the environment (never argv: a process list would show it). The private key is never printed and never leaves <key_file>.
 * NOT here (chain level, section 19): nonce/replay tracking, the canonical transaction string, which wallet a public key belongs to.
 * Exit: 0 ok | 1 failed / invalid | 2 usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/sig_op.+x sig_op.c -lcrypto */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
static int hex2bin(const char *h, unsigned char *o, size_t n) { if (strlen(h) != n * 2) return 0; for (size_t i = 0; i < n; i++) { unsigned v; if (sscanf(h + 2 * i, "%2x", &v) != 1) return 0; o[i] = (unsigned char)v; } return 1; }
static void bin2hex(const unsigned char *b, size_t n, char *o) { for (size_t i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]); o[2 * n] = 0; }
static int pass_cb(char *buf, int size, int rw, void *u) { (void)rw; (void)u; const char *p = getenv("SIG_PASSPHRASE"); if (!p || !*p) return -1; int n = (int)strlen(p); if (n > size) n = size; memcpy(buf, p, (size_t)n); return n; }
static EVP_PKEY *load_priv(const char *path) { FILE *f = fopen(path, "r"); if (!f) return NULL; EVP_PKEY *k = PEM_read_PrivateKey(f, NULL, pass_cb, NULL); fclose(f); return k; }
static unsigned char *build_msg(const char *ctx, const char *msg, size_t *len) { size_t a = strlen(ctx), b = strlen(msg); unsigned char *m = malloc(a + 1 + b); if (!m) return NULL; memcpy(m, ctx, a); m[a] = 0; memcpy(m + a + 1, msg, b); *len = a + 1 + b; return m; }
static int print_pub(EVP_PKEY *k) { unsigned char pub[32]; size_t n = sizeof pub; if (EVP_PKEY_get_raw_public_key(k, pub, &n) != 1 || n != 32) return 0; char h[65]; bin2hex(pub, 32, h); printf("%s\n", h); return 1; }
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "keygen") && argc == 3) {
        if (!getenv("SIG_PASSPHRASE") || !*getenv("SIG_PASSPHRASE")) { fprintf(stderr, "sig_op: SIG_PASSPHRASE is required (environment only)\n"); return 2; }
        int fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600); if (fd < 0) { fprintf(stderr, "sig_op: cannot create %s (exists or no permission); refusing to overwrite\n", argv[2]); return 1; }
        EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL); EVP_PKEY *k = NULL; int ok = c && EVP_PKEY_keygen_init(c) == 1 && EVP_PKEY_keygen(c, &k) == 1; EVP_PKEY_CTX_free(c);
        FILE *f = ok ? fdopen(fd, "w") : NULL; if (!f) { close(fd); unlink(argv[2]); return 1; }
        int w = PEM_write_PKCS8PrivateKey(f, k, EVP_aes_256_cbc(), NULL, 0, pass_cb, NULL); fclose(f);
        if (w != 1) { unlink(argv[2]); fprintf(stderr, "sig_op: could not write the key\n"); return 1; }
        int r = print_pub(k); EVP_PKEY_free(k); return r ? 0 : 1; }
    if (argc == 3 && !strcmp(argv[1], "pubkey")) { EVP_PKEY *k = load_priv(argv[2]); if (!k) { fprintf(stderr, "sig_op: cannot read the key (wrong passphrase?)\n"); return 1; } int r = print_pub(k); EVP_PKEY_free(k); return r ? 0 : 1; }
    if (argc == 5 && !strcmp(argv[1], "sign")) {
        EVP_PKEY *k = load_priv(argv[2]); if (!k) { fprintf(stderr, "sig_op: cannot read the key (wrong passphrase?)\n"); return 1; }
        size_t ml; unsigned char *m = build_msg(argv[3], argv[4], &ml); unsigned char sig[64]; size_t sl = sizeof sig; EVP_MD_CTX *mc = EVP_MD_CTX_new();
        int ok = m && mc && EVP_DigestSignInit(mc, NULL, NULL, NULL, k) == 1 && EVP_DigestSign(mc, sig, &sl, m, ml) == 1 && sl == 64; EVP_MD_CTX_free(mc); free(m); EVP_PKEY_free(k);
        if (!ok) return 1;
        char h[129]; bin2hex(sig, 64, h); printf("%s\n", h); return 0; }
    if (argc == 6 && !strcmp(argv[1], "verify")) {
        unsigned char pub[32], sig[64]; if (!hex2bin(argv[2], pub, 32) || !hex2bin(argv[5], sig, 64)) return 1;
        EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32); if (!k) return 1;
        size_t ml; unsigned char *m = build_msg(argv[3], argv[4], &ml); EVP_MD_CTX *mc = EVP_MD_CTX_new();
        int ok = m && mc && EVP_DigestVerifyInit(mc, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(mc, sig, 64, m, ml) == 1; EVP_MD_CTX_free(mc); free(m); EVP_PKEY_free(k); return ok ? 0 : 1; }
    fprintf(stderr, "usage: SIG_PASSPHRASE=.. sig_op keygen <key_file> | sign <key_file> <context> <message> | pubkey <key_file>;  sig_op verify <pubkey_hex> <context> <message> <sig_hex>\n"); return 2;
}
