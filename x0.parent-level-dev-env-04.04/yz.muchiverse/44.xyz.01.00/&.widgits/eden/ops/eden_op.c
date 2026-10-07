/* eden_op - the one compiled piece of the Eden conductor v0 (GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md sections 1-5, 7).
 *
 * Why it exists: the house event pages cannot compare variables, do arithmetic with a tunable, roll a seeded die, walk a list of rows or spawn a folder
 * (see digipet / ring-board README gap lists), so every Eden rule that needs one of those is a verb here. EVERY NUMBER IS DATA (owner rule "fluid, pdl read"):
 * tunables.pdl, weights.pdl, weather.pdl, crops.pdl, items.pdl, skills.pdl, nodes.pdl, kits.pdl, game.pdl, phrases.pdl. Nothing here is a game constant
 * except the file layout and the hash that makes the dice reproducible.
 *
 * Usage:  eden_op <conductor_dir> <verb> [args]      (pages run with cwd = the conductor dir, so a page line is `exec ops/+x/eden_op.+x . <verb> ...`)
 *   setup                 read game.pdl, spawn each PARTICIPANT as a SCRATCH entity from participant_template/ (fresh identity ED<n>, never a copied hash or uid),
 *                         apply its kit as append-only GRANT rows (idempotent: a triple already granted is skipped), install the lc_clock schedule row once
 *   start | pause | resume   running flag + the lc_clock commands (rate / pause / resume through the mailbox)
 *   reset                 take back EXACTLY the granted items / skills / params (REVOKE rows; items clamp at what the entity still holds); own data untouched
 *   add [name]            spawn another participant (default: the next SPARE row of game.pdl) from the template and apply its kit
 *   remove [name]         revoke the grants and append a RETIRE tombstone row (default: the newest participant); the entity folder is left in place
 *   daytick               ONE game day (the lc_clock event common:eden_day_tick runs this): day+1, weather roll, then per participant hunger, crop growth, one agent act
 *   acts <who>            print the acts the entity can take now (stored skills + skills granted by held items, needs_item, rule preconditions); writes <who>/acts.txt
 *   menu <who>            write <who>/acts_menu.pdl: METHOD rows `Act: <act>` generated from that list
 *   act <who> <act>       take one act by hand (a human seat); same code as the agent seat
 *   save <n> | load <n>   game_snapshot_op over the game tree with the clock state / reminders / schedule ledger as --extra; load = restore --apply --prune --extra-dir
 *   status                writes status.txt (and prints it)
 *   audit                 independent re-derivation from eden_history.txt: item sums, no negative item, hunger and hp in bounds, food conserved (prints AUDIT|ok=1 or the problems)
 *   digest                one hash over the game tree (sorted paths + contents; skips event_page_ledger/out, ops/, and the install files wiring.pdl / install.txt):
 *                         "did this tree change?"
 *   toyread <file> <key>  the taskbar's read_key_value (khtpm_taskbar_manager.c) copied verbatim in behavior, to check a toy.pdl without a GUI
 * v1 PLAYABLE LOOP (EDEN-PLAYABLE-LOOP-AND-BOOK-PAGE-HARNESS-PLAN.md section 2), all numbers in game.pdl / animals.pdl / evolution.pdl / finds.pdl / tunables.pdl:
 *   begin                 "Start game": setup (idempotent) + the PLAYKIT grants + spawn the game.pdl WORLD rows (house, chickens, garden plots) from world_templates/ with fresh
 *                         ED<n> identities, every spawn a SPAWN row of manifest.txt; start the clock. Twice = no duplicates (a name that is live in the manifest is skipped).
 *   stop                  "Stop game": pause the clock, then despawn ONLY the live entities of manifest.txt (DESPAWN rows; participants get their grants revoked and their items
 *                         written off first so the ledger balances); a folder that is not in the manifest is never touched. A later begin spawns fresh ones.
 *   daemon-start | daemon-stop | daemon-status   the lc_clock daemon (`<G>/lc.+x <G> daemon`, fork+setsid+exec, env from wiring prisc / event_runner), pid in wiring daemon_pid (default ../../eden_daemon.pid),
 *                         DAEMON rows in the control ledger; start twice = one process; stop = SIGTERM to the recorded pid only. `WIRING | daemon | 1` makes Start game / Resume start it and Stop game / Pause stop it.
 *   nextday               "Next day": `lc_clock cmd g1 advance 1d` + `step 0` = exactly one Day Tick (with a daemon running: only the advance, the daemon fires it)
 *   days <n>              n x daytick (the harness's 30 days; same code the clock event runs)
 *   total <item>          sum of an item over the live participants;   count   the live manifest entities by kind
 *   save <n> | resave <n> | load <n>   n = 1..max_slots (tunables.pdl, default 10); slots.pdl index (outside the tree, append-only SLOT rows, revision per resave); save refuses a used slot, resave adds a revision
 *   slot [n|next|prev|+10|-10]  the slot.txt cursor;  save-slot | resave-slot | load-slot   act on the cursor (for menus that cannot take an argument)
 *   slots                 latest index row per slot;   gen-slots <toolbar|methods>   print the generated slot menu (2 x max_slots rows) (toolbar_slots.pdl / slots_methods.pdl)
 *   acts added in v1: feed (a chicken, grain), dig (spade, finds.pdl, seeded), repair (the house, clay); trade also sells an item for coin (items.pdl value=);
 *   talk asks the tomom chatbot on a scratch copy (wiring tomom_bin / tomom_dir) and falls back to phrases.pdl; the path used is logged (path=tomom|phrase).
 * Randomness: splitmix-style integer hash of (seed, day, participant ordinal, salt) - no clock, no rand(), so a restored game rolls the same next dice.
 * Ledgers (append-only, no wall time, so runs compare byte for byte): eden_history.txt (DAY WEATHER NEED GROW RIPE ACT ITEM UNLOCK SETUP RESET JOIN),
 * grants.txt (GRANT / REVOKE), participants.txt (JOIN / RETIRE), and the control ledger outside the game tree (wiring `control_ledger`).
 * Errors: a pal exec hides the exit status, so every failure also appends an ERR row to op_errors.txt. Exit 0 ok, 1 refused/failed, 2 usage. No shell. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ftw.h>
#include <signal.h>
#include <time.h>

#define P 4096
static char C[P];                 /* conductor dir (absolute) */

/* ---------- small helpers ---------- */
static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0; }
static char *cp(const char *rel) { static char b[4][P]; static int i; i = (i + 1) & 3; if (rel[0] == '/') snprintf(b[i], P, "%s", rel); else snprintf(b[i], P, "%s/%s", C, rel); return b[i]; }
static char *jp(const char *a, const char *b) { static char buf[4][P]; static int i; i = (i + 1) & 3; snprintf(buf[i], P, "%s/%s", a, b); return buf[i]; }
static void err(const char *fmt, ...) { char m[1024]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
    fprintf(stderr, "eden_op: %s\n", m); FILE *f = fopen(cp("op_errors.txt"), "a"); if (f) { fprintf(f, "ERR|%s\n", m); fclose(f); } }
static void appendf(const char *path, const char *fmt, ...) { FILE *f = fopen(path, "a"); if (!f) return; va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fclose(f); }
static void mkdirs(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }
static int exists(const char *p) { struct stat s; return stat(p, &s) == 0; }

/* key=int files */
static long kv_get(const char *path, const char *key, long def) {
    FILE *f = fopen(path, "r"); char ln[512]; size_t kl = strlen(key); long v = def; if (!f) return def;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { v = atol(ln + kl + 1); break; }
    fclose(f); return v;
}
static void kv_set(const char *path, const char *key, long val) {
    static char buf[1 << 17], out[1 << 17]; size_t n = 0; FILE *f = fopen(path, "r"); size_t kl = strlen(key); int done = 0; out[0] = 0;
    if (f) { n = fread(buf, 1, sizeof buf - 1, f); fclose(f); } buf[n] = 0;
    char *p = buf; size_t o = 0;
    while (*p) { char *e = strchr(p, '\n'); size_t L = e ? (size_t)(e - p) + 1 : strlen(p);
        if (!done && !strncmp(p, key, kl) && p[kl] == '=') { o += snprintf(out + o, sizeof out - o, "%s=%ld\n", key, val); done = 1; }
        else { memcpy(out + o, p, L); o += L; }
        p += L; }
    if (!done) o += snprintf(out + o, sizeof out - o, "%s=%ld\n", key, val);
    f = fopen(path, "w"); if (f) { fwrite(out, 1, o, f); fclose(f); }
}

/* pipe rows */
typedef struct { char f[10][200]; int n; } Row;
static int rows_load(const char *path, Row *r, int max) {
    FILE *f = fopen(path, "r"); char ln[1024]; int n = 0; if (!f) return 0;
    while (n < max && fgets(ln, sizeof ln, f)) {
        trim(ln); if (!ln[0] || ln[0] == '#' || !strncmp(ln, "SECTION", 7) || ln[0] == '-') continue;
        char *p = ln; int k = 0;
        while (k < 10) { char *b = strchr(p, '|'); if (b) *b = 0; snprintf(r[n].f[k], sizeof r[n].f[k], "%s", p); trim(r[n].f[k]); k++; if (!b) break; p = b + 1; }
        r[n].n = k; n++;
    }
    fclose(f); return n;
}
static const char *rkv(const Row *r, const char *key, const char *def) {
    size_t kl = strlen(key);
    for (int i = 1; i < r->n; i++) if (!strncmp(r->f[i], key, kl) && r->f[i][kl] == '=') return r->f[i] + kl + 1;
    return def;
}
static long rkl(const Row *r, const char *key, long def) { const char *v = rkv(r, key, NULL); return v ? atol(v) : def; }

/* ---------- data tables (loaded once) ---------- */
#define MR 48
static Row GAME[MR], ITEMS[MR], SKILLS[MR], NODES[MR], KITS[MR], CROPS[MR], WIRING[MR], PHR[MR], FINDS[MR], ANIMALS[MR], EVOLVE[MR];
static int nGAME, nITEMS, nSKILLS, nNODES, nKITS, nCROPS, nWIRING, nPHR, nFINDS, nANIMALS, nEVOLVE;
static char TUN[P], WGT[P], WEA[P];
static void load_data(void) {
    nGAME = rows_load(cp("game.pdl"), GAME, MR); nITEMS = rows_load(cp("items.pdl"), ITEMS, MR); nSKILLS = rows_load(cp("skills.pdl"), SKILLS, MR);
    nNODES = rows_load(cp("nodes.pdl"), NODES, MR); nKITS = rows_load(cp("kits.pdl"), KITS, MR); nCROPS = rows_load(cp("crops.pdl"), CROPS, MR);
    nWIRING = rows_load(cp("wiring.pdl"), WIRING, MR); nPHR = rows_load(cp("phrases.pdl"), PHR, MR);
    nFINDS = rows_load(cp("finds.pdl"), FINDS, MR); nANIMALS = rows_load(cp("animals.pdl"), ANIMALS, MR); nEVOLVE = rows_load(cp("evolution.pdl"), EVOLVE, MR);
    snprintf(TUN, P, "%s", cp("tunables.pdl")); snprintf(WGT, P, "%s", cp("weights.pdl")); snprintf(WEA, P, "%s", cp("weather.pdl"));
}
static const char *wiring(const char *key, const char *def) { for (int i = nWIRING - 1; i >= 0; i--) if (WIRING[i].n >= 3 && !strcmp(WIRING[i].f[0], "WIRING") && !strcmp(WIRING[i].f[1], key)) return WIRING[i].f[2]; return def; }
static long tun(const char *k, long def) { return kv_get(TUN, k, def); }
static const Row *skill_row(const char *id) { for (int i = 0; i < nSKILLS; i++) if (SKILLS[i].n >= 2 && !strcmp(SKILLS[i].f[1], id)) return &SKILLS[i]; return NULL; }
static const Row *crop_row(void) { return nCROPS ? &CROPS[0] : NULL; }

