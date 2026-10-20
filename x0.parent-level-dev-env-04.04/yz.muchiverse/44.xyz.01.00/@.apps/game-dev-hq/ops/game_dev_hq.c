/* game-dev-hq — one studio week. See 16.game/GAME-DEV-HQ-DESIGN.md.
 * Verbs: hire train rest start assign advance ship status
 * Resident (house, app-dir) refreshes state/ui.txt. A verb is a short run.
 */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAXS 8
#define HIRE_COST 20000
#define OFFICE_CAP 4

/* Dest must be larger than the source plus the longest leaf, or glibc
 * fortify aborts snprintf("%s/studio.txt", ST) when both are 512. */
static char APP[1024];
static char ST[1088];

typedef struct {
    char id[8], name[16], job[16];
    int scenario, graphics, programming, sound, creativity, energy, salary;
    int used;
} Staff;

typedef struct {
    char id[8], title[24], genre[16], type[16], phase[16], assigned[64];
    int fun, creativity, polish, graphics, sound;
    int open; /* 1 = the game in progress */
} Game;

static Staff staff[MAXS];
static int nstaff;
static Game games[8];
static int ngames;
static long funds, fame, week, hype;
static int office;
static char note[240];

static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }

static void split(char *s, char **f, int max, int *n) {
    *n = 0;
    f[(*n)++] = s;
    for (char *p = s; *p && *n < max; p++) if (*p == '|') { *p = 0; f[(*n)++] = p + 1; }
    for (int i = 0; i < *n; i++) {
        char *a = f[i]; while (*a == ' ') a++;
        char *b = a + strlen(a); while (b > a && (b[-1] == ' ' || b[-1] == '\n' || b[-1] == '\r')) *--b = 0;
        f[i] = a;
    }
}

static int *stat_ptr(Staff *s, const char *name) {
    if (!strcmp(name, "scenario")) return &s->scenario;
    if (!strcmp(name, "graphics")) return &s->graphics;
    if (!strcmp(name, "programming")) return &s->programming;
    if (!strcmp(name, "sound")) return &s->sound;
    if (!strcmp(name, "creativity")) return &s->creativity;
    return NULL;
}

static Staff *find_staff(const char *id) {
    for (int i = 0; i < nstaff; i++) if (!strcmp(staff[i].id, id)) return &staff[i];
    return NULL;
}

static Game *open_game(void) {
    for (int i = 0; i < ngames; i++) if (games[i].open) return &games[i];
    return NULL;
}

static void log_line(const char *s) {
    char keep[400]; snprintf(keep, sizeof keep, "%s", s);
    char p[1200]; snprintf(p, sizeof p, "%s/chat.txt", ST);
    FILE *f = fopen(p, "a"); if (f) { fprintf(f, "W%ld %s\n", week, keep); fclose(f); }
    snprintf(note, sizeof note, "%s", keep);
}

static void save_all(void) {
    char p[1200]; FILE *f;
    snprintf(p, sizeof p, "%s/studio.txt", ST);
    f = fopen(p, "w"); if (f) { fprintf(f, "STUDIO | %ld | %ld | %d | %ld\n", funds, fame, office, week); fclose(f); }
    snprintf(p, sizeof p, "%s/hype.txt", ST);
    f = fopen(p, "w"); if (f) { fprintf(f, "%ld\n", hype); fclose(f); }
    snprintf(p, sizeof p, "%s/staff.pdl", ST);
    f = fopen(p, "w");
    if (f) {
        fprintf(f, "STAFF | id | name | job | scenario | graphics | programming | sound | creativity | energy | salary\n");
        for (int i = 0; i < nstaff; i++) {
            Staff *s = &staff[i];
            fprintf(f, "STAFF | %s | %s | %s | %d | %d | %d | %d | %d | %d | %d\n",
                s->id, s->name, s->job, s->scenario, s->graphics, s->programming, s->sound, s->creativity, s->energy, s->salary);
        }
        fclose(f);
    }
    snprintf(p, sizeof p, "%s/games.pdl", ST);
    f = fopen(p, "w");
    if (f) {
        fprintf(f, "GAME | id | title | genre | type | phase | fun | creativity | polish | graphics | sound | assigned | open\n");
        for (int i = 0; i < ngames; i++) {
            Game *g = &games[i];
            fprintf(f, "GAME | %s | %s | %s | %s | %s | %d | %d | %d | %d | %d | %s | %d\n",
                g->id, g->title, g->genre, g->type, g->phase, g->fun, g->creativity, g->polish, g->graphics, g->sound,
                g->assigned[0] ? g->assigned : "-", g->open);
        }
        fclose(f);
    }
    snprintf(p, sizeof p, "%s/note.txt", ST);
    f = fopen(p, "w"); if (f) { fprintf(f, "%s\n", note); fclose(f); }
}

