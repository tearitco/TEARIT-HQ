#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: possess <pet_id>");
        return 1;
    }
    
    FILE *f = fopen("pieces/manager/active_target.txt", "w");
    if (f) {
        fprintf(f, "%s\n", argv[1]);
        fclose(f);
    }
    printf("Possessing %s...", argv[1]);
    return 0;
}
