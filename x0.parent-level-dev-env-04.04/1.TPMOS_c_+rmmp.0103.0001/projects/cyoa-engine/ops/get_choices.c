#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

void trim(char *s) {
    char *p = s;
    int l = strlen(p);
    while(l > 0 && (isspace(p[l - 1]) || p[l-1] == '*')) p[--l] = 0;
    while(*p && (isspace(*p) || *p == '*')) ++p, --l;
    memmove(s, p, l + 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    FILE *fp = fopen(argv[1], "r");
    if (!fp) return 1;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *ptr = strcasestr(line, "turn to page ");
        if (ptr) {
            char *page_num_str = ptr + strlen("turn to page ");
            int page_num = atoi(page_num_str);
            if (page_num > 0) {
                char *start = strchr(line, '*');
                if (!start) start = line; else start++;
                char temp[1024];
                strncpy(temp, start, 1023); temp[1023] = '\0';
                char *ptr_in_temp = strcasestr(temp, "turn to page ");
                if (ptr_in_temp) *ptr_in_temp = '\0';
                trim(temp);
                if (strlen(temp) > 0) printf("%d|%s\n", page_num, temp);
            }
        }
    }
    fclose(fp);
    return 0;
}