/* ---------- conductor state ---------- */
static char *VARS(void) { return cp("variables.txt"); }
static long cur_day(void) { return kv_get(VARS(), "day", 0); }
static void hist(const char *fmt, ...) { FILE *f = fopen(cp("eden_history.txt"), "a"); if (!f) return; va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f); }
static void control(const char *fmt, ...) { char path[P]; snprintf(path, sizeof path, "%s", cp(wiring("control_ledger", "../../eden_control.txt")));
    FILE *f = fopen(path, "a"); if (!f) return; va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f); }

static uint64_t mix(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    uint64_t v[4] = { a, b, c, d }, x = 0x243F6A8885A308D3ULL;
    for (int i = 0; i < 4; i++) { x += v[i] * 0x9E3779B97F4A7C15ULL + 0x1234567ULL; x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ULL; x ^= x >> 27; x *= 0x94D049BB133111EBULL; x ^= x >> 31; }
    return x;
}

/* ---------- participants ---------- */
typedef struct { char name[48], path[P], kit[48], id[24]; int ord, retired; } Part;
static Part PARTS[32]; static int nPARTS;
static void garden_load(void);
static void parts_load(void) {
    Row R[64]; int n = rows_load(cp("participants.txt"), R, 64); nPARTS = 0;
    const char *pd = wiring("participants", "../participants");
    for (int i = 0; i < n; i++) {
        if (!strcmp(R[i].f[0], "JOIN") && R[i].n >= 5 && nPARTS < 32) { Part *p = &PARTS[nPARTS]; memset(p, 0, sizeof *p);
            snprintf(p->name, sizeof p->name, "%s", R[i].f[1]); snprintf(p->kit, sizeof p->kit, "%s", R[i].f[2]); snprintf(p->id, sizeof p->id, "%s", R[i].f[3]);
            snprintf(p->path, P, "%s/%s", cp(pd), p->name); p->ord = nPARTS++; }
        else if (!strcmp(R[i].f[0], "RETIRE") && R[i].n >= 2) { for (int k = 0; k < nPARTS; k++) if (!strcmp(PARTS[k].name, R[i].f[1])) PARTS[k].retired = 1; }
        else if (!strcmp(R[i].f[0], "DESPAWN") && R[i].n >= 2) {      /* v1 Stop: the folder is gone, the name may join again later */
            for (int k = 0; k < nPARTS; k++) if (!strcmp(PARTS[k].name, R[i].f[1])) { memmove(&PARTS[k], &PARTS[k + 1], (size_t)(nPARTS - k - 1) * sizeof(Part)); nPARTS--; break; }
            for (int k = 0; k < nPARTS; k++) PARTS[k].ord = k; }
    }
    garden_load();
}
static Part *part_find(const char *name) { for (int i = 0; i < nPARTS; i++) if (!strcmp(PARTS[i].name, name)) return &PARTS[i]; return NULL; }
static Part *partner_of(Part *me) { for (int k = 1; k < nPARTS; k++) { Part *q = &PARTS[(me->ord + k) % nPARTS]; if (!q->retired) return q; } return NULL; }

/* ---------- world entities (v1): the manifest ---------- */
typedef struct { char kind[24], name[48], id[24], path[P]; } Ent;
static Ent ENTS[96]; static int nENTS; static Ent *GARDEN[48]; static int nGARDEN;
static void ents_load(void) {
    static Row R[512]; int n = rows_load(cp("manifest.txt"), R, 512); nENTS = 0;
    for (int i = 0; i < n; i++) {
        if (!strcmp(R[i].f[0], "SPAWN") && R[i].n >= 4 && nENTS < 96) { Ent *e = &ENTS[nENTS++]; memset(e, 0, sizeof *e);
            snprintf(e->kind, sizeof e->kind, "%s", R[i].f[1]); snprintf(e->name, sizeof e->name, "%s", R[i].f[2]); snprintf(e->id, sizeof e->id, "%s", R[i].f[3]);
            snprintf(e->path, P, "%s/%s", cp(wiring(!strcmp(e->kind, "participant") ? "participants" : "world", !strcmp(e->kind, "participant") ? "../participants" : "../world")), e->name); }
        else if (!strcmp(R[i].f[0], "DESPAWN") && R[i].n >= 2) for (int k = 0; k < nENTS; k++) if (!strcmp(ENTS[k].name, R[i].f[1])) { memmove(&ENTS[k], &ENTS[k + 1], (size_t)(nENTS - k - 1) * sizeof(Ent)); nENTS--; break; }
    }
}
static Ent *ent_find(const char *name) { for (int i = 0; i < nENTS; i++) if (!strcmp(ENTS[i].name, name)) return &ENTS[i]; return NULL; }
static Ent *ent_first(const char *kind) { for (int i = 0; i < nENTS; i++) if (!strcmp(ENTS[i].kind, kind)) return &ENTS[i]; return NULL; }
static void garden_load(void) { ents_load(); nGARDEN = 0; for (int i = 0; i < nENTS && nGARDEN < 48; i++) if (!strcmp(ENTS[i].kind, "plant")) GARDEN[nGARDEN++] = &ENTS[i]; }
static long evar(const Ent *e, const char *k) { return kv_get(jp(e->path, "variables.txt"), k, 0); }
static void evar_set(const Ent *e, const char *k, long v) { kv_set(jp(e->path, "variables.txt"), k, v); }
static int txt_get(const char *path, char *out, size_t n) { FILE *f = fopen(path, "r"); if (!f) { out[0] = 0; return 0; } if (!fgets(out, (int)n, f)) out[0] = 0; fclose(f); trim(out); return out[0] != 0; }
static void txt_set(const char *path, const char *v) { FILE *f = fopen(path, "w"); if (f) { fprintf(f, "%s\n", v); fclose(f); } }

/* ---------- entity state ---------- */
static char *ef(const Part *p, const char *file) { return jp(p->path, file); }
static long item_n(const Part *p, const char *id) { char k[96]; snprintf(k, sizeof k, "item_%s", id); return kv_get(ef(p, "items.txt"), k, 0); }
static void item_add(const Part *p, const char *id, long delta, const char *reason) {
    char k[96]; snprintf(k, sizeof k, "item_%s", id); long v = kv_get(ef(p, "items.txt"), k, 0) + delta;
    kv_set(ef(p, "items.txt"), k, v); hist("ITEM|%ld|%s|%s|%ld|%s", cur_day(), p->name, id, delta, reason);
}
static long var_n(const Part *p, const char *k) { return kv_get(ef(p, "variables.txt"), k, 0); }
static void var_set(const Part *p, const char *k, long v) { kv_set(ef(p, "variables.txt"), k, v); }
static int has_skill(const Part *p, const char *sk) {
    char k[96]; snprintf(k, sizeof k, "skill_1:%s", sk);
    if (kv_get(ef(p, "actor_skills.txt"), k, 0) >= 1) return 1;
    for (int i = 0; i < nITEMS; i++) if (ITEMS[i].n >= 2 && !strcmp(rkv(&ITEMS[i], "grants_skill", ""), sk) && item_n(p, ITEMS[i].f[1]) >= 1) return 1;
    return 0;
}
/* plots: v0 keeps them in the participant's variables.txt (plot_<i>_state/grown/wet, max_plots of them); v1 with a garden (live PLANT entities in the manifest) shares them:
 * plot i = the i-th live plant entity, same three fields in its variables.txt (state, grown, wet) */
static long plot_n(void) { return nGARDEN ? nGARDEN : tun("max_plots", 3); }
static long plot_get(const Part *p, int pl, const char *f) { char k[64]; if (nGARDEN) return evar(GARDEN[pl - 1], f); snprintf(k, sizeof k, "plot_%d_%s", pl, f); return var_n(p, k); }
static void plot_set(const Part *p, int pl, const char *f, long v) { char k[64]; if (nGARDEN) { evar_set(GARDEN[pl - 1], f, v); return; } snprintf(k, sizeof k, "plot_%d_%s", pl, f); var_set(p, k, v); }
static int plot_find(const Part *p, int state) { long mp = plot_n(); for (int i = 1; i <= mp; i++) if (plot_get(p, i, "state") == state) return i; return 0; }
static int plot_find_dry(const Part *p) { long mp = plot_n(); for (int i = 1; i <= mp; i++) if (plot_get(p, i, "state") == 1 && !plot_get(p, i, "wet")) return i; return 0; }
static const Row *food_pick(const Part *p) { for (int i = 0; i < nITEMS; i++) if (ITEMS[i].n >= 2 && rkl(&ITEMS[i], "nutrition", 0) > 0 && item_n(p, ITEMS[i].f[1]) >= 1) return &ITEMS[i]; return NULL; }
static long TRADE_N = 1;       /* how many of `take` the partner hands over (1 for a swap, value= for a sale) */
static const Row *coin_row(void) { for (int i = 0; i < nITEMS; i++) if (ITEMS[i].n >= 2 && rkl(&ITEMS[i], "currency", 0) == 1) return &ITEMS[i]; return NULL; }
static int trade_find(const Part *me, const Part *pa, const Row **give, const Row **take) {
    long keep = tun("trade_keep", 1), gap = tun("trade_gap", 2); TRADE_N = 1;
    for (int a = 0; a < nITEMS; a++) { const Row *A = &ITEMS[a]; if (A->n < 2 || !rkl(A, "tradeable", 0)) continue;
        long ma = item_n(me, A->f[1]), pa_ = item_n(pa, A->f[1]); if (!(ma > keep && ma - pa_ >= gap)) continue;
        for (int b = 0; b < nITEMS; b++) { const Row *B = &ITEMS[b]; if (b == a || B->n < 2 || !rkl(B, "tradeable", 0)) continue;
            long mb = item_n(me, B->f[1]), pb = item_n(pa, B->f[1]); if (pb > keep && pb - mb >= gap) { *give = A; *take = B; return 1; } } }
    /* v1: sell a surplus item with value= to a partner who can pay in the currency item (coin conserved: partner -v, me +v) */
    const Row *cn = coin_row();
    if (cn) for (int a = 0; a < nITEMS; a++) { const Row *A = &ITEMS[a]; if (A->n < 2 || A == cn || !rkl(A, "tradeable", 0)) continue;
        long v = rkl(A, "value", 0), ma = item_n(me, A->f[1]), pa_ = item_n(pa, A->f[1]);
        if (v > 0 && ma > keep && ma - pa_ >= gap && item_n(pa, cn->f[1]) >= v) { *give = A; *take = cn; TRADE_N = v; return 1; } }
    return 0;
}
static const Row *study_target(const Part *p) {
    for (int i = 0; i < nNODES; i++) { const Row *N = &NODES[i]; if (N->n < 2) continue; char k[96]; snprintf(k, sizeof k, "skill_1:%s", N->f[1]);
        if (kv_get(ef(p, "actor_skills.txt"), k, 0) >= 1) continue;      /* already unlocked */
        if (has_skill(p, rkv(N, "requires", ""))) return N; }
    return NULL;
}
static void exp_gain(const Part *p, const char *req_skill, long amount) {
    for (int i = 0; i < nNODES; i++) { const Row *N = &NODES[i]; if (N->n < 2 || strcmp(rkv(N, "requires", ""), req_skill)) continue;
        char k[96]; snprintf(k, sizeof k, "skill_1:%s", N->f[1]); if (kv_get(ef(p, "actor_skills.txt"), k, 0) >= 1) continue;
        char e[96]; snprintf(e, sizeof e, "exp_%s", N->f[1]); long v = var_n(p, e) + amount; var_set(p, e, v);
        if (v >= rkl(N, "exp_to_unlock", 1000000)) { kv_set(ef(p, "actor_skills.txt"), k, 1); hist("UNLOCK|%ld|%s|%s|exp=%ld", cur_day(), p->name, N->f[1], v); } }
}

