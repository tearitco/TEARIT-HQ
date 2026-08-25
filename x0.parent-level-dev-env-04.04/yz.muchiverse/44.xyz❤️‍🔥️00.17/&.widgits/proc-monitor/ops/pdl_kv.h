/* minimal PDL STATE | KEY | VALUE reader for proc-monitor ops */
#ifndef PROC_PDL_KV_H
#define PROC_PDL_KV_H
#include <stdio.h>
#include <string.h>

static void pdl_read_state_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *k = p1 + 1;
        while (*k == ' ' || *k == '\t') k++;
        char *ke = k + strlen(k) - 1;
        while (ke > k && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
        if (strcmp(k, key) != 0) continue;
        char *v = p2 + 1;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *ve = v + strlen(v) - 1;
        while (ve > v && (*ve == ' ' || *ve == '\t')) *ve-- = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}
#endif