static void load_all(void) {
    char p[1200], line[400]; FILE *f; nstaff = ngames = 0; funds = 100000; fame = 0; week = 1; hype = 0; office = 1; note[0] = 0;
    snprintf(p, sizeof p, "%s/studio.txt", ST);
    f = fopen(p, "r");
    if (f) {
        if (fgets(line, sizeof line, f)) {
            char *fl[8]; int n = 0; split(line, fl, 8, &n);
            if (n >= 5 && !strcmp(fl[0], "STUDIO")) { funds = atol(fl[1]); fame = atol(fl[2]); office = atoi(fl[3]); week = atol(fl[4]); }
        }
        fclose(f);
    }
    snprintf(p, sizeof p, "%s/staff.pdl", ST);
    f = fopen(p, "r");
    if (f) {
        while (fgets(line, sizeof line, f) && nstaff < MAXS) {
            char *fl[16]; int n = 0; split(line, fl, 16, &n);
            if (n < 11 || strcmp(fl[0], "STAFF") || !strcmp(fl[1], "id")) continue;
            Staff *s = &staff[nstaff++];
            snprintf(s->id, sizeof s->id, "%s", fl[1]); snprintf(s->name, sizeof s->name, "%s", fl[2]); snprintf(s->job, sizeof s->job, "%s", fl[3]);
            s->scenario = atoi(fl[4]); s->graphics = atoi(fl[5]); s->programming = atoi(fl[6]); s->sound = atoi(fl[7]); s->creativity = atoi(fl[8]);
            s->energy = atoi(fl[9]); s->salary = atoi(fl[10]); s->used = 1;
        }
        fclose(f);
    }
    snprintf(p, sizeof p, "%s/games.pdl", ST);
    f = fopen(p, "r");
    if (f) {
        while (fgets(line, sizeof line, f) && ngames < 8) {
            char *fl[16]; int n = 0; split(line, fl, 16, &n);
            if (n < 13 || strcmp(fl[0], "GAME") || !strcmp(fl[1], "id")) continue;
            Game *g = &games[ngames++];
            snprintf(g->id, sizeof g->id, "%s", fl[1]); snprintf(g->title, sizeof g->title, "%s", fl[2]);
            snprintf(g->genre, sizeof g->genre, "%s", fl[3]); snprintf(g->type, sizeof g->type, "%s", fl[4]); snprintf(g->phase, sizeof g->phase, "%s", fl[5]);
            g->fun = atoi(fl[6]); g->creativity = atoi(fl[7]); g->polish = atoi(fl[8]); g->graphics = atoi(fl[9]); g->sound = atoi(fl[10]);
            snprintf(g->assigned, sizeof g->assigned, "%s", strcmp(fl[11], "-") ? fl[11] : "");
            g->open = atoi(fl[12]);
        }
        fclose(f);
    }
    snprintf(p, sizeof p, "%s/hype.txt", ST);
    f = fopen(p, "r"); if (f) { if (fscanf(f, "%ld", &hype) != 1) hype = 0; fclose(f); }
    snprintf(p, sizeof p, "%s/note.txt", ST);
    f = fopen(p, "r"); if (f) { if (fgets(note, sizeof note, f)) note[strcspn(note, "\n")] = 0; fclose(f); }
}

