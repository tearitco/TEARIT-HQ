/* khtpm_phone.c - give every entity a phone (an item entity at <entity>/inventory/zz.phone/) and a permanent identity.
 *
 * Text-included canonical helper (same family as khtpm_inventory.c / khtpm_locations.c): pure file I/O, no drawing, no environment
 * policy, static + unused-tolerant. Prefix: ph_.  Design: 18.../08-roadmap/design-docs/HAI-ROBOTS-PHONES-SERVER-DESIGN.md sec 3, 3b, 3c.
 * Quest: ^.grave/quests/Q005-phone-in-every-inventory.
 *
 * IDENTITY (design 3c). Each entity gets <entity>/entity_uid.txt, written ONCE and never changed:
 *   - if pal.pdl already has "PAL | hash | <sha256>" (the house's own pal identity, "NFT-ready"), that hash is FROZEN as the uid
 *     (continuity with the identity the house already designed; livedesk_ensure_pal recomputes the pal.pdl hash as a content hash of the
 *     whole folder, so pal.pdl's own copy drifts, the frozen uid does not);
 *   - otherwise a random sha256 (sha256 of 32 bytes of /dev/urandom).
 *   entity_hash = sha256(uid text); phone number = 11 decimal digits of a 64-bit slice of entity_hash, shown NNN-NNNN-NNNN (slice 0..3 on
 *   a collision); wallet_id = "e" + first 24 hex of entity_hash (valid chain wallet_id charset; no wallet is created here).
 *
 * PHONE FILES (phone.pdl, inbox.txt, outbox.txt, history.txt, pal.pdl, glyph.txt) are created only if the phone dir is missing; an existing
 * phone is never touched. The item is named zz.phone so it sorts last in inventory slot order (khtpm_inventory.c: alphabetical) and never
 * shifts an existing inventory_slot.txt. Recursion: every <entity>/inventory/<child> that is itself an entity (has pal.pdl) gets the same.
 *
 * ph_ensure(dir, ctx, depth) with ctx->apply == 0 changes NOTHING (dry run): it only counts and reports.
 */
#ifndef KHTPM_PHONE_C
#define KHTPM_PHONE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PH_UNUSED __attribute__((unused))
#define PH_PATH 4096
#define PH_BUF (PH_PATH + 256)   /* every path buffer carries headroom for a suffix */
#define PH_PHONE_DIR "zz.phone"
#define PH_MAX_NUMBERS 8192

