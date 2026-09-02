/* hatch_egg - one verb, one binary, no shared headers.
 * Flips an egg piece into a pet: type=egg -> type=pet, hatched=0 -> 1,
 * rolls starting stats from the species' rarity, and runs the emoji
 * pipeline for real (emoji_gen_atlas -> emoji_xtract) to produce the
 * pet's own sprite.csv, which egg_window reads as a GL texture.
 *
 * Usage: hatch_egg.+x <pet_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

#define MAX_LINE 512
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)
#define SPRITE_RES 32 /* NxN sprite.csv resolution egg_window expects */

static char project_root[PROJ_MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

#ifdef _WIN32
/* Windows has no real argv array for CreateProcess - just one command
 * line string - so each arg has to be quoted with the documented MS CRT
 * algorithm or the child's own argv parsing splits it wrong (this matters
 * here specifically because project_root can contain spaces, e.g. a path
 * under "OneDrive\Desktop\...(1)"). Also sidesteps system()'s cmd.exe,
 * which only understands double-quote quoting anyway (this file's POSIX
 * branch below uses single quotes, a shell convention cmd.exe doesn't
 * share) - CreateProcess is called directly instead.
 *
 * Wide (CreateProcessW), not ANSI: this file's whole reason to shell out
 * is passing a species' emoji glyph as an argument, and CreateProcessA
 * silently mangles anything outside the system's ANSI codepage into '?'
 * before the child ever sees it (verified: a 4-byte UTF-8 emoji arrived
 * as two literal '?' bytes) - every argument is converted UTF-8 -> UTF-16
 * before quoting so the real codepoint survives the trip. */
static void win_quote_arg_w(const wchar_t *arg, wchar_t *out, size_t out_sz) {
    size_t len = 0;
    if (out_sz < 3) { out[0] = L'\0'; return; }
    out[len++] = L'"';
    for (const wchar_t *p = arg; *p && len < out_sz - 2; ) {
        size_t backslashes = 0;
        while (*p == L'\\') { backslashes++; p++; }
        if (*p == L'"' || *p == L'\0') {
            for (size_t i = 0; i < backslashes * 2 && len < out_sz - 2; i++) out[len++] = L'\\';
            if (*p == L'"') { if (len < out_sz - 2) { out[len++] = L'\\'; out[len++] = L'"'; } p++; }
        } else {
            for (size_t i = 0; i < backslashes && len < out_sz - 2; i++) out[len++] = L'\\';
            if (len < out_sz - 2) out[len++] = *p;
            p++;
        }
    }
    out[len++] = L'"';
    out[len] = L'\0';
}

static void utf8_to_wide(const char *utf8, wchar_t *out, size_t out_wchars) {
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_wchars)) out[0] = L'\0';
}

/* Runs exe_path with a NULL-terminated UTF-8 argv (not counting exe_path
 * itself) and waits for it, mirroring system()'s "block and return an
 * exit status" shape without going through cmd.exe. */
