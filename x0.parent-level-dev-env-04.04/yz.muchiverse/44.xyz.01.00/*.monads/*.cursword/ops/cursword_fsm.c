/* cursword_fsm.c — cursword's onboarding walk-through, v1.
 *
 * A SEPARATE small process (house standard: no linking, no renderer/
 * manager edits). It drives the real taskbar input surfaces the same
 * way nav.sh / the test harnesses do:
 *   - append the resolved USER-cell code (4002) to
 *     #.desktop/strip_history.txt
 *   - append decimal key codes to #.desktop/livedesk_agent_relay.txt
 *   - read back #.desktop/strip_state.txt to confirm each step landed
 *
 * "Walk to the door" scope (owner, 2026-09-03): for a guest (not
 * logged in), on launch, cursword opens the USER menu, activates
 * "New User...", arms the text field, and then NARRATES — the human
 * types their own username + display name. cursword watches the
 * two-stage cli_io flow and narrates each transition, then points at
 * the next step (avatar). It never types the account fields itself
 * unless --auto is passed.
 *
 * Narration goes to <pkg>/say_log.txt (one line per beat). That file
 * is the seam for the future: cursword_say() will call the local
 * "gemma" harness for the line and fall back to the canned string on
 * any failure (the Harnecient pattern — model optional, deterministic
 * fallback always present); cursword_tts() will speak new lines;
 * cursword_stt_poll() will read the user's voice. All three are real
 * functions here, stubbed, with the fallback path already wired.
 *
 * Build:  sh ops/build_cursword_fsm.sh
 * Run:    +x/cursword_fsm.+x <house_root> [--auto]
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* ---------- small helpers ---------------------------------------------- */

static char g_house[1024];
static char g_pkg[1024];        /* cursword package dir (entities/cursword)  */
static char g_login[1024];      /* the 00.login-signup app dir               */
static char g_relay[1024];      /* #.desktop/livedesk_agent_relay.txt        */
static char g_strhist[1024];    /* #.desktop/strip_history.txt              */
static char g_strstate[1024];   /* #.desktop/strip_state.txt                */
static char g_saylog[1024];     /* <pkg>/say_log.txt                        */
static char g_fsmlog[1024];     /* <pkg>/cursword_fsm.log                   */
static char g_fsmstate[1024];   /* <pkg>/cursword_fsm.state                 */
static char g_ttsq[1024];       /* <pkg>/tts_queue.txt                      */
static char g_sttin[1024];      /* <pkg>/stt_in.txt                         */
static int  g_auto = 0;

static void nap_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static long now_s(void) { return (long)time(NULL); }

static void logline(const char *tag, const char *msg) {
    FILE *f = fopen(g_fsmlog, "a");
    if (f) { fprintf(f, "%ld %s %s\n", now_s(), tag, msg); fclose(f); }
    fprintf(stderr, "[cursword_fsm] %s %s\n", tag, msg);
}

static void set_state(const char *s) {
    FILE *f = fopen(g_fsmstate, "w");
    if (f) { fprintf(f, "%s\n", s); fclose(f); }
    logline("STATE", s);
}

/* read a whole small file into buf; returns length (0 if missing/empty) */
static size_t slurp(const char *path, char *buf, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = 0; return 0; }
    size_t r = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[r] = 0;
    return r;
}