/* ---------- acts ---------- */
static const char *ACTS[] = { "plant", "water", "collect", "eat", "trade", "talk", "build", "study", "rest", "feed", "dig", "repair", NULL };   /* v1 acts last: unavailable without a world, so v0 dice are unchanged */
/* v1 helpers: animals, house, digging */
static const Row *animal_row(const char *kind) { for (int i = 0; i < nANIMALS; i++) if (ANIMALS[i].n >= 2 && !strcmp(ANIMALS[i].f[1], kind)) return &ANIMALS[i]; return NULL; }
static int animal_kind(const Ent *e, char *kind, size_t n) { return txt_get(jp(e->path, "kind.txt"), kind, n); }
static Ent *feedable_animal(const Part *me) {      /* first animal not fed today whose feed item the participant holds */
    char kind[48];
    for (int i = 0; i < nENTS; i++) { Ent *e = &ENTS[i]; if (!animal_kind(e, kind, sizeof kind)) continue; const Row *A = animal_row(kind); if (!A) continue;
        if (evar(e, "fed") == 0 && item_n(me, rkv(A, "feed_item", "grain")) >= rkl(A, "feed_amount", 1)) return e; }
    return NULL; }
static long house_cond(const Ent *h) { return h ? evar(h, "condition") : 0; }
static int house_ok(void) { Ent *h = ent_first("house"); return h && house_cond(h) >= tun("house_ok_at", 5); }
static int act_ok(const Part *me, const Part *pa, const char *act) {
    const Row *S = skill_row(act); if (!S || !has_skill(me, act)) return 0;
    const char *ni = rkv(S, "needs_item", ""); if (ni[0] && item_n(me, ni) < 1) return 0;
    if (!strcmp(act, "plant")) return item_n(me, "seed") >= 1 && plot_find(me, 0) != 0;
    if (!strcmp(act, "water")) return kv_get(VARS(), "rain_today", 0) == 0 && plot_find_dry(me) != 0;
    if (!strcmp(act, "collect")) return plot_find(me, 2) != 0;
    if (!strcmp(act, "eat")) return var_n(me, "hunger") >= tun("eat_min_hunger", 2) && food_pick(me) != NULL;
    if (!strcmp(act, "trade")) { const Row *g, *t; return pa && trade_find(me, pa, &g, &t); }
    if (!strcmp(act, "talk")) return pa != NULL;
    if (!strcmp(act, "build")) return var_n(me, "shelter") < tun("max_shelter", 1) && item_n(me, "grain") >= tun("hut_cost_grain", 3) && item_n(me, "seed") >= tun("hut_cost_seed", 1);
    if (!strcmp(act, "study")) return study_target(me) != NULL;
    if (!strcmp(act, "feed")) return feedable_animal(me) != NULL;
    if (!strcmp(act, "dig")) return nFINDS > 0;
    if (!strcmp(act, "repair")) { Ent *h = ent_first("house"); return h && house_cond(h) <= tun("repair_below", 7) && item_n(me, "clay") >= tun("repair_cost_clay", 1); }
    return 1;   /* rest */
}
/* TALKING THROUGH TOMOM: the chatbot is the tomom-hq Ask binary on a SCRATCH copy (wiring tomom_bin / tomom_dir, never the live folder), fork+exec+waitpid with a watchdog, its
 * cwd = tomom_dir, prompt = one word of the speaker's words.txt (seeded pick). Any failure (no binary, no Response line, timeout) falls back to phrases.pdl. Returns "tomom" or "phrase". */
static const char *talk_line(const Part *me, long d, char *line, size_t n, char *prompt, size_t pn) {
    line[0] = 0; long seed = kv_get(VARS(), "seed", 0); char words[32][48]; int nw = 0; FILE *f = fopen(ef(me, "words.txt"), "r"); char ln[256];
    while (f && nw < 32 && fgets(ln, sizeof ln, f)) { trim(ln); if (ln[0] && ln[0] != '#') snprintf(words[nw++], sizeof words[0], "%s", ln); } if (f) fclose(f);
    snprintf(prompt, pn, "%s", nw ? words[mix((uint64_t)seed, (uint64_t)d, (uint64_t)me->ord, 5) % (uint64_t)nw] : "hello");
    const char *tb = wiring("tomom_bin", ""), *td = wiring("tomom_dir", "");
    if (tb[0] && td[0] && exists(cp(tb)) && exists(cp(td))) {
        char bin[P], dir[P], out[P]; snprintf(bin, P, "%s", cp(tb)); snprintf(dir, P, "%s", cp(td)); snprintf(out, P, "%s/ask_out.txt", dir);
        pid_t pid = fork();
        if (pid == 0) { if (chdir(dir)) _exit(127); int o = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644), dn = open("/dev/null", O_RDWR); if (o < 0) _exit(127); dup2(dn, 0); dup2(o, 1); dup2(dn, 2);
            execl(bin, bin, "curriculum_bank.txt", prompt, "12", "0.5", (char *)NULL); _exit(127); }
        if (pid > 0) { int st = 0, done = 0; for (int i = 0; i < 500 && !done; i++) { if (waitpid(pid, &st, WNOHANG) == pid) done = 1; else usleep(10000); }
            if (!done) { kill(pid, SIGKILL); waitpid(pid, &st, 0); }
            else { static char buf[1 << 16]; size_t got = 0; FILE *g = fopen(out, "r"); if (g) { got = fread(buf, 1, sizeof buf - 1, g); fclose(g); } buf[got] = 0;
                char *last = NULL, *p = buf; while ((p = strstr(p, "Response:")) != NULL) { last = p; p += 9; }
                if (last) { last += 9; while (*last == ' ') last++; size_t L = strcspn(last, "\n"); if (L >= n) L = n - 1; memcpy(line, last, L); line[L] = 0;
                    for (char *c = line; *c; c++) if (*c == '|' || *c == '\r' || *c == '\t') *c = ' '; trim(line); }
                if (line[0]) { appendf(jp(dir, "ask_log.txt"), "ASK | %s:%ld | %s | %s | target=%s\n", me->name, d, prompt, line, dir); return "tomom"; } } } }
    int np = 0; const char *txt[MR]; for (int i = 0; i < nPHR; i++) if (PHR[i].n >= 2 && !strcmp(PHR[i].f[0], "PHRASE")) txt[np++] = PHR[i].f[1];
    snprintf(line, n, "%s", np ? txt[mix((uint64_t)seed, (uint64_t)d, (uint64_t)me->ord, 2) % (uint64_t)np] : "...");
    return "phrase";
}
/* DIGGING: depth grows by one per dig (back to 0 after max_dig_depth), the roll picks among finds.pdl rows whose depth is reached, weight = rarity (smaller = rarer) */
static void do_dig(Part *me, long d) {
    long seed = kv_get(VARS(), "seed", 0), dn = var_n(me, "dig_n"), depth = var_n(me, "dig_depth") + 1, tot = 0;
    var_set(me, "dig_n", dn + 1);
    for (int i = 0; i < nFINDS; i++) if (FINDS[i].n >= 3 && rkl(&FINDS[i], "depth", 1) <= depth) tot += rkl(&FINDS[i], "weight", 0);
    const char *item = "-"; if (tot > 0) { long r = (long)(mix((uint64_t)seed, (uint64_t)d, (uint64_t)me->ord, (uint64_t)(100 + dn)) % (uint64_t)tot);
        for (int i = 0; i < nFINDS; i++) { if (FINDS[i].n < 3 || rkl(&FINDS[i], "depth", 1) > depth) continue; long w = rkl(&FINDS[i], "weight", 0); if (r < w) { item = rkv(&FINDS[i], "item", "-"); break; } r -= w; } }
    var_set(me, "dig_depth", depth >= tun("max_dig_depth", 6) ? 0 : depth);
    hist("ACT|%ld|%s|dig|depth=%ld|find=%s", d, me->name, depth, item);
    if (strcmp(item, "-")) { item_add(me, item, 1, "dig"); hist("FIND|%ld|%s|%s|depth=%ld", d, me->name, item, depth); }
}
static int do_act(Part *me, Part *pa, const char *act) {
    long d = cur_day(); char k[96];
    if (!act_ok(me, pa, act)) return -1;
    if (!strcmp(act, "plant")) { int pl = plot_find(me, 0); const Row *cr = crop_row();
        item_add(me, "seed", -1, "plant"); plot_set(me, pl, "state", 1); plot_set(me, pl, "grown", 0); plot_set(me, pl, "wet", 0);
        hist("ACT|%ld|%s|plant|plot=%d|crop=%s", d, me->name, pl, cr ? cr->f[1] : "?"); exp_gain(me, "plant", tun("practice_exp", 1)); }
    else if (!strcmp(act, "water")) { int pl = plot_find_dry(me); plot_set(me, pl, "wet", 1); hist("ACT|%ld|%s|water|plot=%d", d, me->name, pl); }
    else if (!strcmp(act, "collect")) { int pl = plot_find(me, 2); const Row *cr = crop_row(); long y = cr ? rkl(cr, "yield", 1) : 1, sr = cr ? rkl(cr, "seed_return", 0) : 0;
        plot_set(me, pl, "state", 0); plot_set(me, pl, "grown", 0);
        item_add(me, cr ? rkv(cr, "item", "grain") : "grain", y, "collect"); if (sr) item_add(me, "seed", sr, "collect");
        hist("ACT|%ld|%s|collect|plot=%d|yield=%ld", d, me->name, pl, y); exp_gain(me, "collect", tun("practice_exp", 1)); }
    else if (!strcmp(act, "eat")) { const Row *f = food_pick(me); long nu = rkl(f, "nutrition", 0), h = var_n(me, "hunger") - nu; if (h < 0) h = 0;
        item_add(me, f->f[1], -1, "eat"); var_set(me, "hunger", h); hist("ACT|%ld|%s|eat|item=%s|nutrition=%ld|hunger=%ld", d, me->name, f->f[1], nu, h); }
    else if (!strcmp(act, "trade")) { const Row *g, *t; trade_find(me, pa, &g, &t);
        long tn = TRADE_N; item_add(me, g->f[1], -1, "trade"); item_add(pa, g->f[1], +1, "trade"); item_add(me, t->f[1], +tn, "trade"); item_add(pa, t->f[1], -tn, "trade");
        hist("ACT|%ld|%s|trade|give=%s|take=%s|with=%s|n=%ld", d, me->name, g->f[1], t->f[1], pa->name, tn); }
    else if (!strcmp(act, "talk")) { char line[512], prompt[96]; const char *path = talk_line(me, d, line, sizeof line, prompt, sizeof prompt);
        appendf(ef(me, "phone.txt"), "MSG|%ld|OUT|%s|%s\n", d, pa->name, line); appendf(ef(pa, "phone.txt"), "MSG|%ld|IN|%s|%s\n", d, me->name, line);
        hist("ACT|%ld|%s|talk|with=%s|text=%s|path=%s", d, me->name, pa->name, line, path);
        hist("TALK|%ld|%s|%s|path=%s|prompt=%s|text=%s", d, me->name, pa->name, path, prompt, line); }
    else if (!strcmp(act, "build")) { item_add(me, "grain", -tun("hut_cost_grain", 3), "build"); item_add(me, "seed", -tun("hut_cost_seed", 1), "build");
        long s = var_n(me, "shelter") + tun("shelter_gain", 1); var_set(me, "shelter", s); hist("ACT|%ld|%s|build|hut|shelter=%ld", d, me->name, s); }
    else if (!strcmp(act, "study")) { const Row *N = study_target(me); char e[96]; snprintf(e, sizeof e, "exp_%s", N->f[1]); long before = var_n(me, e);
        exp_gain(me, rkv(N, "requires", ""), tun("study_exp", 2)); hist("ACT|%ld|%s|study|node=%s|exp=%ld", d, me->name, N->f[1], var_n(me, e)); (void)before; }
    else if (!strcmp(act, "feed")) { Ent *a = feedable_animal(me); char kind[48]; animal_kind(a, kind, sizeof kind); const Row *A = animal_row(kind);
        const char *fi = rkv(A, "feed_item", "grain"); long fa = rkl(A, "feed_amount", 1);
        item_add(me, fi, -fa, "feed"); evar_set(a, "fed", 1); evar_set(a, "meals", evar(a, "meals") + 1); txt_set(jp(a->path, "keeper.txt"), me->name);
        hist("ACT|%ld|%s|feed|animal=%s|kind=%s|item=%s|meals=%ld", d, me->name, a->name, kind, fi, evar(a, "meals")); }
    else if (!strcmp(act, "dig")) do_dig(me, d);
    else if (!strcmp(act, "repair")) { Ent *h = ent_first("house"); long c = house_cond(h) + tun("repair_gain", 3), mc = tun("max_condition", 10); if (c > mc) c = mc;
        item_add(me, "clay", -tun("repair_cost_clay", 1), "repair"); evar_set(h, "condition", c); hist("ACT|%ld|%s|repair|house=%s|condition=%ld", d, me->name, h->name, c); }
    else hist("ACT|%ld|%s|rest|hp=%ld", d, me->name, var_n(me, "hp"));
    if (!strcmp(act, "rest")) { long hp = kv_get(ef(me, "actor_1_stats.txt"), "hp", 1), mhp = kv_get(ef(me, "actor_1_stats.txt"), "mhp", 1), heal = 1 + (house_ok() ? tun("house_rest_bonus", 1) : 0);
        if (hp < mhp) { hp += heal; if (hp > mhp) hp = mhp; kv_set(ef(me, "actor_1_stats.txt"), "hp", hp); } }
    return 0;
}

