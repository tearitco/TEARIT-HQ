#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE *f = fopen("pieces/manager/active_target.txt", "w");
    if (f) {
        fprintf(f, "liz_char\n");
        fclose(f);
    }
    printf("Possessing liz_char...");
    return 0;
}