/* trim trailing whitespace in place */
static void rstrip(char *s) {
    size_t l = strlen(s);
    while (l && (s[l-1] == '\n' || s[l-1] == '\r' || s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = 0;
}

/* ---------- the three future-facing hooks ---------------------------- */

/* cursword_say: v1 writes the fallback line to say_log.txt (and tts
 * queue). FUTURE: build a prompt from `intent`, call the local gemma
 * harness (POST /api/chat, no schema), use its plain-text reply if it
 * comes back non-empty within a short timeout, else `fallback`. The
 * call site never changes — only the body of this function. */
static void cursword_say(const char *intent, const char *fallback) {
    const char *line = fallback;   /* <-- gemma result would replace this */
    (void)intent;

    FILE *f = fopen(g_saylog, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
    printf("cursword: %s\n", line);
    fflush(stdout);
    /* cursword_tts hook */
    FILE *t = fopen(g_ttsq, "a");
    if (t) { fprintf(t, "%s\n", line); fclose(t); }
}

/* cursword_stt_poll: v1 reads (and clears) <pkg>/stt_in.txt if a line
 * is waiting — lets a human or a test simulate a spoken reply.
 * FUTURE: real speech-to-text. Returns 1 and fills `out` if something
 * was heard, else 0. */
static int cursword_stt_poll(char *out, size_t n) {
    char buf[512];
    if (slurp(g_sttin, buf, sizeof(buf)) == 0) return 0;
    rstrip(buf);
    if (!buf[0]) return 0;
    remove(g_sttin);
    snprintf(out, n, "%s", buf);
    return 1;
}

/* ---------- relay drivers ------------------------------------------- */

static void relay_code(int code) {
    FILE *f = fopen(g_relay, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
    nap_ms(350);   /* parser polls ~300ms; match nav.sh's spacing */
}

static void mgr_code(int code) {
    FILE *f = fopen(g_strhist, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
    nap_ms(500);
}

/* ---------- strip_state.txt readers ------------------------------- */

/* pull the value of a `KEY | <key> | <value>` line from strip_state.txt */
static int strstate_get(const char *key, char *out, size_t n) {
    char buf[8192];
    if (slurp(g_strstate, buf, sizeof(buf)) == 0) { out[0] = 0; return 0; }
    char needle[128];
    snprintf(needle, sizeof(needle), "KEY | %s |", key);
    char *p = strstr(buf, needle);
    if (!p) { out[0] = 0; return 0; }
    p += strlen(needle);
    while (*p == ' ') p++;
    size_t i = 0;
    while (*p && *p != '\n' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
    rstrip(out);
    return 1;
}

static int strstate_int(const char *key, int dflt) {
    char v[64];
    if (!strstate_get(key, v, sizeof(v)) || !v[0]) return dflt;
    return atoi(v);
}

/* wait until predicate() is true or `timeout_s` elapses. Returns 1 ok. */
static int wait_for(int (*pred)(void), int timeout_s, const char *what) {
    long deadline = now_s() + timeout_s;
    while (now_s() <= deadline) {
        if (pred()) return 1;
        nap_ms(300);
    }
    { char m[128]; snprintf(m, sizeof(m), "timeout waiting for %s", what); logline("WARN", m); }
    return 0;
}

static int p_menu_open(void)   { return strstate_int("hq_open", 0) == 2; }

/* ---------- guest detection --------------------------------------- */

/* guest == current_login.txt missing or empty (matches the manager's
 * own whoami fallback to "guest"). */
static int is_guest(void) {
    char p[1200], buf[512];
    snprintf(p, sizeof(p), "%s/current_login.txt", g_login);
    size_t r = slurp(p, buf, sizeof(buf));
    rstrip(buf);
    return (r == 0 || buf[0] == 0);
}

/* ---------- path setup ------------------------------------------------ */

/* find a child dir of `parent` whose name begins with `prefix` */
static int find_child_prefixed(const char *parent, const char *prefix, char *out, size_t n) {
    DIR *d = opendir(parent);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, prefix, strlen(prefix)) == 0) {
            snprintf(out, n, "%s/%s", parent, e->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

static int setup_paths(void) {
    /* login-signup app lives at  <userpal-dir>/00.login-signup  where the
       userpal dir name starts with "0.user-pal" (it carries an emoji). */
    char userpal[1200];
    if (!find_child_prefixed(g_house, "0.user-pal", userpal, sizeof(userpal))) {
        logline("FATAL", "no 0.user-pal* dir under house root");
        return 0;
    }
    snprintf(g_login, sizeof(g_login), "%s/00.login-signup", userpal);

    /* cursword package dir (literal '*' chars in the path names) */
    snprintf(g_pkg, sizeof(g_pkg),
             "%s/*.monads/*.cursword/entities/cursword", g_house);
    { struct stat st; if (stat(g_pkg, &st) != 0) { logline("FATAL", "cursword pkg dir not found"); return 0; } }

    snprintf(g_relay,    sizeof(g_relay),    "%s/#.desktop/livedesk_agent_relay.txt", g_house);
    snprintf(g_strhist,  sizeof(g_strhist),  "%s/#.desktop/strip_history.txt",        g_house);
    snprintf(g_strstate, sizeof(g_strstate), "%s/#.desktop/strip_state.txt",          g_house);
    snprintf(g_saylog,   sizeof(g_saylog),   "%s/say_log.txt",         g_pkg);
    snprintf(g_fsmlog,   sizeof(g_fsmlog),   "%s/cursword_fsm.log",    g_pkg);
    snprintf(g_fsmstate, sizeof(g_fsmstate), "%s/cursword_fsm.state",  g_pkg);
    snprintf(g_ttsq,     sizeof(g_ttsq),     "%s/tts_queue.txt",       g_pkg);
    snprintf(g_sttin,    sizeof(g_sttin),    "%s/stt_in.txt",          g_pkg);
    return 1;
}

/* ---------- the walk-through --------------------------------------- */

static int run_onboarding(void) {
    set_state("OFFER");
    cursword_say("greet.guest",
        "Hi! I'm cursword. You're not signed in yet - let's make you an account. Two quick fields.");

    /* clear any stale menu / typing state */
    set_state("OPEN_USER");
    relay_code(27); relay_code(27);              /* Esc x2 */
    mgr_code(4002);                              /* open the USER header cell */
    if (!wait_for(p_menu_open, 6, "USER menu to open")) {
        cursword_say("err.taskbar",
            "I couldn't reach the taskbar just now. Click the person icon up top and pick \"New User...\" to sign up.");
        set_state("ERROR");
        return 1;
    }

    /* Activating "New User..." now opens the real signup-hq WINDOW
     * (its own manager + .chtpm, launched via livedesk_launchers.pdl) -
     * not the old one-line strip cli_io. So the FSM's job here is just:
     * trigger it, narrate, and watch current_login.txt. */
    set_state("NEW_USER");
    relay_code(13);                              /* activate focused row 0 = "New User..." */
    nap_ms(1500);                                /* window + its manager come up */

    if (g_auto) {
        /* --auto: drive the signup-hq window through its OWN request
         * file (the store-demo / hands-off path). */
        char reqdir[1400], req[1500], id[64];
        snprintf(reqdir, sizeof(reqdir), "%s/#.desktop/signup_hq", g_house);
        mkdir(reqdir, 0777);
        snprintf(req, sizeof(req), "%s/request.txt", reqdir);
        snprintf(id, sizeof(id), "guest_%ld", now_s());
        set_state("AUTO_ID");
        cursword_say("auto.id", "Filling in a temporary account for you.");
        { FILE *f = fopen(req, "w"); if (f) { fprintf(f, "setid:%s\n", id); fclose(f); } }
        nap_ms(1200);
        set_state("AUTO_NAME");
        { FILE *f = fopen(req, "w"); if (f) { fprintf(f, "setname:New Player\n"); fclose(f); } }
    } else {
        /* walk-to-the-door: the window has two clear fields; just guide. */
        set_state("WATCH_SIGNUP");
        cursword_say("prompt.window",
            "A sign-up window just opened. Type a username, Enter, then a display name, Enter. I'll wait.");
    }

    /* wait for the account to actually exist */
    set_state("WATCH_DONE");
    {
        long deadline = now_s() + 180;
        while (now_s() <= deadline) {
            if (!is_guest()) break;
            char heard[256];
            if (cursword_stt_poll(heard, sizeof(heard))) {
                char m[300]; snprintf(m, sizeof(m), "heard: %s", heard); logline("STT", m);
            }
            nap_ms(400);
        }
    }

    if (is_guest()) {
        cursword_say("done.notyet",
            "Still not signed in - that's fine. Pick \"New User...\" from the person icon when you want to.");
        set_state("IDLE");
        return 0;
    }

    set_state("DONE");
    cursword_say("done.welcome",
        "You're in! Next you can build an avatar - the person icon has that too. I'll be around.");
    set_state("IDLE");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: cursword_fsm.+x <house_root> [--auto]\n");
        return 2;
    }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    for (int i = 2; i < argc; i++) if (strcmp(argv[i], "--auto") == 0) g_auto = 1;

    /* strip trailing slash */
    { size_t l = strlen(g_house); if (l > 1 && g_house[l-1] == '/') g_house[l-1] = 0; }

    if (!setup_paths()) return 1;

    logline("START", g_auto ? "mode=auto" : "mode=walk");

    if (!is_guest()) {
        set_state("IDLE");
        logline("DONE", "already signed in - nothing to do");
        return 0;
    }

    return run_onboarding();
}
