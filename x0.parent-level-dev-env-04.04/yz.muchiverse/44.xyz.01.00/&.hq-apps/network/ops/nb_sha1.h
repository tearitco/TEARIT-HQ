/* nb_sha1.h — generic SHA-1 (RFC 3174 / FIPS 180-1) + base64-encoder for the
 * 20-byte digest.  Plain C, no dependency.  Header so BOTH the worker (which
 * exposes __nb_sha1 to page JS) and hermetic fixture servers (which recompute
 * a received SAPISIDHASH signature) use the identical implementation — a
 * page/fixture mismatch then proves the JS-side composition, not the crypto.
 * Algorithm correctness is pinned by openssl vectors in worker_sapisid_test.
 */
#ifndef NB_SHA1_H
#define NB_SHA1_H

#include <stdint.h>
#include <string.h>

typedef struct { uint32_t st[5]; uint64_t len; uint8_t buf[64]; size_t nbuf; } NbSha1;

static void nbsha1_block(NbSha1 *s, const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 80; i++) {
        uint32_t x = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (x << 1) | (x >> 31);
    }
    uint32_t a = s->st[0], b = s->st[1], c = s->st[2], d = s->st[3], e = s->st[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);                k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;                         k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);       k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;                         k = 0xCA62C1D6u; }
        uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
    }
    s->st[0] += a; s->st[1] += b; s->st[2] += c; s->st[3] += d; s->st[4] += e;
    s->nbuf = 0;
}

static void nbsha1_init(NbSha1 *s) {
    s->st[0] = 0x67452301u; s->st[1] = 0xEFCDAB89u;
    s->st[2] = 0x98BADCFEu; s->st[3] = 0x10325476u; s->st[4] = 0xC3D2E1F0u;
    s->len = 0; s->nbuf = 0;
}

static void nbsha1_update(NbSha1 *s, const uint8_t *p, size_t n) {
    s->len += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - s->nbuf;
        if (take > n) take = n;
        memcpy(s->buf + s->nbuf, p, take);
        s->nbuf += take; p += take; n -= take;
        if (s->nbuf == 64) nbsha1_block(s, s->buf);
    }
}

static void nbsha1_final(NbSha1 *s, uint8_t out[20]) {
    uint64_t l = s->len;
    uint8_t pad = 0x80;
    nbsha1_update(s, &pad, 1);
    uint8_t z = 0;
    while (s->nbuf != 56) nbsha1_update(s, &z, 1);
    uint8_t lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(l >> (56 - i * 8));
    nbsha1_update(s, lb, 8);
    for (int i = 0; i < 5; i++) {
        out[i*4]   = (uint8_t)(s->st[i] >> 24);
        out[i*4+1] = (uint8_t)(s->st[i] >> 16);
        out[i*4+2] = (uint8_t)(s->st[i] >> 8);
        out[i*4+3] = (uint8_t)s->st[i];
    }
}

/* convenience: one-shot digest of `data` into out[20] */
static void nbsha1(const uint8_t *data, size_t n, uint8_t out[20]) {
    NbSha1 s; nbsha1_init(&s);
    nbsha1_update(&s, data, n);
    nbsha1_final(&s, out);
}

/* base64 of the 20-byte digest into out (28 chars incl '=' + NUL, fits 29) */
static const char NB_B64C[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void nbsha1_b64_20(const uint8_t in[20], char out[29]) {
    int o = 0;
    for (int i = 0; i < 20; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) |
                     ((uint32_t)(i+1 < 20 ? in[i+1] : 0) << 8) |
                     (uint32_t)(i+2 < 20 ? in[i+2] : 0);
        out[o++] = NB_B64C[(v >> 18) & 63];
        out[o++] = NB_B64C[(v >> 12) & 63];
        out[o++] = (i+1 < 20) ? NB_B64C[(v >> 6) & 63] : '=';
        out[o++] = (i+2 < 20) ? NB_B64C[v & 63] : '=';
    }
    out[o] = 0;
}

#endif /* NB_SHA1_H */