static void seed(void) {
    if (nstaff) return;
    const char *rows[][3] = { {"s1","Rex","programmer"}, {"s2","Pip","writer"}, {"s3","Gus","artist"} };
    int stats[][5] = { {40,30,80,20,50}, {80,20,20,20,60}, {20,80,20,30,40} };
    for (int i = 0; i < 3; i++) {
        Staff *s = &staff[nstaff++];
        snprintf(s->id, 8, "%s", rows[i][0]); snprintf(s->name, 16, "%s", rows[i][1]); snprintf(s->job, 16, "%s", rows[i][2]);
        s->scenario = stats[i][0]; s->graphics = stats[i][1]; s->programming = stats[i][2]; s->sound = stats[i][3]; s->creativity = stats[i][4];
        s->energy = 80; s->salary = 2000; s->used = 1;
    }
    snprintf(note, sizeof note, "Week 1. Hire, train, start a game, assign, advance, ship.");
    save_all();
}

static void pay_week(void) {
    long pay = 0;
    for (int i = 0; i < nstaff; i++) pay += staff[i].salary;
    funds -= pay;
    week++;
}

static int job_add(Staff *s, int scale) {
    if (s->energy <= 0) return 0;
    int v = 0;
    if (!strcmp(s->job, "writer")) v = s->scenario;
    else if (!strcmp(s->job, "designer")) v = s->creativity;
    else if (!strcmp(s->job, "programmer")) v = s->programming;
    else if (!strcmp(s->job, "artist")) v = s->graphics;
    else if (!strcmp(s->job, "sound")) v = s->sound;
    else v = (s->scenario + s->graphics + s->programming + s->sound + s->creativity) / 5;
    return v * s->energy / 100 * scale / 100;
}

static void apply_phase(Game *g, int debug) {
    int scale = debug ? 50 : 100;
    char buf[64]; snprintf(buf, sizeof buf, "%s", g->assigned);
    for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
        Staff *s = find_staff(t); if (!s) continue;
        int add = job_add(s, scale); if (!add) continue;
        if (!strcmp(s->job, "writer")) g->fun += add;
        else if (!strcmp(s->job, "designer")) g->creativity += add;
        else if (!strcmp(s->job, "programmer")) g->polish += add;
        else if (!strcmp(s->job, "artist")) g->graphics += add;
        else if (!strcmp(s->job, "sound")) g->sound += add;
        else { int d = add / 5; g->fun += d; g->creativity += d; g->polish += d; g->graphics += d; g->sound += d; }
        s->energy -= 10; if (s->energy < 0) s->energy = 0;
    }
}

static void event_row(const char *verb, const char *target) {
    char p[1200]; snprintf(p, sizeof p, "%s/events.txt", ST);
    FILE *f = fopen(p, "a"); if (!f) return;
    fprintf(f, "EVENT|%ld|studio|%s|%s|-|queued\n", (long)time(NULL), verb, target);
    fclose(f);
}

static int flag_get(const char *name, int def) {
    char p[1200]; snprintf(p, sizeof p, "%s/flag_%s.txt", ST, name);
    FILE *f = fopen(p, "r"); if (!f) return def;
    int v = def; if (fscanf(f, "%d", &v) != 1) v = def; fclose(f); return v;
}
static void flag_set(const char *name, int v) {
    char p[1200]; snprintf(p, sizeof p, "%s/flag_%s.txt", ST, name);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", v); fclose(f); }
}
static int g_active;
static void read_active(void) {
    char p[1200]; snprintf(p, sizeof p, "%s/active.txt", ST);
    FILE *f = fopen(p, "r"); g_active = 0; if (f) { fscanf(f, "%d", &g_active); fclose(f); }
    if (g_active < 0 || g_active >= nstaff) g_active = 0;
}
static void write_active(void) {
    char p[1200]; snprintf(p, sizeof p, "%s/active.txt", ST);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", g_active); fclose(f); }
}