/* ---------- grants ---------- */
static int grant_active(const char *who, const char *kind, const char *id) {
    FILE *f = fopen(cp("grants.txt"), "r"); char ln[512]; int act = 0; if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) { Row r; char b[512]; snprintf(b, sizeof b, "%s", ln); trim(b); char *fld[10]; int n = 0; char *p = b;
        while (n < 10) { fld[n++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; } (void)r;
        if (n >= 5 && !strcmp(fld[2], who) && !strcmp(fld[3], kind) && !strcmp(fld[4], id)) act = !strcmp(fld[0], "GRANT") && strcmp(fld[6 < n ? 6 : 0], "applied=0") != 0; }
    fclose(f); return act;
}
/* returns 1 if it changed something */
static int grant_one(Part *p, const char *kind, const char *id, long amount) {
    if (grant_active(p->name, kind, id)) return 0;
    long d = cur_day(); int applied = 1; const char *game = "eden";
    if (!strcmp(kind, "item")) item_add(p, id, amount, "grant");
    else if (!strcmp(kind, "skill")) { char k[96]; snprintf(k, sizeof k, "skill_1:%s", id); if (kv_get(ef(p, "actor_skills.txt"), k, 0) >= 1) applied = 0; else kv_set(ef(p, "actor_skills.txt"), k, 1); }
    else if (!strcmp(kind, "param")) { long v = kv_get(ef(p, "actor_1_stats.txt"), id, 0) + amount; kv_set(ef(p, "actor_1_stats.txt"), id, v); }
    appendf(cp("grants.txt"), "GRANT|%s|%s|%s|%s|%ld|applied=%d|day=%ld\n", game, p->name, kind, id, amount, applied, d);
    return 1;
}
static int kit_apply_as(Part *p, const char *kit) {
    int ch = 0;
    for (int i = 0; i < nKITS; i++) { const Row *R = &KITS[i]; if (R->n < 4 || strcmp(R->f[1], kit)) continue;
        if (!strcmp(R->f[0], "KITITEM")) ch += grant_one(p, "item", R->f[2], atol(R->f[3]));
        else if (!strcmp(R->f[0], "KITSKILL")) ch += grant_one(p, "skill", R->f[2], 1);
        else if (!strcmp(R->f[0], "KITPARAM")) ch += grant_one(p, "param", R->f[2], atol(R->f[3])); }
    return ch;
}
static int kit_apply(Part *p) { return kit_apply_as(p, p->kit); }
static int revoke_all(Part *p) {
    Row R[256]; int n = rows_load(cp("grants.txt"), R, 256), ch = 0;
    for (int i = 0; i < n; i++) { if (strcmp(R[i].f[0], "GRANT") || strcmp(R[i].f[2], p->name) || !grant_active(p->name, R[i].f[3], R[i].f[4])) continue;
        const char *kind = R[i].f[3], *id = R[i].f[4]; long amount = atol(R[i].f[5]), took = 0; int applied = strcmp(R[i].f[6], "applied=0") != 0;
        if (!applied) took = 0;
        else if (!strcmp(kind, "item")) { long have = item_n(p, id); took = have < amount ? have : amount; item_add(p, id, -took, "revoke"); }
        else if (!strcmp(kind, "skill")) { char k[96]; snprintf(k, sizeof k, "skill_1:%s", id); kv_set(ef(p, "actor_skills.txt"), k, 0); took = 1; }
        else if (!strcmp(kind, "param")) { long have = kv_get(ef(p, "actor_1_stats.txt"), id, 0); long nv = have - amount; if (nv < 1 && (!strcmp(id, "hp") || !strcmp(id, "mhp"))) nv = 1; kv_set(ef(p, "actor_1_stats.txt"), id, nv); took = have - nv; }
        appendf(cp("grants.txt"), "REVOKE|eden|%s|%s|%s|%ld|took=%ld|day=%ld\n", p->name, kind, id, amount, took, cur_day()); ch++; }
    return ch;
}

/* ---------- spawn from template ---------- */
static int copytree(const char *src, const char *dst, const char *name) {
    mkdirs(dst); DIR *d = opendir(src); if (!d) return 1; struct dirent *e;
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; char s[P], t[P]; snprintf(s, P, "%s/%s", src, e->d_name); snprintf(t, P, "%s/%s", dst, e->d_name);
        struct stat st; if (stat(s, &st)) continue;
        if (S_ISDIR(st.st_mode)) copytree(s, t, name);
        else { FILE *in = fopen(s, "r"), *out = fopen(t, "w"); if (in && out) { char ln[1024]; while (fgets(ln, sizeof ln, in)) { char *at = strstr(ln, "@NAME@");
                    if (at) { *at = 0; fprintf(out, "%s%s%s", ln, name, at + 6); } else fputs(ln, out); } }
            if (in) fclose(in); if (out) fclose(out); } }
    closedir(d); return 0;
}
static int join_one(const char *name, const char *kit) {
    parts_load(); Part *q = part_find(name); if (q) return q->retired ? -2 : 0;
    const char *pd = wiring("participants", "../participants"); char dst[P]; snprintf(dst, P, "%s/%s", cp(pd), name);
    if (exists(dst)) { err("participant folder exists: %s", dst); return -1; }
    mkdirs(cp(pd)); if (copytree(cp(wiring("template", "participant_template")), dst, name)) { err("template missing"); return -1; }
    long id = kv_get(VARS(), "next_id", 1); kv_set(VARS(), "next_id", id + 1);   /* fresh identity ED<n>: minted here, never copied */
    char b[64]; snprintf(b, sizeof b, "ED%ld\n", id); FILE *f = fopen(jp(dst, "instance_id.txt"), "w"); if (f) { fputs(b, f); fclose(f); }
    b[strlen(b) - 1] = 0;
    appendf(cp("participants.txt"), "JOIN|%s|%s|%s|day=%ld\n", name, kit, b, cur_day());
    appendf(cp("manifest.txt"), "SPAWN|participant|%s|%s|day=%ld\n", name, b, cur_day());
    parts_load(); Part *p = part_find(name);
    for (int i = 0; i < nITEMS; i++) if (ITEMS[i].n >= 2) { long n = item_n(p, ITEMS[i].f[1]); if (n) hist("ITEM|%ld|%s|%s|%ld|own", cur_day(), name, ITEMS[i].f[1], n); }
    hist("JOIN|%ld|%s|%s|%s", cur_day(), name, kit, b);
    return 1;
}

/* ---------- process helper ---------- */
static int run_argv(char **argv) {
    pid_t pid = fork(); if (pid < 0) return 127;
    if (pid == 0) { int dn = open("/dev/null", O_RDWR); if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); } execv(argv[0], argv); _exit(127); }
    int st; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
}
/* Env the clock's event runner needs (wiring keys prisc / event_runner; unset = nothing is set, the caller's env, e.g. the harness's, is left alone; an already-set variable is never overwritten). */
static void lc_env(void) {
    if (wiring("event_runner", "")[0]) { setenv("LC_CLOCK_NO_POPUP", "1", 0); setenv("LC_CLOCK_EVENT_RUNNER", cp(wiring("event_runner", "")), 0); setenv("EVENT_PAGE_TRIGGER", "parallel", 0); }
    if (wiring("prisc", "")[0]) setenv("EVENT_PAGE_PRISC", cp(wiring("prisc", "")), 0);
}
static int lc(const char *a, const char *b, const char *c, const char *d, const char *e, const char *f2) {
    lc_env();
    const char *bin = cp(wiring("lc_clock", "")), *house = cp(wiring("house", "../..")); if (!wiring("lc_clock", "")[0]) return 127;
    char *av[12]; int n = 0; av[n++] = (char *)bin; av[n++] = (char *)house; const char *x[] = { a, b, c, d, e, f2 };
    for (int i = 0; i < 6; i++) if (x[i]) av[n++] = (char *)x[i]; av[n] = NULL; return run_argv(av);
}

/* ---------- the clock daemon (wiring daemon_pid; `WIRING | daemon | 1` makes Start game / Resume start it and Stop game / Pause stop it) ----------
 * Started with fork + setsid + execv (no shell, no system()), pid in the pid file, ledger rows DAEMON|... in the control ledger (no wall time). Stopped ONLY by SIGTERM to the recorded
 * pid, and only if /proc/<pid>/cmdline still names the lc binary (a recycled pid is never signalled); never a pkill by pattern. */