/* ---- SHA-256 (compact, FIPS 180-4) ---- */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t n; } PhSha;
static const uint32_t PH_K[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
 0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
 0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
 0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define PH_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static PH_UNUSED void ph_sha_block(PhSha *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h; int i;
    for (i = 0; i < 16; i++) w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) | ((uint32_t)p[i*4+2] << 8) | p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = PH_ROR(w[i-15], 7) ^ PH_ROR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = PH_ROR(w[i-2], 17) ^ PH_ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = PH_ROR(e, 6) ^ PH_ROR(e, 11) ^ PH_ROR(e, 25), ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + PH_K[i] + w[i];
        uint32_t S0 = PH_ROR(a, 2) ^ PH_ROR(a, 13) ^ PH_ROR(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static PH_UNUSED void ph_sha_hex(const unsigned char *data, size_t len, char out[65]) {
    PhSha s; size_t i; unsigned char pad[128]; size_t padn; uint64_t bits = (uint64_t)len * 8;
    s.h[0]=0x6a09e667; s.h[1]=0xbb67ae85; s.h[2]=0x3c6ef372; s.h[3]=0xa54ff53a; s.h[4]=0x510e527f; s.h[5]=0x9b05688c; s.h[6]=0x1f83d9ab; s.h[7]=0x5be0cd19;
    for (i = 0; i + 64 <= len; i += 64) ph_sha_block(&s, data + i);
    padn = len - i; memcpy(pad, data + i, padn); pad[padn++] = 0x80;
    while ((padn % 64) != 56) pad[padn++] = 0;
    for (int k = 7; k >= 0; k--) pad[padn++] = (unsigned char)(bits >> (k * 8));
    for (size_t j = 0; j < padn; j += 64) ph_sha_block(&s, pad + j);
    for (i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", s.h[i]);
    out[64] = '\0';
}

/* ---- small file helpers ---- */
/* out = a + "/" + b, or 0 (out empty) if it does not fit: no silent truncation */
static PH_UNUSED int ph_join(char *out, size_t n, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la + 1 + lb + 1 > n) { out[0] = '\0'; return 0; }
    memcpy(out, a, la); out[la] = '/'; memcpy(out + la + 1, b, lb + 1); return 1;
}
static PH_UNUSED int ph_exists(const char *p) { return access(p, F_OK) == 0; }
static PH_UNUSED int ph_is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static PH_UNUSED int ph_read_line(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "r"); out[0] = '\0'; if (!f) return 0;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f); return out[0] != '\0';
}
static PH_UNUSED int ph_write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w"); if (!f) return 0; fputs(text, f); fclose(f); return 1;
}
/* "PAL | <key> | <value>" from <dir>/pal.pdl */
static PH_UNUSED int ph_pal_field(const char *dir, const char *key, char *out, size_t n) {
    char p[PH_BUF], line[1024], want[96]; FILE *f;
    out[0] = '\0'; ph_join(p, sizeof(p), dir, "pal.pdl"); snprintf(want, sizeof(want), "PAL | %s | ", key);
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) if (!strncmp(line, want, strlen(want))) {
        snprintf(out, n, "%s", line + strlen(want)); out[strcspn(out, "\r\n")] = '\0'; break; }
    fclose(f); return out[0] != '\0';
}
static PH_UNUSED void ph_random_uid(char out[65]) {
    unsigned char r[32]; FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(r, 1, sizeof(r), f) != sizeof(r)) { for (int i = 0; i < 32; i++) r[i] = (unsigned char)rand(); }
    if (f) fclose(f);
    ph_sha_hex(r, sizeof(r), out);
}

/* ---- derived identity ---- */
static PH_UNUSED void ph_entity_hash(const char *uid, char out[65]) { ph_sha_hex((const unsigned char *)uid, strlen(uid), out); }
static PH_UNUSED void ph_wallet_id(const char *entity_hash, char *out, size_t n) { snprintf(out, n, "e%.24s", entity_hash); }
/* 11 decimal digits from a 16-hex slice (slice 0..3) of the entity hash, formatted NNN-NNNN-NNNN */
static PH_UNUSED void ph_number(const char *entity_hash, int slice, char *out, size_t n) {
    char part[17]; unsigned long long v; unsigned long long d;
    memcpy(part, entity_hash + (slice & 3) * 16, 16); part[16] = '\0';
    v = strtoull(part, NULL, 16) % 100000000000ULL;
    d = v;
    snprintf(out, n, "%03llu-%04llu-%04llu", d / 100000000ULL, (d / 10000ULL) % 10000ULL, d % 10000ULL);
}

typedef struct {
    int apply;                       /* 0 = dry run: nothing is written */
    FILE *report;                    /* one line per entity, or NULL */
    char index_path[PH_BUF];        /* phones.index (append-only) */
    char template_dir[PH_BUF];      /* optional: ^.hai-phone/_TEMPLATE; sprite.csv + atlas.png are copied into every phone that lacks them (the hotbar draws an item's sprite; the emoji glyph can be an empty box) */
    int n_sprite_added, n_sprite_missing;
    char numbers[PH_MAX_NUMBERS][16];/* numbers already issued (index + this run) */
    int n_numbers;
    int n_entities, n_nested, n_have_phone, n_new_phone, n_have_uid, n_new_uid_from_hash, n_new_uid_random, n_errors;
} PhCtx;