static void write_ui(void) {
    char p[1200], tmp[1200]; snprintf(p, sizeof p, "%s/ui.txt", ST); snprintf(tmp, sizeof tmp, "%s/ui.txt.tmp", ST);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    read_active();
    int ion = flag_get("int", 0), chat = flag_get("chat", 1), left = flag_get("left", 1), run = flag_get("running", 1);
    const char *who = nstaff ? staff[g_active].name : "nobody";
    fprintf(f, "title=game-dev-hq\n");
    fprintf(f, "canvas_raw=%s/scene.raw\n", ST);
    fprintf(f, "funds=%ld\nfame=%ld\nweek=%ld\nhype=%ld\noffice=%d\n", funds, fame, week, hype, office);
    fprintf(f, "cash_label=Cash %ld\nfame_label=Fame %ld\n", funds, fame);
    fprintf(f, "studio_line=funds %ld   fame %ld   hype %ld   office %d   week %ld\n", funds, fame, hype, office, week);
    fprintf(f, "note=%s\n", note[0] ? note : " ");
    Game *g = open_game();
    if (g) fprintf(f, "game_line=%s %s/%s  %s  fun %d creativity %d polish %d graphics %d sound %d  [%s]\n",
        g->title, g->genre, g->type, g->phase, g->fun, g->creativity, g->polish, g->graphics, g->sound, g->assigned[0] ? g->assigned : "nobody");
    else fprintf(f, "game_line=no game in progress\n");
    fprintf(f, "interact_class=%s\ninteract_label=%s\n", ion ? "interact-active" : "", ion ? who : "off");
    fprintf(f, "rp_h1=%s/interact_relay.txt\nrp_h2=%s/keyboard/history.txt\n", ST, ST);
    fprintf(f, "chat_visible=%d\nleft_visible=%d\n", chat, left);
    fprintf(f, "active_name=%s\nbook_label=%s\npage_label=Office\n", who, who);
    fprintf(f, "run_label=%s\nrun_cls=%s\n", run ? "GO started" : "STOP stopped", run ? "ph-green" : "ph-red");
    fprintf(f, "control_label=control: %s\n", who);
    fprintf(f, "n_staff=%d\nn_party=%d\n", nstaff, nstaff);
    for (int i = 0; i < nstaff; i++) {
        Staff *s = &staff[i];
        fprintf(f, "st_%d_id=%s\nst_%d_line=%s %s  %s  sc %d gr %d pr %d so %d cr %d  energy %d\n",
            i, s->id, i, s->name, s->id, s->job, s->scenario, s->graphics, s->programming, s->sound, s->creativity, s->energy);
        fprintf(f, "party_%d_id=%s\nparty_%d_label=%s\nparty_%d_cls=%s\n", i, s->id, i, s->name, i, i == g_active ? "ph-green" : "");
    }
    const char *jobs[] = {"writer","designer","programmer","artist","sound","director"};
    const char *names[] = {"Ada","Bea","Cal","Dee","Eve","Fay"};
    for (int i = 0; i < 6; i++) fprintf(f, "hire_%d_job=%s\nhire_%d_name=%s\nhire_%d_line=Hire %s (%s)  $%d\n", i, jobs[i], i, names[i], i, names[i], jobs[i], HIRE_COST);
    fprintf(f, "n_hire=%d\n", 6);
    char chatp[1200], lines[10][180]; int nln = 0;
    snprintf(chatp, sizeof chatp, "%s/chat.txt", ST);
    FILE *cf = fopen(chatp, "r");
    if (cf) {
        char b[180];
        while (fgets(b, sizeof b, cf)) { b[strcspn(b, "\n")] = 0; if (!b[0]) continue; snprintf(lines[nln % 10], 180, "%s", b); nln++; }
        fclose(cf);
    }
    for (int i = 0; i < 10; i++) {
        int j = nln - 10 + i; const char *cl = j >= 0 ? lines[j % 10] : "";
        fprintf(f, "chat_%d=%s\nchat_cls_%d=%s\n", i, cl, i, "");
    }
    fclose(f); rename(tmp, p);
}

static void staff_chat(void) {
    if (!flag_get("running", 1) || flag_get("int", 0) || nstaff < 2) return;
    static int tick;
    if (++tick % 4) return;
    int a = tick % nstaff, b = (a + 1) % nstaff;
    const char *bit[] = {"the fun score is thin", "I can take the polish", "graphics need another pass", "ship when review opens"};
    char line[160];
    snprintf(line, sizeof line, "%s: %s, %s", staff[a].name, staff[b].name, bit[tick % 4]);
    log_line(line);
}