static char *dpid_path(void) { return cp(wiring("daemon_pid", "../../eden_daemon.pid")); }
static long dpid_read(void) { FILE *f = fopen(dpid_path(), "r"); long v = 0; if (f) { if (fscanf(f, "%ld", &v) != 1) v = 0; fclose(f); } return v; }
/* the daemon is exec'd by its normalised absolute path so argv0 (and ps) read cleanly and /proc/<pid>/cmdline can be matched */
static void lc_bin(char *out) { char *b = cp(wiring("lc_clock", "")); if (!realpath(b, out)) snprintf(out, P, "%s", b); }
static int dpid_is_lc(long pid) {
    if (pid <= 1 || kill((pid_t)pid, 0) != 0) return 0;
    char pp[64], buf[4096]; snprintf(pp, sizeof pp, "/proc/%ld/cmdline", pid); FILE *f = fopen(pp, "r"); if (!f) return 0;
    size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;                      /* argv0 is the first NUL-terminated string */
    char want[P]; lc_bin(want); return n > 0 && !strcmp(buf, want);
}
static int v_daemon_start(void) {
    if (!wiring("lc_clock", "")[0]) { err("daemon-start: no lc_clock wiring"); return 1; }
    long old = dpid_read();
    if (old && dpid_is_lc(old)) { printf("daemon already running pid=%ld\n", old); control("DAEMON|start-refused|pid=%ld|already-running", old); return 0; }
    if (old > 1 && kill((pid_t)old, 0) == 0) { err("daemon-start refused: %s names pid %ld, a live process that is not %s", dpid_path(), old, cp(wiring("lc_clock", ""))); return 1; }
    if (!kv_get(cp("install.txt"), "clock_installed", 0)) { err("daemon-start refused: run Setup first (no clock installed)"); return 1; }
    lc_env();
    char bin[P], house[P]; lc_bin(bin); if (!realpath(cp(wiring("house", "../..")), house)) snprintf(house, sizeof house, "%s", cp(wiring("house", "../..")));
    pid_t pid = fork(); if (pid < 0) { err("daemon-start: fork failed"); return 1; }
    if (pid == 0) { setsid(); int dn = open("/dev/null", O_RDWR); if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        char *av[4] = { bin, house, "daemon", NULL }; execv(bin, av); _exit(127); }
    FILE *f = fopen(dpid_path(), "w"); if (f) { fprintf(f, "%ld\n", (long)pid); fclose(f); }
    for (int i = 0; i < 100; i++) { if (dpid_is_lc(pid)) break; usleep(10000); }            /* wait for the exec to land (cmdline = lc path) */
    if (!dpid_is_lc(pid)) { err("daemon-start: %s did not stay up", bin); remove(dpid_path()); return 1; }
    control("DAEMON|start|pid=%ld", (long)pid); printf("daemon started pid=%ld\n", (long)pid); return 0;
}
static int v_daemon_stop(void) {
    long pid = dpid_read();
    if (!pid) { printf("daemon not running\n"); return 0; }
    if (dpid_is_lc(pid)) { kill((pid_t)pid, SIGTERM); int i; for (i = 0; i < 300 && kill((pid_t)pid, 0) == 0; i++) usleep(10000);
        if (kill((pid_t)pid, 0) == 0) { err("daemon-stop: pid %ld did not exit after SIGTERM", pid); return 1; } control("DAEMON|stop|pid=%ld", pid); }
    else if (pid > 1 && kill((pid_t)pid, 0) == 0) { err("daemon-stop: pid %ld is a live process that is not the clock daemon, NOT signalled", pid); return 1; }
    else control("DAEMON|stop|pid=%ld|stale-pid-file", pid);
    remove(dpid_path()); printf("daemon stopped\n"); return 0;
}
static int v_daemon_status(void) { long pid = dpid_read(); if (pid && dpid_is_lc(pid)) { printf("daemon running pid=%ld\n", pid); return 0; } printf("daemon stopped\n"); return 1; }
static int daemon_wanted(void) { return atoi(wiring("daemon", "0")) == 1; }
/* Next day: exactly one game day, deterministic. With no daemon: `cmd advance 1d` then `step 0` (one pass fires the Day Tick once). With a daemon running the daemon owns the clock
 * (`step` must not run beside it), so only the advance goes into the mailbox and the daemon fires the tick on its next pass. */
static int v_nextday(void) {
    if (!kv_get(VARS(), "setup_done", 0) || !kv_get(cp("install.txt"), "clock_installed", 0)) { err("nextday refused: run Setup / Start game first"); return 1; }
    const char *clk = wiring("clock", "g1"); long d0 = cur_day(); int rc = lc("cmd", clk, "advance", "1d", NULL, NULL);
    long dp = dpid_read(); int queued = dp && dpid_is_lc(dp);
    if (!queued && !rc) rc = lc("step", "0", NULL, NULL, NULL, NULL);
    control("NEXTDAY|day_before=%ld|rc=%d|%s", d0, rc, queued ? "queued-to-daemon" : "stepped"); printf("nextday rc=%d %s\n", rc, queued ? "queued" : "stepped");
    return rc ? 1 : 0;
}

/* ---------- verbs ---------- */
static const char *game_val(const char *tag, const char *key, const char *def) { for (int i = nGAME - 1; i >= 0; i--) if (GAME[i].n >= 3 && !strcmp(GAME[i].f[0], tag) && !strcmp(GAME[i].f[1], key)) return GAME[i].f[2]; return def; }

static int v_setup(void) {
    int ch = 0;
    if (kv_get(VARS(), "setup_done", 0) == 0) {
        long seed = atol(game_val("SETUP", "seed", "1")), len = atol(game_val("SETUP", "length", "20"));
        kv_set(VARS(), "seed", seed); kv_set(VARS(), "length", len); kv_set(VARS(), "setup_done", 1); ch++;
        if (kv_get(VARS(), "day", -1) < 0) kv_set(VARS(), "day", 0);
    }
    for (int i = 0; i < nGAME; i++) if (GAME[i].n >= 2 && !strcmp(GAME[i].f[0], "PARTICIPANT")) {
        const char *kit = rkv(&GAME[i], "kit", "farmer"); int r = join_one(GAME[i].f[1], kit); if (r == 1) ch++; if (r == -1) return 1; }
    parts_load();
    for (int i = 0; i < nPARTS; i++) if (!PARTS[i].retired) ch += kit_apply(&PARTS[i]);
    if (wiring("lc_clock", "")[0] && !kv_get(cp("install.txt"), "clock_installed", 0)) {
        const char *clk = wiring("clock", "g1"); char first[32]; snprintf(first, sizeof first, "%s", wiring("first_ms", "86400000"));
        lc("new", clk, NULL, NULL, NULL, NULL);
        int rc = lc("reminder-add", clk, first, "common:eden_day_tick", "", wiring("repeat", "every:1day"));
        if (rc) { err("reminder-add failed rc=%d", rc); return 1; }
        kv_set(cp("install.txt"), "clock_installed", 1); ch++; }
    if (ch) hist("SETUP|%ld|changes=%d", cur_day(), ch);
    printf("setup changes=%d\n", ch); return 0;
}
static int v_reset(void) {
    parts_load(); int ch = 0; for (int i = 0; i < nPARTS; i++) if (!PARTS[i].retired) ch += revoke_all(&PARTS[i]);
    kv_set(VARS(), "setup_done", 0); kv_set(VARS(), "running", 0);
    hist("RESET|%ld|revoked=%d", cur_day(), ch); control("RESET|day=%ld|revoked=%d", cur_day(), ch);
    if (wiring("lc_clock", "")[0] && kv_get(cp("install.txt"), "clock_installed", 0)) lc("cmd", wiring("clock", "g1"), "pause", NULL, NULL, NULL);
    printf("reset revoked=%d\n", ch); return 0;
}
static int v_ctl(const char *what) {
    const char *clk = wiring("clock", "g1"); int rc = 0;
    if (!kv_get(VARS(), "setup_done", 0)) { err("%s refused: run Setup first", what); return 1; }
    if (!strcmp(what, "start")) { kv_set(VARS(), "running", 1); lc("cmd", clk, "rate", wiring("start_rate", "x86400"), NULL, NULL); rc = lc("cmd", clk, "resume", NULL, NULL, NULL); }
    else if (!strcmp(what, "pause")) { kv_set(VARS(), "running", 0); rc = lc("cmd", clk, "pause", NULL, NULL, NULL); }
    else { kv_set(VARS(), "running", 1); rc = lc("cmd", clk, "resume", NULL, NULL, NULL); }
    control("%s|day=%ld|rc=%d", what, cur_day(), rc);
    if (daemon_wanted()) { if (!strcmp(what, "pause")) v_daemon_stop(); else if (strcmp(what, "start")) v_daemon_start(); } printf("%s rc=%d\n", what, rc); return 0;
}
static int v_add(const char *name) {
    if (!kv_get(VARS(), "setup_done", 0)) { err("add refused: run Setup first"); return 1; }
    char nm[64] = ""; const char *kit = "farmer"; parts_load();
    if (name && name[0]) { snprintf(nm, sizeof nm, "%s", name); for (int i = 0; i < nGAME; i++) if (!strcmp(GAME[i].f[0], "SPARE") && !strcmp(GAME[i].f[1], nm)) kit = rkv(&GAME[i], "kit", "farmer"); }
    else for (int i = 0; i < nGAME; i++) if (GAME[i].n >= 2 && !strcmp(GAME[i].f[0], "SPARE") && !part_find(GAME[i].f[1])) { snprintf(nm, sizeof nm, "%s", GAME[i].f[1]); kit = rkv(&GAME[i], "kit", "farmer"); break; }
    if (!nm[0]) { err("add: no SPARE participant left"); return 1; }
    int r = join_one(nm, kit); if (r < 0) { err("add %s refused (%s)", nm, r == -2 ? "retired: tombstones are not reused" : "folder exists"); return 1; }
    parts_load(); int ch = kit_apply(part_find(nm)); hist("SETUP|%ld|add=%s|grants=%d", cur_day(), nm, ch); printf("add %s grants=%d\n", nm, ch); return 0;
}
static int v_remove(const char *name) {
    parts_load(); Part *p = NULL;
    if (name && name[0]) p = part_find(name); else for (int i = nPARTS - 1; i >= 0; i--) if (!PARTS[i].retired) { p = &PARTS[i]; break; }
    if (!p || p->retired) { err("remove: no such active participant"); return 1; }
    int ch = revoke_all(p); appendf(cp("participants.txt"), "RETIRE|%s|day=%ld\n", p->name, cur_day()); hist("RETIRE|%ld|%s|revoked=%d", cur_day(), p->name, ch);
    printf("remove %s revoked=%d\n", p->name, ch); return 0;
}