static PH_UNUSED int ph_number_taken(PhCtx *c, const char *num) {
    for (int i = 0; i < c->n_numbers; i++) if (!strcmp(c->numbers[i], num)) return 1;
    return 0;
}
static PH_UNUSED void ph_load_index(PhCtx *c) {
    FILE *f = fopen(c->index_path, "r"); char line[PH_BUF]; if (!f) return;
    while (fgets(line, sizeof(line), f) && c->n_numbers < PH_MAX_NUMBERS) {
        char *bar = strchr(line, '|'); if (!bar) continue; *bar = '\0';
        if (strlen(line) == 13) snprintf(c->numbers[c->n_numbers++], 16, "%s", line);
    }
    fclose(f);
}

/* copy src -> dst only if dst does not exist; 1 = copied */
static PH_UNUSED int ph_copy_if_missing(const char *src, const char *dst) {
    FILE *in, *out; char buf[8192]; size_t n; int ok = 1;
    if (ph_exists(dst) || !(in = fopen(src, "rb"))) return 0;
    if (!(out = fopen(dst, "wb"))) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in); if (fclose(out) != 0) ok = 0;
    if (!ok) remove(dst);
    return ok;
}
static PH_UNUSED const char *ph_base(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

static PH_UNUSED void ph_ensure(const char *dir, PhCtx *c, int depth) {
    char uid[128], upath[PH_BUF], phone[PH_BUF], inv0[PH_BUF], label[256], src[32] = "existing", hash[65], number[16] = "-", wallet[40] = "-", note[64] = "";
    int have_uid, have_phone, slice;
    c->n_entities++; if (depth > 0) c->n_nested++;
    ph_join(upath, sizeof(upath), dir, "instance_id.txt");
    if (!ph_read_line(upath, label, sizeof(label))) snprintf(label, sizeof(label), "%s", ph_base(dir));
    ph_join(upath, sizeof(upath), dir, "entity_uid.txt");
    have_uid = ph_read_line(upath, uid, sizeof(uid));
    if (have_uid) c->n_have_uid++;
    else {
        char palhash[128];
        if (ph_pal_field(dir, "hash", palhash, sizeof(palhash)) && strlen(palhash) == 64) { snprintf(uid, sizeof(uid), "%s", palhash); snprintf(src, sizeof(src), "frozen pal hash"); c->n_new_uid_from_hash++; }
        else { ph_random_uid(uid); snprintf(src, sizeof(src), "random"); c->n_new_uid_random++; }
        if (c->apply) { char tmp[160]; snprintf(tmp, sizeof(tmp), "%s\n", uid); if (!ph_write_file(upath, tmp)) c->n_errors++; }
    }
    ph_entity_hash(uid, hash);
    ph_wallet_id(hash, wallet, sizeof(wallet));
    ph_join(inv0, sizeof(inv0), dir, "inventory"); ph_join(phone, sizeof(phone), inv0, PH_PHONE_DIR);
    have_phone = ph_exists(phone);
    if (have_phone) {
        c->n_have_phone++; snprintf(note, sizeof(note), "has phone");
        { char pp[PH_BUF], pl[128]; ph_join(pp, sizeof(pp), phone, "phone.pdl"); if (ph_read_line(pp, pl, sizeof(pl))) snprintf(number, sizeof(number), "(kept)"); }
    } else {
        for (slice = 0; slice < 4; slice++) { ph_number(hash, slice, number, sizeof(number)); if (!ph_number_taken(c, number)) break; }
        if (slice == 4) { snprintf(number, sizeof(number), "COLLISION"); c->n_errors++; }
        c->n_new_phone++; snprintf(note, sizeof(note), c->apply ? "phone created" : "would create phone");
        if (c->apply && strcmp(number, "COLLISION")) {
            char p[PH_BUF], body[2048], inv[PH_BUF]; FILE *ix;
            ph_join(inv, sizeof(inv), dir, "inventory"); mkdir(inv, 0755); mkdir(phone, 0755);
            snprintf(body, sizeof(body), "SECTION      | KEY                | VALUE\n----------------------------------------\n"
                "PHONE        | number               | %s\nPHONE        | owner_uid            | %s\nPHONE        | owner_label          | %s\n"
                "PHONE        | glyph                | \xF0\x9F\x93\xB1\nPHONE        | state                | idle\nPHONE        | created              | migration\n"
                "PHONE        | created_by           | phone_ensure_op\nPHONE        | history_max_lines    | 2000\nPHONE        | history_max_bytes    | 262144\n"
                "PHONE        | rotate_keep          | 3\n", number, uid, label);
            ph_join(p, sizeof(p), phone, "phone.pdl"); if (!ph_write_file(p, body)) c->n_errors++;
            { char phash[65], seed[200]; snprintf(seed, sizeof(seed), "phone:%s", uid); ph_sha_hex((const unsigned char *)seed, strlen(seed), phash);
              snprintf(body, sizeof(body), "PAL | name | %s\nPAL | hash | %s\nPAL | glyph | \xF0\x9F\x93\xB1\n", PH_PHONE_DIR, phash);
              ph_join(p, sizeof(p), phone, "pal.pdl"); ph_write_file(p, body); }
            ph_join(p, sizeof(p), phone, "glyph.txt"); ph_write_file(p, "\xF0\x9F\x93\xB1\n");
            ph_join(p, sizeof(p), phone, "inbox.txt");   ph_write_file(p, "# append-only, written only by the server.  <epoch_ms>|<from>|<to>|<kind>|<ref>|<text>\n");
            ph_join(p, sizeof(p), phone, "outbox.txt");  ph_write_file(p, "# append-only, written only by the phone's owner.  <epoch_ms>|<from>|<to>|<kind>|<ref>|<text>\n");
            ph_join(p, sizeof(p), phone, "history.txt"); ph_write_file(p, "# merged readable conversation, written by the server.\n");
            if ((ix = fopen(c->index_path, "a"))) { fprintf(ix, "%s|%s|%s|%s|%s\n", number, uid, label, dir, phone); fclose(ix); }
            else c->n_errors++;
            if (c->n_numbers < PH_MAX_NUMBERS) snprintf(c->numbers[c->n_numbers++], 16, "%s", number);
        } else if (!c->apply && c->n_numbers < PH_MAX_NUMBERS && strcmp(number, "COLLISION")) {
            snprintf(c->numbers[c->n_numbers++], 16, "%s", number);   /* reserve in the preview so two previews cannot collide */
        }
    }
    if (c->template_dir[0] && (c->apply ? ph_exists(phone) : have_phone)) {   /* the phone's picture: copy-if-missing, never overwrites */
        static const char *spr[2] = { "sprite.csv", "atlas.png" };
        for (int k = 0; k < 2; k++) {
            char src[PH_BUF], dst[PH_BUF];
            if (!ph_join(src, sizeof(src), c->template_dir, spr[k]) || !ph_join(dst, sizeof(dst), phone, spr[k]) || !ph_exists(src) || ph_exists(dst)) continue;
            if (c->apply) { if (ph_copy_if_missing(src, dst)) c->n_sprite_added++; else c->n_errors++; } else c->n_sprite_missing++;
        }
    }
    {   char pp[PH_BUF]; ph_join(pp, sizeof(pp), dir, "pal.pdl");
        if (!ph_exists(pp)) { size_t l = strlen(note); snprintf(note + l, sizeof(note) - l, " [NO pal.pdl]"); }
    }
    if (c->report)
        fprintf(c->report, "%-3s %-28s uid:%-16s number:%-14s wallet:%-26s %s\n", depth ? "  >" : "", label, have_uid ? "existing" : src, number, wallet, note);
    {   /* recurse into inventory items that are entities */
        char inv[PH_BUF]; DIR *d; struct dirent *e;
        ph_join(inv, sizeof(inv), dir, "inventory");
        if ((d = opendir(inv))) {
            while ((e = readdir(d))) {
                char child[PH_BUF], cp[PH_BUF];
                if (e->d_name[0] == '.' || !strcmp(e->d_name, PH_PHONE_DIR)) continue;
                if (!ph_join(child, sizeof(child), inv, e->d_name)) continue;
                ph_join(cp, sizeof(cp), child, "pal.pdl");
                if (ph_is_dir(child) && ph_exists(cp)) ph_ensure(child, c, depth + 1);
            }
            closedir(d);
        }
    }
}

#endif
