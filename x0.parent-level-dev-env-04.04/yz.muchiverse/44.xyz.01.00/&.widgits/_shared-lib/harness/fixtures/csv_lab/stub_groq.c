/* stub_groq - test double for ^.hai-horn/ops/horn_chat_backend.+x, used ONLY by the csv_lab harnesses (never talks to a network).
 * Same contract as the real backend: argv[1] = prompt, reply written to $HORN_REPLY_FILE, usage numbers left in $PRISC_PROJECT_ROOT/pieces/horn/.req_raw.json.
 * Reads the rows after the line "INPUT" (N<TAB>English<TAB>context) and answers "N<TAB>Chinese<TAB>pinyin" from a tiny dictionary.
 * Special sources: "Tonefail" answers pinyin without tone marks unless the context holds FIX:, "Nevergood" is always wrong, "Missingrow" is never answered.
 * $CSV_STUB_MODE=quota exits 3 (rate limit), =fail exits 4, =sleep sleeps 30 s, =leak writes the key file text to stderr (to prove csv_lab never prints it).
 * $CSV_STUB_COUNT = file that gets one byte appended per call (call counter). Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o stub_groq stub_groq.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *D[][3] = {
    {"Water", "水", "shuǐ"}, {"Fire", "火", "huǒ"}, {"Acetic Acid", "乙酸", "yǐ suān"}, {"Ethanol", "乙醇", "yǐ chún"}, {"Hydrogen", "氢", "qīng"}, {"Salt", "盐", "yán"},
    {"Sodium Chloride", "氯化钠", "lǜ huà nà"}, {"Gold", "金", "jīn"}, {"The field looks green today", "今天田野看起来很绿", "jīn tiān tián yě kàn qǐ lái hěn lǜ"},
    {"Sulfuric Acid (H2SO4)", "硫酸 H2SO4", "liú suān H2SO4"}, {"BAD word", "坏BAD词", "huài cí"}, {"Tonefail", "水", "shuǐ"}, {"Nevergood", "水", "shui3"}, {NULL, NULL, NULL} };
int main(int argc, char **argv) {
    const char *mode = getenv("CSV_STUB_MODE"), *cnt = getenv("CSV_STUB_COUNT"), *rf = getenv("HORN_REPLY_FILE"), *ed = getenv("HORN_ENTITY_DIR"), *root = getenv("PRISC_PROJECT_ROOT");
    if (cnt) { FILE *c = fopen(cnt, "a"); if (c) { fputc('x', c); fclose(c); } }
    if (argc < 2 || !rf) return 1;
    if (mode && !strcmp(mode, "leak") && ed) { char p[4096], k[256] = ""; snprintf(p, sizeof p, "%s/raw_groq.txt", ed); FILE *f = fopen(p, "r"); if (f) { if (!fgets(k, sizeof k, f)) k[0] = 0; fclose(f); } fprintf(stderr, "backend debug: key=%s\n", k); }
    if (mode && !strcmp(mode, "quota")) return 3;
    if (mode && !strcmp(mode, "fail")) return 4;
    if (mode && !strcmp(mode, "sleep")) sleep(30);
    FILE *o = fopen(rf, "w"); if (!o) return 1;
    const char *p = strstr(argv[1], "\nINPUT"); p = p ? strchr(p + 1, '\n') : NULL;
    while (p && *p) {
        p++; char line[2048]; size_t n = 0; while (*p && *p != '\n' && n < sizeof line - 1) line[n++] = *p++; line[n] = 0;
        char *t1 = strchr(line, '\t'); if (!t1) continue; *t1++ = 0; char *t2 = strchr(t1, '\t'); const char *ctx = ""; if (t2) { *t2++ = 0; ctx = t2; }
        int fix = strstr(ctx, "FIX:") != NULL;
        if (!strcmp(t1, "Missingrow")) continue;
        int hit = 0;
        for (int i = 0; D[i][0]; i++) if (!strcmp(D[i][0], t1)) {
            const char *py = D[i][2];
            if (!strcmp(t1, "Tonefail") && !fix) py = "shui";
            fprintf(o, "%s\t%s\t%s\n", line, D[i][1], py); hit = 1; break; }
        if (!hit) fprintf(o, "%s\t译\tyì\n", line);
    }
    fclose(o);
    if (root) { char d[4096]; snprintf(d, sizeof d, "%s/pieces/horn/.req_raw.json", root); FILE *r = fopen(d, "w"); if (r) { fputs("{\"usage\":{\"prompt_tokens\":100,\"completion_tokens\":50,\"total_tokens\":150}}\n", r); fclose(r); } }
    puts("stub reply written");
    return 0;
}