static int known_job(const char *j) {
    return !strcmp(j,"writer")||!strcmp(j,"designer")||!strcmp(j,"programmer")||!strcmp(j,"artist")||!strcmp(j,"sound")||!strcmp(j,"director");
}

static void do_hire(const char *job, const char *name) {
    if (!known_job(job)) { snprintf(note, sizeof note, "unknown job"); return; }
    if (nstaff >= OFFICE_CAP) { snprintf(note, sizeof note, "office full"); log_line(note); return; }
    if (funds < HIRE_COST) { snprintf(note, sizeof note, "broke"); log_line(note); return; }
    funds -= HIRE_COST;
    Staff *s = &staff[nstaff];
    snprintf(s->id, 8, "s%d", nstaff + 1);
    snprintf(s->name, 16, "%s", name && name[0] ? name : "New");
    snprintf(s->job, 16, "%s", job);
    s->scenario = s->graphics = s->programming = s->sound = s->creativity = 30;
    s->energy = 80; s->salary = 2000; s->used = 1; nstaff++;
    char b[80]; snprintf(b, sizeof b, "hired %s the %s", s->name, s->job); log_line(b);
    event_row("hire", job);
}

static void do_train(const char *spec) {
    char buf[32]; snprintf(buf, sizeof buf, "%s", spec);
    char *c = strchr(buf, ':'); if (!c) { snprintf(note, sizeof note, "train staff:stat"); return; }
    *c = 0; Staff *s = find_staff(buf); int *sp = s ? stat_ptr(s, c + 1) : NULL;
    if (!s || !sp) { snprintf(note, sizeof note, "no such train"); return; }
    if (s->energy < 20) { snprintf(note, sizeof note, "%s is tired", s->name); log_line(note); return; }
    *sp += 10; if (*sp > 500) *sp = 500;
    s->energy -= 20;
    pay_week();
    char b[80]; snprintf(b, sizeof b, "%s trains %s", s->name, c + 1); log_line(b);
    event_row("train", spec);
}

static void do_rest(const char *id) {
    Staff *s = find_staff(id); if (!s) { snprintf(note, sizeof note, "no such staff"); return; }
    s->energy = 100; pay_week();
    char b[80]; snprintf(b, sizeof b, "%s rests", s->name); log_line(b);
    event_row("rest", id);
}

static void do_start(const char *pair) {
    if (open_game()) { snprintf(note, sizeof note, "busy"); log_line(note); return; }
    char buf[40]; snprintf(buf, sizeof buf, "%s", pair);
    char *c = strchr(buf, ':'); if (!c) { snprintf(note, sizeof note, "start genre:type"); return; }
    *c = 0;
    const char *genres = " action rpg puzzle "; const char *types = " sim adventure shooter ";
    char gbuf[20], tbuf[20]; snprintf(gbuf, sizeof gbuf, " %s ", buf); snprintf(tbuf, sizeof tbuf, " %s ", c + 1);
    if (!strstr(genres, gbuf) || !strstr(types, tbuf)) { snprintf(note, sizeof note, "unknown genre or type"); log_line(note); return; }
    Game *g = &games[ngames++];
    memset(g, 0, sizeof *g);
    snprintf(g->id, 8, "g%d", ngames);
    snprintf(g->title, 24, "%s %s", buf, c + 1);
    snprintf(g->genre, 16, "%s", buf); snprintf(g->type, 16, "%s", c + 1);
    snprintf(g->phase, 16, "proposal");
    g->open = 1;
    char b[80]; snprintf(b, sizeof b, "started %s", g->title); log_line(b);
    event_row("start", pair);
}