/* ---------- v1: Start game / Stop game ---------- */
static int spawn_world(const char *kind, const char *name) {
    ents_load(); if (ent_find(name)) return 0;                      /* already live in the manifest: Start twice spawns nothing new */
    char dst[P], src[P]; snprintf(dst, P, "%s/%s", cp(wiring("world", "../world")), name);
    if (exists(dst)) { err("begin refused: %s exists but is not in the manifest", dst); return -1; }
    snprintf(src, P, "%s", cp(wiring("world_templates", "world_templates")));
    char tp[P]; snprintf(tp, P, "%s/%s", src, kind); if (!exists(tp)) { err("begin: no template for kind %s", kind); return -1; }
    mkdirs(cp(wiring("world", "../world"))); if (copytree(tp, dst, name)) { err("begin: copy of template %s failed", kind); return -1; }
    long id = kv_get(VARS(), "next_id", 1); kv_set(VARS(), "next_id", id + 1); char b[64]; snprintf(b, sizeof b, "ED%ld", id); txt_set(jp(dst, "instance_id.txt"), b);
    appendf(cp("manifest.txt"), "SPAWN|%s|%s|%s|day=%ld\n", kind, name, b, cur_day()); hist("SPAWN|%ld|%s|%s|%s", cur_day(), kind, name, b);
    return 1;
}
static int v_begin(void) {
    int ch = 0, rc = v_setup(); if (rc) return rc;
    parts_load();
    for (int i = 0; i < nGAME; i++) if (GAME[i].n >= 2 && !strcmp(GAME[i].f[0], "PLAYKIT")) { const char *kit = rkv(&GAME[i], "kit", ""); for (int k = 0; k < nPARTS; k++) if (!PARTS[k].retired) ch += kit_apply_as(&PARTS[k], kit); }
    for (int i = 0; i < nGAME; i++) if (GAME[i].n >= 3 && !strcmp(GAME[i].f[0], "WORLD")) { const char *kind = GAME[i].f[1]; long cnt = rkl(&GAME[i], "count", 1);
        for (long c = 1; c <= cnt; c++) { char nm[48]; snprintf(nm, sizeof nm, "%s_%ld", kind, c); int r = spawn_world(kind, nm); if (r < 0) return 1; ch += r; } }
    garden_load();
    if (ch) hist("BEGIN|%ld|changes=%d", cur_day(), ch);
    control("BEGIN|day=%ld|changes=%d", cur_day(), ch);
    printf("begin changes=%d\n", ch);
    rc = v_ctl("start"); if (!rc && daemon_wanted()) rc = v_daemon_start(); return rc;
}
static int rm_cb(const char *p, const struct stat *sb, int tf, struct FTW *fw) { (void)sb; (void)tf; (void)fw; return remove(p); }
static int safe_under(const char *path, const char *root) { char rp[P], rr[P]; if (!realpath(path, rp) || !realpath(root, rr)) return 0; size_t L = strlen(rr); return !strncmp(rp, rr, L) && rp[L] == '/'; }
static int v_stop(void) {
    const char *clk = wiring("clock", "g1"); int gone = 0, skipped = 0; ents_load(); parts_load();
    kv_set(VARS(), "running", 0);
    if (daemon_wanted()) v_daemon_stop();
    if (wiring("lc_clock", "")[0] && kv_get(cp("install.txt"), "clock_installed", 0)) lc("cmd", clk, "pause", NULL, NULL, NULL);
    Ent list[96]; int n = nENTS; memcpy(list, ENTS, sizeof(Ent) * (size_t)n);
    for (int i = n - 1; i >= 0; i--) { Ent *e = &list[i];
        const char *root = cp(wiring(!strcmp(e->kind, "participant") ? "participants" : "world", !strcmp(e->kind, "participant") ? "../participants" : "../world"));
        if (strchr(e->name, '/') || e->name[0] == '.' || !e->name[0]) { skipped++; continue; }
        if (!exists(e->path)) { appendf(cp("manifest.txt"), "DESPAWN|%s|day=%ld|missing=1\n", e->name, cur_day()); continue; }
        if (!safe_under(e->path, root)) { err("stop: %s is not under %s, left alone", e->path, root); skipped++; continue; }
        if (!strcmp(e->kind, "participant")) { Part *p = part_find(e->name);
            if (p) { if (!p->retired) revoke_all(p);
                for (int k = 0; k < nITEMS; k++) if (ITEMS[k].n >= 2) { long have = item_n(p, ITEMS[k].f[1]); if (have) item_add(p, ITEMS[k].f[1], -have, "despawn"); }
                appendf(cp("participants.txt"), "DESPAWN|%s|day=%ld\n", e->name, cur_day()); } }
        if (nftw(e->path, rm_cb, 16, FTW_DEPTH | FTW_PHYS)) { err("stop: could not remove %s", e->path); skipped++; continue; }
        appendf(cp("manifest.txt"), "DESPAWN|%s|day=%ld\n", e->name, cur_day()); hist("DESPAWN|%ld|%s|%s", cur_day(), e->kind, e->name); gone++; }
    kv_set(VARS(), "setup_done", 0);
    control("STOP|day=%ld|despawned=%d|skipped=%d", cur_day(), gone, skipped);
    printf("stop despawned=%d skipped=%d\n", gone, skipped); return skipped ? 1 : 0;
}
static int v_daytick(void);
static int v_days(long n) { int rc = 0; for (long i = 0; i < n && !rc; i++) rc = v_daytick(); return rc; }
static int v_total(const char *item) { parts_load(); long t = 0; for (int i = 0; i < nPARTS; i++) if (!PARTS[i].retired) t += item_n(&PARTS[i], item); printf("%ld\n", t); return 0; }
static int v_count(void) { ents_load(); int c[8] = { 0 }; const char *k[] = { "participant", "house", "chicken", "plant", NULL }; int other = 0;
    for (int i = 0; i < nENTS; i++) { int f = 0; for (int j = 0; k[j]; j++) if (!strcmp(ENTS[i].kind, k[j])) { c[j]++; f = 1; } if (!f) other++; }
    printf("ENTS|total=%d|participant=%d|house=%d|chicken=%d|plant=%d|other=%d\n", nENTS, c[0], c[1], c[2], c[3], other); return 0; }

static void weather_roll(long seed, long d) {
    long pct = kv_get(WEA, "rain_pct", 30); int rain = (long)(mix(seed, d, 0, 3) % 100) < pct;
    kv_set(VARS(), "rain_today", rain); if (rain) kv_set(VARS(), "rain_days", kv_get(VARS(), "rain_days", 0) + 1);
    hist("WEATHER|%ld|%s", d, rain ? "rain" : "clear");
}
/* the world's day: animals (fed -> fed_days, a product every lay_every fed days to the keeper, evolution when its data row is met), house wear */
static void world_tick(long d) {
    char kind[48];
    for (int i = 0; i < nENTS; i++) { Ent *e = &ENTS[i]; if (!animal_kind(e, kind, sizeof kind)) continue; const Row *A = animal_row(kind); if (!A) continue;
        if (evar(e, "fed")) { long fd = evar(e, "fed_days") + 1, le = rkl(A, "lay_every", 0); evar_set(e, "fed", 0);
            if (le > 0 && fd >= le) { const char *prod = rkv(A, "product", "-"); char kn[48]; txt_get(jp(e->path, "keeper.txt"), kn, sizeof kn); Part *k = part_find(kn);
                if (strcmp(prod, "-") && k && !k->retired) { item_add(k, prod, 1, "lay"); hist("LAY|%ld|%s|%s|%s|keeper=%s", d, e->name, kind, prod, kn); evar_set(e, "laid", evar(e, "laid") + 1); } fd = 0; }
            evar_set(e, "fed_days", fd); }
        for (int r = 0; r < nEVOLVE; r++) { const Row *V = &EVOLVE[r]; if (V->n < 2 || strcmp(V->f[1], kind)) continue;
            if (evar(e, "meals") >= rkl(V, "requires_meals", 1 << 30) && d >= rkl(V, "requires_day", 0)) { const char *to = rkv(V, "to", ""); const Row *B = animal_row(to);
                if (to[0] && B) { txt_set(jp(e->path, "kind.txt"), to); hist("EVOLVE|%ld|%s|%s|%s|rideable=%ld", d, e->name, kind, to, rkl(B, "rideable", 0)); } } break; } }
    Ent *h = ent_first("house");
    if (h) { long dd = tun("house_decay_days", 3); if (dd > 0 && d % dd == 0 && evar(h, "condition") > 0) { evar_set(h, "condition", evar(h, "condition") - 1); hist("HOUSE|%ld|%s|condition=%ld", d, h->name, evar(h, "condition")); } }
}
static int v_daytick(void) {
    if (!kv_get(VARS(), "setup_done", 0)) { appendf(cp("eden_history.txt"), "SKIP|not set up\n"); err("daytick: not set up"); return 1; }
    long d = cur_day() + 1, seed = kv_get(VARS(), "seed", 0); kv_set(VARS(), "day", d); parts_load();
    hist("DAY|%ld", d); weather_roll(seed, d); long rain = kv_get(VARS(), "rain_today", 0);
    const Row *cr = crop_row(); long gdays = cr ? rkl(cr, "growth_days", 4) : 4;
    if (nGARDEN) for (int pl = 1; pl <= nGARDEN; pl++) {     /* v1 shared garden: grown once per day, before anyone acts */
        if (plot_get(NULL, pl, "state") != 1) continue;
        if (rain) plot_set(NULL, pl, "wet", 1);
        if (plot_get(NULL, pl, "wet")) { long g = plot_get(NULL, pl, "grown") + 1; plot_set(NULL, pl, "grown", g); plot_set(NULL, pl, "wet", 0); hist("GROW|%ld|garden|plot=%d|grown=%ld|by=%s", d, pl, g, rain ? "rain" : "water");
            if (g >= gdays) { plot_set(NULL, pl, "state", 2); hist("RIPE|%ld|garden|plot=%d", d, pl); } } }
    world_tick(d);
    for (int i = 0; i < nPARTS; i++) { Part *me = &PARTS[i]; if (me->retired) continue;
        /* 1 hunger */
        long h = var_n(me, "hunger") + tun("hunger_step", 1), mh = tun("max_hunger", 10); if (h > mh) h = mh; var_set(me, "hunger", h);
        long hp = kv_get(ef(me, "actor_1_stats.txt"), "hp", 1);
        if (h >= tun("starving_at", 8)) { long loss = tun("hp_loss", 1); hp = hp - loss < 1 ? 1 : hp - loss; kv_set(ef(me, "actor_1_stats.txt"), "hp", hp); }
        hist("NEED|%ld|%s|hunger=%ld|hp=%ld", d, me->name, h, hp);
        /* 2 weather + 3 crops */
        long mp = nGARDEN ? 0 : tun("max_plots", 3); char k[64];
        for (int pl = 1; pl <= mp; pl++) { snprintf(k, sizeof k, "plot_%d_state", pl); if (var_n(me, k) != 1) continue;
            char kw[64], kg[64]; snprintf(kw, sizeof kw, "plot_%d_wet", pl); snprintf(kg, sizeof kg, "plot_%d_grown", pl);
            if (rain) var_set(me, kw, 1);
            if (var_n(me, kw)) { long g = var_n(me, kg) + 1; var_set(me, kg, g); var_set(me, kw, 0); hist("GROW|%ld|%s|plot=%d|grown=%ld|by=%s", d, me->name, pl, g, rain ? "rain" : "water");
                if (g >= gdays) { var_set(me, k, 2); hist("RIPE|%ld|%s|plot=%d", d, me->name, pl); } } }
        /* 4 agent seat */
        Part *pa = partner_of(me); const char *pick = NULL;
        if (h >= tun("hungry_at", 4) && act_ok(me, pa, "eat")) pick = "eat";
        else { long tot = 0, w[16]; int ok[16], n = 0; char wk[64];
            for (n = 0; ACTS[n]; n++) { ok[n] = act_ok(me, pa, ACTS[n]); snprintf(wk, sizeof wk, "w_%s", ACTS[n]); w[n] = ok[n] ? kv_get(WGT, wk, 1) : 0; if (w[n] < 0) w[n] = 0; tot += w[n]; }
            if (tot > 0) { long r = (long)(mix(seed, d, me->ord, 1) % (uint64_t)tot); for (int a = 0; a < n; a++) { if (r < w[a]) { pick = ACTS[a]; break; } r -= w[a]; } }
            if (!pick) pick = "rest"; }
        do_act(me, pa, pick); }
    return 0;
}