static int win_run_wait(const char *exe_path, char *const argv[]) {
    wchar_t cmdline[PATH_BUF * 4];
    wchar_t wbuf[PATH_BUF], qbuf[PATH_BUF];

    utf8_to_wide(exe_path, wbuf, PATH_BUF);
    win_quote_arg_w(wbuf, qbuf, PATH_BUF);
    size_t pos = (size_t)swprintf(cmdline, PATH_BUF * 4, L"%ls", qbuf);
    for (int i = 0; argv[i] && pos < (PATH_BUF * 4); i++) {
        utf8_to_wide(argv[i], wbuf, PATH_BUF);
        win_quote_arg_w(wbuf, qbuf, PATH_BUF);
        pos += (size_t)swprintf(cmdline + pos, (PATH_BUF * 4) - pos, L" %ls", qbuf);
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return (int)code;
}
#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/pieces/world_01/map_lobby/%s", project_root, pet_id);
    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", dir);

    FILE *f = fopen(state_path, "r");
    if (!f) { printf("Hatch failed: unknown piece.\n"); return 1; }

    char lines[32][MAX_LINE];
    int nlines = 0;
    int hatched = 0, rarity = 1;
    char emoji[32] = "";
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "hatched") == 0) hatched = atoi(eq + 1);
            else if (strcmp(lines[nlines], "rarity") == 0) rarity = atoi(eq + 1);
            else if (strcmp(lines[nlines], "species_emoji") == 0) snprintf(emoji, sizeof(emoji), "%s", eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (hatched) { printf("%s has already hatched.\n", pet_id); return 0; }
    if (!emoji[0]) { printf("Hatch failed: no species_emoji on %s.\n", pet_id); return 1; }

    int hp = 10 + rarity * 10;
    int mp = 5 + rarity * 5;

    /* Run the emoji pipeline for real: glyph -> PNG -> NxN pixel CSV. */
    char png_path[PATH_BUF + 32], csv_path[PATH_BUF + 32];
    snprintf(png_path, sizeof(png_path), "%s/atlas.png", dir);
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", dir);

    int rc1, rc2;
#ifndef _WIN32
    char cmd[PATH_BUF * 4];
#endif
#ifdef _WIN32
    char exe_path[PATH_BUF];
    char sprite_res_str[16];
    snprintf(sprite_res_str, sizeof(sprite_res_str), "%d", SPRITE_RES);

    snprintf(exe_path, sizeof(exe_path), "%s/system/emoji_gen_atlas.exe", project_root);
    char *gen_atlas_argv[] = { emoji, png_path, NULL };
    rc1 = win_run_wait(exe_path, gen_atlas_argv);

    snprintf(exe_path, sizeof(exe_path), "%s/system/emoji_xtract.exe", project_root);
    char *xtract_argv[] = { png_path, "0", sprite_res_str, csv_path, NULL };
    rc2 = win_run_wait(exe_path, xtract_argv);
#else
    snprintf(cmd, sizeof(cmd), "'%s/system/emoji_gen_atlas' '%s' '%s'", project_root, emoji, png_path);
    rc1 = system(cmd);
    snprintf(cmd, sizeof(cmd), "'%s/system/emoji_xtract' '%s' 0 %d '%s'", project_root, png_path, SPRITE_RES, csv_path);
    rc2 = system(cmd);
#endif
    if (rc1 != 0 || rc2 != 0) {
        printf("Hatch warning: sprite generation failed, hatching without sprite.\n");
    }

    f = fopen(state_path, "w");
    if (!f) { printf("Hatch failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "type") == 0) { fprintf(f, "type=pet\n"); *eq = '='; continue; }
            if (strcmp(lines[i], "hatched") == 0) { fprintf(f, "hatched=1\n"); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    fprintf(f, "hp=%d\n", hp);
    fprintf(f, "hp_max=%d\n", hp);
    fprintf(f, "mp=%d\n", mp);
    fprintf(f, "mp_max=%d\n", mp);
    fprintf(f, "skills=peck\n");
    fprintf(f, "hunger=0\n");
    fprintf(f, "energy=100\n");
    fprintf(f, "poop_count=0\n");
    fprintf(f, "poop_timer=0\n");
    fprintf(f, "asleep=0\n");
    fprintf(f, "level=1\n");
    fprintf(f, "xp=0\n");
    /* Center of tick_pets.c's WORLD_GRID_W x WORLD_GRID_H logical grid -
     * duplicated constant, same "no shared header" convention as
     * elsewhere; keep this in sync with tick_pets.c if that ever changes. */
    fprintf(f, "grid_x=20\n");
    fprintf(f, "grid_y=15\n");
    fprintf(f, "z=0\n");
    fprintf(f, "facing=right\n");
    fprintf(f, "tick_seq=0\n");
    fprintf(f, "card_status=none\n");
    fprintf(f, "card_serial=0\n");
    fprintf(f, "card_id=\n");
    fclose(f);

    printf("%s hatched! HP:%d MP:%d\n", pet_id, hp, mp);
    return 0;
}