static void do_assign(const char *id) {
    Game *g = open_game(); Staff *s = find_staff(id);
    if (!g || !s) { snprintf(note, sizeof note, "need a game and a person"); return; }
    if (strstr(g->assigned, id)) { snprintf(note, sizeof note, "%s already on it", s->name); return; }
    if (strlen(g->assigned) + strlen(id) + 2 >= sizeof g->assigned) return;
    if (g->assigned[0]) strncat(g->assigned, ",", sizeof g->assigned - strlen(g->assigned) - 1);
    strncat(g->assigned, id, sizeof g->assigned - strlen(g->assigned) - 1);
    char b[80]; snprintf(b, sizeof b, "%s joins %s", s->name, g->title); log_line(b);
    event_row("assign", id);
}

static void do_advance(void) {
    Game *g = open_game();
    if (!g) { snprintf(note, sizeof note, "no game"); return; }
    if (!strcmp(g->phase, "review")) { snprintf(note, sizeof note, "in review: ship"); log_line(note); return; }
    int debug = !strcmp(g->phase, "debug");
    apply_phase(g, debug);
    if (!strcmp(g->phase, "proposal")) snprintf(g->phase, 16, "production");
    else if (!strcmp(g->phase, "production")) snprintf(g->phase, 16, "debug");
    else snprintf(g->phase, 16, "review");
    pay_week();
    char b[80]; snprintf(b, sizeof b, "%s moves to %s", g->title, g->phase); log_line(b);
    event_row("advance", g->phase);
}

static void do_advertise(const char *tier) {
    long cost = 15000, add = 15;
    if (!strcmp(tier, "small")) { cost = 5000; add = 5; }
    else if (!strcmp(tier, "big")) { cost = 40000; add = 40; }
    else tier = "mid";
    if (funds < cost) { snprintf(note, sizeof note, "broke"); log_line(note); return; }
    funds -= cost; hype += add; fame += add / 5;
    char b[80]; snprintf(b, sizeof b, "advertised %s  hype %ld", tier, hype); log_line(b);
}

static void do_ship(void) {
    Game *g = open_game();
    if (!g) { snprintf(note, sizeof note, "no game"); return; }
    if (strcmp(g->phase, "review")) { snprintf(note, sizeof note, "not in review"); log_line(note); return; }
    int rank = (g->fun + g->creativity + g->polish + g->graphics + g->sound) / 5;
    if (rank > 100) rank = 100;
    long sales = (long)rank * 1000 + fame * 10 + hype * 100;
    funds += sales; fame += rank / 10; if (fame < 0) fame = 0; hype = 0;
    g->open = 0;
    char b[120]; snprintf(b, sizeof b, "shipped %s  rank %d  sales %ld", g->title, rank, sales); log_line(b);
    event_row("ship", g->id);
}

void gd_export_office(const char *app);