static int v_acts(const char *who, int write_menu) {
    parts_load(); Part *me = part_find(who); if (!me) { err("acts: unknown %s", who); return 1; }
    Part *pa = partner_of(me); char out[2048] = "", menu[8192] = ""; FILE *mf = write_menu ? fopen(ef(me, "acts_menu.pdl"), "w") : NULL;
    if (mf) fprintf(mf, "SECTION      | KEY                | VALUE\n----------------------------------------\n");
    for (int i = 0; ACTS[i]; i++) if (act_ok(me, pa, ACTS[i])) { strcat(out, ACTS[i]); strcat(out, "\n");
        if (mf) fprintf(mf, "METHOD       | Act: %-10s | sh -c 'exec \"$0/../../conductor/ops/+x/eden_op.+x\" \"$0/../../conductor\" act %s %s'\n", ACTS[i], me->name, ACTS[i]); }
    (void)menu; if (mf) fclose(mf);
    FILE *f = fopen(ef(me, "acts.txt"), "w"); if (f) { fputs(out, f); fclose(f); }
    fputs(out, stdout); return 0;
}
static int v_act(const char *who, const char *act) {
    parts_load(); Part *me = part_find(who); if (!me || me->retired) { err("act: no active %s", who); return 1; }
    int r = do_act(me, partner_of(me), act); if (r) { err("act %s %s: not available", who, act); return 1; } printf("act %s %s ok\n", who, act); return 0;
}

/* save / load through game_snapshot_op */
static int snap_args(char **av, int *n, int load, const char *slot, char *store, char *tree, char *gso, char *clk[3], char *dest) {
    snprintf(gso, P, "%s", cp(wiring("snapshot", ""))); snprintf(store, P, "%s", cp(wiring("store", "../../store"))); snprintf(tree, P, "%s", cp(wiring("tree", "..")));
    char cdir[P]; snprintf(cdir, P, "%s/#.desktop/clocks", cp(wiring("house", "../..")));
    const char *cid = wiring("clock", "g1"); char names[3][P]; snprintf(names[0], P, "%s/%s.pdl", cdir, cid); snprintf(names[1], P, "%s/reminders.pdl", cdir); snprintf(names[2], P, "%s/schedule_ledger.txt", cdir);
    int k = 0; av[k++] = gso; av[k++] = store; av[k++] = tree;
    if (!load) { av[k++] = "save"; av[k++] = (char *)slot; for (int i = 0; i < 3; i++) if (exists(names[i])) { snprintf(clk[i], P, "%s", names[i]); av[k++] = "--extra"; av[k++] = clk[i]; } }
    else { snprintf(dest, P, "%s", tree); av[k++] = "restore"; av[k++] = (char *)slot; av[k++] = dest; av[k++] = "--apply"; av[k++] = "--prune"; av[k++] = "--extra-dir"; snprintf(clk[0], P, "%s", cdir); av[k++] = clk[0]; }
    av[k] = NULL; *n = k; return 0;
}
/* slots: n = 1..max_slots. The index slots.pdl lives OUTSIDE the game tree (a Load must not rewind it), append-only: SLOT | n | id=<checkpoint> | rev=<k> | day=<d> | label=Day <d>.
 * The newest row of a slot wins. `save` uses id slot<n> (game_snapshot_op refuses an id that exists, so a used slot is refused); `resave` writes revision k+1 as slot<n>_r<k+1>, keeping the old one. */
static char *slots_path(void) { return cp(wiring("slots_index", "../../slots.pdl")); }
static int slot_ok(const char *num) { if (!num || !num[0]) return 0; for (const char *c = num; *c; c++) if (*c < '0' || *c > '9') return 0; long v = atol(num); return v >= 1 && v <= tun("max_slots", 10) && strlen(num) <= 3; }
static int slot_last(long n, char *id, size_t idn, long *rev, long *day) {
    static Row R[1200]; int c = rows_load(slots_path(), R, 1200), found = 0;
    for (int i = 0; i < c; i++) if (!strcmp(R[i].f[0], "SLOT") && R[i].n >= 3 && atol(R[i].f[1]) == n) { found = 1; snprintf(id, idn, "%s", rkv(&R[i], "id", "")); *rev = rkl(&R[i], "rev", 1); *day = rkl(&R[i], "day", 0); }
    return found; }
static int v_snap(int load, const char *num, int resave) {
    if (!wiring("snapshot", "")[0]) { err("no snapshot wiring"); return 1; }
    if (!slot_ok(num)) { err("%s refused: slot must be a number 1..%ld (got '%s')", load ? "load" : "save", tun("max_slots", 10), num ? num : ""); return 1; }
    long n = atol(num), rev = 0, sday = 0; char last[64] = "", slot[64], store[P], tree[P], gso[P], dest[P], c0[P], c1[P], c2[P]; char *clk[3] = { c0, c1, c2 }; char *av[24]; int k;
    int have = slot_last(n, last, sizeof last, &rev, &sday);
    if (load) { if (have && last[0]) snprintf(slot, sizeof slot, "%s", last); else snprintf(slot, sizeof slot, "slot%ld", n); }
    else if (resave && have) { snprintf(slot, sizeof slot, "slot%ld_r%ld", n, rev + 1); }
    else snprintf(slot, sizeof slot, "slot%ld", n);
    snap_args(av, &k, load, slot, store, tree, gso, clk, dest);
    long d = cur_day(); int rc = run_argv(av);
    control("%s|%s|day_before=%ld|rc=%d", load ? "LOAD" : "SAVE", slot, d, rc);
    if (rc) { err("%s %s failed rc=%d", load ? "load" : "save", slot, rc); return 1; }
    if (!load) appendf(slots_path(), "SLOT | %ld | id=%s | rev=%ld | day=%ld | label=Day %ld\n", n, slot, (resave && have) ? rev + 1 : 1, d, d);
    printf("%s %s ok\n", load ? "load" : "save", slot); return 0;
}
static int v_slots(void) {
    static Row R[1200]; int c = rows_load(slots_path(), R, 1200); long max = tun("max_slots", 10);
    for (long n = 1; n <= max; n++) { int at = -1; for (int i = 0; i < c; i++) if (!strcmp(R[i].f[0], "SLOT") && R[i].n >= 3 && atol(R[i].f[1]) == n) at = i;
        if (at >= 0) printf("slot %ld | %s | %s | %s\n", n, rkv(&R[at], "id", ""), rkv(&R[at], "day", ""), rkv(&R[at], "label", "")); }
    return 0; }
/* slot.txt cursor: the menu rows that cannot take an argument act on it */
static char *slot_file(void) { return cp(wiring("slot_file", "../../slot.txt")); }
static long slot_cur(void) { char b[32]; if (!txt_get(slot_file(), b, sizeof b)) return 1; long v = atol(b); return (v >= 1 && v <= tun("max_slots", 10)) ? v : 1; }
static int v_slot(const char *arg) {
    long max = tun("max_slots", 10), c = slot_cur();
    if (arg) { if (!strcmp(arg, "next")) c = c % max + 1; else if (!strcmp(arg, "prev")) c = c == 1 ? max : c - 1;
        else if (!strcmp(arg, "+10")) c = (c - 1 + 10) % max + 1; else if (!strcmp(arg, "-10")) c = (c - 1 + max - 10) % max + 1;
        else if (slot_ok(arg)) c = atol(arg); else { err("slot refused: '%s'", arg); return 1; }
        char b[16]; snprintf(b, sizeof b, "%ld", c); txt_set(slot_file(), b); }
    printf("%ld\n", c); return 0; }
/* generated menus: max_slots/10 blocks of 10 slots (Save then Load); toolbar = pdl rows for the taskbar's <prefix>_menu_<N>_label/_cmd reader, methods = entity-menu METHOD rows */
static int v_gen(FILE *o, const char *kind, const char *prefix, long base, const char *condir) {
    long max = tun("max_slots", 10), per = 10, N = base; int toolbar = !strcmp(kind, "toolbar");
    if (toolbar) fprintf(o, "# toolbar_slots.pdl - generated by `eden_op <conductor> gen-slots toolbar` (do not hand edit; the harness compares it with a fresh run).\n"
        "# Rows for the taskbar's pdl-driven menu reader (livedesk_pdl_menu_rows): SECTION | <prefix>_menu_<N>_label | text  and  SECTION | <prefix>_menu_<N>_cmd | command.\n"
        "# Each cmd is a bare shell string (no single quote: the taskbar wraps it in sh -c '...'); ' .' is replaced by the house root by the taskbar, so it runs from any cwd.\n"
        "# max_slots/10 blocks x (header + 10 Save + 10 Load) + cancel (max_slots is a tunable, default 10). See eden/README.md for what the taskbar needs.\n");
    else fprintf(o, "# slots_methods.pdl - generated by `eden_op <conductor> gen-slots methods`: 2 x max_slots METHOD rows for an entity menu that can hold them (paste into meta.pdl). $0 = the conductor dir.\n");
    for (long p = 0; p < (max + per - 1) / per; p++) {
        if (toolbar) { fprintf(o, "SECTION | %s_menu_%ld_label | -- slots %ld-%ld --\nSECTION | %s_menu_%ld_cmd | \n", prefix, N, p * per + 1, (p + 1) * per > max ? max : (p + 1) * per, prefix, N); N++; }
        for (int ld = 0; ld < 2; ld++) for (long j = 1; j <= per; j++) { long sl = p * per + j; if (sl > max) break; const char *v = ld ? "load" : "save", *V = ld ? "Load" : "Save";
            if (toolbar) { fprintf(o, "SECTION | %s_menu_%ld_label | %s %ld\nSECTION | %s_menu_%ld_cmd | cd . && %s/ops/+x/eden_op.+x . %s %ld\n", prefix, N, V, sl, prefix, N, condir, v, sl); N++; }
            else fprintf(o, "METHOD       | %s %ld | sh -c 'exec \"$0/ops/+x/eden_op.+x\" \"$0\" %s %ld'\n", V, sl, v, sl); } }
    if (toolbar) fprintf(o, "SECTION | %s_menu_%ld_label | cancel\nSECTION | %s_menu_%ld_cmd | \n", prefix, N, prefix, N);
    return 0; }

/* gen-check: the generated menu must equal the committed file byte for byte */
static int v_gen_check(const char *kind, const char *file, const char *prefix, long base, const char *condir) {
    FILE *t = tmpfile(); if (!t) return 1; v_gen(t, kind, prefix, base, condir); rewind(t); FILE *f = fopen(file, "r"); if (!f) { printf("NOFILE\n"); fclose(t); return 1; }
    int a, b, line = 1, same = 1; while (1) { a = fgetc(t); b = fgetc(f); if (a != b) { same = 0; break; } if (a == EOF) break; if (a == '\n') line++; }
    fclose(t); fclose(f); if (same) printf("MATCH\n"); else printf("DIFF at line %d\n", line); return same ? 0 : 1; }