static void verb(int argc, char **argv) {
    const char *v = argc > 0 ? argv[0] : "status";
    if (!strcmp(v, "hire") && argc >= 2) do_hire(argv[1], argc >= 3 ? argv[2] : "");
    else if (!strcmp(v, "advertise") && argc >= 2) do_advertise(argv[1]);
    else if (!strcmp(v, "train") && argc >= 2) do_train(argv[1]);
    else if (!strcmp(v, "rest") && argc >= 2) do_rest(argv[1]);
    else if (!strcmp(v, "start") && argc >= 2) do_start(argv[1]);
    else if (!strcmp(v, "assign") && argc >= 2) do_assign(argv[1]);
    else if (!strcmp(v, "advance")) do_advance();
    else if (!strcmp(v, "ship")) do_ship();
    else if (!strcmp(v, "status")) { if (!note[0]) snprintf(note, sizeof note, "ready"); }
    else if (!strcmp(v, "exportbook")) { if (!note[0]) snprintf(note, sizeof note, "office book"); }
    else if (!strcmp(v, "int")) {
        int on = !flag_get("int", 0); flag_set("int", on); read_active();
        snprintf(note, sizeof note, on ? "INT on: %s" : "INT off", nstaff ? staff[g_active].name : "nobody");
        log_line(note);
    }
    else if (!strcmp(v, "select") && argc >= 2) {
        read_active();
        for (int i = 0; i < nstaff; i++) if (!strcmp(staff[i].id, argv[1]) || !strcmp(staff[i].name, argv[1])) g_active = i;
        write_active();
        snprintf(note, sizeof note, "selected %s", staff[g_active].name);
    }
    else if (!strcmp(v, "toggle") && argc >= 2) {
        int on = !flag_get(argv[1], 1); flag_set(argv[1], on);
        snprintf(note, sizeof note, "%s %s", argv[1], on ? "on" : "off");
    }
    else if (!strcmp(v, "play")) { flag_set("running", 1); snprintf(note, sizeof note, "GO started"); log_line(note); }
    else if (!strcmp(v, "stop")) { flag_set("running", 0); snprintf(note, sizeof note, "STOP stopped"); log_line(note); }
    else if (!strcmp(v, "say") && argc >= 2) {
        char b[160]; snprintf(b, sizeof b, "You: %s", argv[1]); log_line(b);
        read_active();
        if (nstaff) { snprintf(b, sizeof b, "%s: heard", staff[g_active].name); log_line(b); }
        snprintf(note, sizeof note, "said");
    }
    else if (!strcmp(v, "clearchat")) {
        char p[1200]; snprintf(p, sizeof p, "%s/chat.txt", ST);
        FILE *f = fopen(p, "w"); if (f) fclose(f);
        snprintf(note, sizeof note, "chat cleared");
    }
    else if (!strcmp(v, "step") && argc >= 3) {
        read_active();
        int dx = atoi(argv[1]), dy = atoi(argv[2]);
        int x = 2 + g_active * 3, y = 5;
        char pp[1200]; snprintf(pp, sizeof pp, "%s/pos.txt", ST);
        FILE *pf = fopen(pp, "r");
        int xs[8], ys[8]; for (int i = 0; i < 8; i++) { xs[i] = 2 + (i % 4) * 3; ys[i] = 5; }
        if (pf) { char id[16]; int px, py; while (fscanf(pf, "%15s %d %d", id, &px, &py) == 3) for (int i = 0; i < nstaff; i++) if (!strcmp(staff[i].id, id)) { xs[i] = px; ys[i] = py; } fclose(pf); }
        xs[g_active] += dx; ys[g_active] += dy;
        if (xs[g_active] < 1) xs[g_active] = 1; if (xs[g_active] > 11) xs[g_active] = 11;
        if (ys[g_active] < 2) ys[g_active] = 2; if (ys[g_active] > 7) ys[g_active] = 7;
        pf = fopen(pp, "w"); if (pf) { for (int i = 0; i < nstaff; i++) fprintf(pf, "%s %d %d\n", staff[i].id, xs[i], ys[i]); fclose(pf); }
        snprintf(note, sizeof note, "%s steps", staff[g_active].name);
    }
    else snprintf(note, sizeof note, "verbs: hire train rest start assign advance ship int select say play stop step");
    save_all(); write_ui();
    gd_export_office(APP);
    printf("%s\n", note);
}

static void locate(const char *exe) {
    char d[1024]; snprintf(d, sizeof d, "%s", exe);
    for (int i = 0; i < 3; i++) { char *s = strrchr(d, '/'); if (s) *s = 0; }
    snprintf(APP, sizeof APP, "%s", d);
    snprintf(ST, sizeof ST, "%s/state", APP);
    mkdir(ST, 0755);
}

int main(int argc, char **argv) {
    char exe[PATH_MAX]; if (!realpath(argv[0], exe)) snprintf(exe, sizeof exe, "%s", argv[0]);
    int resident = (argc == 3 && is_dir(argv[1]) && is_dir(argv[2]));
    if (resident) snprintf(APP, sizeof APP, "%s", argv[2]);
    else locate(exe);
    snprintf(ST, sizeof ST, "%s/state", APP);
    mkdir(ST, 0755);
    load_all(); seed(); gd_export_office(APP); write_ui();
    if (resident) {
        while (1) { load_all(); staff_chat(); write_ui(); sleep(2); }
    }
    verb(argc - 1, argv + 1);
    return 0;
}

/* The playthrough harness compiles this one file. The office painter stays in its own file. */
#include "office_book.c"