/* status, audit, digest, toyread */
static int v_status(void) {
    parts_load(); FILE *f = fopen(cp("status.txt"), "w"); if (!f) return 1;
    fprintf(f, "STATUS|day=%ld|length=%ld|seed=%ld|running=%ld|setup_done=%ld|rain_days=%ld|over=%d\n", cur_day(), kv_get(VARS(), "length", 0), kv_get(VARS(), "seed", 0),
            kv_get(VARS(), "running", 0), kv_get(VARS(), "setup_done", 0), kv_get(VARS(), "rain_days", 0), cur_day() >= kv_get(VARS(), "length", 1 << 30));
    for (int i = 0; i < nPARTS; i++) { Part *p = &PARTS[i]; fprintf(f, "PART|%s|%s|id=%s|retired=%d|hunger=%ld|hp=%ld|mhp=%ld|shelter=%ld", p->name, p->kit, p->id, p->retired, var_n(p, "hunger"),
            kv_get(ef(p, "actor_1_stats.txt"), "hp", 0), kv_get(ef(p, "actor_1_stats.txt"), "mhp", 0), var_n(p, "shelter"));
        for (int k = 0; k < nITEMS; k++) if (ITEMS[k].n >= 2) fprintf(f, "|%s=%ld", ITEMS[k].f[1], item_n(p, ITEMS[k].f[1]));
        fprintf(f, "\n"); }
    garden_load();
    for (int i = 0; i < nENTS; i++) { Ent *e = &ENTS[i]; if (!strcmp(e->kind, "participant")) continue; char kn[48] = "";
        fprintf(f, "ENT|%s|%s|id=%s", e->name, e->kind, e->id); if (txt_get(jp(e->path, "kind.txt"), kn, sizeof kn)) { const Row *A = animal_row(kn); fprintf(f, "|kind=%s|fed=%ld|meals=%ld|laid=%ld|rideable=%ld", kn, evar(e, "fed"), evar(e, "meals"), evar(e, "laid"), A ? rkl(A, "rideable", 0) : 0); }
        else if (!strcmp(e->kind, "house")) fprintf(f, "|condition=%ld", evar(e, "condition")); else if (!strcmp(e->kind, "plant")) fprintf(f, "|state=%ld|grown=%ld|wet=%ld", evar(e, "state"), evar(e, "grown"), evar(e, "wet"));
        fprintf(f, "\n"); }
    fclose(f); FILE *g = fopen(cp("status.txt"), "r"); char ln[1024]; while (g && fgets(ln, sizeof ln, g)) fputs(ln, stdout); if (g) fclose(g); return 0;
}
static int v_audit(void) {
    parts_load(); FILE *f = fopen(cp("eden_history.txt"), "r"); char ln[1024]; long sums[MR] = { 0 }, trade[MR] = { 0 }, eat_rows = 0, eat_items = 0; int bad = 0;
    while (f && fgets(ln, sizeof ln, f)) { trim(ln); char *fl[8]; int n = 0; char *p = ln; while (n < 8) { fl[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
        if (n >= 6 && !strcmp(fl[0], "ITEM")) for (int i = 0; i < nITEMS; i++) if (ITEMS[i].n >= 2 && !strcmp(ITEMS[i].f[1], fl[3])) { long dl = atol(fl[4]); sums[i] += dl;
            if (!strcmp(fl[5], "trade")) trade[i] += dl; if (!strcmp(fl[5], "eat")) eat_items += dl; }
        if (n >= 4 && !strcmp(fl[0], "ACT") && !strcmp(fl[3], "eat")) eat_rows++; }
    if (f) fclose(f);
    long food_now = 0, food_exp = 0;
    for (int i = 0; i < nITEMS; i++) { if (ITEMS[i].n < 2) continue; long now = 0; for (int k = 0; k < nPARTS; k++) { long v = item_n(&PARTS[k], ITEMS[i].f[1]); if (v < 0) { printf("BAD|negative item %s of %s=%ld\n", ITEMS[i].f[1], PARTS[k].name, v); bad++; } now += v; }
        if (now != sums[i]) { printf("BAD|item %s sum now=%ld ledger=%ld\n", ITEMS[i].f[1], now, sums[i]); bad++; }
        if (trade[i] != 0) { printf("BAD|trade not zero-sum for %s (%ld)\n", ITEMS[i].f[1], trade[i]); bad++; }
        if (rkl(&ITEMS[i], "nutrition", 0) > 0) { food_now += now; food_exp += sums[i]; } }
    if (eat_rows != -eat_items) { printf("BAD|eat rows %ld vs item rows %ld\n", eat_rows, -eat_items); bad++; }
    for (int k = 0; k < nPARTS; k++) { long h = var_n(&PARTS[k], "hunger"), hp = kv_get(ef(&PARTS[k], "actor_1_stats.txt"), "hp", 1), mhp = kv_get(ef(&PARTS[k], "actor_1_stats.txt"), "mhp", 1);
        if (h < 0 || h > tun("max_hunger", 10)) { printf("BAD|hunger out of bounds %s=%ld\n", PARTS[k].name, h); bad++; }
        if (hp < 1 || hp > mhp) { printf("BAD|hp out of bounds %s hp=%ld mhp=%ld\n", PARTS[k].name, hp, mhp); bad++; } }
    printf("AUDIT|ok=%d|bad=%d|food_now=%ld|food_ledger=%ld|eats=%ld\n", bad == 0, bad, food_now, food_exp, eat_rows); return bad ? 1 : 0;
}
static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static char *FILES[8192]; static int nFILES;
static void walk(const char *dir, const char *rel) {
    DIR *d = opendir(dir); if (!d) return; struct dirent *e;
    while ((e = readdir(d))) { if (e->d_name[0] == '.' ) continue; char full[P], r[P]; snprintf(full, P, "%s/%s", dir, e->d_name); snprintf(r, P, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        struct stat st; if (lstat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (!strcmp(r, "conductor/ops")) continue; walk(full, r); }
        else if (S_ISREG(st.st_mode)) { if (!strcmp(e->d_name, "event_page_ledger.txt") || !strcmp(e->d_name, "event_page_out.txt") || !strcmp(e->d_name, "wiring.pdl") || !strcmp(e->d_name, "install.txt")) continue; if (nFILES < 8192) FILES[nFILES++] = strdup(r); } }
    closedir(d);
}
static int v_digest(void) {
    char root[P]; snprintf(root, P, "%s", cp(wiring("tree", ".."))); char rr[P]; if (realpath(root, rr)) snprintf(root, P, "%s", rr);
    nFILES = 0; walk(root, ""); qsort(FILES, nFILES, sizeof(char *), cmpstr); uint64_t h = 1469598103934665603ULL; long bytes = 0;
    for (int i = 0; i < nFILES; i++) { for (const char *c = FILES[i]; *c; c++) { h ^= (unsigned char)*c; h *= 1099511628211ULL; } h ^= 0xff; h *= 1099511628211ULL;
        FILE *f = fopen(jp(root, FILES[i]), "r"); int ch; while (f && (ch = fgetc(f)) != EOF) { h ^= (unsigned char)ch; h *= 1099511628211ULL; bytes++; } if (f) fclose(f); h ^= 0xfe; h *= 1099511628211ULL; }
    printf("%016llx files=%d bytes=%ld\n", (unsigned long long)h, nFILES, bytes); return 0;
}
/* the taskbar's read_key_value (khtpm_taskbar_manager.c line ~1735), same rules */
static int v_toyread(const char *path, const char *key) {
    FILE *f = fopen(path, "r"); char line[4096], out[1024] = ""; if (!f) return 1;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "SECTION", 7) == 0) { char *after = line + 7; while (*after == ' ' || *after == '\t') after++;
            if (*after == '|') { after++; while (*after == ' ' || *after == '\t') after++; if (strncmp(after, "KEY", 3) == 0) continue; } else continue; }
        if (strncmp(line, "META", 4) == 0) continue;
        char *p = strstr(line, key); if (!p) continue; char *eq = strchr(p, '='), *bar = strrchr(p, '|'), *v = NULL;
        if (eq && (!bar || eq < bar)) v = eq + 1; else if (bar) v = bar + 1; if (!v) continue;
        while (*v == ' ' || *v == '\t') v++; v[strcspn(v, "\r\n")] = 0; size_t n = strlen(v); while (n > 0 && (v[n-1] == ' ' || v[n-1] == '\t')) v[--n] = 0;
        if (v[0]) { snprintf(out, sizeof out, "%s", v); break; } }
    fclose(f); printf("%s\n", out); return out[0] ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: eden_op <conductor_dir> <verb> [args]\n"); return 2; }
    /* the argument may be the conductor dir, or a HOUSE root holding game/conductor (what a taskbar menu row can pass: ' .' becomes the house root) */
    { char probe[P]; snprintf(probe, sizeof probe, "%s/game.pdl", argv[1]); if (!exists(probe)) { snprintf(probe, sizeof probe, "%s/game/conductor/game.pdl", argv[1]);
        if (exists(probe)) { snprintf(probe, sizeof probe, "%s/game/conductor", argv[1]); if (!realpath(probe, C)) return 2; goto resolved; } } }
    if (!realpath(argv[1], C)) { fprintf(stderr, "eden_op: no conductor dir %s\n", argv[1]); return 2; }
resolved:
    const char *v = argv[2], *a1 = argc > 3 ? argv[3] : NULL, *a2 = argc > 4 ? argv[4] : NULL;
    if (!strcmp(v, "toyread") && a1 && a2) return v_toyread(a1, a2);
    load_data();
    if (!strcmp(v, "setup")) return v_setup();
    if (!strcmp(v, "reset")) return v_reset();
    if (!strcmp(v, "start") || !strcmp(v, "pause") || !strcmp(v, "resume")) return v_ctl(v);
    if (!strcmp(v, "add")) return v_add(a1);
    if (!strcmp(v, "remove")) return v_remove(a1);
    if (!strcmp(v, "daytick")) return v_daytick();
    if (!strcmp(v, "acts") && a1) return v_acts(a1, 0);
    if (!strcmp(v, "menu") && a1) return v_acts(a1, 1);
    if (!strcmp(v, "act") && a1 && a2) return v_act(a1, a2);
    if ((!strcmp(v, "save") || !strcmp(v, "load") || !strcmp(v, "resave")) && a1) return v_snap(!strcmp(v, "load"), a1, !strcmp(v, "resave"));
    if (!strcmp(v, "save-slot") || !strcmp(v, "load-slot") || !strcmp(v, "resave-slot")) { char b[16]; snprintf(b, sizeof b, "%ld", slot_cur()); return v_snap(!strcmp(v, "load-slot"), b, !strcmp(v, "resave-slot")); }
    if (!strcmp(v, "slot")) return v_slot(a1);
    if (!strcmp(v, "slots")) return v_slots();
    if (!strcmp(v, "gen-slots") && a1) return v_gen(stdout, a1, argc > 4 ? argv[4] : "eden", argc > 5 ? atol(argv[5]) : 1, argc > 6 ? argv[6] : "game/conductor");
    if (!strcmp(v, "gen-check") && a1 && a2) return v_gen_check(a1, a2, argc > 5 ? argv[5] : "eden", argc > 6 ? atol(argv[6]) : 1, argc > 7 ? argv[7] : "game/conductor");
    if (!strcmp(v, "daemon-start")) return v_daemon_start();
    if (!strcmp(v, "daemon-stop")) return v_daemon_stop();
    if (!strcmp(v, "daemon-status")) return v_daemon_status();
    if (!strcmp(v, "nextday")) return v_nextday();
    if (!strcmp(v, "begin")) return v_begin();
    if (!strcmp(v, "stop")) return v_stop();
    if (!strcmp(v, "days") && a1) return v_days(atol(a1));
    if (!strcmp(v, "total") && a1) return v_total(a1);
    if (!strcmp(v, "count")) return v_count();
    if (!strcmp(v, "status")) return v_status();
    if (!strcmp(v, "audit")) return v_audit();
    if (!strcmp(v, "digest")) return v_digest();
    fprintf(stderr, "eden_op: unknown verb or missing argument: %s\n", v); return 2;
}
